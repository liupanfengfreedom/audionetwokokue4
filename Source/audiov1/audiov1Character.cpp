// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "audiov1Character.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Runtime/Engine/Public/TimerManager.h"

#include "Kismet/KismetSystemLibrary.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWaveProcedural.h"
#include "Generators/AudioGenerator.h"
#include "AudioCapture.h"
#include "Engine.h"
#include "TTGameInstance.h"
#include "UdpChatClient.h"
#include "UTcpCommunicatorv2.h"

//////////////////////////////////////////////////////////////////////////
// Aaudiov1Character

Aaudiov1Character::Aaudiov1Character()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // ...at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named MyCharacter (to avoid direct content references in C++)
	VoiceCaptureAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("VoiceCaptureAudioComponent"));
	VoiceCaptureAudioComponent->SetupAttachment(RootComponent);
	VoiceCaptureAudioComponent->bAutoActivate = true;
	VoiceCaptureAudioComponent->bAlwaysPlay = true;
	VoiceCaptureAudioComponent->PitchMultiplier = 1;
	VoiceCaptureAudioComponent->VolumeMultiplier = 5.f;
}
void Aaudiov1Character::BeginPlay()
{
	Super::BeginPlay();
	VoiceCaptureSoundWaveProcedural = NewObject<USoundWaveProcedural>();
	VoiceCaptureSoundWaveProcedural->SetSampleRate(24000);
	VoiceCaptureSoundWaveProcedural->NumChannels = 1;
	VoiceCaptureSoundWaveProcedural->Duration = INDEFINITELY_LOOPING_DURATION;
	VoiceCaptureSoundWaveProcedural->SoundGroup = SOUNDGROUP_Voice;
	VoiceCaptureSoundWaveProcedural->bLooping = false;
	VoiceCaptureSoundWaveProcedural->bProcedural = true;
	VoiceCaptureSoundWaveProcedural->Pitch = 1;
	VoiceCaptureSoundWaveProcedural->Volume = 2.f;
	VoiceCaptureAudioComponent->SetSound(VoiceCaptureSoundWaveProcedural);
	bislocalplayer = IsLocallyControlled();
	bIsDedicatedServer = UKismetSystemLibrary::IsDedicatedServer(this);
	if (bIsDedicatedServer)
	{
		return;
	}
	if (bislocalplayer)
	{
		UTTGameInstance* gameinstance = Cast<UTTGameInstance>(GetGameInstance());
		FString number = gameinstance->tcpcommunicationv2->number;
		FString roomid = gameinstance->tcpcommunicationv2->roomid;
		chatclient = new UdpChatClient(1);
		chatclient->localplayerjoinroom(number, roomid);
		chatclient->Onorderproxytoreportdelegate.BindLambda([this](const FString & str) {
			AsyncTask(
				ENamedThreads::GameThread,
				[this, str]() {
					OnClientCaptureVoice(str);
				}
			);
			});
		//GetWorldTimerManager().SetTimer(PlayVoiceCaptureTimer, this, &Aaudiov1Character::PlayVoiceCapture, 0.5f, true, 0.f);

		audiocapturev1 = UAudioCaptureFunctionLibrary::CreateAudioCapture();
		audiocapturev1->AddGeneratorDelegate([this](const float* InAudio, int32 NumSamples) {
			//GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Yellow, *FString("i: ").Append(FString::FromInt(NumSamples)));
#define DOWNSAMPLE 1
			int16 int16pcmdataarraysize = NumSamples >> DOWNSAMPLE;//downsample
			int16pcmdata.SetNumUninitialized(int16pcmdataarraysize);
			for (int i = 0; i < int16pcmdataarraysize; i++)
			{
				int16pcmdata[i] = InAudio[i << DOWNSAMPLE] * 32767;//downsample 32bit float to 16bit int 
			}
			int16 VoiceCaptureBufferarraysize = int16pcmdataarraysize << 1;
			VoiceCaptureBuffer.SetNumUninitialized(VoiceCaptureBufferarraysize);
			FMemory::Memcpy(VoiceCaptureBuffer.GetData(), int16pcmdata.GetData(), VoiceCaptureBufferarraysize);
			chatclient->sendunreliable1(VoiceCaptureBuffer.GetData(), VoiceCaptureBuffer.Num());
			//AsyncTask(
			//	ENamedThreads::GameThread,
			//	[this]() {
			//		//OnClientCaptureVoice(VoiceCaptureBuffer);
			//		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString(" audio local").Append(FString::FromInt(VoiceCaptureBuffer.Num())));

			//		VoiceCaptureSoundWaveProcedural->QueueAudio(VoiceCaptureBuffer.GetData(), VoiceCaptureBuffer.Num());
			//		bool isplaying = VoiceCaptureAudioComponent->IsPlaying();
			//		if (!isplaying)
			//		{
			//			VoiceCaptureAudioComponent->Play();
			//			//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, *FString("VoiceCaptureAudioComponent->Play()").Append(FString::FromInt(1)));
			//		}
			//	}
			//);

			});
		audiocapturev1->OpenDefaultAudioStream();
		int32 samplerate = audiocapturev1->GetSampleRate();
		int32 NumChannels = audiocapturev1->GetNumChannels();
		//VoiceCaptureSoundWaveProcedural->SetSampleRate(samplerate>> DOWNSAMPLE);
		//VoiceCaptureSoundWaveProcedural->NumChannels = NumChannels;
		GEngine->AddOnScreenDebugMessage(-1, 17.0f, FColor::Yellow, *FString("samplerate:").Append(FString::FromInt(samplerate)));
		GEngine->AddOnScreenDebugMessage(-1, 17.0f, FColor::Yellow, *FString("NumChannels:").Append(FString::FromInt(NumChannels)));
		//audiocapturev1->StartCapturingAudio();
		//audiocapturev1->StopCapturingAudio();
	}
}
//void Aaudiov1Character::PlayVoiceCapture()
//{
//	int32 availablebyte = VoiceCaptureSoundWaveProcedural->GetAvailableAudioByteCount();
//	if (availablebyte)
//	{
//		GEngine->AddOnScreenDebugMessage(-1, 0.5f, FColor::Green, *FString("availablebyte ").Append(FString::FromInt(availablebyte)));
//	}
//	else
//	{
//		VoiceCaptureAudioComponent->FadeOut(.2f, 0.f);
//		//GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, *FString("FadeOut"));
//	}
//}
void Aaudiov1Character::startcapturevoice()
{
	if (!audiocapturev1->IsCapturingAudio())
	{
		audiocapturev1->StartCapturingAudio();
	}

}
void Aaudiov1Character::stopcapturevoice()
{
	if (audiocapturev1->IsCapturingAudio())
	{
		audiocapturev1->StopCapturingAudio();
	}

}
void Aaudiov1Character::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAxis("MoveForward", this, &Aaudiov1Character::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &Aaudiov1Character::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &Aaudiov1Character::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &Aaudiov1Character::LookUpAtRate);

	// handle touch devices
	PlayerInputComponent->BindTouch(IE_Pressed, this, &Aaudiov1Character::TouchStarted);
	PlayerInputComponent->BindTouch(IE_Released, this, &Aaudiov1Character::TouchStopped);

	// VR headset functionality
	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &Aaudiov1Character::OnResetVR);
}


