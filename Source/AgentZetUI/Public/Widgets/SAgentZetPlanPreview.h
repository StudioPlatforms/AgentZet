// Copyright AgentZet. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "AgentZetTypes.h"

DECLARE_DELEGATE_OneParam(FOnAgentZetPlanApproved, const FAgentZetActionPlan&);
DECLARE_DELEGATE_OneParam(FOnAgentZetPlanRejected, const FAgentZetActionPlan&);

/**
 * Plan preview widget showing proposed actions with risk badges and approve/reject buttons.
 *
 * HIDDEN by default (Collapsed). Only becomes visible when:
 * - Tool calls are received and auto-approve is OFF
 * - The user needs to review and approve/reject the pending actions
 *
 * After approval or rejection, hides itself again.
 */
class AGENTZETUI_API SAgentZetPlanPreview : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAgentZetPlanPreview) {}
		SLATE_EVENT(FOnAgentZetPlanApproved, OnPlanApproved)
		SLATE_EVENT(FOnAgentZetPlanRejected, OnPlanRejected)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Display a new action plan for review */
	void ShowPlan(const FAgentZetActionPlan& Plan);

	/** Display pending tool calls for review (auto-generates an action plan) */
	void ShowToolCalls(const TArray<FAgentZetToolCall>& ToolCalls);

	/** Hide the plan preview */
	void HidePlan();

	/** Whether a plan is currently being shown */
	bool IsPlanVisible() const { return bPlanVisible; }

private:
	FReply OnApproveClicked();
	FReply OnRejectClicked();

	FAgentZetActionPlan CurrentPlan;
	bool bPlanVisible = false;
	FOnAgentZetPlanApproved OnPlanApproved;
	FOnAgentZetPlanRejected OnPlanRejected;
	TSharedPtr<SVerticalBox> ActionList;
};
