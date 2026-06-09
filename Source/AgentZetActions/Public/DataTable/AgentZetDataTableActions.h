// Copyright AgentZet. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentZetInterfaces.h"

/**
 * FAgentZetDataTableActions
 *
 * Provides tools for creating and populating DataTable assets:
 *   - create_data_table: Create a DataTable from an existing row struct
 *   - import_json_to_datatable: Populate a DataTable from inline JSON data
 *
 * DataTables are used extensively in UE for game balancing, item databases,
 * weapon stats, dialogue, level configuration, and more.
 *
 * The AI can:
 *   - Create DataTables targeting any FTableRowBase-derived struct
 *   - Generate balanced game data (e.g., RPG weapon progression curves)
 *   - Import pre-computed JSON data directly into rows
 *   - Modify existing DataTable rows
 */
class AGENTZETACTIONS_API FAgentZetDataTableActions : public IAgentZetActionExecutor
{
public:
	FAgentZetDataTableActions();
	virtual ~FAgentZetDataTableActions();

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
	/** Create a new DataTable asset with a specified row struct. */
	FAgentZetActionResult ExecuteCreateDataTable(const TSharedRef<FJsonObject>& Params, FAgentZetActionResult& Result);

	/** Import JSON data into an existing or new DataTable. */
	FAgentZetActionResult ExecuteImportJsonToDataTable(const TSharedRef<FJsonObject>& Params, FAgentZetActionResult& Result);
};
