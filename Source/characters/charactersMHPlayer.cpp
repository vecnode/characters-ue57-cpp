// Copyright (c) vecnode 2026. All Rights Reserved.

#include "charactersMHPlayer.h"
#include "charactersGameInstance.h"
#include "characters.h"
#include "Components/SkeletalMeshComponent.h"

AcharactersMHPlayer::AcharactersMHPlayer()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AcharactersMHPlayer::BeginPlay()
{
	Super::BeginPlay();

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		// Keep mesh aligned to capsule origin to avoid inherited BP offset drift.
		MeshComp->SetRelativeLocation(FVector::ZeroVector);
		MeshComp->SetRelativeRotation(FRotator::ZeroRotator);
		MeshComp->SetRelativeScale3D(FVector::OneVector);
	}

	if (Controller == nullptr && HasAuthority())
	{
		SpawnDefaultController();
	}

	if (UcharactersGameInstance* GI = UcharactersGameInstance::Get(this))
	{
		GI->ActivePlayerCharacter = this;
		UE_LOG(Logcharacters, Log,
			TEXT("charactersMHPlayer: '%s' registered as ActivePlayerCharacter in Game Instance."),
			*GetName());
	}
	else
	{
		UE_LOG(Logcharacters, Warning,
			TEXT("charactersMHPlayer: Game Instance is not UcharactersGameInstance. "
			     "Set it in Project Settings → Maps & Modes → Game Instance Class."));
	}
}
