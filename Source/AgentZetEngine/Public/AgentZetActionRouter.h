// Copyright AgentZet. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentZetTypes.h"
#include "AgentZetInterfaces.h"

/**
 * Routes tool calls from the AI to the appropriate action executor.
 */
class AGENTZETENGINE_API FAgentZetActionRouter
{
public:
	FAgentZetActionRouter();
	~FAgentZetActionRouter();

	/** Register an action executor */
	void RegisterExecutor(TSharedRef<IAgentZetActionExecutor> Executor);

	/** Clear all registered executors */
	void ClearExecutors();

	/** Route a tool call to the appropriate executor */
	FAgentZetActionResult RouteToolCall(const FAgentZetToolCall& ToolCall);

	/** Preview a tool call without executing */
	FAgentZetActionPlan PreviewToolCall(const FAgentZetToolCall& ToolCall);

	/** Get all registered executor names */
	TArray<FName> GetRegisteredExecutorNames() const;

	/** Get all registered tool names (keys of the executor map) */
	TArray<FString> GetRegisteredToolNames() const;

	/** Find executor by tool name */
	TSharedPtr<IAgentZetActionExecutor> FindExecutorForTool(const FString& ToolName) const;

private:
	/** Map of tool name -> executor */
	TMap<FString, TSharedRef<IAgentZetActionExecutor>> ExecutorMap;
};
