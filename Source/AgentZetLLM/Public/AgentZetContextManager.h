// Copyright AgentZet. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentZetTypes.h"
#include "AgentZetContextCondenser.h"

class IAgentZetLLMClient;
class FAgentZetConversationManager;

/**
 * Result of a context management operation.
 */
struct FAgentZetContextManagementResult
{
	/** True if any context management action was taken */
	bool bDidManage = false;

	/** True if condensation was performed */
	bool bDidCondense = false;

	/** True if truncation was performed */
	bool bDidTruncate = false;

	/** Token count before management */
	int32 PrevContextTokens = 0;

	/** Token count after management (0 if no action taken) */
	int32 NewContextTokens = 0;

	/** Context usage percentage before management (0-100) */
	float ContextPercent = 0.0f;

	/** Error message if management failed */
	FString ErrorMessage;

	/** Number of messages removed by truncation */
	int32 MessagesRemoved = 0;
};

/**
 * Orchestrates context management: condensation and truncation fallback.
 *
 * Ported from Roo Code's manageContext() in context-management/index.ts.
 *
 * Called after each successful API response. Logic:
 * 1. Compute current context usage (estimated tokens / context window)
 * 2. If autoCondense AND usage >= threshold → attempt condensation
 * 3. If condensation fails OR tokens > allowed → sliding window truncation fallback
 *
 * Usage in MainPanel:
 *   ContextManager->ManageContext(SystemPrompt, [callback]);
 */
class AGENTZETLLM_API FAgentZetContextManager
{
public:
	FAgentZetContextManager(
		TSharedPtr<IAgentZetLLMClient> InLLMClient,
		TSharedPtr<FAgentZetConversationManager> InConversationManager);

	~FAgentZetContextManager();

	/**
	 * Checks and manages context after an API response.
	 *
	 * @param SystemPrompt    Current system prompt (used for token estimation)
	 * @param LastTokenUsage  Token usage reported by the last API response
	 * @param OnComplete      Callback fired on game thread with the result
	 */
	void ManageContext(
		const FString& SystemPrompt,
		const FAgentZetTokenUsage& LastTokenUsage,
		TFunction<void(const FAgentZetContextManagementResult&)> OnComplete);

	/** Update the LLM client (called when settings change) */
	void SetLLMClient(TSharedPtr<IAgentZetLLMClient> InLLMClient);

	/** True if context management is currently in progress */
	bool IsManaging() const { return bIsManaging; }

	/** Token buffer percentage (10% of context window reserved for response) */
	static constexpr float TokenBufferPercent = 0.1f;

	/** Default fraction to remove in truncation (50%) */
	static constexpr float TruncationFrac = 0.5f;

private:
	/** Non-destructive sliding window truncation */
	void TruncateConversation(float FracToRemove, FAgentZetContextManagementResult& OutResult);

	TSharedPtr<IAgentZetLLMClient> LLMClient;
	TSharedPtr<FAgentZetConversationManager> ConversationManager;
	TSharedPtr<FAgentZetContextCondenser> Condenser;

	bool bIsManaging = false;
};
