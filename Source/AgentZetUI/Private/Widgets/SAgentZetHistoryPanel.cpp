// Copyright AgentZet. All Rights Reserved.

#include "Widgets/SAgentZetHistoryPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/MessageDialog.h"

// ============================================================================
// Construct
// ============================================================================

void SAgentZetHistoryPanel::Construct(const FArguments& InArgs)
{
	OnLoadTask = InArgs._OnLoadTask;
	OnDeleteTask = InArgs._OnDeleteTask;
	OnRenameTask = InArgs._OnRenameTask;

	ChildSlot
	[
		SNew(SVerticalBox)

		// Header row: title + sort dropdown
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8, 6, 8, 2)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("\xF0\x9F\x93\x8B Task History")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
				.ColorAndOpacity(FLinearColor(0.85f, 0.85f, 1.0f))
			]

			// Sort dropdown
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				BuildSortDropdown()
			]
		]

		// Search box
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8, 4, 8, 4)
		[
			SAssignNew(SearchBox, SEditableTextBox)
			.HintText(FText::FromString(TEXT("\xF0\x9F\x94\x8D Search tasks...")))
			.OnTextChanged(this, &SAgentZetHistoryPanel::OnSearchTextChanged)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4, 0)
		[
			SNew(SSeparator).Thickness(1.0f)
		]

		// History list (scrollable)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SAssignNew(HistoryList, SScrollBox)
		]
	];
}

// ============================================================================
// Data Management
// ============================================================================

void SAgentZetHistoryPanel::RefreshHistory(const TArray<FAgentZetTaskHistoryItem>& Items)
{
	AllItems = Items;
	RebuildList();
}

void SAgentZetHistoryPanel::OnSearchTextChanged(const FText& NewText)
{
	CurrentSearchQuery = NewText.ToString();
	RebuildList();
}

void SAgentZetHistoryPanel::OnSortModeChanged(EAgentZetHistorySortMode NewMode)
{
	CurrentSortMode = NewMode;
	RebuildList();
}

