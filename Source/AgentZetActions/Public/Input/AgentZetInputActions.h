// Copyright AgentZet. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentZetInterfaces.h"

/**
 * Enhanced Input actions for AgentZet.
 *
 * Provides programmatic creation and modification of Enhanced Input assets:
 * - create_input_action: Create a UInputAction data asset (IA_*)
 * - create_input_mapping_context: Create a UInputMappingContext data asset (IMC_*)
 * - add_input_mapping: Add a key→action binding to an existing IMC (with optional modifiers/triggers)
 *
 * These tools eliminate the need for manual editor interaction when configuring
 * the Enhanced Input System, upholding the zero-manual-steps protocol.
 */
class AGENTZETACTIONS_API FAgentZetInputActions : public IAgentZetActionExecutor
{
public:
	FAgentZetInputActions();
	virtual ~FAgentZetInputActions();

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
	FAgentZetActionResult ExecuteCreateInputAction(const TSharedRef<FJsonObject>& Params);
	FAgentZetActionResult ExecuteCreateInputMappingContext(const TSharedRef<FJsonObject>& Params);
	FAgentZetActionResult ExecuteAddInputMapping(const TSharedRef<FJsonObject>& Params);
};
