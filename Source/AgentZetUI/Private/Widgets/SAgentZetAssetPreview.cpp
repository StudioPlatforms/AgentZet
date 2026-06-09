// Copyright AgentZet. All Rights Reserved.

#include "Widgets/SAgentZetAssetPreview.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"

void SAgentZetAssetPreview::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SAssignNew(ThumbnailImage, SImage)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(4)
		[
			SAssignNew(AssetInfoText, STextBlock).AutoWrapText(true)
		]
	];
}

void SAgentZetAssetPreview::ShowAssetPreview(const FString& AssetPath)
{
	if (AssetInfoText.IsValid())
	{
		AssetInfoText->SetText(FText::FromString(AssetPath));
	}
	// Stub: load thumbnail from asset registry
}

void SAgentZetAssetPreview::ClearPreview()
{
	if (AssetInfoText.IsValid()) AssetInfoText->SetText(FText::GetEmpty());
}
