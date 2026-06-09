// Copyright AgentZet. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentZetTypes.h"

class IAgentZetLLMClient;
class FAgentZetConversationManager;

/**
 * Result of a conversation condensation operation.
 */
struct FAgentZetCondenseResult
{
	/** True if condensation succeeded */
	bool bSuccess = false;

	/** The summary text that was generated */
	FString Summary;

	/** Error message if condensation failed */
	FString ErrorMessage;

	/** Unique ID of the summary message created (referenced by CondenseParent on old messages) */
	FString CondenseId;

	/** Estimated token count after condensation */
	int32 NewContextTokens = 0;
};

/**
 * LLM-based conversation summarization (condenser).
 *
 * Ported from Roo Code's summarizeConversation() in condense/index.ts.
 *
 * Sends the conversation history to Claude with a SUMMARY_PROMPT requesting
 * a condensed summary of the conversation. The summary is created as a user
 * message (fresh start model) and all old messages are tagged with CondenseParent.
 *
 * Usage:
 *   FAgentZetContextCondenser Condenser(ClaudeClient, ConversationManager);
 *   Condenser.SummarizeConversation(SystemPrompt, [callback]);
 */
class AGENTZETLLM_API FAgentZetContextCondenser
{
public:
	FAgentZetContextCondenser(
		TSharedPtr<IAgentZetLLMClient> InLLMClient,
		TSharedPtr<FAgentZetConversationManager> InConversationManager);

	~FAgentZetContextCondenser();

	/**
	 * Asynchronously summarizes the conversation.
	 *
	 * Sends the full history (with tool blocks converted to text) to Claude
	 * with the SUMMARY_PROMPT. On success:
	 *   - Inserts a summary message (bIsSummary=true) into history
	 *   - Tags all old messages with CondenseParent = summary.CondenseId
	 *   - Injects synthetic tool_results for any orphaned tool_uses
	 *
	 * @param SystemPrompt    The current system prompt (used for context)
	 * @param OnComplete      Callback fired on game thread with the result
	 * @param FoldedCodeContext Optional folded file context (signatures only) to inject
	 *                          into the summary as <file-context> blocks.
	 *                          Ported from Roo Code's foldedFileContext.ts:
	 *                          these blocks persist across condensation, preventing
	 *                          Claude from losing awareness of code structure.
	 */
	void SummarizeConversation(
		const FString& SystemPrompt,
		TFunction<void(const FAgentZetCondenseResult&)> OnComplete,
		const FString& FoldedCodeContext = FString());

	/** True if a condensation request is currently in flight */
	bool IsCondensing() const { return bIsCondensing; }

	/** Update the LLM client (called when settings change) */
	void SetLLMClient(TSharedPtr<IAgentZetLLMClient> InLLMClient) { LLMClient = InLLMClient; }

	/** The summary prompt text (CRITICAL: no tool calls, text only) */
	static const FString SummaryPrompt;

	/** The condense instructions sent as the final user message */
	static const FString CondenseInstructions;

private:
	/** Convert all tool_use/tool_result blocks in history to text for summarization */
	static TArray<FAgentZetMessage> ConvertToolBlocksToText(const TArray<FAgentZetMessage>& Messages);

	/** Inject synthetic tool_results for orphaned tool_calls */
	void InjectSyntheticToolResults(TArray<FAgentZetMessage>& Messages);

	/** Build the JSON messages array for the summarization request */
	TArray<TSharedPtr<FJsonValue>> BuildSummarizationMessages(const TArray<FAgentZetMessage>& History);

	/** Apply condensation result to the conversation manager */
	void ApplyCondensation(const FString& Summary, const FAgentZetCondenseResult& Result);

	TSharedPtr<IAgentZetLLMClient> LLMClient;
	TSharedPtr<FAgentZetConversationManager> ConversationManager;

	bool bIsCondensing = false;

	/** Delegate handles for the condensation Claude request */
	FDelegateHandle StreamingHandle;
	FDelegateHandle MessageCompleteHandle;
	FDelegateHandle RequestCompletedHandle;

	/** Accumulated summary text during streaming */
	FString AccumulatedSummary;

	/** Stored callback for async completion */
	TFunction<void(const FAgentZetCondenseResult&)> PendingCallback;
};
