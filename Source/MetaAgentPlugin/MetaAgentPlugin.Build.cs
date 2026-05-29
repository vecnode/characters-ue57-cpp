using UnrealBuildTool;
using System.IO;

public class MetaAgentPlugin : ModuleRules
{
	public MetaAgentPlugin(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[]
			{
				Path.Combine(ModuleDirectory, "Migrated")
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"EnhancedInput",
				"AIModule",
				"NavigationSystem",
				"StateTreeModule",
				"GameplayStateTreeModule",
				"HTTP",
				"HTTPServer",
				"Json",
				"JsonUtilities",
				"UMG",
				"Slate",
				"LevelSequence",
				"MovieRenderPipelineCore",
				"MovieRenderPipelineRenderPasses",
				"MovieRenderPipelineMP4Encoder",
				"DeveloperSettings"
			}
		);

		if (Target.bBuildEditor)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"TakeRecorder",
					"TakesCore",
					"TakeRecorderSources"
				}
			);
		}
	}
}
