// Copyright AgentZet. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

/**
 * Editor commands for the AgentZet plugin.
 * Registers keyboard shortcuts and menu commands.
 */
class AGENTZETUI_API FAgentZetCommands : public TCommands<FAgentZetCommands>
{
public:
	FAgentZetCommands();

	virtual void RegisterCommands() override;

	/** Command to open the AgentZet chat panel */
	TSharedPtr<FUICommandInfo> OpenAgentZetPanel;

	/** Command to send the current prompt */
	TSharedPtr<FUICommandInfo> SendPrompt;

	/** Command to cancel the current AI request */
	TSharedPtr<FUICommandInfo> CancelRequest;

	/** Command to clear the conversation */
	TSharedPtr<FUICommandInfo> ClearConversation;
};
