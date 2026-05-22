// Copyright (c) vecnode 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "charactersCharacter.h"
#include "charactersMHPlayer.generated.h"

/** MetaHuman player pawn that adds C++ movement fallback and GI registration. */
UCLASS()
class AcharactersMHPlayer : public AcharactersCharacter
{
	GENERATED_BODY()

public:

	AcharactersMHPlayer();
	virtual void Tick(float DeltaSeconds) override;
	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;

protected:

	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;

private:

	float CameraEnforceRemainingSeconds = 2.0f;
};
