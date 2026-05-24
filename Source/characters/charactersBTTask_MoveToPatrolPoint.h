// Copyright (c) vecnode 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "charactersBTTask_MoveToPatrolPoint.generated.h"

/** MoveTo task pre-configured to read the PatrolLocation blackboard key. */
UCLASS()
class UcharactersBTTask_MoveToPatrolPoint : public UBTTask_MoveTo
{
	GENERATED_BODY()

public:
	UcharactersBTTask_MoveToPatrolPoint();
};
