// Copyright (c) vecnode 2026. All Rights Reserved.

#include "Gameplay/AI/Tasks/MetaAgentBTTask_MoveToPatrolPoint.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"

UMetaAgentBTTask_MoveToPatrolPoint::UMetaAgentBTTask_MoveToPatrolPoint()
{
	NodeName = TEXT("Walk To Patrol Location");
	BlackboardKey.SelectedKeyName = TEXT("PatrolLocation");
	bNotifyTick = true;
}

EBTNodeResult::Type UMetaAgentBTTask_MoveToPatrolPoint::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!AIController || !BlackboardComp)
	{
		return EBTNodeResult::Failed;
	}

	APawn* ControlledPawn = AIController->GetPawn();
	if (!ControlledPawn)
	{
		return EBTNodeResult::Failed;
	}

	const FVector TargetLocation = BlackboardComp->GetValueAsVector(GetSelectedBlackboardKey());
	if (TargetLocation.ContainsNaN())
	{
		return EBTNodeResult::Failed;
	}

	const float Distance2D = FVector::Dist2D(ControlledPawn->GetActorLocation(), TargetLocation);
	if (Distance2D <= AcceptableDistance)
	{
		return EBTNodeResult::Succeeded;
	}

	FMoveToPatrolMemory* Memory = reinterpret_cast<FMoveToPatrolMemory*>(NodeMemory);
	Memory->ElapsedSeconds = 0.0f;
	Memory->StuckSeconds = 0.0f;
	Memory->LastLocation = ControlledPawn->GetActorLocation();

	return EBTNodeResult::InProgress;
}

void UMetaAgentBTTask_MoveToPatrolPoint::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!AIController || !BlackboardComp)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	APawn* ControlledPawn = AIController->GetPawn();
	if (!ControlledPawn)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	const FVector TargetLocation = BlackboardComp->GetValueAsVector(GetSelectedBlackboardKey());
	const FVector PawnLocation = ControlledPawn->GetActorLocation();
	const FVector ToTarget = TargetLocation - PawnLocation;
	const float Distance2D = FVector::Dist2D(PawnLocation, TargetLocation);

	if (Distance2D <= AcceptableDistance)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	const FVector MoveDirection = FVector(ToTarget.X, ToTarget.Y, 0.0f).GetSafeNormal();
	if (MoveDirection.IsNearlyZero())
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	AIController->SetControlRotation(MoveDirection.Rotation());
	ControlledPawn->AddMovementInput(MoveDirection, MoveInputScale);

	FMoveToPatrolMemory* Memory = reinterpret_cast<FMoveToPatrolMemory*>(NodeMemory);
	Memory->ElapsedSeconds += DeltaSeconds;

	const float TravelSinceLastTick2D = FVector::Dist2D(PawnLocation, Memory->LastLocation);
	if (TravelSinceLastTick2D < 1.0f)
	{
		Memory->StuckSeconds += DeltaSeconds;
	}
	else
	{
		Memory->StuckSeconds = 0.0f;
	}

	Memory->LastLocation = PawnLocation;

	if (Memory->StuckSeconds >= StuckTimeoutSeconds || Memory->ElapsedSeconds >= MaxMoveSeconds)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
	}
}

uint16 UMetaAgentBTTask_MoveToPatrolPoint::GetInstanceMemorySize() const
{
	return sizeof(FMoveToPatrolMemory);
}

