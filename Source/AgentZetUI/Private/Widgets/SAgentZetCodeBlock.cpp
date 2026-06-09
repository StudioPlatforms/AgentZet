// Copyright AgentZet. All Rights Reserved.

#include "Widgets/SAgentZetCodeBlock.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "HAL/PlatformApplicationMisc.h"

void SAgentZetCodeBlock::Construct(const FArguments& InArgs)
{
	CodeContent = InArgs._Code;
	LanguageHint = InArgs._Language;

	ChildSlot
	[
		SNew(SBorder)
		.BorderBackgroundColor(FLinearColor(0.05f, 0.05f, 0.08f))
		.Padding(8.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SNew(STextBlock).Text(FText::FromString(LanguageHint))
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("Copy")))
					.OnClicked(FOnClicked::CreateSP(this, &SAgentZetCodeBlock::OnCopyClicked))
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
			[
				SNew(STextBlock)
				.Text(FText::FromString(CodeContent))
				.AutoWrapText(false)
			]
		]
	];
}

FReply SAgentZetCodeBlock::OnCopyClicked()
{
	FPlatformApplicationMisc::ClipboardCopy(*CodeContent);
	return FReply::Handled();
}
