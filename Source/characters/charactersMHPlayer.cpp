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
	UClass* LoadCharacterClass(const TCHAR* ClassPath)
	{
		return StaticLoadClass(ACharacter::StaticClass(), nullptr, ClassPath);
	}

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
			MoveComp->MaxWalkSpeed = 500.0f;
			MoveComp->MinAnalogWalkSpeed = 20.0f;
			MoveComp->BrakingDecelerationWalking = 2000.0f;
			MoveComp->BrakingDecelerationFalling = 1500.0f;
		}
	}

	void ApplyCharacter1AnimFallback(ACharacter* Character)
	{
		if (!Character || !Character->GetMesh())
		{
			return;
		}

		if (Character->GetMesh()->GetAnimClass() != nullptr)
		{
			return;
		}

		UClass* Character1Class = LoadCharacterClass(TEXT("/Game/MetaHumans/character1/BP_character1.BP_character1_C"));
		const ACharacter* Character1CDO = Character1Class ? Cast<ACharacter>(Character1Class->GetDefaultObject()) : nullptr;

		if (Character1CDO && Character1CDO->GetMesh() && Character1CDO->GetMesh()->GetAnimClass())
		{
			Character->GetMesh()->SetAnimInstanceClass(Character1CDO->GetMesh()->GetAnimClass());
			UE_LOG(Logcharacters, Log, TEXT("charactersMHPlayer: Applied character1 unarmed AnimBP fallback."));
		}
	}

	void EnsureThirdPersonCameraActive(ACharacter* Character)
	{
		if (!Character)
		{
			return;
		}

		AcharactersMHPlayer* Player = Cast<AcharactersMHPlayer>(Character);
		USpringArmComponent* DesiredSpringArm = Player ? Player->GetCameraBoom() : nullptr;
		UCameraComponent* DesiredCamera = Player ? Player->GetFollowCamera() : nullptr;

		TArray<USpringArmComponent*> SpringArms;
		Character->GetComponents<USpringArmComponent>(SpringArms);

		if (!DesiredSpringArm)
		{
			for (USpringArmComponent* SpringArm : SpringArms)
			{
				if (!SpringArm)
				{
					continue;
				}

				if (SpringArm->GetName().Contains(TEXT("CameraBoom"), ESearchCase::IgnoreCase))
				{
					DesiredSpringArm = SpringArm;
					break;
				}
			}
		}

		if (!DesiredSpringArm && SpringArms.Num() > 0)
		{
			DesiredSpringArm = SpringArms[0];
		}

		if (DesiredSpringArm)
		{
			DesiredSpringArm->bUsePawnControlRotation = true;
			DesiredSpringArm->TargetArmLength = 350.0f;
			DesiredSpringArm->bDoCollisionTest = false;
		}

		TArray<UCameraComponent*> Cameras;
		Character->GetComponents<UCameraComponent>(Cameras);

		if (Cameras.Num() == 0)
		{
			UE_LOG(Logcharacters, Warning,
				TEXT("charactersMHPlayer: No camera component found on '%s'."),
				*GetNameSafe(Character));
			return;
		}

		if (!DesiredCamera)
		{
			for (UCameraComponent* Camera : Cameras)
			{
				if (!Camera)
				{
					continue;
				}

				USpringArmComponent* AttachedSpringArm = Cast<USpringArmComponent>(Camera->GetAttachParent());
				if (AttachedSpringArm && (!DesiredSpringArm || AttachedSpringArm == DesiredSpringArm))
				{
					DesiredCamera = Camera;
					break;
				}
			}
		}

		if (!DesiredCamera)
		{
			DesiredCamera = Cameras[0];

			if (DesiredSpringArm)
			{
				DesiredCamera->AttachToComponent(
					DesiredSpringArm,
					FAttachmentTransformRules::SnapToTargetNotIncludingScale,
					USpringArmComponent::SocketName);
			}
		}

		for (UCameraComponent* Camera : Cameras)
		{
			if (!Camera)
			{
				continue;
			}

			const bool bIsDesiredCamera = (Camera == DesiredCamera);
			Camera->SetActive(bIsDesiredCamera);
		}

		if (USpringArmComponent* SpringArm = Cast<USpringArmComponent>(DesiredCamera->GetAttachParent()))
		{
			SpringArm->bUsePawnControlRotation = true;
		}

		DesiredCamera->bUsePawnControlRotation = false;
		DesiredCamera->SetRelativeLocation(FVector::ZeroVector);
		DesiredCamera->SetRelativeRotation(FRotator::ZeroRotator);
		DesiredCamera->Activate();

		UE_LOG(Logcharacters, Log,
			TEXT("charactersMHPlayer: Activated camera '%s' for '%s' (ArmLength=%.1f)."),
			*GetNameSafe(DesiredCamera),
			*GetNameSafe(Character),
			DesiredSpringArm ? DesiredSpringArm->TargetArmLength : -1.0f);
	}

	void EnsurePlayerViewTarget(ACharacter* Character)
	{
		if (!Character)
		{
			return;
		}

		if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
		{
			PC->bAutoManageActiveCameraTarget = false;
			PC->SetViewTargetWithBlend(Character, 0.0f);
			UE_LOG(Logcharacters, Log,
				TEXT("charactersMHPlayer: Set view target to '%s' for controller '%s'."),
				*GetNameSafe(Character),
				*GetNameSafe(PC));
		}
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
		EnsurePlayerViewTarget(this);
		CameraEnforceRemainingSeconds -= DeltaSeconds;
	}
}

void AcharactersMHPlayer::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	if (UCameraComponent* ActiveFollowCamera = GetFollowCamera())
	{
		ActiveFollowCamera->GetCameraView(DeltaTime, OutResult);
		return;
	}

	Super::CalcCamera(DeltaTime, OutResult);
}

void AcharactersMHPlayer::BeginPlay()
{
	Super::BeginPlay();

	// Apply movement defaults at runtime so BP_character2 moves like the UE third-person character.
	ApplyDefaultThirdPersonMovement(this);

	// If BP_character2 has no AnimBP assigned, use character1's unarmed AnimBP as fallback.
	ApplyCharacter1AnimFallback(this);

	if (Controller == nullptr && HasAuthority())
	{
		SpawnDefaultController();
	}

	EnsureThirdPersonCameraActive(this);
	EnsurePlayerViewTarget(this);

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

void AcharactersMHPlayer::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	EnsureThirdPersonCameraActive(this);
	EnsurePlayerViewTarget(this);
}

void AcharactersMHPlayer::OnRep_Controller()
{
	Super::OnRep_Controller();

	EnsureThirdPersonCameraActive(this);
	EnsurePlayerViewTarget(this);
}
