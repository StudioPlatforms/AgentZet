// Copyright AgentZet. All Rights Reserved.

#include "AgentZetEngineModule.h"
#include "AgentZetCoreModule.h"

#define LOCTEXT_NAMESPACE "FAgentZetEngineModule"

void FAgentZetEngineModule::StartupModule() { UE_LOG(LogAgentZet, Log, TEXT("AgentZetEngine module started.")); }
void FAgentZetEngineModule::ShutdownModule() { UE_LOG(LogAgentZet, Log, TEXT("AgentZetEngine module shut down.")); }
FAgentZetEngineModule& FAgentZetEngineModule::Get() { return FModuleManager::LoadModuleChecked<FAgentZetEngineModule>("AgentZetEngine"); }
bool FAgentZetEngineModule::IsAvailable() { return FModuleManager::Get().IsModuleLoaded("AgentZetEngine"); }

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAgentZetEngineModule, AgentZetEngine)
