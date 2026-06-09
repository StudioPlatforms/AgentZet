// Copyright AgentZet. All Rights Reserved.

#include "Diagnostics/AgentZetDiagnosticsActions.h"
#include "AgentZetCoreModule.h"
#include "Logging/MessageLog.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/OutputDeviceHelper.h"

#define LOCTEXT_NAMESPACE "AgentZetDiagnosticsActions"

/**
 * Custom output device that captures log messages into a buffer.
 * Attached temporarily during read_message_log to capture recent entries.
 */
class FAgentZetLogCapture : public FOutputDevice
{
public:
	struct FLogEntry
	{
		FString Category;
		FString Message;
		ELogVerbosity::Type Verbosity;
		double Time;
	};

	TArray<FLogEntry> Entries;
	int32 MaxEntries = 500;

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		if (Entries.Num() < MaxEntries)
		{
			FLogEntry Entry;
			Entry.Category = Category.ToString();
			Entry.Message = V;
			Entry.Verbosity = Verbosity;
			Entry.Time = FPlatformTime::Seconds();
			Entries.Add(MoveTemp(Entry));
		}
	}
};

// Static log capture instance — always listening
static TSharedPtr<FAgentZetLogCapture> GAgentZetLogCapture;

// ============================================================================
// Lifecycle
// ============================================================================

FAgentZetDiagnosticsActions::FAgentZetDiagnosticsActions()
{
	// Create and attach the log capture device so it accumulates messages
	if (!GAgentZetLogCapture.IsValid())
	{
		GAgentZetLogCapture = MakeShared<FAgentZetLogCapture>();
		GLog->AddOutputDevice(GAgentZetLogCapture.Get());
	}
}

FAgentZetDiagnosticsActions::~FAgentZetDiagnosticsActions()
{
	// We intentionally do NOT remove the device here — it's a singleton
	// that persists for the editor session.
}

// ============================================================================
// IAgentZetActionExecutor Interface
// ============================================================================

FName FAgentZetDiagnosticsActions::GetActionName() const { return FName(TEXT("Diagnostics")); }
FText FAgentZetDiagnosticsActions::GetDisplayName() const { return LOCTEXT("DisplayName", "Diagnostics & Message Log"); }
EAgentZetActionCategory FAgentZetDiagnosticsActions::GetCategory() const { return EAgentZetActionCategory::General; }
EAgentZetRiskLevel FAgentZetDiagnosticsActions::GetDefaultRiskLevel() const { return EAgentZetRiskLevel::Low; }
bool FAgentZetDiagnosticsActions::CanUndo() const { return false; }
bool FAgentZetDiagnosticsActions::UndoAction() { return false; }

TArray<FString> FAgentZetDiagnosticsActions::GetSupportedToolNames() const
{
	return { TEXT("read_message_log") };
}

bool FAgentZetDiagnosticsActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	return true; // All params optional
}

FAgentZetActionPlan FAgentZetDiagnosticsActions::PreviewAction(const TSharedRef<FJsonObject>& Params)
{
	FAgentZetActionPlan Plan;
	Plan.Summary = TEXT("Read recent Output Log entries (read-only)");
	Plan.MaxRiskLevel = EAgentZetRiskLevel::Low;
	FAgentZetAction Action;
	Action.Description = Plan.Summary;
	Action.Category = EAgentZetActionCategory::General;
	Action.RiskLevel = EAgentZetRiskLevel::Low;
	Plan.Actions.Add(Action);
	return Plan;
}

FAgentZetActionResult FAgentZetDiagnosticsActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FAgentZetActionResult Result;
	Result.bSuccess = false;
	return ExecuteReadMessageLog(Params, Result);
}

// ============================================================================
// read_message_log
// ============================================================================

FAgentZetActionResult FAgentZetDiagnosticsActions::ExecuteReadMessageLog(
	const TSharedRef<FJsonObject>& Params, FAgentZetActionResult& Result)
{
	if (!GAgentZetLogCapture.IsValid())
	{
		Result.Errors.Add(TEXT("Log capture device is not initialized."));
		return Result;
	}

	// Parse optional filters
	int32 MaxLines = 100;
	Params->TryGetNumberField(TEXT("max_lines"), MaxLines);
	MaxLines = FMath::Clamp(MaxLines, 1, 500);

	FString CategoryFilter;
	Params->TryGetStringField(TEXT("category_filter"), CategoryFilter);

	FString SeverityFilter;
	Params->TryGetStringField(TEXT("severity_filter"), SeverityFilter);

	bool bErrorsOnly = false;
	if (SeverityFilter.Equals(TEXT("Error"), ESearchCase::IgnoreCase))
		bErrorsOnly = true;

	bool bWarningsAndErrors = false;
	if (SeverityFilter.Equals(TEXT("Warning"), ESearchCase::IgnoreCase))
		bWarningsAndErrors = true;

	// Filter and format entries
	FString Output = TEXT("=== Output Log ===\n");
	int32 OutputCount = 0;
	int32 TotalErrors = 0;
	int32 TotalWarnings = 0;

	// Read from end (most recent first)
	const auto& Entries = GAgentZetLogCapture->Entries;
	int32 StartIdx = FMath::Max(0, Entries.Num() - MaxLines * 2); // Over-read to account for filtering

	for (int32 i = Entries.Num() - 1; i >= StartIdx && OutputCount < MaxLines; --i)
	{
		const auto& Entry = Entries[i];

		// Count stats
		if (Entry.Verbosity == ELogVerbosity::Error || Entry.Verbosity == ELogVerbosity::Fatal)
			TotalErrors++;
		if (Entry.Verbosity == ELogVerbosity::Warning)
			TotalWarnings++;

		// Apply category filter
		if (!CategoryFilter.IsEmpty() && !Entry.Category.Contains(CategoryFilter, ESearchCase::IgnoreCase))
			continue;

		// Apply severity filter
		if (bErrorsOnly && Entry.Verbosity != ELogVerbosity::Error && Entry.Verbosity != ELogVerbosity::Fatal)
			continue;
		if (bWarningsAndErrors && Entry.Verbosity != ELogVerbosity::Error
			&& Entry.Verbosity != ELogVerbosity::Fatal && Entry.Verbosity != ELogVerbosity::Warning)
			continue;

		// Format the entry
		FString Severity;
		switch (Entry.Verbosity)
		{
		case ELogVerbosity::Fatal:   Severity = TEXT("FATAL"); break;
		case ELogVerbosity::Error:   Severity = TEXT("ERROR"); break;
		case ELogVerbosity::Warning: Severity = TEXT("WARN "); break;
		default:                     Severity = TEXT("LOG  "); break;
		}

		// Truncate very long messages
		FString Msg = Entry.Message.Left(500);
		Output += FString::Printf(TEXT("[%s] %s: %s\n"), *Severity, *Entry.Category, *Msg);
		OutputCount++;
	}

	if (OutputCount == 0)
	{
		Output += TEXT("  (no matching log entries found)\n");
	}

	Output += FString::Printf(TEXT("\n--- Showing %d of %d total entries | %d errors, %d warnings ---\n"),
		OutputCount, Entries.Num(), TotalErrors, TotalWarnings);

	// Optionally clear after reading
	bool bClearAfterRead = false;
	Params->TryGetBoolField(TEXT("clear_after_read"), bClearAfterRead);
	if (bClearAfterRead)
	{
		GAgentZetLogCapture->Entries.Empty();
		Output += TEXT("(log buffer cleared)\n");
	}

	Result.bSuccess = true;
	Result.ResultMessage = Output;
	return Result;
}

#undef LOCTEXT_NAMESPACE
