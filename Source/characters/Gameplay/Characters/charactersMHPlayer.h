// Copyright (c) vecnode 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Characters/charactersCharacter.h"
#include "charactersMHPlayer.generated.h"

/** MetaHuman player pawn that adds C++ movement fallback and GI registration. */
UCLASS()
class AcharactersMHPlayer : public AcharactersCharacter
{
	GENERATED_BODY()

public:

	AcharactersMHPlayer();

protected:

	virtual void BeginPlay() override;
};
