// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UObject/SoftObjectPtr.h"
#include "charactersGameMode.generated.h"

/**
 *  Runtime game mode that possesses an existing placed character in the level.
 *  Falls back to spawning a new pawn only when no placed character is found.
 */
UCLASS()
class AcharactersGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	AcharactersGameMode();

protected:

	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void BeginPlay() override;

	/** Optional class filter for placed pawns to possess first. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Player")
	TSoftClassPtr<APawn> PreferredPlacedPawnClass;

	/** Optional actor tag used to pick a specific placed pawn (for example: PlayerCharacter). */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Player")
	FName PreferredPlacedPawnTag = TEXT("PlayerCharacter");

	/** Optional actor name/label from the World Outliner to pick first (for example: MAIN_CHARACTER). */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Player")
	FString PreferredPlacedPawnName = TEXT("MAIN_CHARACTER");

	/** If false, no pawn is spawned when a placed pawn cannot be found. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Player")
	bool bAllowSpawnFallback = false;

	/**
	 * If the preferred named actor exists but is not a Pawn, spawn a controllable pawn
	 * at that actor transform and hide the original actor to avoid a static duplicate.
	 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Player")
	bool bSpawnFromPreferredNamedActorIfNotPawn = true;

private:

	/** Runtime bridge pawn cache keyed by controller to avoid duplicate spawns across restarts. */
	TMap<TWeakObjectPtr<AController>, TWeakObjectPtr<APawn>> CachedBridgePawns;

	/** Removes invalid cache entries and returns a valid cached bridge pawn for this controller if one exists. */
	APawn* FindCachedBridgePawn(AController* Controller);

	/** Stores bridge pawn cache entry and binds destruction invalidation callback. */
	void CacheBridgePawn(AController* Controller, APawn* Pawn);

	/** Removes cache entries that point at a destroyed pawn and prunes stale keys/values. */
	UFUNCTION()
	void HandleCachedBridgeDestroyed(AActor* DestroyedActor);
};



