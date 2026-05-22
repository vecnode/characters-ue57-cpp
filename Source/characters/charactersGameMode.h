// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UObject/SoftObjectPtr.h"
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

protected:

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;

	/** Optional pawn class override set from config/editor to avoid hardcoded asset paths. */
	UPROPERTY(EditDefaultsOnly, Config, Category="Classes")
	TSoftClassPtr<APawn> PreferredDefaultPawnClass;
};



