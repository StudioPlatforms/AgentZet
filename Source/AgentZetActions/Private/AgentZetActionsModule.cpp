// Copyright AgentZet. All Rights Reserved.

#include "AgentZetActionsModule.h"
#include "AgentZetCoreModule.h"

#define LOCTEXT_NAMESPACE "FAgentZetActionsModule"

void FAgentZetActionsModule::StartupModule() { UE_LOG(LogAgentZet, Log, TEXT("AgentZetActions module started.")); }
void FAgentZetActionsModule::ShutdownModule() { UE_LOG(LogAgentZet, Log, TEXT("AgentZetActions module shut down.")); }
FAgentZetActionsModule& FAgentZetActionsModule::Get() { return FModuleManager::LoadModuleChecked<FAgentZetActionsModule>("AgentZetActions"); }
bool FAgentZetActionsModule::IsAvailable() { return FModuleManager::Get().IsModuleLoaded("AgentZetActions"); }

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAgentZetActionsModule, AgentZetActions)
