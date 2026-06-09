// Copyright AgentZet. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentZetInterfaces.h"
#include "AgentZetTypes.h"

/**
 * Factory for creating the correct IAgentZetLLMClient implementation
 * based on the currently selected provider in UAgentZetDeveloperSettings.
 *
 * Usage:
 *   TSharedPtr<IAgentZetLLMClient> Client = FAgentZetLLMClientFactory::CreateClient();
 *   // Wires up all settings from UAgentZetDeveloperSettings automatically.
 *
 * To force a specific provider (e.g. for testing):
 *   TSharedPtr<IAgentZetLLMClient> Client = FAgentZetLLMClientFactory::CreateClientForProvider(
 *       EAgentZetProvider::OpenAI, Settings);
 */
class AGENTZETLLM_API FAgentZetLLMClientFactory
{
public:
	/**
	 * Create the client for the currently configured active provider.
	 * Reads all configuration from UAgentZetDeveloperSettings::Get().
	 * Returns nullptr if the provider is not configured (missing API key, etc.).
	 */
	static TSharedPtr<IAgentZetLLMClient> CreateClient();

	/**
	 * Create a client for a specific provider using the given settings object.
	 */
	static TSharedPtr<IAgentZetLLMClient> CreateClientForProvider(
		EAgentZetProvider Provider,
		const class UAgentZetDeveloperSettings* Settings
	);

	/**
	 * Get a human-readable display name for the current provider + model combination.
	 * Used for display in the chat panel header.
	 */
	static FString GetActiveProviderDisplayName();
};
