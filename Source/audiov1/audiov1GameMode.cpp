// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "audiov1GameMode.h"
#include "audiov1Character.h"
#include "UObject/ConstructorHelpers.h"

Aaudiov1GameMode::Aaudiov1GameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPersonCPP/Blueprints/ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
