// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "audiov1Character.generated.h"

UCLASS(config=Game)
class Aaudiov1Character : public ACharacter
{
	GENERATED_BODY()
	bool bislocalplayer;
	bool bIsDedicatedServer;
	class UdpChatClient * chatclient;
	UPROPERTY()
	class UAudioCapture* audiocapturev1;
	class USoundWaveProcedural* VoiceCaptureSoundWaveProcedural;
//	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UAudioComponent* VoiceCaptureAudioComponent;
	UPROPERTY()
		TArray<uint8> VoiceCaptureBuffer;
	UPROPERTY()
		TArray<int16> int16pcmdata;
	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* FollowCamera;
public:
	Aaudiov1Character();

	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseTurnRate;

	/** Base look up/down rate, in deg/sec. Other scaling may affect final rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseLookUpRate;
	UFUNCTION(BlueprintCallable)
		void startcapturevoice();
	UFUNCTION(BlueprintCallable)
		void stopcapturevoice();
	FTimerHandle PlayVoiceCaptureTimer;
	void PlayVoiceCapture();
protected:
	virtual void BeginPlay();
protected:

	/** Resets HMD orientation in VR. */
	void OnResetVR();

	/** Called for forwards/backward input */
	void MoveForward(float Value);

	/** Called for side to side input */
	void MoveRight(float Value);

	/** 
	 * Called via input to turn at a given rate. 
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void TurnAtRate(float Rate);

	/**
	 * Called via input to turn look up/down at a given rate. 
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void LookUpAtRate(float Rate);

	/** Handler for when a touch input begins. */
	void TouchStarted(ETouchIndex::Type FingerIndex, FVector Location);

	/** Handler for when a touch input stops. */
	void TouchStopped(ETouchIndex::Type FingerIndex, FVector Location);

protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	// End of APawn interface
	UFUNCTION(Server, Reliable, Withvalidation)
		//UFUNCTION(Server, unreliable, Withvalidation)
		void OnClientCaptureVoice(const FString & str);
	virtual void OnClientCaptureVoice_Implementation(const FString & str);
	virtual bool OnClientCaptureVoice_Validate(const FString & str);
	UFUNCTION(NetMulticast, Reliable, Withvalidation)
		//UFUNCTION(NetMulticast, unreliable, Withvalidation)
		void OnServerreceiveVoice(const FString & str);
	virtual void OnServerreceiveVoice_Implementation(const FString & str);
	virtual bool OnServerreceiveVoice_Validate(const FString & str);
public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};

