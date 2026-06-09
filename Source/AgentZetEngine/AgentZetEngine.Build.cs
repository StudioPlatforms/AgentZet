// Copyright AgentZet. All Rights Reserved.

using UnrealBuildTool;

public class AgentZetEngine : ModuleRules
{
	public AgentZetEngine(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Json",
			"JsonUtilities",
			"AgentZetCore",
			"AgentZetLLM"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"AssetTools",
			"AssetRegistry",
			"SourceControl",
			"Settings",
			"ContentBrowser",
			"EditorSubsystem",
			"Slate",
			"SlateCore",
			"InputCore",
			// Phase 1: AgentZetIgnoreController + AgentZetFileContextTracker use IDirectoryWatcher
			"DirectoryWatcher",
			// Phase 2: AgentZetEnvironmentDetails uses blueprint/level editor APIs
			"LevelEditor",
			"MessageLog",
		});
	}
}
