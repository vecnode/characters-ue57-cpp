// Copyright (c) vecnode 2026. All Rights Reserved.

#include "charactersMHPlayer.h"
#include "charactersGameInstance.h"
#include "characters.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
	void ApplyDefaultThirdPersonMovement(ACharacter* Character)
	{
		if (!Character)
		{
			return;
		}

		Character->bUseControllerRotationPitch = false;
		Character->bUseControllerRotationYaw = false;
		Character->bUseControllerRotationRoll = false;

		if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
		{
			MoveComp->bOrientRotationToMovement = true;
			MoveComp->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
			MoveComp->JumpZVelocity = 500.0f;
			MoveComp->AirControl = 0.35f;
			MoveComp->MaxWalkSpeed = 220.0f;
			MoveComp->MinAnalogWalkSpeed = 20.0f;
			MoveComp->BrakingDecelerationWalking = 2000.0f;
			MoveComp->BrakingDecelerationFalling = 1500.0f;
		}
	}
}

AcharactersMHPlayer::AcharactersMHPlayer()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AcharactersMHPlayer::BeginPlay()
{
	Super::BeginPlay();

	ApplyDefaultThirdPersonMovement(this);

	if (UcharactersGameInstance* GI = UcharactersGameInstance::Get(this))
	{
		GI->ActivePlayerCharacter = this;
	}
}

