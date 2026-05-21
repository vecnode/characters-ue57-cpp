// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class characters : ModuleRules
{
	public characters(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"characters",
			"characters/Variant_Platforming",
			"characters/Variant_Platforming/Animation",
			"characters/Variant_Combat",
			"characters/Variant_Combat/AI",
			"characters/Variant_Combat/Animation",
			"characters/Variant_Combat/Gameplay",
			"characters/Variant_Combat/Interfaces",
			"characters/Variant_Combat/UI",
			"characters/Variant_SideScrolling",
			"characters/Variant_SideScrolling/AI",
			"characters/Variant_SideScrolling/Gameplay",
			"characters/Variant_SideScrolling/Interfaces",
			"characters/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
