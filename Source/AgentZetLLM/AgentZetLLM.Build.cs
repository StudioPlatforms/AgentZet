// Copyright AgentZet. All Rights Reserved.

using UnrealBuildTool;

public class AgentZetLLM : ModuleRules
{
	public AgentZetLLM(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"HTTP",
			"Json",
			"JsonUtilities",
			"AgentZetCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"ApplicationCore"
		});
	}
}