void Aaudiov1Character::OnResetVR()
{
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}

void Aaudiov1Character::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
		Jump();
}

void Aaudiov1Character::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
		StopJumping();
}

void Aaudiov1Character::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void Aaudiov1Character::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void Aaudiov1Character::MoveForward(float Value)
{
	if ((Controller != NULL) && (Value != 0.0f))
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void Aaudiov1Character::MoveRight(float Value)
{
	if ( (Controller != NULL) && (Value != 0.0f) )
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
	
		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}
bool Aaudiov1Character::OnClientCaptureVoice_Validate(const FString & str)
{
	return true;
}
void Aaudiov1Character::OnClientCaptureVoice_Implementation(const FString & str)
{
	OnServerreceiveVoice(str);
}
bool Aaudiov1Character::OnServerreceiveVoice_Validate(const FString & str)
{
	return true;
}
void Aaudiov1Character::OnServerreceiveVoice_Implementation(const FString & str)
{
	if (bIsDedicatedServer)
	{
		return;
	}
	if (bislocalplayer)
	{
		//GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, *FString("bislocalplayer").Append(FString::FromInt(it.Num())));
		return;
	}
	if (!VoiceCaptureSoundWaveProcedural)
	{
		return;
	}
	if (!VoiceCaptureAudioComponent)
	{
		return;
	}
	if (chatclient)
	{
		return;
	}
	UTTGameInstance* gameinstance = Cast<UTTGameInstance>(GetGameInstance());
	FString number = gameinstance->tcpcommunicationv2->number;//the local player's number on the client-side
	FString roomid = gameinstance->tcpcommunicationv2->roomid;//the roomid of the local player on the client-side
	chatclient = new UdpChatClient(1);								   //str  // this pawn is proxying some local player ,because this is execute in mulitcast,triger from some localplayer and execute on all it's proxy
	chatclient->proxyreport(number, roomid, str);
	chatclient->onunreliablebytestreamreceived.BindLambda([this](const TArray<uint8>& data) {
		AsyncTask(
			ENamedThreads::GameThread,
			[this, data]() {

				GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Yellow, FString(" audio").Append(FString::FromInt(data.Num())));
				VoiceCaptureSoundWaveProcedural->QueueAudio(data.GetData(), data.Num());
				bool isplaying = VoiceCaptureAudioComponent->IsPlaying();
				if (!isplaying)
				{
					VoiceCaptureAudioComponent->Play();
				}
			}
		);
		});


/////////////////////////////////////////////////////////////////////////////////////////
	//AsyncTask(
	//	ENamedThreads::GameThread,
	//	[this, it]() {
	//		VoiceCaptureSoundWaveProcedural->QueueAudio(it.GetData(), it.Num());
	//		bool isplaying = VoiceCaptureAudioComponent->IsPlaying();
	//		if (!isplaying)
	//		{
	//			VoiceCaptureAudioComponent->Play();
	//		}
	//	}
 //   );
	//VoiceCaptureSoundWaveProcedural->QueueAudio(it.GetData(), it.Num());
	//bool isplaying = VoiceCaptureAudioComponent->IsPlaying();
	//if (!isplaying)
	//{
	//	VoiceCaptureAudioComponent->Play();
	//}
}