TArray<FAgentZetTaskHistoryItem> SAgentZetHistoryPanel::GetFilteredItems() const
{
	TArray<FAgentZetTaskHistoryItem> Result;
	for (const FAgentZetTaskHistoryItem& Item : AllItems)
	{
		if (!CurrentSearchQuery.IsEmpty())
		{
			if (!Item.Title.Contains(CurrentSearchQuery, ESearchCase::IgnoreCase) &&
				!Item.FirstUserMessage.Contains(CurrentSearchQuery, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}
		Result.Add(Item);
	}
	return Result;
}

void SAgentZetHistoryPanel::SortItems(TArray<FAgentZetTaskHistoryItem>& Items) const
{
	switch (CurrentSortMode)
	{
	case EAgentZetHistorySortMode::Newest:
		Items.Sort([](const FAgentZetTaskHistoryItem& A, const FAgentZetTaskHistoryItem& B)
		{
			return A.LastActiveAt > B.LastActiveAt;
		});
		break;

	case EAgentZetHistorySortMode::Oldest:
		Items.Sort([](const FAgentZetTaskHistoryItem& A, const FAgentZetTaskHistoryItem& B)
		{
			return A.LastActiveAt < B.LastActiveAt;
		});
		break;

	case EAgentZetHistorySortMode::MostExpensive:
		Items.Sort([](const FAgentZetTaskHistoryItem& A, const FAgentZetTaskHistoryItem& B)
		{
			return A.TotalCostUSD > B.TotalCostUSD;
		});
		break;

	case EAgentZetHistorySortMode::MostTokens:
		Items.Sort([](const FAgentZetTaskHistoryItem& A, const FAgentZetTaskHistoryItem& B)
		{
			return A.TotalTokenUsage.TotalTokens() > B.TotalTokenUsage.TotalTokens();
		});
		break;
	}
}

// ============================================================================
// Date Grouping
// ============================================================================

EAgentZetDateGroup SAgentZetHistoryPanel::GetDateGroup(const FDateTime& Timestamp)
{
	const FDateTime Now = FDateTime::UtcNow();
	const FDateTime TodayStart(Now.GetYear(), Now.GetMonth(), Now.GetDay());
	const FDateTime YesterdayStart = TodayStart - FTimespan::FromDays(1);

	if (Timestamp >= TodayStart)
	{
		return EAgentZetDateGroup::Today;
	}
	if (Timestamp >= YesterdayStart)
	{
		return EAgentZetDateGroup::Yesterday;
	}

	const FTimespan Delta = Now - Timestamp;
	if (Delta.GetTotalDays() <= 7.0)
	{
		return EAgentZetDateGroup::Previous7Days;
	}
	if (Delta.GetTotalDays() <= 30.0)
	{
		return EAgentZetDateGroup::Previous30Days;
	}
	return EAgentZetDateGroup::OlderMonth;
}

FString SAgentZetHistoryPanel::GetDateGroupLabel(EAgentZetDateGroup Group, const FDateTime& SampleTimestamp)
{
	switch (Group)
	{
	case EAgentZetDateGroup::Today:          return TEXT("Today");
	case EAgentZetDateGroup::Yesterday:      return TEXT("Yesterday");
	case EAgentZetDateGroup::Previous7Days:  return TEXT("Previous 7 Days");
	case EAgentZetDateGroup::Previous30Days: return TEXT("Previous 30 Days");
	case EAgentZetDateGroup::OlderMonth:
		// Format as "Month Year", e.g. "March 2026"
		return SampleTimestamp.ToString(TEXT("%B %Y"));
	default: return TEXT("Older");
	}
}

// ============================================================================
// Core Rebuild — builds the full scrollbox content
// ============================================================================

void SAgentZetHistoryPanel::RebuildList()
{
	if (!HistoryList.IsValid()) return;
	HistoryList->ClearChildren();

	TArray<FAgentZetTaskHistoryItem> FilteredItems = GetFilteredItems();

	if (FilteredItems.Num() == 0)
	{
		HistoryList->AddSlot()
		.Padding(12, 20)
		[
			SNew(STextBlock)
			.Text(FText::FromString(
				AllItems.Num() == 0
					? TEXT("No history yet.\nCompleted tasks will appear here.")
					: TEXT("No tasks match your search.")))
			.ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.4f))
			.AutoWrapText(true)
			.Justification(ETextJustify::Center)
		];
		return;
	}

	SortItems(FilteredItems);

	// Build date-grouped display
	FString CurrentGroupLabel;
	for (const FAgentZetTaskHistoryItem& Item : FilteredItems)
	{
		// Determine which date group this item belongs to
		const EAgentZetDateGroup Group = GetDateGroup(Item.LastActiveAt);
		FString GroupLabel = GetDateGroupLabel(Group, Item.LastActiveAt);

		// For OlderMonth, use the month+year as unique key so different months get separate headers
		if (Group == EAgentZetDateGroup::OlderMonth)
		{
			GroupLabel = Item.LastActiveAt.ToString(TEXT("%B %Y"));
		}

		// Insert group header when group changes
		if (GroupLabel != CurrentGroupLabel)
		{
			CurrentGroupLabel = GroupLabel;
			HistoryList->AddSlot()
			.Padding(0, 4, 0, 0)
			[
				BuildDateGroupHeader(GroupLabel)
			];
		}

		// Add the entry
		HistoryList->AddSlot()
		.Padding(4, 0, 4, 2)
		[
			BuildHistoryEntry(Item)
		];
	}
}

// ============================================================================
// Widget Builders
// ============================================================================

TSharedRef<SWidget> SAgentZetHistoryPanel::BuildDateGroupHeader(const FString& GroupLabel)
{
	return SNew(SBorder)
		.BorderBackgroundColor(FLinearColor(0.06f, 0.06f, 0.08f))
		.Padding(FMargin(8, 4, 8, 3))
		[
			SNew(STextBlock)
			.Text(FText::FromString(GroupLabel))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
			.ColorAndOpacity(FLinearColor(0.55f, 0.55f, 0.7f))
		];
}

