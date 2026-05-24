// Copyright (c) vecnode 2026. All Rights Reserved.

#include "charactersBTTask_SetRandomPatrolPoint.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"
#include "Engine/World.h"

namespace
{
	constexpr double NoNavWarningCooldownSeconds = 2.0;
	double GLastNoNavWarningTimeSeconds = -1000.0;

	FVector MakeFallbackRandomPoint(const FVector& Origin, const float Radius)
	{
		const FVector RandomDir3D = FMath::VRand();
		const FVector2D RandomDir2D = FVector2D(RandomDir3D.X, RandomDir3D.Y).GetSafeNormal();
		const float Distance = FMath::FRandRange(Radius * 0.35f, Radius);
		return Origin + FVector(RandomDir2D.X * Distance, RandomDir2D.Y * Distance, 0.0f);
	}
}

UcharactersBTTask_SetRandomPatrolPoint::UcharactersBTTask_SetRandomPatrolPoint()
{
	NodeName = TEXT("Choose Random Patrol Point");
	BlackboardKey.SelectedKeyName = TEXT("PatrolLocation");
}

EBTNodeResult::Type UcharactersBTTask_SetRandomPatrolPoint::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return EBTNodeResult::Failed;
	}

	APawn* ControlledPawn = AIController->GetPawn();
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!ControlledPawn || !BlackboardComp)
	{
		return EBTNodeResult::Failed;
	}

	const FVector Origin = ControlledPawn->GetActorLocation();
	const UWorld* World = ControlledPawn->GetWorld();
	FNavLocation RandomLocation;

	if (UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(ControlledPawn->GetWorld()))
	{
		FNavLocation ProjectedOrigin;
		const bool bProjectedToNav = NavSystem->ProjectPointToNavigation(
			Origin,
			ProjectedOrigin,
			FVector(5000.0f, 5000.0f, 2000.0f));

		const FVector SamplingOrigin = bProjectedToNav ? ProjectedOrigin.Location : Origin;

		if (NavSystem->GetRandomReachablePointInRadius(SamplingOrigin, SearchRadius, RandomLocation))
		{
			BlackboardComp->SetValueAsVector(GetSelectedBlackboardKey(), RandomLocation.Location);
			return EBTNodeResult::Succeeded;
		}

		if (bProjectedToNav)
		{
			// If random sampling fails, still move to nearest projected nav point.
			BlackboardComp->SetValueAsVector(GetSelectedBlackboardKey(), ProjectedOrigin.Location);
			return EBTNodeResult::Succeeded;
		}

		const FVector FallbackPoint = MakeFallbackRandomPoint(Origin, SearchRadius);
		BlackboardComp->SetValueAsVector(GetSelectedBlackboardKey(), FallbackPoint);

		const double Now = World ? World->GetTimeSeconds() : 0.0;
		if ((Now - GLastNoNavWarningTimeSeconds) >= NoNavWarningCooldownSeconds)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("WanderAI: no reachable nav point found (origin=%s radius=%.1f). Using non-nav fallback patrol point."),
				*Origin.ToCompactString(),
				SearchRadius);
			GLastNoNavWarningTimeSeconds = Now;
		}

		return EBTNodeResult::Succeeded;
	}
	else
	{
		const FVector FallbackPoint = MakeFallbackRandomPoint(Origin, SearchRadius);
		BlackboardComp->SetValueAsVector(GetSelectedBlackboardKey(), FallbackPoint);

		const double Now = World ? World->GetTimeSeconds() : 0.0;
		if ((Now - GLastNoNavWarningTimeSeconds) >= NoNavWarningCooldownSeconds)
		{
			UE_LOG(LogTemp, Warning, TEXT("WanderAI: NavigationSystem missing; using non-nav fallback patrol point."));
			GLastNoNavWarningTimeSeconds = Now;
		}

		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed;
}
