// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Main log category used across the project */
DECLARE_LOG_CATEGORY_EXTERN(LogMetaAgent, Log, All);

/**
 * Central runtime switch shared across subsystem, game mode, controller, and AI systems.
 * Controlled by plugin settings and MetaAgentMainActor active toggle.
 */
extern bool GMetaAgentRuntimeActive;

FORCEINLINE bool IsMetaAgentRuntimeActive()
{
	return GMetaAgentRuntimeActive;
}
