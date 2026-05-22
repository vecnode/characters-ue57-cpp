// Copyright (c) vecnode 2026. All Rights Reserved.

#include "charactersMHPlayer.h"
#include "charactersGameInstance.h"
#include "characters.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"

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

	void EnsureThirdPersonCameraActive(AcharactersMHPlayer* Player)
	{
		if (!Player)
		{
			return;
		}

		USpringArmComponent* DesiredSpringArm = Player->GetCameraBoom();
		UCameraComponent* DesiredCamera = Player->GetFollowCamera();

		if (!DesiredSpringArm || !DesiredCamera)
		{
			UE_LOG(Logcharacters, Warning,
				TEXT("charactersMHPlayer: Missing CameraBoom or FollowCamera on '%s'."),
				*GetNameSafe(Player));
			return;
		}

		DesiredSpringArm->bUsePawnControlRotation = true;
		DesiredSpringArm->TargetArmLength = 200.0f;
		DesiredSpringArm->bDoCollisionTest = false;
		DesiredSpringArm->SocketOffset = FVector(0.0f, 0.0f, 70.0f);
		DesiredCamera->bUsePawnControlRotation = false;
		DesiredCamera->Activate();
	}
}

AcharactersMHPlayer::AcharactersMHPlayer()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AcharactersMHPlayer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (CameraEnforceRemainingSeconds > 0.0f)
	{
		EnsureThirdPersonCameraActive(this);
		CameraEnforceRemainingSeconds -= DeltaSeconds;
	}
}

void AcharactersMHPlayer::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	if (USpringArmComponent* ActiveCameraBoom = GetCameraBoom())
	{
		const FTransform SocketTransform = ActiveCameraBoom->GetSocketTransform(USpringArmComponent::SocketName, RTS_World);
		OutResult.Location = SocketTransform.GetLocation();
		OutResult.Rotation = SocketTransform.Rotator();

		if (UCameraComponent* ActiveFollowCamera = GetFollowCamera())
		{
			OutResult.FOV = ActiveFollowCamera->FieldOfView;
			OutResult.PostProcessBlendWeight = ActiveFollowCamera->PostProcessBlendWeight;
			OutResult.PostProcessSettings = ActiveFollowCamera->PostProcessSettings;
		}

		OutResult.bConstrainAspectRatio = false;
		return;
	}

	Super::CalcCamera(DeltaTime, OutResult);
}

void AcharactersMHPlayer::BeginPlay()
{
	Super::BeginPlay();

	ApplyDefaultThirdPersonMovement(this);
	EnsureThirdPersonCameraActive(this);

	if (UcharactersGameInstance* GI = UcharactersGameInstance::Get(this))
	{
		GI->ActivePlayerCharacter = this;
	}
}

void AcharactersMHPlayer::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	EnsureThirdPersonCameraActive(this);
}

void AcharactersMHPlayer::OnRep_Controller()
{
	Super::OnRep_Controller();
	EnsureThirdPersonCameraActive(this);
}

