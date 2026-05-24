// Copyright (c) vecnode 2026. All Rights Reserved.

#include "charactersBTTask_MoveToPatrolPoint.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"

UcharactersBTTask_MoveToPatrolPoint::UcharactersBTTask_MoveToPatrolPoint()
{
	NodeName = TEXT("Move To Patrol Location");
	BlackboardKey.SelectedKeyName = TEXT("PatrolLocation");

	AcceptableRadius = FValueOrBBKey_Float(30.0f);
	bAllowPartialPath = FValueOrBBKey_Bool(true);
	bTrackMovingGoal = FValueOrBBKey_Bool(false);
	bUsePathfinding = false;
	bProjectGoalLocation = false;
}
