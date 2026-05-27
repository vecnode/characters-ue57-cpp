// Based on Unreal Engine template code.
// Project-specific implementation and modifications Copyright (c) vecnode, 2026.

#include "Gameplay/Controllers/charactersPlayerController.h"
#include "Core/characters.h"
#include "UI/HUD/charactersHUD.h"
#include "Gameplay/AI/charactersWanderAIController.h"
#include "AIController.h"
#include "Engine/World.h"

void AcharactersPlayerController::HandleToggleAutopilotPressed()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float CurrentTimeSeconds = World->GetTimeSeconds();
	if ((CurrentTimeSeconds - Autopilot.LastToggleTimeSeconds) < Autopilot.ToggleDebounceSeconds)
	{
		return;
	}

	Autopilot.LastToggleTimeSeconds = CurrentTimeSeconds;

	if (Autopilot.bEnabled)
	{
		StopAutopilotTakeRecording();
		DisableAutopilotAndRepossess();
	}
	else
	{
		EnableAutopilotForCurrentPawn();
		StartAutopilotTakeRecording();
	}
}

void AcharactersPlayerController::EnableAutopilotForCurrentPawn()
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		UE_LOG(Logcharacters, Warning, TEXT("Autopilot: no currently possessed pawn to hand to AI."));
		return;
	}

	Autopilot.Pawn = ControlledPawn;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	UClass* DesiredAIControllerClass = Autopilot.AIControllerClass.Get();
	if (!DesiredAIControllerClass)
	{
		DesiredAIControllerClass = AcharactersWanderAIController::StaticClass();
	}

	AAIController* AIController = GetWorld()->SpawnActor<AAIController>(
		DesiredAIControllerClass,
		ControlledPawn->GetActorLocation(),
		ControlledPawn->GetActorRotation(),
		SpawnParams);

	if (!AIController)
	{
		UE_LOG(Logcharacters, Error, TEXT("Autopilot: failed to spawn AI controller '%s'."), *GetNameSafe(DesiredAIControllerClass));
		return;
	}

	UnPossess();
	AIController->Possess(ControlledPawn);
	SetViewTargetWithBlend(ControlledPawn, 0.0f);

	Autopilot.bEnabled = true;

	UE_LOG(Logcharacters, Log, TEXT("Autopilot: ENABLED for pawn '%s'. Press J again to return to player control."), *GetNameSafe(ControlledPawn));

	if (AcharactersHUD* charactersHUD = GetHUD<AcharactersHUD>())
	{
		charactersHUD->AddTransientMessage(TEXT("AI Control: ON (press J to return)"), FColor::Cyan, 2.5f);
	}

	EnableCinematicCameraMode();
}

void AcharactersPlayerController::DisableAutopilotAndRepossess()
{
	APawn* PawnToRepossess = Autopilot.Pawn.Get();
	if (!PawnToRepossess)
	{
		UE_LOG(Logcharacters, Warning, TEXT("Autopilot: no cached pawn to repossess."));
		Autopilot.bEnabled = false;
		return;
	}

	if (AController* CurrentController = PawnToRepossess->GetController())
	{
		if (CurrentController != this)
		{
			CurrentController->UnPossess();

			if (CurrentController->IsA<AAIController>())
			{
				CurrentController->Destroy();
			}
		}
	}

	Possess(PawnToRepossess);
	SetViewTargetWithBlend(PawnToRepossess, 0.0f);

	Autopilot.bEnabled = false;
	Autopilot.Pawn.Reset();

	UE_LOG(Logcharacters, Log, TEXT("Autopilot: DISABLED. Player control restored."));

	if (AcharactersHUD* charactersHUD = GetHUD<AcharactersHUD>())
	{
		charactersHUD->AddTransientMessage(TEXT("AI Control: OFF (player control restored)"), FColor::Green, 2.5f);
	}

	DisableCinematicCameraMode();
}
