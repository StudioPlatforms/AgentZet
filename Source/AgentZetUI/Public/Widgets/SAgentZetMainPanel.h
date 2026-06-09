// Copyright AgentZet. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "AgentZetTypes.h"
#include "AgentZetCostTracker.h"
#include "AgentZetAutoApprovalHandler.h"

class SAgentZetChatView;
class SAgentZetInputArea;
class SAgentZetPlanPreview;
class SAgentZetProgress;
class SAgentZetTodoList;
class SButton;
class SHorizontalBox;
class IAgentZetLLMClient;
class FAgentZetClaudeClient;  // Still needed for OnContextWindowExceeded downcast
class FAgentZetConversationManager;
class FAgentZetToolSchemaRegistry;
class FAgentZetActionRouter;
class FAgentZetExecutionJournal;
class FAgentZetEditorContextCapture;
class FAgentZetContextGatherer;
class FAgentZetContextManager;
class FAgentZetBackupManager;

// Phase 1 additions
class FAgentZetToolRepetitionDetector;
class FAgentZetIgnoreController;
class FAgentZetFileContextTracker;

// Phase 2 additions
class FAgentZetEnvironmentDetails;
class FAgentZetDiffApplicator;

// Phase 3 additions
class FAgentZetCheckpointManager;
class FAgentZetReferenceParser;
class FAgentZetTaskDelegation;

// Phase 4 additions
class FAgentZetTaskHistory;
class FAgentZetSlashCommandRegistry;
class FAgentZetSkillsManager;
class FAgentZetMCPClient;

// Second-pass additions (tree-sitter equivalent)
class FAgentZetCodeStructureParser;

// New UI widgets (from Roo-Code comparison)
class SAgentZetContextBar;
class SAgentZetCheckpointPanel;
class SAgentZetHistoryPanel;
class SAgentZetFollowUpBar;
class SAgentZetFileChangesPanel;

/**
 * Root widget for the AgentZet editor panel.
 *
 * ORCHESTRATES THE FULL AGENTIC LOOP:
 * 1. User types prompt
 * 2. Privacy disclosure check (first use only)
 * 3. Build system prompt with project context + editor context
 * 4. Send to Claude API with tool schemas
 * 5. Stream response tokens to chat view in real-time
 * 6. On tool_use: compute pre-hash -> execute tool -> compute post-hash -> record in journal -> add tool_result
 * 7. Re-send conversation to Claude (loop back to step 4)
 * 8. Repeat until Claude responds with only text (no tool_use)
 *
 * v3.0: Added context management (auto-condense + truncation) via FAgentZetContextManager.
 * Context usage percentage is displayed in the header.
 */
class AGENTZETUI_API SAgentZetMainPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SAgentZetMainPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual ~SAgentZetMainPanel();

