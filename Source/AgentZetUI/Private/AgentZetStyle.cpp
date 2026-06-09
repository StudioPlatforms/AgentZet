// Copyright AgentZet. All Rights Reserved.

#include "AgentZetStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateTypes.h"
#include "Misc/Paths.h"

TSharedPtr<FSlateStyleSet> FAgentZetStyle::StyleInstance = nullptr;

void FAgentZetStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FAgentZetStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

void FAgentZetStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FAgentZetStyle::Get()
{
	return *StyleInstance;
}

FName FAgentZetStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("AgentZetStyle"));
	return StyleSetName;
}

const FSlateBrush* FAgentZetStyle::GetPluginIcon()
{
	return StyleInstance->GetBrush(TEXT("AgentZet.PluginIcon"));
}

TSharedRef<FSlateStyleSet> FAgentZetStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style = MakeShareable(new FSlateStyleSet(GetStyleSetName()));
	Style->SetContentRoot(FPaths::ProjectPluginsDir() / TEXT("AgentZet/Content/Icons"));

	// Placeholder brush — icons will be added later
	Style->Set("AgentZet.PluginIcon", new FSlateNoResource());

	return Style;
}