TSharedRef<SWidget> SAgentZetHistoryPanel::BuildHistoryEntry(const FAgentZetTaskHistoryItem& Item)
{
	const FString StatusIcon = GetStatusIcon(Item.Status);
	const FLinearColor StatusColor = GetStatusColor(Item.Status);
	const FString RelTime = FormatRelativeTime(Item.LastActiveAt);
	const FString TokenStr = FormatTokenCount(Item.TotalTokenUsage.TotalTokens());
	const FString CostStr = FormatCost(Item.TotalCostUSD);
	const FString DisplayTitle = TruncateTitle(Item.Title.IsEmpty() ? Item.FirstUserMessage.Left(60) : Item.Title);

	// Model badge text (compact)
	FString ModelBadge = Item.ModelId;
	if (ModelBadge.Len() > 20)
	{
		// Shorten "claude-3-5-sonnet-20241022" -> "claude-3-5-son..."
		ModelBadge = ModelBadge.Left(17) + TEXT("...");
	}

	// Check if this entry is being renamed — build title widget accordingly
	const bool bIsRenaming = !RenamingTabId.IsEmpty() && RenamingTabId == Item.TabId;
	TSharedRef<SWidget> TitleWidget = SNullWidget::NullWidget;
	if (bIsRenaming)
	{
		TitleWidget = SAssignNew(RenameTextBox, SEditableTextBox)
			.Text(FText::FromString(DisplayTitle))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
			.OnTextCommitted(this, &SAgentZetHistoryPanel::OnRenameCommitted)
			.SelectAllTextWhenFocused(true);
	}
	else
	{
		TitleWidget = SNew(STextBlock)
			.Text(FText::FromString(DisplayTitle))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
			.ColorAndOpacity(FLinearColor(0.92f, 0.92f, 0.92f));
	}

	return SNew(SBorder)
		.BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.12f))
		.Padding(FMargin(8, 6))
		[
			SNew(SVerticalBox)

			// Row 1: Status icon + Title + Timestamp
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				// Status icon
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0, 0, 4, 0)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(StatusIcon))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					.ColorAndOpacity(StatusColor)
				]

				// Title (or rename text box)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					TitleWidget
				]

				// Relative timestamp
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(6, 0, 0, 0)
				[
					SNew(STextBlock)
					.Text(FText::FromString(RelTime))
					.ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.4f))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 7))
				]
			]

			// Row 2: Model badge + Token count + Cost
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 3, 0, 0)
			[
				SNew(SHorizontalBox)

				// Model badge
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0, 0, 6, 0)
				.VAlign(VAlign_Center)
				[
					SNew(SBorder)
					.BorderBackgroundColor(FLinearColor(0.15f, 0.15f, 0.2f))
					.Padding(FMargin(4, 1))
					[
						SNew(STextBlock)
						.Text(FText::FromString(ModelBadge.IsEmpty() ? TEXT("unknown") : ModelBadge))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 6))
						.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.65f))
					]
				]

				// Token count
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0, 0, 6, 0)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TokenStr))
					.ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.5f))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 7))
				]

				// Cost
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0, 0, 6, 0)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(CostStr))
					.ColorAndOpacity(FLinearColor(0.3f, 0.7f, 0.3f))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 7))
					.Visibility(CostStr.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
				]

				// Message count
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("%d msgs"), Item.MessageCount)))
					.ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.45f))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 7))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SSpacer)
				]
			]

			// Row 3: Action buttons
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4, 0, 0)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SSpacer)
				]

				// Rename button
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0, 0, 4, 0)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("\xE2\x9C\x8F")))  // pencil icon
					.ToolTipText(FText::FromString(TEXT("Rename task")))
					.OnClicked_Lambda([this, TabId = Item.TabId, Title = Item.Title]() -> FReply
					{
						OnRenameStarted(TabId, Title);
						return FReply::Handled();
					})
				]

				// Open/Resume button
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0, 0, 4, 0)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("\xE2\x96\xB6 Open")))
					.ToolTipText(FText::FromString(TEXT("Open this task in a new tab")))
					.OnClicked_Lambda([this, TabId = Item.TabId]() -> FReply
					{
						OnOpenClicked(TabId);
						return FReply::Handled();
					})
				]

				// Delete button
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("\xE2\x9C\x95")))  // X icon
					.ToolTipText(FText::FromString(TEXT("Delete task from history")))
					.OnClicked_Lambda([this, TabId = Item.TabId]() -> FReply
					{
						OnDeleteClicked(TabId);
						return FReply::Handled();
					})
				]
			]
		];
}

