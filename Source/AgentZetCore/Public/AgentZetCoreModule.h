// Copyright AgentZet. All Rights Reserved.

#pragma once

// ============================================================================
// Engine Version Compatibility Macros
// ============================================================================
// Use these throughout the plugin to guard API calls that differ between UE versions.
// Example: #if AGENTZETUE_VERSION_AT_LEAST(5, 4) ... #endif

#include "Runtime/Launch/Resources/Version.h"

/** Check if the current engine version is at least Major.Minor */
#define AGENTZETUE_VERSION_AT_LEAST(Major, Minor) \
    (ENGINE_MAJOR_VERSION > (Major) || (ENGINE_MAJOR_VERSION == (Major) && ENGINE_MINOR_VERSION >= (Minor)))

/** UE 5.3+ specific guard */
#define AGENTZETUE53_OR_LATER AGENTZETUE_VERSION_AT_LEAST(5, 3)

/** UE 5.4+ specific guard */
#define AGENTZETUE54_OR_LATER AGENTZETUE_VERSION_AT_LEAST(5, 4)

/** UE 5.5+ specific guard */
#define AGENTZETUE55_OR_LATER AGENTZETUE_VERSION_AT_LEAST(5, 5)

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

AGENTZETCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogAgentZet, Log, All);

class FAgentZetCoreModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**
	 * Singleton-like access to this module's interface.
	 * Beware of calling this during the shutdown phase — the module might have been unloaded already.
	 */
	static FAgentZetCoreModule& Get();

	/** Check whether the module is loaded and ready. */
	static bool IsAvailable();
};
