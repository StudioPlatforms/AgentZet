// Copyright AgentZet. All Rights Reserved.

#include "Widgets/SAgentZetPlanPreview.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"

void SAgentZetPlanPreview::Construct(const FArguments& InArgs)
{
	OnPlanApproved = InArgs._OnPlanApproved;
	OnPlanRejected = InArgs._OnPlanRejected;

	// Start hidden — only shown when there's a pending plan to review
	SetVisibility(EVisibility::Collapsed);

	ChildSlot
	[
		SNew(SBorder)
		.BorderBackgroundColor(FLinearColor(0.15f, 0.15f, 0.2f))
		.Padding(8.0f)
		[
			SNew(SVerticalBox)

			// Header
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("🔧 Pending Tool Actions — Review Before Execution")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
				.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.9f, 0.3f)))
			]

			// Action list
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(ActionList, SVerticalBox)
			]

			// Approve / Reject buttons
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 6, 0, 0)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0, 0, 8, 0)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("✅ Approve & Execute")))
					.OnClicked(FOnClicked::CreateSP(this, &SAgentZetPlanPreview::OnApproveClicked))
					.ButtonColorAndOpacity(FLinearColor(0.2f, 0.6f, 0.2f))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0, 0, 8, 0)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("❌ Reject")))
					.OnClicked(FOnClicked::CreateSP(this, &SAgentZetPlanPreview::OnRejectClicked))
					.ButtonColorAndOpacity(FLinearColor(0.6f, 0.2f, 0.2f))
				]
			]
		]
	];
}

void SAgentZetPlanPreview::ShowPlan(const FAgentZetActionPlan& Plan)
{
	CurrentPlan = Plan;
	bPlanVisible = true;
	SetVisibility(EVisibility::Visible);

	// Clear previous actions
	if (ActionList.IsValid())
	{
		ActionList->ClearChildren();
	}

	// Populate the action list with tool call descriptions
	for (const FAgentZetAction& Action : Plan.Actions)
	{
		FString RiskBadge;
		FLinearColor RiskColor;
		switch (Action.RiskLevel)
		{
		case EAgentZetRiskLevel::Low:
			RiskBadge = TEXT("🟢 Low");
			RiskColor = FLinearColor(0.3f, 0.8f, 0.3f);
			break;
		case EAgentZetRiskLevel::Medium:
			RiskBadge = TEXT("🟡 Medium");
			RiskColor = FLinearColor(0.8f, 0.8f, 0.2f);
			break;
		case EAgentZetRiskLevel::High:
			RiskBadge = TEXT("🟠 High");
			RiskColor = FLinearColor(0.9f, 0.5f, 0.1f);
			break;
		case EAgentZetRiskLevel::Critical:
			RiskBadge = TEXT("🔴 Critical");
			RiskColor = FLinearColor(0.9f, 0.2f, 0.2f);
			break;
		}

		ActionList->AddSlot()
			.AutoHeight()
			.Padding(4, 2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0, 0, 8, 0)
				[
					SNew(STextBlock)
					.Text(FText::FromString(RiskBadge))
					.ColorAndOpacity(FSlateColor(RiskColor))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Action.Description))
					.AutoWrapText(true)
				]
			];
	}
}

void SAgentZetPlanPreview::ShowToolCalls(const TArray<FAgentZetToolCall>& ToolCalls)
{
	// Build a quick action plan from tool calls for display
	FAgentZetActionPlan Plan;
	Plan.Summary = FString::Printf(TEXT("%d tool(s) pending execution"), ToolCalls.Num());

	for (const FAgentZetToolCall& TC : ToolCalls)
	{
		FAgentZetAction Action;
		Action.Description = FString::Printf(TEXT("🔧 %s"), *TC.ToolName);
		Action.Category = TC.Category;

		// Assign risk based on category
		switch (TC.Category)
		{
		case EAgentZetActionCategory::Cpp:
		case EAgentZetActionCategory::Build:
		case EAgentZetActionCategory::Settings:
			Action.RiskLevel = EAgentZetRiskLevel::High;
			break;
		case EAgentZetActionCategory::SourceControl:
		case EAgentZetActionCategory::FileSystem:
			Action.RiskLevel = EAgentZetRiskLevel::Medium;
			break;
		default:
			Action.RiskLevel = EAgentZetRiskLevel::Low;
			break;
		}

		Plan.Actions.Add(Action);
	}

	ShowPlan(Plan);
}

void SAgentZetPlanPreview::HidePlan()
{
	bPlanVisible = false;
	SetVisibility(EVisibility::Collapsed);
	if (ActionList.IsValid()) ActionList->ClearChildren();
}

FReply SAgentZetPlanPreview::OnApproveClicked()
{
	OnPlanApproved.ExecuteIfBound(CurrentPlan);
	HidePlan();
	return FReply::Handled();
}

FReply SAgentZetPlanPreview::OnRejectClicked()
{
	OnPlanRejected.ExecuteIfBound(CurrentPlan);
	HidePlan();
	return FReply::Handled();
}
