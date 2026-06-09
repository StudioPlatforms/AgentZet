// Copyright AgentZet. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentZetTypes.h"

/**
 * Static model info database for all supported providers.
 * Modeled after Roo Code's packages/types/src/providers/*.ts model records.
 *
 * FAgentZetModelRegistry::GetModelInfo() returns capability data (context window,
 * max tokens, thinking support, pricing) for any known model ID.
 * Unknown models get safe defaults.
 */
class AGENTZETLLM_API FAgentZetModelRegistry
{
public:
	/**
	 * Look up model info for a given provider + model ID.
	 * Returns reasonable defaults if the model is not found in the registry.
	 */
	static FAgentZetModelInfo GetModelInfo(EAgentZetProvider Provider, const FString& ModelId);

	/**
	 * Get all known model IDs for a given provider.
	 */
	static TArray<FString> GetKnownModelIds(EAgentZetProvider Provider);

	/**
	 * Get all known models for a provider as FAgentZetModelInfo structs.
	 */
	static TArray<FAgentZetModelInfo> GetKnownModels(EAgentZetProvider Provider);

	/**
	 * Returns true if the model is known to support extended thinking (Anthropic budget_tokens
	 * or OpenAI reasoning effort or Gemini thinkingBudget).
	 */
	static bool ModelSupportsThinking(EAgentZetProvider Provider, const FString& ModelId);

	/**
	 * Returns true if the model is known to support Anthropic's 1M context beta.
	 */
	static bool ModelSupports1MContext(EAgentZetProvider Provider, const FString& ModelId);

private:
	// ----------------------------------------------------------------
	// Anthropic (Claude) models
	// ----------------------------------------------------------------
	static TArray<FAgentZetModelInfo> GetAnthropicModels();

	// ----------------------------------------------------------------
	// OpenAI models
	// ----------------------------------------------------------------
	static TArray<FAgentZetModelInfo> GetOpenAIModels();

	// ----------------------------------------------------------------
	// Google Gemini models
	// ----------------------------------------------------------------
	static TArray<FAgentZetModelInfo> GetGeminiModels();

	// ----------------------------------------------------------------
	// DeepSeek models
	// ----------------------------------------------------------------
	static TArray<FAgentZetModelInfo> GetDeepSeekModels();

	// ----------------------------------------------------------------
	// Mistral models
	// ----------------------------------------------------------------
	static TArray<FAgentZetModelInfo> GetMistralModels();

	// ----------------------------------------------------------------
	// xAI (Grok) models
	// ----------------------------------------------------------------
	static TArray<FAgentZetModelInfo> GetxAIModels();
};
