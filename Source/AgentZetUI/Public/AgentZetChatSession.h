// Copyright AgentZet. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "AgentZetTypes.h"
#include "AgentZetAutoApprovalHandler.h"

// Forward declarations
class IAgentZetLLMClient;
class FAgentZetActionRouter;
class FAgentZetToolSchemaRegistry;
class FAgentZetConversationManager;
class FAgentZetExecutionJournal;
class FAgentZetToolRepetitionDetector;
class FAgentZetFileContextTracker;
class FAgentZetContextManager;
class FAgentZetCheckpointManager;
struct FAgentZetToolCall;
struct FAgentZetMessage;
struct FAgentZetTokenUsage;
struct FAgentZetActionPlan;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnToolRequiresApproval, const FAgentZetActionPlan& /*Plan*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAgentFinished, const FString& /*Reason*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTokenUsageUpdated, const FAgentZetTokenUsage& /*Usage*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMessageAdded, const FAgentZetMessage& /*Message*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnMessageUpdated, const FGuid& /*MessageId*/, const FString& /*DeltaText*/, EAgentZetMessageRole /*Role*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnStatusUpdated, const FString& /*StatusText*/); // For progress overlay
DECLARE_MULTICAST_DELEGATE(FOnSessionCompletedContextManagement); // Trigger to resume after context condense

/**
 * FAgentZetChatSession
 * Manages the lifecycle of an AI conversation, handling the agentic loop, 
 * LLM streaming callbacks, tool execution queuing, and token usage tracking.
 */
class AGENTZETUI_API FAgentZetChatSession : public TSharedFromThis<FAgentZetChatSession>
{
public:
    FAgentZetChatSession();
    virtual ~FAgentZetChatSession();

    void Initialize(TSharedPtr<IAgentZetLLMClient> InLLMClient,
                    TSharedPtr<FAgentZetConversationManager> InConvManager,
                    TSharedPtr<FAgentZetActionRouter> InActionRouter,
                    TSharedPtr<FAgentZetExecutionJournal> InExecutionJournal,
                    TSharedPtr<FAgentZetToolRepetitionDetector> InToolRepetitionDetector,
                    TSharedPtr<FAgentZetFileContextTracker> InFileContextTracker,
                    TSharedPtr<FAgentZetContextManager> InContextManager,
                    TSharedPtr<FAgentZetToolSchemaRegistry> InToolSchemaRegistry,
                    TSharedPtr<FAgentZetCheckpointManager> InCheckpointManager);

    // Agentic Loop execution methods
    void ProcessToolCallQueue();
    void ContinueAgenticLoop();
    FString ExecuteToolCall(const FAgentZetToolCall& ToolCall, bool& bOutIsError);

    // LLM Callbacks
    void OnStreamingText(const FGuid& MessageId, const FString& DeltaText);
    void OnToolCallReceived(const FAgentZetToolCall& ToolCall);
    void OnMessageComplete(const FAgentZetMessage& Message);
    void OnRequestStarted();
    void OnRequestCompleted(bool bSuccess);
    void OnRequestCompletedPostContextManagement();

    // Approval Flow
    void OnToolCallsApproved(const FAgentZetActionPlan& Plan);
    void OnToolCallsRejected(const FAgentZetActionPlan& Plan);

    // Callbacks to request environment strings or actions from UI
    DECLARE_DELEGATE_RetVal(FString, FGetEnvironmentDetailsString);
    FGetEnvironmentDetailsString OnGetEnvironmentDetailsString;

    DECLARE_DELEGATE_RetVal(FString, FGetSystemPromptString);
    FGetSystemPromptString OnGetSystemPromptString;

    DECLARE_DELEGATE(FOnSaveTabsToDisk);
    FOnSaveTabsToDisk OnSaveTabsToDisk;

    // Meta-tool handlers
    DECLARE_DELEGATE_RetVal_OneParam(FString, FOnHandleUpdateTodoList, const FAgentZetToolCall&);
    FOnHandleUpdateTodoList OnHandleUpdateTodoList;

    DECLARE_DELEGATE_RetVal_OneParam(FString, FOnHandleAttemptCompletion, const FAgentZetToolCall&);
    FOnHandleAttemptCompletion OnHandleAttemptCompletion;

    DECLARE_DELEGATE_RetVal_OneParam(FString, FOnHandleSwitchMode, const FAgentZetToolCall&);
    FOnHandleSwitchMode OnHandleSwitchMode;

    // Delegates for UI state tracking
    FOnAgentFinished& GetOnAgentFinished() { return OnAgentFinished; }
    FOnToolRequiresApproval& GetOnToolRequiresApproval() { return OnToolRequiresApproval; }
    FOnTokenUsageUpdated& GetOnTokenUsageUpdated() { return OnTokenUsageUpdated; }
    FOnMessageAdded& GetOnMessageAdded() { return OnMessageAdded; }
    FOnMessageUpdated& GetOnMessageUpdated() { return OnMessageUpdated; }
    FOnStatusUpdated& GetOnStatusUpdated() { return OnStatusUpdated; }
    FOnSessionCompletedContextManagement& GetOnSessionCompletedContextManagement() { return OnSessionCompletedContextManagement; }

