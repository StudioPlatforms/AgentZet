// Copyright AgentZet. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentZetInterfaces.h"

/**
 * FAgentZetDiagnosticsActions
 *
 * Runtime diagnostics tools:
 *   - read_message_log: Capture the Output Log (errors, warnings, asserts)
 *
 * These are read-only observation tools that let the AI see runtime issues
 * without needing to launch PIE. The Output Log captures Blueprint errors,
 * Accessed None warnings, asset loading failures, and C++ asserts.
 */
class AGENTZETACTIONS_API FAgentZetDiagnosticsActions : public IAgentZetActionExecutor
{
public:
	FAgentZetDiagnosticsActions();
	virtual ~FAgentZetDiagnosticsActions();

	virtual FName GetActionName() const override;
	virtual FText GetDisplayName() const override;
	virtual EAgentZetActionCategory GetCategory() const override;
	virtual EAgentZetRiskLevel GetDefaultRiskLevel() const override;
	virtual FAgentZetActionPlan PreviewAction(const TSharedRef<FJsonObject>& Params) override;
	virtual FAgentZetActionResult ExecuteAction(const TSharedRef<FJsonObject>& Params) override;
	virtual bool CanUndo() const override;
	virtual bool UndoAction() override;
	virtual TArray<FString> GetSupportedToolNames() const override;
	virtual bool ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const override;

private:
	/**
	 * Read recent entries from the Output Log.
	 *
	 * Captures the last N messages from GLog, optionally filtered by
	 * category (LogBlueprintUserMessages, LogScript, etc.) or severity
	 * (Error, Warning, Display).
	 */
	FAgentZetActionResult ExecuteReadMessageLog(const TSharedRef<FJsonObject>& Params, FAgentZetActionResult& Result);
};
