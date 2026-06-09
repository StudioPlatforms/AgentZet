// Copyright AgentZet. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "AgentZetTaskHistory.h"

DECLARE_DELEGATE_OneParam(FOnAgentZetLoadHistoryTask, const FString& /*TabId*/);
DECLARE_DELEGATE_OneParam(FOnAgentZetDeleteHistoryTask, const FString& /*TabId*/);
DECLARE_DELEGATE_TwoParams(FOnAgentZetRenameHistoryTask, const FString& /*TabId*/, const FString& /*NewTitle*/);

/** Sort modes for history list */
enum class EAgentZetHistorySortMode : uint8
{
	Newest,        // by LastActiveAt descending (default)
	Oldest,        // by LastActiveAt ascending
	MostExpensive, // by TotalCostUSD descending
	MostTokens     // by total tokens descending
};

/** Date group categories for history items */
enum class EAgentZetDateGroup : uint8
{
	Today,
	Yesterday,
	Previous7Days,
	Previous30Days,
	OlderMonth  // grouped by month name
};

/**
 * Enhanced task history browser panel.
 *
 * v5.0: Major rewrite with date grouping, rich per-entry display,
 * sort options, inline rename, delete confirmation, and search.
 *
 * Ported from Roo Code's HistoryView.tsx.
 */
class AGENTZETUI_API SAgentZetHistoryPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAgentZetHistoryPanel) {}
		SLATE_EVENT(FOnAgentZetLoadHistoryTask, OnLoadTask)
		SLATE_EVENT(FOnAgentZetDeleteHistoryTask, OnDeleteTask)
		SLATE_EVENT(FOnAgentZetRenameHistoryTask, OnRenameTask)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Refresh history list from external data */
	void RefreshHistory(const TArray<FAgentZetTaskHistoryItem>& Items);

	/** Auto-generate a title from the first user message (public for MainPanel access) */
	static FString GenerateAutoTitle(const FString& FirstUserMessage);

private:
	// ---- Data ----
	TSharedPtr<class SScrollBox> HistoryList;
	TSharedPtr<class SEditableTextBox> SearchBox;
	TArray<FAgentZetTaskHistoryItem> AllItems;
	FString CurrentSearchQuery;
	EAgentZetHistorySortMode CurrentSortMode = EAgentZetHistorySortMode::Newest;

	// ---- Delegates ----
	FOnAgentZetLoadHistoryTask OnLoadTask;
	FOnAgentZetDeleteHistoryTask OnDeleteTask;
	FOnAgentZetRenameHistoryTask OnRenameTask;

	// ---- Inline rename state ----
	FString RenamingTabId;  // empty = not renaming
	TSharedPtr<class SEditableTextBox> RenameTextBox;

	// ---- Event handlers ----
	void OnSearchTextChanged(const FText& NewText);
	void OnSortModeChanged(EAgentZetHistorySortMode NewMode);

	// ---- Core rebuild ----
	void RebuildList();

	/** Apply current search filter to items */
	TArray<FAgentZetTaskHistoryItem> GetFilteredItems() const;

	/** Sort items by current sort mode */
	void SortItems(TArray<FAgentZetTaskHistoryItem>& Items) const;

	/** Determine the date group for a timestamp */
	static EAgentZetDateGroup GetDateGroup(const FDateTime& Timestamp);

	/** Get display string for a date group */
	static FString GetDateGroupLabel(EAgentZetDateGroup Group, const FDateTime& SampleTimestamp);

	// ---- Widget builders ----
	TSharedRef<SWidget> BuildDateGroupHeader(const FString& GroupLabel);
	TSharedRef<SWidget> BuildHistoryEntry(const FAgentZetTaskHistoryItem& Item);
	TSharedRef<SWidget> BuildSortDropdown();

	// ---- Formatting helpers ----
	static FString FormatRelativeTime(const FDateTime& Timestamp);
	static FString FormatTokenCount(int32 TotalTokens);
	static FString FormatCost(float CostUSD);
	static FString GetStatusIcon(EAgentZetTaskStatus Status);
	static FLinearColor GetStatusColor(EAgentZetTaskStatus Status);
	static FString TruncateTitle(const FString& Title, int32 MaxChars = 60);

	// ---- Actions ----
	void OnOpenClicked(const FString& TabId);
	void OnDeleteClicked(const FString& TabId);
	void OnRenameStarted(const FString& TabId, const FString& CurrentTitle);
	void OnRenameCommitted(const FText& NewText, ETextCommit::Type CommitType);
};
