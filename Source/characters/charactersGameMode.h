// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "charactersGameMode.generated.h"

/**
 *  Runtime game mode with hard defaults for pawn/controller wiring.
 */
UCLASS()
class AcharactersGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	
	/** Constructor */
	AcharactersGameMode();
};



