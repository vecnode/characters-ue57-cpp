// Copyright (c) vecnode 2026. All Rights Reserved.

#include "Gameplay/Characters/charactersMHPlayer.h"
#include "Systems/Runtime/charactersGameInstance.h"
#include "Core/characters.h"
#include "Components/SkeletalMeshComponent.h"

AcharactersMHPlayer::AcharactersMHPlayer()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AcharactersMHPlayer::BeginPlay()
{
	Super::BeginPlay();

	if (UcharactersGameInstance* GI = UcharactersGameInstance::Get(this))
	{
		GI->ActivePlayerCharacter = this;
	}
}

