// Copyright (c) vecnode 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "charactersBTTask_MoveToPatrolPoint.generated.h"

/**
 * Input-driven move task that walks toward PatrolLocation using AddMovementInput.
 * This preserves player-like locomotion animation while still allowing collision blocking.
 */
UCLASS()
class UcharactersBTTask_MoveToPatrolPoint : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UcharactersBTTask_MoveToPatrolPoint();

	UPROPERTY(EditAnywhere, Category = "AI|Patrol", meta=(ClampMin="10.0"))
	float AcceptableDistance = 75.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Patrol", meta=(ClampMin="0.1"))
	float MaxMoveSeconds = 8.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Patrol", meta=(ClampMin="0.1"))
	float StuckTimeoutSeconds = 1.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Patrol", meta=(ClampMin="0.1"))
	float MoveInputScale = 1.0f;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual uint16 GetInstanceMemorySize() const override;

private:
	struct FMoveToPatrolMemory
	{
		float ElapsedSeconds = 0.0f;
		float StuckSeconds = 0.0f;
		FVector LastLocation = FVector::ZeroVector;
	};
};
