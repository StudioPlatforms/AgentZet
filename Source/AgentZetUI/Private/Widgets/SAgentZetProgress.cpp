// Copyright AgentZet. All Rights Reserved.

#include "Widgets/SAgentZetProgress.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Notifications/SProgressBar.h"

void SAgentZetProgress::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(4)
		[
			SAssignNew(MessageText, STextBlock)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(4)
		[
			SAssignNew(ProgressBar, SProgressBar)
		]
	];
	SetVisibility(EVisibility::Collapsed);
}

void SAgentZetProgress::ShowProgress(const FString& Message, float Progress)
{
	SetVisibility(EVisibility::Visible);
	bVisible = true;
	UpdateMessage(Message);
	UpdateProgress(Progress);
}

void SAgentZetProgress::UpdateProgress(float Progress)
{
	if (ProgressBar.IsValid())
	{
		ProgressBar->SetPercent(Progress >= 0.0f ? Progress : TOptional<float>());
	}
}

void SAgentZetProgress::UpdateMessage(const FString& Message)
{
	if (MessageText.IsValid())
	{
		MessageText->SetText(FText::FromString(Message));
	}
}

void SAgentZetProgress::HideProgress()
{
	SetVisibility(EVisibility::Collapsed);
	bVisible = false;
}
