// Copyright AgentZet. All Rights Reserved.

#include "DataTable/AgentZetDataTableActions.h"
#include "AgentZetCoreModule.h"
#include "AgentZetSettings.h"
#include "Engine/DataTable.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/DataTableFactory.h"
#include "UObject/SavePackage.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

#define LOCTEXT_NAMESPACE "AgentZetDataTableActions"

// ============================================================================
// Lifecycle
// ============================================================================

FAgentZetDataTableActions::FAgentZetDataTableActions() {}
FAgentZetDataTableActions::~FAgentZetDataTableActions() {}

// ============================================================================
// IAgentZetActionExecutor Interface
// ============================================================================

FName FAgentZetDataTableActions::GetActionName() const { return FName(TEXT("DataTable")); }
FText FAgentZetDataTableActions::GetDisplayName() const { return LOCTEXT("DisplayName", "DataTable Tools"); }
EAgentZetActionCategory FAgentZetDataTableActions::GetCategory() const { return EAgentZetActionCategory::General; }
EAgentZetRiskLevel FAgentZetDataTableActions::GetDefaultRiskLevel() const { return EAgentZetRiskLevel::Medium; }
bool FAgentZetDataTableActions::CanUndo() const { return true; }
bool FAgentZetDataTableActions::UndoAction() { return GEditor && GEditor->UndoTransaction(); }

TArray<FString> FAgentZetDataTableActions::GetSupportedToolNames() const
{
	return {
		TEXT("create_data_table"),
		TEXT("import_json_to_datatable")
	};
}

bool FAgentZetDataTableActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	return true;
}

FAgentZetActionPlan FAgentZetDataTableActions::PreviewAction(const TSharedRef<FJsonObject>& Params)
{
	FAgentZetActionPlan Plan;
	FString Action;
	Params->TryGetStringField(TEXT("action"), Action);
	if (Action.IsEmpty()) Params->TryGetStringField(TEXT("tool_name"), Action);

	if (Action == TEXT("create_data_table"))
	{
		FString AssetPath;
		Params->TryGetStringField(TEXT("asset_path"), AssetPath);
		Plan.Summary = FString::Printf(TEXT("Create DataTable: %s"), *AssetPath);
	}
	else
	{
		Plan.Summary = TEXT("Import JSON data into DataTable");
	}

	Plan.MaxRiskLevel = EAgentZetRiskLevel::Medium;
	FAgentZetAction A;
	A.Description = Plan.Summary;
	A.Category = EAgentZetActionCategory::General;
	A.RiskLevel = EAgentZetRiskLevel::Medium;
	Plan.Actions.Add(A);
	return Plan;
}

FAgentZetActionResult FAgentZetDataTableActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FAgentZetActionResult Result;
	Result.bSuccess = false;

	FString Action;
	if (!Params->TryGetStringField(TEXT("action"), Action) || Action.IsEmpty())
		Params->TryGetStringField(TEXT("tool_name"), Action);

	if (Action == TEXT("create_data_table"))
		return ExecuteCreateDataTable(Params, Result);
	else if (Action == TEXT("import_json_to_datatable"))
		return ExecuteImportJsonToDataTable(Params, Result);

	// Infer from params
	if (Params->HasField(TEXT("row_struct")))
		return ExecuteCreateDataTable(Params, Result);
	if (Params->HasField(TEXT("json_data")))
		return ExecuteImportJsonToDataTable(Params, Result);

	Result.Errors.Add(TEXT("Could not determine DataTable action. Provide 'action' field or appropriate parameters."));
	return Result;
}

// ============================================================================
// create_data_table
// ============================================================================