    DECLARE_MULTICAST_DELEGATE(FOnRequestStartedDelegate);
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnRequestCompletedDelegate, bool);

    FOnRequestStartedDelegate& GetOnRequestStarted() { return OnRequestStartedDelegate; }
    FOnRequestCompletedDelegate& GetOnRequestCompleted() { return OnRequestCompletedDelegate; }

    /** State-change delegate — UI binds to this for Stop/Send swap, input enable/disable, etc. */
    FOnConversationStateChanged& GetOnConversationStateChanged() { return OnConversationStateChanged; }

    // Getters
    bool IsInAgenticLoop() const { return bInAgenticLoop; }
    int32 GetAgenticLoopCount() const { return AgenticLoopCount; }
    float GetLastRequestCost() const { return LastRequestCost; }
    FAgentZetTokenUsage GetLastResponseTokenUsage() const { return LastResponseTokenUsage; }

    /** Get the current conversation state (single source of truth) */
    EConversationState GetConversationState() const { return CurrentState; }

    /** Backward-compat: returns true when Streaming or Cancelling */
    bool IsProcessing() const { return CurrentState == EConversationState::Streaming || CurrentState == EConversationState::Cancelling; }

    /** Transition to a new conversation state. Broadcasts OnConversationStateChanged. */
    void SetState(EConversationState NewState);

    // Setters
    void SetAgentMode(EAgentZetAgentMode NewMode) { CurrentAgentMode = NewMode; }

    /** Stop the agentic loop immediately — clears tool queue, resets flags.
     *  Called when the user clicks the Stop button. */
    void StopAgenticLoop();

    /**
     * Resume an interrupted task.
     *
     * Called when the user clicks "Continue Task" on a restored conversation.
     * This method:
     * 1. Injects synthetic tool_result messages for any orphaned tool_use blocks
     * 2. Injects a time-aware resumption prompt as a user message
     * 3. Restarts the agentic loop (sends the conversation to the LLM)
     *
     * The resumption prompt follows the "Discovery Hypothesis Pattern" — it forces
     * the AI to replan rather than blindly resume from the exact point of interruption.
     *
     * @param InterruptedAt  When the task was interrupted (for time-aware prompt)
     */
    void ResumeTask(const FDateTime& InterruptedAt);

    // ---- DynamicallyLoadedTools persistence (v4.0) ----

    /** Get the set of tools discovered via get_tool_info during this session */
    const TSet<FString>& GetDynamicallyLoadedTools() const { return DynamicallyLoadedTools; }

    /** Restore dynamically loaded tools from persisted state (called during tab load) */
    void SetDynamicallyLoadedTools(const TSet<FString>& InTools) { DynamicallyLoadedTools = InTools; }

    /** Update the LLM client (called when settings change) */
    void SetLLMClient(TSharedPtr<IAgentZetLLMClient> InLLMClient) { LLMClient = InLLMClient; }

private:
    // Dependencies
    TSharedPtr<IAgentZetLLMClient> LLMClient;
    TSharedPtr<FAgentZetConversationManager> ConversationManager;
    TSharedPtr<FAgentZetActionRouter> ActionRouter;
    TSharedPtr<FAgentZetExecutionJournal> ExecutionJournal;
    TSharedPtr<FAgentZetToolRepetitionDetector> ToolRepetitionDetector;
    TSharedPtr<FAgentZetFileContextTracker> FileContextTracker;
    TSharedPtr<FAgentZetToolSchemaRegistry> ToolSchemaRegistry;
    TSharedPtr<FAgentZetContextManager> ContextManager;
    TSharedPtr<FAgentZetCheckpointManager> CheckpointManager;

    // Conversation State (single source of truth)
    EConversationState CurrentState = EConversationState::Idle;

    // Loop State
    TArray<FAgentZetToolCall> ToolCallQueue;
    bool bInAgenticLoop = false;
    bool bStopRequested = false;  // Set by StopAgenticLoop(), checked during tool execution
    int32 AgenticLoopCount = 0;
    int32 ConsecutiveNoToolCount = 0;
    static const int32 MaxConsecutiveNoToolResponses = 3;
    float LastRequestCost = 0.0f;
    EAgentZetAgentMode CurrentAgentMode = EAgentZetAgentMode::General;

    /** Tools discovered via get_tool_info during the current session.
     *  These are injected into the tools array on subsequent API calls
     *  so that strict-mode providers (OpenAI Responses API) can actually
     *  call them. Fixes the "discovery loop" where the model repeatedly
     *  calls get_tool_info but can never invoke the discovered tool.
     *  Cleared on new conversation / tab switch. */
    TSet<FString> DynamicallyLoadedTools;
    FGuid CurrentStreamingMessageId;
    FAgentZetTokenUsage LastResponseTokenUsage;

    // Auto Approval
    FAgentZetAutoApprovalHandler AutoApprovalHandler;
    void HandleAutoApprovalLimitReached(const FAgentZetAutoApprovalCheck& Check);

    // Delegates
    FOnAgentFinished OnAgentFinished;
    FOnToolRequiresApproval OnToolRequiresApproval;
    FOnTokenUsageUpdated OnTokenUsageUpdated;
    FOnMessageAdded OnMessageAdded;
    FOnMessageUpdated OnMessageUpdated;
    FOnStatusUpdated OnStatusUpdated;
    FOnSessionCompletedContextManagement OnSessionCompletedContextManagement;
    FOnRequestStartedDelegate OnRequestStartedDelegate;
    FOnRequestCompletedDelegate OnRequestCompletedDelegate;
    FOnConversationStateChanged OnConversationStateChanged;
};
