// Copyright AgentZet. All Rights Reserved.

#include "SourceControl/AgentZetSourceControlActions.h"
#include "AgentZetCoreModule.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "SourceControlOperations.h"
#include "SourceControlHelpers.h"

FAgentZetSourceControlActions::FAgentZetSourceControlActions() {}
FAgentZetSourceControlActions::~FAgentZetSourceControlActions() {}
FName FAgentZetSourceControlActions::GetActionName() const { return FName(TEXT("SourceControl")); }
FText FAgentZetSourceControlActions::GetDisplayName() const { return FText::FromString(TEXT("Source Control Actions")); }
EAgentZetActionCategory FAgentZetSourceControlActions::GetCategory() const { return EAgentZetActionCategory::SourceControl; }
EAgentZetRiskLevel FAgentZetSourceControlActions::GetDefaultRiskLevel() const { return EAgentZetRiskLevel::Medium; }
bool FAgentZetSourceControlActions::CanUndo() const { return true; }
bool FAgentZetSourceControlActions::UndoAction() { return false; }

TArray<FString> FAgentZetSourceControlActions::GetSupportedToolNames() const
{
	return {
		TEXT("source_control_checkout"),
		TEXT("source_control_add"),
		TEXT("source_control_revert"),
		TEXT("source_control_status")
	};
}

bool FAgentZetSourceControlActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const { return true; }

FAgentZetActionPlan FAgentZetSourceControlActions::PreviewAction(const TSharedRef<FJsonObject>& Params)
{
	FAgentZetActionPlan Plan;
	Plan.Summary = TEXT("Source control operation");
	FAgentZetAction Action;
	Action.Description = Plan.Summary;
	Action.Category = EAgentZetActionCategory::SourceControl;
	Action.RiskLevel = EAgentZetRiskLevel::Medium;
	Plan.Actions.Add(Action);
	return Plan;
}

FAgentZetActionResult FAgentZetSourceControlActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FAgentZetActionResult Result;
	Result.bSuccess = false;

	ISourceControlModule& SCModule = ISourceControlModule::Get();
	if (!SCModule.IsEnabled())
	{
		Result.Errors.Add(TEXT("Source control is not enabled in the editor."));
		return Result;
	}

	ISourceControlProvider& Provider = SCModule.GetProvider();

	FString Action;
	if (!Params->TryGetStringField(TEXT("action"), Action))
	{
		Result.Errors.Add(TEXT("Missing 'action' field."));
		return Result;
	}

	// Collect file paths
	TArray<FString> FilePaths;
	const TArray<TSharedPtr<FJsonValue>>* PathsArray = nullptr;
	if (Params->TryGetArrayField(TEXT("files"), PathsArray))
	{
		for (const auto& Val : *PathsArray)
		{
			FString Path;
			if (Val->TryGetString(Path)) FilePaths.Add(Path);
		}
	}
	else
	{
		FString SinglePath;
		if (Params->TryGetStringField(TEXT("file"), SinglePath))
		{
			FilePaths.Add(SinglePath);
		}
	}

	if (Action == TEXT("checkout"))
	{
		if (FilePaths.Num() == 0)
		{
			Result.Errors.Add(TEXT("No files specified for checkout."));
			return Result;
		}

		bool bSuccess = USourceControlHelpers::CheckOutFiles(FilePaths);
		Result.bSuccess = bSuccess;
		Result.ResultMessage = bSuccess
			? FString::Printf(TEXT("Checked out %d file(s)."), FilePaths.Num())
			: TEXT("Failed to check out files.");
	}
	else if (Action == TEXT("add"))
	{
		if (FilePaths.Num() == 0)
		{
			Result.Errors.Add(TEXT("No files specified for add."));
			return Result;
		}

		bool bSuccess = USourceControlHelpers::MarkFilesForAdd(FilePaths);
		Result.bSuccess = bSuccess;
		Result.ResultMessage = bSuccess
			? FString::Printf(TEXT("Marked %d file(s) for add."), FilePaths.Num())
			: TEXT("Failed to mark files for add.");
	}
	else if (Action == TEXT("revert"))
	{
		if (FilePaths.Num() == 0)
		{
			Result.Errors.Add(TEXT("No files specified for revert."));
			return Result;
		}

		// Revert via provider execute
		TSharedRef<FRevert, ESPMode::ThreadSafe> RevertOp = ISourceControlOperation::Create<FRevert>();
		ECommandResult::Type OpResult = Provider.Execute(RevertOp, FilePaths);
		Result.bSuccess = (OpResult == ECommandResult::Succeeded);
		Result.ResultMessage = Result.bSuccess
			? FString::Printf(TEXT("Reverted %d file(s)."), FilePaths.Num())
			: TEXT("Revert operation failed.");
	}
	else if (Action == TEXT("status"))
	{
		FString StatusReport = TEXT("Source Control Status:\n");
		StatusReport += FString::Printf(TEXT("Provider: %s\n"), *Provider.GetName().ToString());
		StatusReport += FString::Printf(TEXT("Connected: %s\n"), Provider.IsAvailable() ? TEXT("Yes") : TEXT("No"));

		if (FilePaths.Num() > 0)
		{
			for (const FString& Path : FilePaths)
			{
				FSourceControlStatePtr State = Provider.GetState(Path, EStateCacheUsage::ForceUpdate);
				if (State.IsValid())
				{
					StatusReport += FString::Printf(TEXT("  %s: %s\n"), *Path, *State->GetDisplayName().ToString());
				}
			}
		}

		Result.bSuccess = true;
		Result.ResultMessage = StatusReport;
	}

	return Result;
}
