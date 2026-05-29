// Copyright (c) vecnode 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Characters/MetaAgentCharacter.h"
#include "MetaAgentMHPlayer.generated.h"

/** MetaHuman player pawn that adds C++ movement fallback and GI registration. */
UCLASS()
class AMetaAgentMHPlayer : public AMetaAgentCharacter
{
	GENERATED_BODY()

public:

	AMetaAgentMHPlayer();

protected:

	virtual void BeginPlay() override;
};

