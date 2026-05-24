// Copyright (c) vecnode 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "charactersBTTask_SetRandomPatrolPoint.generated.h"

/** Writes a random reachable location to the PatrolLocation blackboard key. */
UCLASS()
class UcharactersBTTask_SetRandomPatrolPoint : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UcharactersBTTask_SetRandomPatrolPoint();

	/** Search radius used when selecting the next patrol point. */
	UPROPERTY(EditAnywhere, Category = "AI|Patrol", meta=(ClampMin="100.0"))
	float SearchRadius = 1200.0f;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