TSharedRef<SWidget> SAgentZetHistoryPanel::BuildSortDropdown()
{
	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 0, 2, 0)
		[
			SNew(SButton)
			.Text(FText::FromString(TEXT("Newest")))
			.ToolTipText(FText::FromString(TEXT("Sort by most recent")))
			.OnClicked_Lambda([this]() -> FReply
			{
				OnSortModeChanged(EAgentZetHistorySortMode::Newest);
				return FReply::Handled();
			})
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 0, 2, 0)
		[
			SNew(SButton)
			.Text(FText::FromString(TEXT("Oldest")))
			.ToolTipText(FText::FromString(TEXT("Sort by oldest")))
			.OnClicked_Lambda([this]() -> FReply
			{
				OnSortModeChanged(EAgentZetHistorySortMode::Oldest);
				return FReply::Handled();
			})
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 0, 2, 0)
		[
			SNew(SButton)
			.Text(FText::FromString(TEXT("$$$")))
			.ToolTipText(FText::FromString(TEXT("Sort by most expensive")))
			.OnClicked_Lambda([this]() -> FReply
			{
				OnSortModeChanged(EAgentZetHistorySortMode::MostExpensive);
				return FReply::Handled();
			})
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Text(FText::FromString(TEXT("Tok")))
			.ToolTipText(FText::FromString(TEXT("Sort by most tokens")))
			.OnClicked_Lambda([this]() -> FReply
			{
				OnSortModeChanged(EAgentZetHistorySortMode::MostTokens);
				return FReply::Handled();
			})
		];
}

// ============================================================================
// Formatting Helpers
// ============================================================================

FString SAgentZetHistoryPanel::FormatRelativeTime(const FDateTime& Timestamp)
{
	const FTimespan Delta = FDateTime::UtcNow() - Timestamp;
	if (Delta.GetTotalSeconds() < 60)   return TEXT("just now");
	if (Delta.GetTotalMinutes() < 60)   return FString::Printf(TEXT("%dm ago"), FMath::FloorToInt(Delta.GetTotalMinutes()));
	if (Delta.GetTotalHours() < 24)     return FString::Printf(TEXT("%dh ago"), FMath::FloorToInt(Delta.GetTotalHours()));

	const FDateTime Now = FDateTime::UtcNow();
	const FDateTime YesterdayStart = FDateTime(Now.GetYear(), Now.GetMonth(), Now.GetDay()) - FTimespan::FromDays(1);
	if (Timestamp >= YesterdayStart)    return TEXT("Yesterday");

	if (Delta.GetTotalDays() < 7)       return FString::Printf(TEXT("%dd ago"), FMath::FloorToInt(Delta.GetTotalDays()));

	// Absolute for older items
	return Timestamp.ToString(TEXT("%b %d, %Y"));
}

FString SAgentZetHistoryPanel::FormatTokenCount(int32 TotalTokens)
{
	if (TotalTokens < 1000)
	{
		return FString::Printf(TEXT("%d tokens"), TotalTokens);
	}
	if (TotalTokens < 100000)
	{
		return FString::Printf(TEXT("%.1fK tokens"), TotalTokens / 1000.0f);
	}
	return FString::Printf(TEXT("%.0fK tokens"), TotalTokens / 1000.0f);
}

FString SAgentZetHistoryPanel::FormatCost(float CostUSD)
{
	if (CostUSD < 0.001f)  return FString();
	if (CostUSD < 0.01f)   return FString::Printf(TEXT("$%.3f"), CostUSD);
	if (CostUSD < 1.0f)    return FString::Printf(TEXT("$%.2f"), CostUSD);
	return FString::Printf(TEXT("$%.2f"), CostUSD);
}

FString SAgentZetHistoryPanel::GetStatusIcon(EAgentZetTaskStatus Status)
{
	switch (Status)
	{
	case EAgentZetTaskStatus::Completed:   return TEXT("\xE2\x9C\x85");  // ✅
	case EAgentZetTaskStatus::Interrupted: return TEXT("\xE2\x8F\xB8");  // ⏸
	case EAgentZetTaskStatus::Errored:     return TEXT("\xE2\x9D\x8C");  // ❌
	case EAgentZetTaskStatus::Active:
	default:                                return TEXT("\xF0\x9F\x94\x84");  // 🔄
	}
}

