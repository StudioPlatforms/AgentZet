// Copyright AgentZet. All Rights Reserved.

#include "AgentZetCoreModule.h"

DEFINE_LOG_CATEGORY(LogAgentZet);

#define LOCTEXT_NAMESPACE "FAgentZetCoreModule"

void FAgentZetCoreModule::StartupModule()
{
	UE_LOG(LogAgentZet, Log, TEXT("AgentZetCore module started."));
}

void FAgentZetCoreModule::ShutdownModule()
{
	UE_LOG(LogAgentZet, Log, TEXT("AgentZetCore module shut down."));
}

FAgentZetCoreModule& FAgentZetCoreModule::Get()
{
	return FModuleManager::LoadModuleChecked<FAgentZetCoreModule>("AgentZetCore");
}

bool FAgentZetCoreModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded("AgentZetCore");
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAgentZetCoreModule, AgentZetCore)
