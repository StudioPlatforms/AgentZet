// Copyright AgentZet. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

/**
 * Code block widget with syntax highlighting hints and copy-to-clipboard button.
 */
class AGENTZETUI_API SAgentZetCodeBlock : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAgentZetCodeBlock) {}
		SLATE_ARGUMENT(FString, Code)
		SLATE_ARGUMENT(FString, Language)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FReply OnCopyClicked();

	FString CodeContent;
	FString LanguageHint;
};