private:
    /** Per-tab state for multi-conversation sessions.
     *  v4.0: Added TaskStatus, CreatedAt, DynamicallyLoadedTools for per-task persistence.
     *  v4.1: Added LastActivityAt for time-aware task resumption. */
    struct FAgentZetConversationTabState
    {
        FString TabId;
        FString Title;
        TSharedPtr<FAgentZetConversationManager> ConversationManager;
        TSharedPtr<FAgentZetContextManager> ContextManager;
        FAgentZetTokenUsage SessionTokenUsage;
        FAgentZetTokenUsage LastResponseTokenUsage;
        float ContextUsagePercent = 0.0f;
        float LastRequestCost = 0.0f;
        FAgentZetCostTracker CostTracker;
        TSharedPtr<class FAgentZetChatSession> ChatSession;
        TArray<FAgentZetTodoItem> Todos;

        // Phase 2: per-tab agent mode
        EAgentZetAgentMode AgentMode = EAgentZetAgentMode::General;

        // v4.0: Per-task persistence fields
        EAgentZetTaskStatus TaskStatus = EAgentZetTaskStatus::Active;
        FDateTime CreatedAt = FDateTime::UtcNow();
        FDateTime LastActivityAt = FDateTime::UtcNow();
        TSet<FString> DynamicallyLoadedTools;
    };

    // ---- Sub-widgets ----
    TSharedPtr<SHorizontalBox> TabButtonContainer;
    TSharedPtr<SAgentZetChatView> ChatView;
    TSharedPtr<SAgentZetInputArea> InputArea;
    TSharedPtr<SAgentZetPlanPreview> PlanPreview;
    TSharedPtr<SAgentZetProgress> ProgressOverlay;
    TSharedPtr<SAgentZetTodoList> TodoListWidget;
    TSharedPtr<SButton> CondenseButton;

    // ---- Backend components ----
    // LLMClient holds the active provider (Anthropic/OpenAI/Gemini/etc.) created by FAgentZetLLMClientFactory.
    // Use ClaudeClientPtr() helper when Claude-specific APIs (OnContextWindowExceeded) are needed.
    TSharedPtr<IAgentZetLLMClient> LLMClient;
    TSharedPtr<FAgentZetConversationManager> ConversationManager;
    TSharedPtr<FAgentZetToolSchemaRegistry> ToolSchemaRegistry;
    TSharedPtr<FAgentZetActionRouter> ActionRouter;
    TSharedPtr<FAgentZetExecutionJournal> ExecutionJournal;
    TSharedPtr<FAgentZetEditorContextCapture> EditorContextCapture;
    TSharedPtr<FAgentZetContextGatherer> ContextGatherer;
    TSharedPtr<FAgentZetContextManager> ContextManager;
    TSharedPtr<FAgentZetBackupManager> BackupManager;

    // ---- Phase 1: Safety & Reliability ----

    /** Detects consecutive identical tool calls to prevent infinite loops */
    TSharedPtr<FAgentZetToolRepetitionDetector> ToolRepetitionDetector;

    /** Manages .AgentZetignore file — blocks AI from accessing excluded paths */
    TSharedPtr<FAgentZetIgnoreController> IgnoreController;

    /** Tracks files the AI has read; detects when user edits them externally */
    TSharedPtr<FAgentZetFileContextTracker> FileContextTracker;

    // ---- Phase 2: Developer Productivity ----

    /** Builds fresh per-message environment details (open files, errors, selection, etc.) */
    TSharedPtr<FAgentZetEnvironmentDetails> EnvironmentDetails;

    /** Applies fuzzy multi-search-replace diffs with Levenshtein matching */
    TSharedPtr<FAgentZetDiffApplicator> DiffApplicator;

    // ---- Cost & Auto-Approval Tracking ----

    /** Tracks per-request and session costs */
    FAgentZetCostTracker CostTracker;

    /** Tracks consecutive auto-approved requests and cumulative cost */
    FAgentZetAutoApprovalHandler AutoApprovalHandler;

    /** Cost of the last completed API request */
    float LastRequestCost = 0.0f;

    

    // ---- Phase 2: Agent Mode (per panel, reflects active tab) ----

    /** Current agent mode for the active tab.
     *  Determines: tool schema set, system prompt role, write permissions. */
    EAgentZetAgentMode CurrentAgentMode = EAgentZetAgentMode::General;

    // ---- Phase 3: Checkpoint Manager ----

    /** Git-based shadow checkpoint system — saves state before each tool batch */
    TSharedPtr<FAgentZetCheckpointManager> CheckpointManager;

    /** @Reference parser — resolves @file, @errors, @selection, etc. in user input */
    TSharedPtr<FAgentZetReferenceParser> ReferenceParser;

    /** Task delegation — manages AI-initiated sub-tasks via new_task tool */
    TSharedPtr<FAgentZetTaskDelegation> TaskDelegation;

    // ---- Phase 4: Advanced Features ----

    /** Task history — persists all conversation sessions with metadata */
    TSharedPtr<FAgentZetTaskHistory> TaskHistory;

    /** Slash command registry — /new-actor, /fix-errors, etc. */
    TSharedPtr<FAgentZetSlashCommandRegistry> SlashCommandRegistry;

    /** Skills manager — loadable step-by-step instruction guides for Claude */
    TSharedPtr<FAgentZetSkillsManager> SkillsManager;

    /** MCP client — connects to external tool servers */
    TSharedPtr<FAgentZetMCPClient> MCPClient;

    /** Code structure parser — extracts UE class/function signatures for folded context */
    TSharedPtr<FAgentZetCodeStructureParser> CodeStructureParser;

    /** Cached folded code structure context (injected into system prompt after condensation) */
    FString CachedCodeStructureContext;

    /** Update the cached code structure context from tracked files */
    void UpdateCodeStructureContext();

    // ---- New UI widgets (Roo-Code parity) ----

    /** Context bar: colored progress bar + token/cost + mode selector + Condense button */
    TSharedPtr<SAgentZetContextBar> ContextBar;

    /** Checkpoint timeline panel (toggled by header button) */
    TSharedPtr<SAgentZetCheckpointPanel> CheckpointPanel;

    /** Task history browser panel (toggled by header button) */
    TSharedPtr<SAgentZetHistoryPanel> HistoryPanel;

    /** Follow-up suggestions bar (shown after attempt_completion) */
    TSharedPtr<SAgentZetFollowUpBar> FollowUpBar;

    /** File changes panel: lists all files modified during session */
    TSharedPtr<SAgentZetFileChangesPanel> FileChangesPanel;

    /** Whether the checkpoint panel is visible */
    bool bShowCheckpointPanel = false;

    /** Whether the history panel is visible */
    bool bShowHistoryPanel = false;

    /** Whether the file changes panel is visible */
    bool bShowFileChangesPanel = false;

    // ---- New UI event handlers ----

    /** Toggle checkpoint panel visibility */
    FReply OnCheckpointsPanelToggled();

    /** Toggle history panel visibility */
    FReply OnHistoryPanelToggled();

    /** Toggle file changes panel visibility */
    FReply OnFileChangesPanelToggled();

    /** Called when user selects a follow-up suggestion */
    void OnFollowUpSelected(const FString& SuggestionText);

    /** Called when user clicks Restore on a checkpoint */
    void OnRestoreCheckpoint(const FString& CommitHash);

    /** Called when user clicks Diff on a checkpoint */
    void OnViewCheckpointDiff(const FString& CommitHash);

    /** Called when user clicks Resume on a history item */
    void OnLoadHistoryTask(const FString& TabId);

    /** Called when user clicks Delete on a history item */
    void OnDeleteHistoryTask(const FString& TabId);

    /** Called when user renames a history item inline */
    void OnRenameHistoryTask(const FString& TabId, const FString& NewTitle);

    /** Generate an auto-title from the first user message and apply it to the active tab.
     *  Called after the first user-assistant exchange completes. */
    void TryAutoTitleActiveTab(const FString& FirstUserMessage);

    /** Update all live UI elements (context bar, file changes, etc.) */
    void UpdateLiveUI();

    /** Token usage tracking */
    FAgentZetTokenUsage SessionTokenUsage;

    /** Last reported token usage (for context management) */
    FAgentZetTokenUsage LastResponseTokenUsage;

    /** Context usage percentage (0-100), updated after each API response */
    float ContextUsagePercent = 0.0f;

    // ---- Event handlers ----

    /** Called when the user submits a prompt via the input area */
    void OnPromptSubmitted(const FString& PromptText);

    

    // ---- Delegates from ChatSession ----
    void OnMessageAdded(const FAgentZetMessage& Message);
    void OnMessageUpdated(const FGuid& MessageId, const FString& DeltaText, EAgentZetMessageRole Role);
    void OnStatusUpdated(const FString& StatusText);
    void OnAgentFinished(const FString& Reason);
    void OnToolRequiresApproval(const FAgentZetActionPlan& Plan);
    void OnRequestStarted();
    void OnRequestCompleted(bool bSuccess);

    /** Called when an HTTP error is received */
    void OnErrorReceived(const FAgentZetHTTPError& Error);

    /** Called when token usage is updated */
    void OnTokenUsageUpdated(const FAgentZetTokenUsage& Usage);

    /** Called when the Stop button is clicked */
    FReply OnStopClicked();

    /** Called when the Condense Context button is clicked */
    FReply OnCondenseContextClicked();

    // ---- Todo / Task Management ----

    /** Handle the update_todo_list meta-tool: parse markdown checklist, update widget.
     *  Returns the tool_result string. */
    FString HandleUpdateTodoList(const FAgentZetToolCall& ToolCall);

    // ---- Phase 2: Mode switching ----

    /**
     * Handle the attempt_completion tool — TERMINATES the agentic loop.
     * This is the PRIMARY way Claude signals "I'm done with this task."
     * Displays the result to the user and stops processing.
     * Returns the tool_result string sent back to Claude (for context).
     */
    FString HandleAttemptCompletion(const FAgentZetToolCall& ToolCall);

    /** Handle the switch_mode meta-tool: change active tab's agent mode.
     *  Returns the tool_result string. */
    FString HandleSwitchMode(const FAgentZetToolCall& ToolCall);

    /** Apply a mode change to the active tab and update UI + system prompt */
    void ApplyAgentMode(EAgentZetAgentMode NewMode);

    /** Get display name for a mode (for UI display) */
    static FString GetModeDisplayName(EAgentZetAgentMode Mode);

    // ---- Phase 1+2: Context window overflow handling ----

    /** Called by ClaudeClient when context window is exceeded.
     *  Trims history by ContextWindowForcedKeepFraction and retries. */
    void HandleContextWindowExceeded(int32 RetryCount);

    // ---- Phase 3: Meta-tool handlers ----

    /** Handle the new_task tool: spawn a child tab and delegate to it */
    FString HandleNewTask(const FAgentZetToolCall& ToolCall);

    /** Handle the skill tool: load a skill document and inject as context */
    FString HandleSkillTool(const FAgentZetToolCall& ToolCall);

    /** Parse @references in user input and resolve to content blocks */
    FString ResolveReferencesInInput(FString& InOutUserInput) const;

    // ---- Phase 4: Sub-task completion callback ----

    /** Called when a child tab's task completes (via attempt_completion). */
    void OnSubTaskCompleted(const FString& SubTaskId, bool bSuccess, const FString& ResultMessage);

    // ---- Agentic Loop Core ----
    
