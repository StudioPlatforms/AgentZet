// Copyright AgentZet. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentZetInterfaces.h"

/**
 * FAgentZetPIEActions
 *
 * Play-In-Editor automation tools:
 *   - start_pie_session: Launch PIE for runtime testing
 *   - simulate_input: Inject key presses during PIE
 *   - stop_pie_session: End the current PIE session
 *
 * Combined with read_message_log (from DiagnosticsActions), the AI can:
 *   1. Build a Blueprint
 *   2. Start PIE
 *   3. Simulate player input (move forward, press interact)
 *   4. Read the Output Log for Accessed None errors
 *   5. Stop PIE and fix the bugs
 *
 * SAFETY:
 *   - PIE is inherently risky (editor crash potential), so rated High risk
 *   - Requires Developer security mode
 *   - Input simulation is limited to keyboard/gamepad (no mouse movement)
 *   - PIE sessions auto-stop after a configurable timeout
 */
class AGENTZETACTIONS_API FAgentZetPIEActions : public IAgentZetActionExecutor
{
public:
	FAgentZetPIEActions();
	virtual ~FAgentZetPIEActions();

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
	/** Launch Play-In-Editor. */
	FAgentZetActionResult ExecuteStartPIE(const TSharedRef<FJsonObject>& Params, FAgentZetActionResult& Result);

	/** Inject keyboard input during a running PIE session. */
	FAgentZetActionResult ExecuteSimulateInput(const TSharedRef<FJsonObject>& Params, FAgentZetActionResult& Result);

	/** Stop the current PIE session. */
	FAgentZetActionResult ExecuteStopPIE(const TSharedRef<FJsonObject>& Params, FAgentZetActionResult& Result);

	/** Whether PIE is currently running. */
	static bool IsPIERunning();
};
