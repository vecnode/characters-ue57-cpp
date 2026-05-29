// Copyright (c) vecnode 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "MetaAgentWanderAIController.generated.h"

class UBehaviorTree;
class UBlackboardData;

/**
 * AI controller that builds and runs a runtime behavior tree:
 * Pick random reachable point -> Move To -> Wait (2-5s) -> Repeat.
 */
UCLASS()
class AMetaAgentWanderAIController : public AAIController
{
	GENERATED_BODY()

public:
	AMetaAgentWanderAIController();

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

private:
	void BuildRuntimeBehaviorTree();
	void StartRuntimeBehaviorTree();

	UPROPERTY(EditAnywhere, Category = "AI|Patrol", meta=(ClampMin="100.0"))
	float PatrolRadius = 50000.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Patrol", meta=(ClampMin="0.1"))
	float WaitMinSeconds = 2.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Patrol", meta=(ClampMin="0.1"))
	float WaitMaxSeconds = 5.0f;

	UPROPERTY()
	TObjectPtr<UBlackboardData> RuntimeBlackboard = nullptr;

	UPROPERTY()
	TObjectPtr<UBehaviorTree> RuntimeBehaviorTree = nullptr;
};