FAgentZetActionResult FAgentZetDataTableActions::ExecuteCreateDataTable(
	const TSharedRef<FJsonObject>& Params, FAgentZetActionResult& Result)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		Result.Errors.Add(TEXT("Missing required field: 'asset_path'"));
		return Result;
	}

	FString RowStructName;
	if (!Params->TryGetStringField(TEXT("row_struct"), RowStructName))
	{
		Result.Errors.Add(TEXT("Missing required field: 'row_struct' — the name of the FTableRowBase-derived struct (e.g., 'FWeaponStatsRow')"));
		return Result;
	}

	// Resolve the row struct by name
	UScriptStruct* RowStruct = FindFirstObject<UScriptStruct>(*RowStructName, EFindFirstObjectOptions::NativeFirst);
	if (!RowStruct)
	{
		// Try without the 'F' prefix
		FString WithF = TEXT("F") + RowStructName;
		RowStruct = FindFirstObject<UScriptStruct>(*WithF, EFindFirstObjectOptions::NativeFirst);
	}

	if (!RowStruct)
	{
		Result.Errors.Add(FString::Printf(
			TEXT("Row struct '%s' not found. Ensure it derives from FTableRowBase and is compiled. "
				 "For built-in UE structs, use the full name (e.g., 'FCharacterInfoRow')."),
			*RowStructName));
		return Result;
	}

	// Verify it derives from FTableRowBase
	if (!RowStruct->IsChildOf(FTableRowBase::StaticStruct()))
	{
		Result.Errors.Add(FString::Printf(
			TEXT("Struct '%s' does not derive from FTableRowBase. DataTable row structs must inherit from FTableRowBase."),
			*RowStructName));
		return Result;
	}

	// Create the DataTable using the factory
	FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	UDataTableFactory* Factory = NewObject<UDataTableFactory>();
	Factory->Struct = RowStruct;

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UDataTable::StaticClass(), Factory);

	if (!NewAsset)
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create DataTable at '%s'. Check that the path is valid."), *AssetPath));
		return Result;
	}

	UDataTable* DataTable = Cast<UDataTable>(NewAsset);

	Result.bSuccess = true;
	Result.ModifiedAssets.Add(AssetPath);
	Result.ResultMessage = FString::Printf(
		TEXT("Created DataTable '%s' with row struct '%s'. "
			 "The table is empty — use import_json_to_datatable to populate it with rows."),
		*AssetPath, *RowStruct->GetName());

	return Result;
}

// ============================================================================
// import_json_to_datatable
// ============================================================================

FAgentZetActionResult FAgentZetDataTableActions::ExecuteImportJsonToDataTable(
	const TSharedRef<FJsonObject>& Params, FAgentZetActionResult& Result)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		Result.Errors.Add(TEXT("Missing required field: 'asset_path'"));
		return Result;
	}

	FString JsonDataStr;
	if (!Params->TryGetStringField(TEXT("json_data"), JsonDataStr))
	{
		Result.Errors.Add(TEXT("Missing required field: 'json_data' — JSON array of row objects"));
		return Result;
	}

	// Load the DataTable
	UDataTable* DataTable = LoadObject<UDataTable>(nullptr, *AssetPath);
	if (!DataTable)
	{
		Result.Errors.Add(FString::Printf(TEXT("DataTable not found at '%s'. Create it first with create_data_table."), *AssetPath));
		return Result;
	}

	// Parse the JSON — expect an array of objects, each with a "Name" key
	// Format: [{"Name": "Row_1", "Damage": 50, "FireRate": 0.5}, ...]
	TArray<TSharedPtr<FJsonValue>> JsonRows;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonDataStr);

	if (!FJsonSerializer::Deserialize(Reader, JsonRows) || JsonRows.Num() == 0)
	{
		Result.Errors.Add(TEXT("Failed to parse 'json_data' as a JSON array. Expected format: [{\"Name\": \"Row_1\", ...}, ...]"));
		return Result;
	}

	// Convert to the format UDataTable::CreateTableFromJSONString expects
	// UDataTable::CreateTableFromJSONString takes an array of objects where
	// the first column "Name" is the row key
	FString FormattedJson;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&FormattedJson);
	FJsonSerializer::Serialize(JsonRows, Writer);

	// Use DataTable's built-in JSON import
	TArray<FString> ImportProblems;
	DataTable->Modify();
	DataTable->EmptyTable();

	// DataTable expects JSON in the format:
	// [{"Name":"RowName","Column1":Value1,...},...]
	TArray<FString> ImportErrors = DataTable->CreateTableFromJSONString(FormattedJson);

	if (ImportErrors.Num() > 0)
	{
		for (const FString& Err : ImportErrors)
		{
			Result.Warnings.Add(FString::Printf(TEXT("Import note: %s"), *Err));
		}
	}

	int32 RowCount = DataTable->GetRowMap().Num();

	if (RowCount == 0)
	{
		// Fallback: try to manually add rows if the bulk import failed
		Result.Errors.Add(FString::Printf(
			TEXT("DataTable import produced 0 rows. Ensure the JSON matches the struct '%s'. "
				 "Each object must have a 'Name' field for the row key, and property names must match "
				 "the struct's UPROPERTY names exactly."),
			*DataTable->GetRowStructPathName().ToString()));
		return Result;
	}

	// Mark dirty for save
	DataTable->MarkPackageDirty();

	Result.bSuccess = true;
	Result.ModifiedAssets.Add(AssetPath);
	Result.ResultMessage = FString::Printf(
		TEXT("Successfully imported %d rows into DataTable '%s'."),
		RowCount, *AssetPath);

	return Result;
}

#undef LOCTEXT_NAMESPACE
