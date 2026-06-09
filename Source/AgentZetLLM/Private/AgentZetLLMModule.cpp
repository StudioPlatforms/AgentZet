// Copyright AgentZet. All Rights Reserved.

#include "AgentZetLLMModule.h"
#include "AgentZetCoreModule.h"

#define LOCTEXT_NAMESPACE "FAgentZetLLMModule"

void FAgentZetLLMModule::StartupModule()
{
	UE_LOG(LogAgentZet, Log, TEXT("AgentZetLLM module started."));
}

void FAgentZetLLMModule::ShutdownModule()
{
	UE_LOG(LogAgentZet, Log, TEXT("AgentZetLLM module shut down."));
}

FAgentZetLLMModule& FAgentZetLLMModule::Get()
{
	return FModuleManager::LoadModuleChecked<FAgentZetLLMModule>("AgentZetLLM");
}

bool FAgentZetLLMModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded("AgentZetLLM");
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAgentZetLLMModule, AgentZetLLM)
