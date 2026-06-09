// Copyright AgentZet. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentZetTypes.h"

class AGENTZETENGINE_API FAgentZetTransactionManager
{
public:
	FAgentZetTransactionManager();
	~FAgentZetTransactionManager();

	/** Begin a named transaction group for multi-action rollback */
	void BeginUndoGroup(const FString& GroupName);
	/** End the current undo group */
	void EndUndoGroup();
	/** Begin a scoped UObject transaction */
	void BeginTransaction(const FString& Description);
	/** End the current transaction */
	void EndTransaction();
	/** Undo the last undo group */
	bool UndoLastGroup();
	/** Get the current undo group name */
	FString GetCurrentUndoGroupName() const { return CurrentUndoGroup; }

private:
	FString CurrentUndoGroup;
	TArray<FString> UndoGroupHistory;
};
