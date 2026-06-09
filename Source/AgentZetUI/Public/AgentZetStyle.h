// Copyright AgentZet. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

/**
 * Manages Slate style definitions for the AgentZet UI.
 * Handles icons, brushes, text styles, and color palette.
 */
class AGENTZETUI_API FAgentZetStyle
{
public:
	static void Initialize();
	static void Shutdown();
	static void ReloadTextures();
	static const ISlateStyle& Get();
	static FName GetStyleSetName();

	/** Get the plugin icon brush */
	static const FSlateBrush* GetPluginIcon();

private:
	static TSharedRef<FSlateStyleSet> Create();
	static TSharedPtr<FSlateStyleSet> StyleInstance;
};