/** Build the per-message environment details string (injected at each API call) */
FString BuildEnvironmentDetailsString() const;

    // ---- Approval Flow ----

    /** Called when the user approves the pending tool actions */
    void OnToolCallsApproved(const FAgentZetActionPlan& Plan);

    /** Called when the user rejects the pending tool actions */
    void OnToolCallsRejected(const FAgentZetActionPlan& Plan);

    // ---- Auto-Approval Limit Handling ----

    /** Called when auto-approval limits (request count or cost) are exceeded.
     *  Shows a dialog asking the user to confirm continuation or cancel. */
    void HandleAutoApprovalLimitReached(const FAgentZetAutoApprovalCheck& Check);

    // ---- Asset Checkpoints (Phase 5.2) ----

    /** Create a checkpoint snapshot of all assets that will be affected by the pending tool calls.
     *  Called before each tool execution batch so the user can restore if needed. */
    void CreateCheckpointForToolBatch(const TArray<FAgentZetToolCall>& ToolCalls, int32 LoopIteration);

    /** Get all asset paths that will be affected by a set of tool calls */
    TArray<FString> GetAffectedPathsFromToolCalls(const TArray<FAgentZetToolCall>& ToolCalls) const;

    // ---- Helpers ----

    /** Build the system prompt with project context + editor context */
    FString BuildSystemPrompt() const;

    /** Initialize backend components with current settings */
    void InitializeBackend();

    /** Register all action executors with the router */
    void RegisterExecutors();

    /** Configure the LLM client from developer settings.
     *  Recreates the client when the active provider changes. */
    void ConfigureClientFromSettings(FName PropertyName = NAME_None);

    /**
     * Get a typed pointer to the active LLM client downcast as FAgentZetClaudeClient.
     * Returns nullptr for non-Anthropic providers that don't have context window exceeded support.
     * Used only for Anthropic-specific features like OnContextWindowExceeded.
     */
    FAgentZetClaudeClient* ClaudeClientPtr() const;

    /** Show the privacy disclosure dialog (first use only) */
    bool CheckPrivacyDisclosure();

    /** Get context window size in tokens from current settings */
    int32 GetContextWindowTokens() const;

    /** The ID of the message currently being streamed */
    FGuid CurrentStreamingMessageId;

    /** The active chat session for the current tab */
    TSharedPtr<class FAgentZetChatSession> ActiveChatSession;

    /** Helper: query conversation state from active ChatSession (returns Idle if none) */
    EConversationState GetCurrentConversationState() const;

    /** Helper: backward-compat check — true when Streaming or Cancelling */
    bool IsProcessing() const;

    /** Called when the conversation state changes — forwards to InputArea */
    void OnConversationStateChanged(EConversationState NewState);

    // ---- Message Queue (Roo Code's MessageQueueService concept) ----

    /** Messages queued while IsProcessing() == true.
     *  When the current task finishes, the next queued message is auto-started.
     *  Prevents the "already processing, please wait" UX problem. */
    TArray<FString> PendingMessageQueue;

    /** Process the next queued message (called when a task completes) */
    void ProcessNextQueuedMessage();

    // ---- Multi-Tab Session State ----

    TArray<FAgentZetConversationTabState> ConversationTabs;
    int32 ActiveTabIndex = INDEX_NONE;
    int32 NextTabNumber = 1;

    FAgentZetConversationTabState* GetActiveTabState();
    const FAgentZetConversationTabState* GetActiveTabState() const;
    void CreateNewTab(const FString& InTitle = FString(), bool bMakeActive = true);
    void CloseTab(int32 TabIndex);
    void SwitchToTab(int32 TabIndex);
    void RefreshTabStrip();
    void RenderActiveConversation();
    void SyncRuntimeStateToActiveTab();
    void LoadRuntimeStateFromActiveTab();

    void LoadTabsFromDisk();
    void SaveTabsToDisk();
    static FString GetTabsSessionDir();
    static FString GetTabsManifestPath();
    static FString MakeTabConversationFileName(const FString& TabId);
    static FString MakeDefaultTabTitle(int32 TabNumber);
    static bool TryParseDefaultTabNumber(const FString& Title, int32& OutNumber);
    int32 GetNextAvailableTabNumber() const;

    FReply OnAddTabClicked();

    // ---- v4.0: Per-Task Directory Model ----

    /** Get base directory for per-task storage: Saved/AgentZet/Tasks/ */
    static FString GetTasksBaseDir();

    /** Get directory for a specific task: Saved/AgentZet/Tasks/<TaskId>/ */
    static FString GetTaskDir(const FString& TaskId);

    /** Get path to the task index file: Saved/AgentZet/task_index.json */
    static FString GetTaskIndexPath();

    /** Build metadata for a single tab (used when writing task_index.json) */
    FAgentZetTaskMetadata BuildTaskMetadata(const FAgentZetConversationTabState& TabState) const;

    /** Save task_index.json with metadata for all tabs */
    void SaveTaskIndex();

    /** Attempt to migrate from legacy Conversations/Tabs/ format to per-task directories.
     *  Called once during LoadTabsFromDisk if new format is not found but legacy is present.
     *  @return true if migration occurred */
    bool MigrateFromLegacyFormat();

    /** Set task status on the active tab and persist */
    void SetActiveTaskStatus(EAgentZetTaskStatus NewStatus);

    // ---- v4.1: Task Resumption ----

    /** Called when the user clicks "Continue Task" on an interrupted task.
     *  Injects synthetic tool_results, a resumption prompt, and restarts the agentic loop. */
    void OnContinueInterruptedTask();

    /** Called when the user clicks "End Task" on an interrupted task.
     *  Marks the task as Completed, hides the resumption bar, and returns to idle. */
    void OnEndInterruptedTask();

    /** Check if the active tab has an interrupted task and show the resumption bar if so.
     *  Called during RenderActiveConversation() and SwitchToTab(). */
    void CheckAndShowResumptionBar();

    /** Build a human-readable "X hours" / "X minutes" string from a timestamp */
    static FString BuildTimeAgoText(const FDateTime& InterruptedAt);
};
