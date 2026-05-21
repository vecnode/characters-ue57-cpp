// Copyright Epic Games, Inc. All Rights Reserved.

#include "charactersGameMode.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

AcharactersGameMode::AcharactersGameMode()
{
	// Bulletproof default pawn: always try to spawn BP_character1 in gameplay.
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(
		TEXT("/Game/MetaHumans/character1/BP_character1"));

	if (PlayerPawnBPClass.Class != nullptr)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}

	// Use the existing third person controller BP so Enhanced Input mapping contexts
	// configured in the editor continue to apply.
	static ConstructorHelpers::FClassFinder<APlayerController> PlayerControllerBPClass(
		TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController"));

	if (PlayerControllerBPClass.Class != nullptr)
	{
		PlayerControllerClass = PlayerControllerBPClass.Class;
	}
}