FLinearColor SAgentZetHistoryPanel::GetStatusColor(EAgentZetTaskStatus Status)
{
	switch (Status)
	{
	case EAgentZetTaskStatus::Completed:   return FLinearColor(0.2f, 0.8f, 0.2f);
	case EAgentZetTaskStatus::Interrupted: return FLinearColor(0.8f, 0.7f, 0.2f);
	case EAgentZetTaskStatus::Errored:     return FLinearColor(0.9f, 0.2f, 0.2f);
	case EAgentZetTaskStatus::Active:
	default:                                return FLinearColor(0.3f, 0.6f, 0.9f);
	}
}

FString SAgentZetHistoryPanel::TruncateTitle(const FString& Title, int32 MaxChars)
{
	if (Title.Len() <= MaxChars)
	{
		return Title;
	}

	// Truncate at word boundary
	FString Truncated = Title.Left(MaxChars);
	int32 LastSpace = INDEX_NONE;
	Truncated.FindLastChar(TEXT(' '), LastSpace);
	if (LastSpace > MaxChars / 2)
	{
		Truncated = Truncated.Left(LastSpace);
	}
	return Truncated + TEXT("...");
}

// ============================================================================
// Actions
// ============================================================================

void SAgentZetHistoryPanel::OnOpenClicked(const FString& TabId)
{
	OnLoadTask.ExecuteIfBound(TabId);
}

void SAgentZetHistoryPanel::OnDeleteClicked(const FString& TabId)
{
	// Confirmation dialog
	const EAppReturnType::Type Result = FMessageDialog::Open(
		EAppMsgType::YesNo,
		FText::FromString(TEXT("Are you sure you want to delete this task?\nThis will remove all conversation data and cannot be undone."))
	);

	if (Result == EAppReturnType::Yes)
	{
		OnDeleteTask.ExecuteIfBound(TabId);
	}
}

void SAgentZetHistoryPanel::OnRenameStarted(const FString& TabId, const FString& CurrentTitle)
{
	RenamingTabId = TabId;
	RebuildList();

	// Focus the rename text box on next tick
	if (RenameTextBox.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(RenameTextBox, EFocusCause::SetDirectly);
	}
}

void SAgentZetHistoryPanel::OnRenameCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter && !RenamingTabId.IsEmpty())
	{
		const FString NewTitle = NewText.ToString().TrimStartAndEnd();
		if (!NewTitle.IsEmpty())
		{
			// Update local cache
			for (FAgentZetTaskHistoryItem& Item : AllItems)
			{
				if (Item.TabId == RenamingTabId)
				{
					Item.Title = NewTitle;
					break;
				}
			}
			// Notify parent
			OnRenameTask.ExecuteIfBound(RenamingTabId, NewTitle);
		}
	}

	// Clear rename state and rebuild
	RenamingTabId.Empty();
	RenameTextBox.Reset();
	RebuildList();
}

// ============================================================================
// Auto-Titling
// ============================================================================

FString SAgentZetHistoryPanel::GenerateAutoTitle(const FString& FirstUserMessage)
{
	if (FirstUserMessage.IsEmpty())
	{
		return TEXT("New Task");
	}

	// Clean up the message: remove leading whitespace, @references
	FString Clean = FirstUserMessage.TrimStartAndEnd();

	// Remove @file(...) references for cleaner titles
	while (Clean.Contains(TEXT("@")))
	{
		int32 AtIdx = INDEX_NONE;
		Clean.FindChar(TEXT('@'), AtIdx);
		if (AtIdx == INDEX_NONE) break;

		int32 EndIdx = AtIdx + 1;
		while (EndIdx < Clean.Len() && !FChar::IsWhitespace(Clean[EndIdx]))
		{
			EndIdx++;
		}
		Clean = Clean.Left(AtIdx) + Clean.Mid(EndIdx);
	}

	Clean = Clean.TrimStartAndEnd();
	if (Clean.IsEmpty())
	{
		return TEXT("New Task");
	}

	// Take first 60 characters, truncate at word boundary
	if (Clean.Len() <= 60)
	{
		return Clean;
	}

	FString Truncated = Clean.Left(60);
	int32 LastSpace = INDEX_NONE;
	Truncated.FindLastChar(TEXT(' '), LastSpace);
	if (LastSpace > 30)
	{
		Truncated = Truncated.Left(LastSpace);
	}
	return Truncated + TEXT("...");
}
