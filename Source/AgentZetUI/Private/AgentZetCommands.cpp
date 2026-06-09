// Copyright AgentZet. All Rights Reserved.

#include "AgentZetCommands.h"

#define LOCTEXT_NAMESPACE "FAgentZetCommands"

FAgentZetCommands::FAgentZetCommands()
	: TCommands<FAgentZetCommands>(
		TEXT("AgentZet"),
		NSLOCTEXT("Contexts", "AgentZet", "AgentZet Plugin"),
		NAME_None,
		TEXT("AgentZetStyle"))
{
}

void FAgentZetCommands::RegisterCommands()
{
	UI_COMMAND(OpenAgentZetPanel, "AgentZet", "Open the AgentZet AI assistant panel", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::A));
	UI_COMMAND(SendPrompt, "Send", "Send the current prompt to the AI", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(CancelRequest, "Cancel", "Cancel the current AI request", EUserInterfaceActionType::Button, FInputChord(EKeys::Escape));
	UI_COMMAND(ClearConversation, "Clear", "Clear the conversation history", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
