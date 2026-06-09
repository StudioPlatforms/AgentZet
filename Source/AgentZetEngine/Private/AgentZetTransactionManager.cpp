// Copyright AgentZet. All Rights Reserved.

#include "AgentZetTransactionManager.h"
#include "AgentZetCoreModule.h"

FAgentZetTransactionManager::FAgentZetTransactionManager() {}
FAgentZetTransactionManager::~FAgentZetTransactionManager() {}
void FAgentZetTransactionManager::BeginUndoGroup(const FString& GroupName) { CurrentUndoGroup = GroupName; UndoGroupHistory.Add(GroupName); }
void FAgentZetTransactionManager::EndUndoGroup() { CurrentUndoGroup.Empty(); }
void FAgentZetTransactionManager::BeginTransaction(const FString& Description) { /* Stub: will use FScopedTransaction */ }
void FAgentZetTransactionManager::EndTransaction() { /* Stub */ }
bool FAgentZetTransactionManager::UndoLastGroup() { /* Stub */ return false; }
