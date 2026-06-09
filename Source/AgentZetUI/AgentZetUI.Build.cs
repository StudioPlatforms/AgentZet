// Copyright AgentZet. All Rights Reserved.

using UnrealBuildTool;

public class AgentZetUI : ModuleRules
{
	public AgentZetUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Slate",
			"SlateCore",
			"AgentZetCore",
			"AgentZetLLM"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"EditorStyle",
			"ToolMenus",
			"Projects",
			"EditorSubsystem",
			"ApplicationCore",
			"Json",
			"JsonUtilities",
			"AgentZetEngine",
			"AgentZetActions",
			// For SAgentZetFileChangesPanel: open files in IDE
			"SourceCodeAccess",
		});
	}
}
