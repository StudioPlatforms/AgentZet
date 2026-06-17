// Copyright AgentZet. All Rights Reserved.

#include "AgentZetClaudeClient.h"
#include "AgentZetCoreModule.h"
#include "AgentZetSettings.h"
#include "AgentZetModelRegistry.h"
#include "AgentZetToolResultValidator.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Async/Async.h"

static const FString DefaultEndpoint = TEXT("https://api.anthropic.com/v1/messages");
static const FString AnthropicVersion = TEXT("2023-06-01");

FAgentZetClaudeClient::FAgentZetClaudeClient()
	: Endpoint(DefaultEndpoint)
	, Model(TEXT("claude-sonnet-4-6"))
	, MaxTokens(8192)
	, bRequestInFlight(false)
	, bRequestCancelled(false)
	, LastBytesReceived(0)
	, bBuildingToolCall(false)
	, ConsecutiveRateLimits(0)
	, ContextWindowRetryCount(0)
{
}

FAgentZetClaudeClient::~FAgentZetClaudeClient()
{
	CancelRequest();
}

void FAgentZetClaudeClient::SendMessage(
	const TArray<FAgentZetMessage>& ConversationHistory,
	const FString& SystemPrompt,
	const TArray<TSharedPtr<FJsonObject>>& ToolSchemas)
{
	// Reset context window retry count on new top-level SendMessage call only.
	// RetryWithTrimmedHistory calls SendMessageInternal to avoid resetting this counter.
	ContextWindowRetryCount = 0;
	SendMessageInternal(ConversationHistory, SystemPrompt, ToolSchemas);
}

void FAgentZetClaudeClient::SendMessageInternal(
	const TArray<FAgentZetMessage>& ConversationHistory,
	const FString& SystemPrompt,
	const TArray<TSharedPtr<FJsonObject>>& ToolSchemas)
{
	if (bRequestInFlight)
	{
		UE_LOG(LogAgentZet, Warning, TEXT("ClaudeClient: Request already in flight. Ignoring new request."));
		return;
	}

	if (ApiKey.IsEmpty())
	{
		UE_LOG(LogAgentZet, Error, TEXT("ClaudeClient: API key is not set. Cannot send request."));
		ErrorReceivedDelegate.Broadcast(FAgentZetHTTPError::ConnectionFailed(TEXT("Anthropic")));
		RequestCompletedDelegate.Broadcast(false);
		return;
	}

	// Store for potential retry on rate limit or context window exceeded
	PendingRetryHistory = ConversationHistory;
	PendingRetrySystemPrompt = SystemPrompt;
	PendingRetryToolSchemas = ToolSchemas;

	const UAgentZetDeveloperSettings* Settings = UAgentZetDeveloperSettings::Get();

	TSharedPtr<FJsonObject> RequestBody = BuildRequestBody(ConversationHistory, SystemPrompt, ToolSchemas);
	if (!RequestBody.IsValid())
	{
		UE_LOG(LogAgentZet, Error, TEXT("ClaudeClient: Failed to build request body."));
		RequestCompletedDelegate.Broadcast(false);
		return;
	}

	FString RequestBodyString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyString);
	FJsonSerializer::Serialize(RequestBody.ToSharedRef(), Writer);

	CurrentRequest = FHttpModule::Get().CreateRequest();
	CurrentRequest->SetURL(Endpoint);
	CurrentRequest->SetVerb(TEXT("POST"));
	CurrentRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	CurrentRequest->SetHeader(TEXT("x-api-key"), ApiKey);
	CurrentRequest->SetHeader(TEXT("anthropic-version"), AnthropicVersion);

	// Build beta flags — Zoo-Code pattern: always include fine-grained-tool-streaming,
	// conditionally add context-1m and interleaved-thinking based on model + settings.
	FString BetaFlags = TEXT("fine-grained-tool-streaming-2025-05-14");

	// 1M context beta (matches Zoo-Code anthropic.ts 'context-1m-2025-08-07' gate)
	// Models with beta-based 1M: sonnet-4-x, opus-4-6
	// Models with native 1M: opus-4-7, opus-4-8 — NO beta flag needed
	if (Settings && Settings->ContextWindow == EAgentZetContextWindow::Extended_1M)
	{
		const bool bNeeds1MBeta =
			Model.Contains(TEXT("sonnet-4-6")) || Model.Contains(TEXT("sonnet-4-5")) ||
			Model.Contains(TEXT("sonnet-4-20250514")) || Model.Contains(TEXT("opus-4-6"));
		const bool bHasNative1M =
			Model.Contains(TEXT("opus-4-7")) || Model.Contains(TEXT("opus-4-8"));

		if (bNeeds1MBeta)
		{
			BetaFlags += TEXT(",context-1m-2025-08-07");
			UE_LOG(LogAgentZet, Log, TEXT("ClaudeClient: 1M context beta enabled for %s"), *Model);
		}
		else if (bHasNative1M)
		{
			UE_LOG(LogAgentZet, Log, TEXT("ClaudeClient: Native 1M context for %s — no beta flag needed"), *Model);
		}
	}

	// Extended thinking — claude-3-7-sonnet requires interleaved-thinking beta
	if (Settings && Settings->bEnableExtendedThinking && Model.Contains(TEXT("3-7-sonnet")))
	{
		BetaFlags += TEXT(",interleaved-thinking-2025-05-14");
	}

	CurrentRequest->SetHeader(TEXT("anthropic-beta"), BetaFlags);
	CurrentRequest->SetContentAsString(RequestBodyString);

	if (Settings)
	{
		CurrentRequest->SetTimeout(Settings->RequestTimeoutSeconds);

		// CRITICAL: Explicitly disable activity timeout for SSE streaming.
		// ActivityTimeout (CURLOPT_LOW_SPEED_TIME) kills connections during natural
		// pauses in SSE data delivery (model thinking, generating tool JSON).
		// Setting to 0 disables it. NOT calling SetActivityTimeout lets UE5's
		// default (~30s from HttpActivityTimeout CVar) take effect and kill us.
		CurrentRequest->SetActivityTimeout(0.0f);
	}

	CurrentRequest->OnRequestProgress64().BindRaw(this, &FAgentZetClaudeClient::HandleRequestProgress);
	CurrentRequest->OnProcessRequestComplete().BindRaw(this, &FAgentZetClaudeClient::HandleRequestComplete);

	// Reset streaming state
	SSEParser.Reset();
	LastBytesReceived = 0;
	RawByteBuffer.Empty();
	CurrentAssistantContent.Empty();
	PendingToolCalls.Empty();
	CurrentToolCall = FAgentZetToolCall();
	CurrentToolCallInput.Empty();
	bBuildingToolCall = false;
	bRequestCancelled = false;
	CurrentMessageId = FGuid::NewGuid();
	LastTokenUsage = FAgentZetTokenUsage();
	LastStopReason.Empty();

	bRequestInFlight = true;
	RequestStartedDelegate.Broadcast();
	CurrentRequest->ProcessRequest();

	UE_LOG(LogAgentZet, Log, TEXT("ClaudeClient: Request sent to %s with model %s"), *Endpoint, *Model);
}

void FAgentZetClaudeClient::CancelRequest()
{
	if (CurrentRequest.IsValid() && bRequestInFlight)
	{
		// CRITICAL (Gemini): Set bRequestCancelled BEFORE calling CancelRequest()
		// to prevent the race condition where HandleRequestComplete fires one last time
		bRequestCancelled = true;
		CurrentRequest->CancelRequest();
		CurrentRequest.Reset();
		bRequestInFlight = false;
		UE_LOG(LogAgentZet, Log, TEXT("ClaudeClient: Request cancelled by user."));
		RequestCompletedDelegate.Broadcast(false);
	}
}

bool FAgentZetClaudeClient::IsRequestInFlight() const
{
	return bRequestInFlight;
}

void FAgentZetClaudeClient::SetEndpoint(const FString& InEndpoint) { Endpoint = InEndpoint; }
void FAgentZetClaudeClient::SetApiKey(const FString& InApiKey) { ApiKey = InApiKey; }
void FAgentZetClaudeClient::SetModel(const FString& InModel) { Model = InModel; }
void FAgentZetClaudeClient::SetMaxTokens(int32 InMaxTokens) { MaxTokens = FMath::Clamp(InMaxTokens, 1, 200000); }

// ============================================================================
// Request Body Construction
// ============================================================================

// Places an Anthropic cache_control: "ephemeral" breakpoint on the last content block of a
// message. If the message stores its content as a plain string, it is promoted to a single
// text block so the marker has a block to attach to.
static void AddCacheControlToLastContentBlock(const TSharedPtr<FJsonObject>& MessageObj)
{
	if (!MessageObj.IsValid())
	{
		return;
	}

	auto MakeEphemeral = []()
	{
		TSharedPtr<FJsonObject> CacheControl = MakeShared<FJsonObject>();
		CacheControl->SetStringField(TEXT("type"), TEXT("ephemeral"));
		return CacheControl;
	};

	// content as an array of blocks (tool_result blocks, assistant tool_use/text blocks) → last block
	const TArray<TSharedPtr<FJsonValue>>* ContentArray = nullptr;
	if (MessageObj->TryGetArrayField(TEXT("content"), ContentArray) && ContentArray && ContentArray->Num() > 0)
	{
		const TSharedPtr<FJsonObject>* LastBlock = nullptr;
		if ((*ContentArray)[ContentArray->Num() - 1]->TryGetObject(LastBlock) && LastBlock)
		{
			(*LastBlock)->SetObjectField(TEXT("cache_control"), MakeEphemeral());
		}
		return;
	}

	// content as a plain string → promote to a single cached text block
	FString ContentString;
	if (MessageObj->TryGetStringField(TEXT("content"), ContentString))
	{
		TSharedPtr<FJsonObject> TextBlock = MakeShared<FJsonObject>();
		TextBlock->SetStringField(TEXT("type"), TEXT("text"));
		TextBlock->SetStringField(TEXT("text"), ContentString);
		TextBlock->SetObjectField(TEXT("cache_control"), MakeEphemeral());

		TArray<TSharedPtr<FJsonValue>> NewContent;
		NewContent.Add(MakeShared<FJsonValueObject>(TextBlock));
		MessageObj->SetArrayField(TEXT("content"), NewContent);
	}
}

TSharedPtr<FJsonObject> FAgentZetClaudeClient::BuildRequestBody(
	const TArray<FAgentZetMessage>& ConversationHistory,
	const FString& SystemPrompt,
	const TArray<TSharedPtr<FJsonObject>>& ToolSchemas) const
{
	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	const UAgentZetDeveloperSettings* Settings = UAgentZetDeveloperSettings::Get();

	Body->SetStringField(TEXT("model"), Model);
	Body->SetNumberField(TEXT("max_tokens"), MaxTokens);
	Body->SetBoolField(TEXT("stream"), true);

	// System prompt: structured array format with cache_control for Anthropic prompt caching.
	//
	// As of Phase 2B, BuildSystemPrompt() returns a fully STATIC prompt (no DYNAMIC_SECTION
	// marker) — per-iteration-volatile context now rides in the environment-details block
	// appended to the last message. So the common path is the single-block branch below: the
	// whole tools + system prefix gets one cache_control: "ephemeral" breakpoint and is cached
	// for 5 minutes (~90% off on hits). A second breakpoint is placed on the conversation
	// history (see after the messages array is built) so the transcript caches too. The
	// marker-split branch is retained as a backward-compatible fallback if a marker is present.
	if (!SystemPrompt.IsEmpty())
	{
		static const FString DynamicMarker = TEXT("\n\n===AgentZet_DYNAMIC_SECTION===\n\n");

		TArray<TSharedPtr<FJsonValue>> SystemArray;
		int32 SplitIdx = SystemPrompt.Find(DynamicMarker);

		if (SplitIdx != INDEX_NONE)
		{
			// Split into static prefix (cacheable) + dynamic suffix (always re-processed)
			FString StaticPrefix = SystemPrompt.Left(SplitIdx);
			FString DynamicSuffix = SystemPrompt.Mid(SplitIdx + DynamicMarker.Len());

			// Block 1: Static prefix with cache_control
			TSharedPtr<FJsonObject> StaticBlock = MakeShared<FJsonObject>();
			StaticBlock->SetStringField(TEXT("type"), TEXT("text"));
			StaticBlock->SetStringField(TEXT("text"), StaticPrefix);
			TSharedPtr<FJsonObject> CacheControl = MakeShared<FJsonObject>();
			CacheControl->SetStringField(TEXT("type"), TEXT("ephemeral"));
			StaticBlock->SetObjectField(TEXT("cache_control"), CacheControl);
			SystemArray.Add(MakeShared<FJsonValueObject>(StaticBlock));

			// Block 2: Dynamic suffix (no cache_control — changes each call)
			if (!DynamicSuffix.IsEmpty())
			{
				TSharedPtr<FJsonObject> DynamicBlock = MakeShared<FJsonObject>();
				DynamicBlock->SetStringField(TEXT("type"), TEXT("text"));
				DynamicBlock->SetStringField(TEXT("text"), DynamicSuffix);
				SystemArray.Add(MakeShared<FJsonValueObject>(DynamicBlock));
			}

			UE_LOG(LogAgentZet, Log, TEXT("ClaudeClient: System prompt split for caching — static: %d chars, dynamic: %d chars"),
				StaticPrefix.Len(), DynamicSuffix.Len());
		}
		else
		{
			// No marker found — send as single block with cache_control
			TSharedPtr<FJsonObject> SystemBlock = MakeShared<FJsonObject>();
			SystemBlock->SetStringField(TEXT("type"), TEXT("text"));
			SystemBlock->SetStringField(TEXT("text"), SystemPrompt);
			TSharedPtr<FJsonObject> CacheControl = MakeShared<FJsonObject>();
			CacheControl->SetStringField(TEXT("type"), TEXT("ephemeral"));
			SystemBlock->SetObjectField(TEXT("cache_control"), CacheControl);
			SystemArray.Add(MakeShared<FJsonValueObject>(SystemBlock));
		}

		Body->SetArrayField(TEXT("system"), SystemArray);
	}

	// Look up model info for capability checks (binary thinking, temperature)
	FAgentZetModelInfo ModelInfo = FAgentZetModelRegistry::GetModelInfo(EAgentZetProvider::Anthropic, Model);

	// Extended Thinking — budget-based vs binary (adaptive) thinking
	// Zoo-Code reasoning.ts: getAnthropicProviderReasoning()
	// - Binary models (Opus 4.7/4.8): {type: "enabled"} — no budget_tokens
	// - Budget models (Sonnet 4.x, Opus 4.6, etc.): {type: "enabled", budget_tokens: N}
	if (Settings && Settings->bEnableExtendedThinking)
	{
		TSharedPtr<FJsonObject> ThinkingObj = MakeShared<FJsonObject>();

		if (ModelInfo.bSupportsReasoningBinary)
		{
			// Binary/adaptive thinking — model decides its own budget
			// Opus 4.7/4.8 reject budget_tokens entirely
			ThinkingObj->SetStringField(TEXT("type"), TEXT("enabled"));
			UE_LOG(LogAgentZet, Log, TEXT("ClaudeClient: Adaptive thinking enabled for %s (no budget_tokens)"), *Model);
		}
		else
		{
			// Budget-based thinking — user controls token budget
			int32 SafeBudget = FMath::Max(1024, Settings->ThinkingBudgetTokens);
			if (MaxTokens <= SafeBudget)
			{
				UE_LOG(LogAgentZet, Warning, TEXT("ClaudeClient: max_tokens (%d) must be > budget_tokens (%d). Adjusting."),
					MaxTokens, SafeBudget);
			}

			ThinkingObj->SetStringField(TEXT("type"), TEXT("enabled"));
			ThinkingObj->SetNumberField(TEXT("budget_tokens"), SafeBudget);
		}

		Body->SetObjectField(TEXT("thinking"), ThinkingObj);
	}

	// Temperature — skip for models that don't support it (Opus 4.7/4.8)
	// Zoo-Code model-params.ts: if (model.supportsTemperature === false) { params.temperature = undefined }
	if (!ModelInfo.bSupportsTemperature)
	{
		Body->RemoveField(TEXT("temperature"));
		UE_LOG(LogAgentZet, Log, TEXT("ClaudeClient: Temperature omitted for %s (not supported)"), *Model);
	}

	TArray<TSharedPtr<FJsonValue>> MessagesArray = ConvertMessagesToJson(ConversationHistory);

	// CRITICAL: Validate and fix tool_result IDs before sending to Claude.
	// This prevents HTTP 400 "unexpected tool_use_id" errors caused by:
	// - ID mismatches between tool_use and tool_result blocks
	// - Duplicate tool_results
	// - Orphaned tool_use blocks without results (e.g. from interrupted executions)
	FAgentZetToolResultValidator::ValidateAndFixToolResults(MessagesArray);

	// PROMPT CACHING (Phase 2B): cache the conversation history so the growing transcript is
	// reused across agentic-loop iterations instead of re-processed at full price. The LAST
	// message carries the volatile per-call environment-details block, so the breakpoint goes
	// on the SECOND-to-last message — everything up to and including it (frozen tools + system
	// prefix + append-only history) is byte-stable across iterations. Together with the system
	// breakpoint this uses 2 of the 4 allowed breakpoints. Skipped when there is no prior turn.
	if (MessagesArray.Num() >= 2)
	{
		const TSharedPtr<FJsonObject>* SecondToLastMsg = nullptr;
		if (MessagesArray[MessagesArray.Num() - 2]->TryGetObject(SecondToLastMsg) && SecondToLastMsg)
		{
			AddCacheControlToLastContentBlock(*SecondToLastMsg);
		}
	}

	Body->SetArrayField(TEXT("messages"), MessagesArray);

	if (ToolSchemas.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> ToolsArray;
		for (const auto& Schema : ToolSchemas)
		{
			ToolsArray.Add(MakeShared<FJsonValueObject>(Schema));
		}
		Body->SetArrayField(TEXT("tools"), ToolsArray);
	}

	return Body;
}

TArray<TSharedPtr<FJsonValue>> FAgentZetClaudeClient::ConvertMessagesToJson(
	const TArray<FAgentZetMessage>& Messages) const
{
	TArray<TSharedPtr<FJsonValue>> Result;

	int32 i = 0;
	while (i < Messages.Num())
	{
		const FAgentZetMessage& Msg = Messages[i];

		// Skip truncation markers -- they're internal tracking only
		if (Msg.bIsTruncationMarker)
		{
			i++;
			continue;
		}

		if (Msg.Role == EAgentZetMessageRole::ToolResult)
		{
			TSharedPtr<FJsonObject> UserObj = MakeShared<FJsonObject>();
			UserObj->SetStringField(TEXT("role"), TEXT("user"));
			TArray<TSharedPtr<FJsonValue>> ContentArray;

			while (i < Messages.Num() && Messages[i].Role == EAgentZetMessageRole::ToolResult)
			{
				if (Messages[i].bIsTruncationMarker)
				{
					i++;
					continue;
				}

				TSharedPtr<FJsonObject> ToolResultObj = MakeShared<FJsonObject>();
				ToolResultObj->SetStringField(TEXT("type"), TEXT("tool_result"));

				// Sanitize tool_use_id: Claude requires pattern ^[a-zA-Z0-9_-]+$
				// After save/load cycles, IDs may be empty or contain invalid characters
				FString SafeToolUseId = Messages[i].ToolUseId;
				if (SafeToolUseId.IsEmpty())
				{
					SafeToolUseId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
					UE_LOG(LogAgentZet, Warning, TEXT("ClaudeClient: Empty tool_use_id at message %d, generated placeholder: %s"), i, *SafeToolUseId);
				}
				else
				{
					// Strip any characters that don't match [a-zA-Z0-9_-]
					FString Sanitized;
					for (TCHAR Ch : SafeToolUseId)
					{
						if (FChar::IsAlnum(Ch) || Ch == TEXT('_') || Ch == TEXT('-'))
						{
							Sanitized.AppendChar(Ch);
						}
					}
					if (Sanitized.IsEmpty())
					{
						Sanitized = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
						UE_LOG(LogAgentZet, Warning, TEXT("ClaudeClient: tool_use_id '%s' had no valid chars, generated placeholder: %s"), *SafeToolUseId, *Sanitized);
					}
					SafeToolUseId = Sanitized;
				}

				ToolResultObj->SetStringField(TEXT("tool_use_id"), SafeToolUseId);
				ToolResultObj->SetStringField(TEXT("content"), Messages[i].Content);

				if (Messages[i].ToolName == TEXT("error"))
				{
					ToolResultObj->SetBoolField(TEXT("is_error"), true);
				}

				ContentArray.Add(MakeShared<FJsonValueObject>(ToolResultObj));
				i++;
			}

			if (ContentArray.Num() > 0)
			{
				UserObj->SetArrayField(TEXT("content"), ContentArray);
				Result.Add(MakeShared<FJsonValueObject>(UserObj));
			}
		}
		else if (Msg.Role == EAgentZetMessageRole::User)
		{
			TSharedPtr<FJsonObject> MsgObj = MakeShared<FJsonObject>();
			MsgObj->SetStringField(TEXT("role"), TEXT("user"));
			MsgObj->SetStringField(TEXT("content"), Msg.Content);
			Result.Add(MakeShared<FJsonValueObject>(MsgObj));
			i++;
		}
		else if (Msg.Role == EAgentZetMessageRole::Assistant)
		{
			TSharedPtr<FJsonObject> MsgObj = MakeShared<FJsonObject>();
			MsgObj->SetStringField(TEXT("role"), TEXT("assistant"));

			// Use structured content blocks if available (preserves tool_use blocks)
			if (!Msg.ContentBlocksJson.IsEmpty())
			{
				TArray<TSharedPtr<FJsonValue>> ParsedBlocks;
				TSharedRef<TJsonReader<>> BlockReader = TJsonReaderFactory<>::Create(Msg.ContentBlocksJson);
				if (FJsonSerializer::Deserialize(BlockReader, ParsedBlocks))
				{
					// Sanitize tool_use block IDs to match Claude's pattern ^[a-zA-Z0-9_-]+$
					for (TSharedPtr<FJsonValue>& Block : ParsedBlocks)
					{
						const TSharedPtr<FJsonObject>* BlockObj = nullptr;
						if (!Block->TryGetObject(BlockObj)) continue;
						FString BlockType;
						(*BlockObj)->TryGetStringField(TEXT("type"), BlockType);
						if (BlockType == TEXT("tool_use"))
						{
							FString OrigId;
							(*BlockObj)->TryGetStringField(TEXT("id"), OrigId);
							FString SanitizedId;
							for (TCHAR Ch : OrigId)
							{
								if (FChar::IsAlnum(Ch) || Ch == TEXT('_') || Ch == TEXT('-'))
								{
									SanitizedId.AppendChar(Ch);
								}
							}
							if (SanitizedId.IsEmpty())
							{
								SanitizedId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
								UE_LOG(LogAgentZet, Warning, TEXT("ClaudeClient: Empty/invalid tool_use id '%s', generated placeholder: %s"), *OrigId, *SanitizedId);
							}
							if (SanitizedId != OrigId)
							{
								// Must create a mutable copy
								TSharedPtr<FJsonObject> FixedBlock = MakeShared<FJsonObject>();
								for (const auto& Pair : (*BlockObj)->Values)
								{
									FixedBlock->SetField(Pair.Key, Pair.Value);
								}
								FixedBlock->SetStringField(TEXT("id"), SanitizedId);
								Block = MakeShared<FJsonValueObject>(FixedBlock);
							}
						}
					}
					MsgObj->SetArrayField(TEXT("content"), ParsedBlocks);
				}
				else
				{
					// Fallback to flat text if JSON parse fails
					MsgObj->SetStringField(TEXT("content"), Msg.Content);
				}
			}
			else
			{
				MsgObj->SetStringField(TEXT("content"), Msg.Content);
			}

			Result.Add(MakeShared<FJsonValueObject>(MsgObj));
			i++;
		}
		else
		{
			i++;
		}
	}

	return Result;
}

// ============================================================================
// Raw byte buffering for UTF-8 safe SSE streaming
// ============================================================================

void FAgentZetClaudeClient::HandleRequestProgress(
	FHttpRequestPtr Request, uint64 BytesSent, uint64 BytesReceived)
{
	// Guard against stale callbacks from a previous request (race condition when
	// the agentic loop starts Request B before Request A's callbacks fully drain)
	if (Request != CurrentRequest)
	{
		UE_LOG(LogAgentZet, Verbose, TEXT("ClaudeClient: Ignoring stale progress callback for previous request."));
		return;
	}

	// CRITICAL (Gemini): Check bRequestCancelled FIRST to prevent race condition
	if (bRequestCancelled || !Request.IsValid()) return;

	FHttpResponsePtr Response = Request->GetResponse();
	if (!Response.IsValid()) return;

	const TArray<uint8>& RawContent = Response->GetContent();
	if (RawContent.Num() <= LastBytesReceived) return;

	int32 NewByteCount = RawContent.Num() - LastBytesReceived;
	const uint8* NewBytes = RawContent.GetData() + LastBytesReceived;
	LastBytesReceived = RawContent.Num();

	RawByteBuffer.Append(NewBytes, NewByteCount);

	int32 LastNewlinePos = -1;
	for (int32 j = RawByteBuffer.Num() - 1; j >= 0; --j)
	{
		if (RawByteBuffer[j] == '\n')
		{
			LastNewlinePos = j;
			break;
		}
	}

	if (LastNewlinePos < 0) return;

	int32 SafeByteCount = LastNewlinePos + 1;
	FString SafeChunk;
	FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(RawByteBuffer.GetData()), SafeByteCount);
	SafeChunk = FString(Converter.Length(), Converter.Get());

	RawByteBuffer.RemoveAt(0, SafeByteCount);

	if (SafeChunk.IsEmpty()) return;

	TArray<FAgentZetSSEEvent> Events;
	SSEParser.ProcessChunk(SafeChunk, Events);

	if (Events.Num() > 0)
	{
		ProcessSSEEvents(Events);
	}
}

// ============================================================================
// Request Completion
// ============================================================================

void FAgentZetClaudeClient::HandleRequestComplete(
	FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	// Guard against stale callbacks from a previous request (race condition when
	// the agentic loop starts Request B before Request A's callbacks fully drain)
	if (Request != CurrentRequest)
	{
		UE_LOG(LogAgentZet, Verbose, TEXT("ClaudeClient: Ignoring stale completion callback for previous request."));
		return;
	}

	// CRITICAL (Gemini): First check -- if cancelled, exit cleanly.
	// CancelRequest() can trigger this delegate one more time.
	// Do NOT broadcast errors, do NOT trigger retry pipeline.
	if (bRequestCancelled)
	{
		bRequestInFlight = false;
		CurrentRequest.Reset();
		return;
	}

	bRequestInFlight = false;

	if (!bConnectedSuccessfully || !Response.IsValid())
	{
		UE_LOG(LogAgentZet, Error, TEXT("ClaudeClient: Connection failed."));
		FAgentZetHTTPError Err = FAgentZetHTTPError::ConnectionFailed(TEXT("Anthropic"));
		ErrorReceivedDelegate.Broadcast(Err);
		RequestCompletedDelegate.Broadcast(false);
		return;
	}

	const int32 ResponseCode = Response->GetResponseCode();

	if (ResponseCode == 429)
	{
		ConsecutiveRateLimits++;
		// ScheduleRetryWithBackoff handles the max retry check internally
		// and will broadcast a hard error if retries are exhausted
		ScheduleRetryWithBackoff();
		return;
	}

	// -----------------------------------------------------------------------
	// Context window exceeded detection (HTTP 400 with specific error body)
	// Anthropic returns: {"type":"error","error":{"type":"invalid_request_error",
	//   "message":"prompt is too long: ... tokens > ... maximum"}}
	// -----------------------------------------------------------------------
	if (ResponseCode == 400)
	{
		FString ResponseBody = Response->GetContentAsString();
		if (IsContextWindowExceededError(ResponseCode, ResponseBody))
		{
			ContextWindowRetryCount++;

			if (ContextWindowRetryCount <= MaxContextWindowRetries)
			{
				UE_LOG(LogAgentZet, Warning,
					TEXT("ClaudeClient: Context window exceeded (HTTP 400). Firing OnContextWindowExceeded delegate (retry %d/%d)."),
					ContextWindowRetryCount, MaxContextWindowRetries);

				// Fire delegate — owner (SAgentZetMainPanel) MUST call RetryWithTrimmedHistory()
				// or CancelRequest() in response
				OnContextWindowExceeded.ExecuteIfBound(ContextWindowRetryCount);
				return;  // Don't fire generic error — owner handles it
			}
			else
			{
				// Exhausted retries — surface as hard error
				UE_LOG(LogAgentZet, Error,
					TEXT("ClaudeClient: Context window exceeded after %d retries. Giving up."),
					MaxContextWindowRetries);

				FAgentZetHTTPError Err;
				Err.Type = EAgentZetHTTPErrorType::ContextWindowExceeded;
				Err.UserFriendlyMessage = FString::Printf(
					TEXT("Context window is full and could not be reduced after %d attempts.\n"
					     "Try starting a new conversation or enabling Auto-Condense Context in settings."),
					MaxContextWindowRetries
				);
				ErrorReceivedDelegate.Broadcast(Err);
				RequestCompletedDelegate.Broadcast(false);
				return;
			}
		}
	}

	if (ResponseCode != 200)
	{
		FString ResponseBody = Response->GetContentAsString().Left(500);
		UE_LOG(LogAgentZet, Error, TEXT("ClaudeClient: HTTP %d -- %s"), ResponseCode, *ResponseBody);

		FAgentZetHTTPError Err = FAgentZetHTTPError::FromStatusCode(ResponseCode, ResponseBody, TEXT("Anthropic"));
		ErrorReceivedDelegate.Broadcast(Err);
		RequestCompletedDelegate.Broadcast(false);
		return;
	}

	ConsecutiveRateLimits = 0;

	// Flush remaining bytes
	if (RawByteBuffer.Num() > 0)
	{
		FString Remainder;
		FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(RawByteBuffer.GetData()), RawByteBuffer.Num());
		Remainder = FString(Converter.Length(), Converter.Get());
		RawByteBuffer.Empty();

		if (!Remainder.IsEmpty())
		{
			TArray<FAgentZetSSEEvent> RemainingEvents;
			SSEParser.ProcessChunk(Remainder + TEXT("\n\n"), RemainingEvents);
			if (RemainingEvents.Num() > 0)
			{
				ProcessSSEEvents(RemainingEvents);
			}
		}
	}

	// CRITICAL (Gemini): Check stop_reason for max_tokens truncation.
	// If the response was cut off, discard any incomplete tool calls.
	if (LastStopReason == TEXT("max_tokens"))
	{
		if (PendingToolCalls.Num() > 0)
		{
			UE_LOG(LogAgentZet, Warning,
				TEXT("ClaudeClient: Response truncated (max_tokens). Discarding %d incomplete tool calls."),
				PendingToolCalls.Num());
			PendingToolCalls.Empty();
		}

		// Notify the user
		FAgentZetHTTPError Err;
		Err.Type = EAgentZetHTTPErrorType::InvalidResponse;
		Err.UserFriendlyMessage = TEXT("Response was truncated due to token limit. Try increasing Max Response Tokens in settings, or simplify your request.");
		ErrorReceivedDelegate.Broadcast(Err);
	}

	// CRITICAL: Log BEFORE FinalizeResponse/Broadcast because the broadcast
	// synchronously triggers the agentic loop, which calls SendMessageInternal
	// and resets LastStopReason, PendingToolCalls, LastTokenUsage for the NEXT request.
	UE_LOG(LogAgentZet, Log, TEXT("ClaudeClient: Request completed (stop_reason=%s). %d tool calls, %d input tokens, %d output tokens."),
		*LastStopReason, PendingToolCalls.Num(), LastTokenUsage.InputTokens, LastTokenUsage.OutputTokens);

	FinalizeResponse();

	// CRITICAL: Do NOT call CurrentRequest.Reset() after this broadcast!
	// The broadcast synchronously triggers ChatSession -> agentic loop -> SendMessageInternal
	// which sets CurrentRequest = Request B. If we Reset() after broadcast returns, we destroy
	// Request B. Stale callback protection already works naturally: when Request B starts,
	// CurrentRequest points to B, and any lingering Request A callbacks fail the
	// "if (Request != CurrentRequest)" guard because Request A != Request B.
	RequestCompletedDelegate.Broadcast(true);
}

void FAgentZetClaudeClient::RetryWithTrimmedHistory(const TArray<FAgentZetMessage>& TrimmedHistory)
{
	if (bRequestInFlight)
	{
		UE_LOG(LogAgentZet, Warning, TEXT("ClaudeClient: RetryWithTrimmedHistory called while request in flight. Ignoring."));
		return;
	}

	UE_LOG(LogAgentZet, Log,
		TEXT("ClaudeClient: Retrying with trimmed history (%d -> %d messages, retry %d/%d)."),
		PendingRetryHistory.Num(), TrimmedHistory.Num(),
		ContextWindowRetryCount, MaxContextWindowRetries);

	// Update stored history for the retry
	PendingRetryHistory = TrimmedHistory;

	// CRITICAL FIX: Use SendMessageInternal here, NOT SendMessage.
	// SendMessage resets ContextWindowRetryCount = 0 which would bypass the MaxContextWindowRetries
	// guard and create an infinite retry loop. SendMessageInternal preserves the counter.
	SendMessageInternal(TrimmedHistory, PendingRetrySystemPrompt, PendingRetryToolSchemas);
}

bool FAgentZetClaudeClient::IsContextWindowExceededError(int32 HttpCode, const FString& ResponseBody) const
{
	if (HttpCode != 400) return false;

	// Parse the error body to check for context-related error messages
	TSharedPtr<FJsonObject> ErrorJson;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (!FJsonSerializer::Deserialize(Reader, ErrorJson) || !ErrorJson.IsValid())
	{
		return false;
	}

	// Check error.type == "invalid_request_error"
	const TSharedPtr<FJsonObject>* ErrorObj = nullptr;
	if (!ErrorJson->TryGetObjectField(TEXT("error"), ErrorObj))
	{
		return false;
	}

	FString ErrorType;
	(*ErrorObj)->TryGetStringField(TEXT("type"), ErrorType);
	if (ErrorType != TEXT("invalid_request_error"))
	{
		return false;
	}

	// Check message for context/token related keywords
	FString ErrorMessage;
	(*ErrorObj)->TryGetStringField(TEXT("message"), ErrorMessage);
	ErrorMessage = ErrorMessage.ToLower();

	return ErrorMessage.Contains(TEXT("too long"))
		|| ErrorMessage.Contains(TEXT("context_length"))
		|| ErrorMessage.Contains(TEXT("context window"))
		|| ErrorMessage.Contains(TEXT("prompt is too long"))
		|| ErrorMessage.Contains(TEXT("maximum context"))
		|| ErrorMessage.Contains(TEXT("tokens > "))
		|| ErrorMessage.Contains(TEXT("exceeds the maximum"));
}

void FAgentZetClaudeClient::ScheduleRetryWithBackoff()
{
	// =========================================================================
	// FIX: Rate limit backoff was far too aggressive.
	//
	// OLD behavior: 1s, 2s, 4s, 8s, 16s — 5 retries in 31 seconds.
	// With a per-minute rate limit of 30K tokens, retrying after 1-2s is
	// guaranteed to fail AND burns more tokens, making it worse.
	//
	// NEW behavior (matches Roo Code pattern):
	//   - Minimum 60s delay (per-minute rate limits need at least 60s to reset)
	//   - Exponential backoff starting at 60s: 60, 120, 240
	//   - Max 3 retries (not 5 — more retries just wastes the user's patience)
	//   - Parse Retry-After header if available
	// =========================================================================
	static constexpr float MinRateLimitDelay = 60.0f;  // Per-minute limits need at least 60s
	static constexpr int32 MaxRateLimitRetries = 3;

	if (ConsecutiveRateLimits > MaxRateLimitRetries)
	{
		// Exhausted retries — give a clear error explaining what happened
		UE_LOG(LogAgentZet, Error,
			TEXT("ClaudeClient: Rate limited %d times. Stopping retries. Request may be too large for your rate tier."),
			ConsecutiveRateLimits);

		FAgentZetHTTPError Err;
		Err.Type = EAgentZetHTTPErrorType::RateLimited;
		Err.StatusCode = 429;
		Err.UserFriendlyMessage = FString::Printf(
			TEXT("Rate limited by Anthropic after %d retries.\n\n")
			TEXT("Your request used too many tokens for your current rate tier (30K/min on free tier).\n")
			TEXT("Possible fixes:\n")
			TEXT("  \u2022 Wait 60+ seconds and try again\n")
			TEXT("  \u2022 Simplify your request (shorter messages, fewer files)\n")
			TEXT("  \u2022 Upgrade your Anthropic plan for higher rate limits\n")
			TEXT("  \u2022 Contact sales at https://claude.com/contact-sales"),
			ConsecutiveRateLimits);
		ErrorReceivedDelegate.Broadcast(Err);
		RequestCompletedDelegate.Broadcast(false);
		return;
	}

	// Exponential backoff starting at 60s: 60, 120, 240
	float DelaySeconds = MinRateLimitDelay * FMath::Pow(2.0f, (float)(ConsecutiveRateLimits - 1));

	UE_LOG(LogAgentZet, Warning, TEXT("ClaudeClient: Rate limited (429). Retrying in %.0f seconds (attempt %d/%d)."),
		DelaySeconds, ConsecutiveRateLimits, MaxRateLimitRetries);

	FAgentZetHTTPError RateLimitInfo;
	RateLimitInfo.Type = EAgentZetHTTPErrorType::RateLimited;
	RateLimitInfo.UserFriendlyMessage = FString::Printf(
		TEXT("Rate limited by Anthropic. Retrying in %.0f seconds... (attempt %d/%d)"),
		DelaySeconds, ConsecutiveRateLimits, MaxRateLimitRetries);
	ErrorReceivedDelegate.Broadcast(RateLimitInfo);

	// CRITICAL FIX: Capture a TWeakPtr to avoid use-after-free if the client is destroyed
	// before the timer fires. The raw [this] capture was a crash hazard.
	TWeakPtr<FAgentZetClaudeClient> WeakSelf = AsShared();

	AsyncTask(ENamedThreads::GameThread, [WeakSelf, DelaySeconds]()
	{
		FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda([WeakSelf](float DeltaTime) -> bool
			{
				TSharedPtr<FAgentZetClaudeClient> Self = WeakSelf.Pin();
				if (Self.IsValid() && !Self->PendingRetryHistory.IsEmpty())
				{
					// Rate limit retry uses SendMessageInternal to preserve rate limit context
					Self->SendMessageInternal(
						Self->PendingRetryHistory,
						Self->PendingRetrySystemPrompt,
						Self->PendingRetryToolSchemas);
				}
				return false;
			}),
			DelaySeconds
		);
	});
}

// ============================================================================
// SSE Event Processing
// ============================================================================

void FAgentZetClaudeClient::ProcessSSEEvents(const TArray<FAgentZetSSEEvent>& Events)
{
	for (const FAgentZetSSEEvent& Event : Events)
	{
		HandleSSEEvent(Event);
	}
}

void FAgentZetClaudeClient::HandleSSEEvent(const FAgentZetSSEEvent& Event)
{
	switch (Event.Type)
	{
	case EAgentZetSSEEventType::MessageStart:
		if (Event.JsonData.IsValid())
		{
			ExtractTokenUsage(Event.JsonData);
		}
		break;

	case EAgentZetSSEEventType::ContentBlockStart:
		if (Event.JsonData.IsValid())
		{
			const TSharedPtr<FJsonObject>* ContentBlock = nullptr;
			if (Event.JsonData->TryGetObjectField(TEXT("content_block"), ContentBlock))
			{
				FString BlockType;
				if ((*ContentBlock)->TryGetStringField(TEXT("type"), BlockType))
				{
					if (BlockType == TEXT("tool_use"))
					{
						CurrentToolCall = FAgentZetToolCall();
						(*ContentBlock)->TryGetStringField(TEXT("id"), CurrentToolCall.ToolUseId);
						(*ContentBlock)->TryGetStringField(TEXT("name"), CurrentToolCall.ToolName);
						CurrentToolCallInput.Empty();
						bBuildingToolCall = true;
					}
					else if (BlockType == TEXT("thinking"))
					{
						UE_LOG(LogAgentZet, Verbose, TEXT("ClaudeClient: Thinking block started."));
					}
					else if (BlockType == TEXT("text"))
					{
						bBuildingToolCall = false;
					}
				}
			}
		}
		break;

	case EAgentZetSSEEventType::ContentBlockDelta:
		if (Event.JsonData.IsValid())
		{
			const TSharedPtr<FJsonObject>* DeltaObj = nullptr;
			if (Event.JsonData->TryGetObjectField(TEXT("delta"), DeltaObj))
			{
				FString DeltaType;
				(*DeltaObj)->TryGetStringField(TEXT("type"), DeltaType);

				if (DeltaType == TEXT("text_delta"))
				{
					FString DeltaText;
					if ((*DeltaObj)->TryGetStringField(TEXT("text"), DeltaText))
					{
						CurrentAssistantContent += DeltaText;
						StreamingTextDelegate.Broadcast(CurrentMessageId, DeltaText);
					}
				}
				else if (DeltaType == TEXT("input_json_delta"))
				{
					FString PartialJson;
					if ((*DeltaObj)->TryGetStringField(TEXT("partial_json"), PartialJson))
					{
						CurrentToolCallInput += PartialJson;
					}
				}
				else if (DeltaType == TEXT("thinking_delta"))
				{
					FString ThinkingText;
					if ((*DeltaObj)->TryGetStringField(TEXT("thinking"), ThinkingText))
					{
						UE_LOG(LogAgentZet, Verbose, TEXT("ClaudeClient: Thinking: %s"), *ThinkingText.Left(100));
					}
				}
			}
		}
		break;

	case EAgentZetSSEEventType::ContentBlockStop:
		if (bBuildingToolCall && !CurrentToolCall.ToolUseId.IsEmpty())
		{
			// CRITICAL (Gemini): Graceful JSON failure handling.
			// If the response was truncated (max_tokens), the JSON may be incomplete.
			// FJsonSerializer::Deserialize will fail -- we must handle this gracefully.
			TSharedPtr<FJsonObject> InputJson;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(CurrentToolCallInput);
			if (FJsonSerializer::Deserialize(Reader, InputJson) && InputJson.IsValid())
			{
				CurrentToolCall.InputParams = InputJson;
				PendingToolCalls.Add(CurrentToolCall);
				UE_LOG(LogAgentZet, Log, TEXT("ClaudeClient: Tool call accumulated: %s (id: %s)"),
					*CurrentToolCall.ToolName, *CurrentToolCall.ToolUseId);
			}
			else
			{
				// JSON parse failed -- likely truncated by max_tokens
				UE_LOG(LogAgentZet, Warning,
					TEXT("ClaudeClient: Failed to parse tool input JSON for %s (likely truncated). Discarding tool call. Input: %s"),
					*CurrentToolCall.ToolName, *CurrentToolCallInput.Left(200));

				// Do NOT add to PendingToolCalls -- this is an incomplete tool call
			}

			CurrentToolCall = FAgentZetToolCall();
			CurrentToolCallInput.Empty();
			bBuildingToolCall = false;
		}
		break;

	case EAgentZetSSEEventType::MessageDelta:
		if (Event.JsonData.IsValid())
		{
			ExtractTokenUsage(Event.JsonData);

			// Extract stop_reason for max_tokens detection
			const TSharedPtr<FJsonObject>* DeltaObj = nullptr;
			if (Event.JsonData->TryGetObjectField(TEXT("delta"), DeltaObj))
			{
				(*DeltaObj)->TryGetStringField(TEXT("stop_reason"), LastStopReason);
			}
		}
		break;

	case EAgentZetSSEEventType::MessageStop:
		break;

	case EAgentZetSSEEventType::Error:
		if (Event.JsonData.IsValid())
		{
			FString ErrorType, ErrorMessage;
			const TSharedPtr<FJsonObject>* ErrorObj = nullptr;
			if (Event.JsonData->TryGetObjectField(TEXT("error"), ErrorObj))
			{
				(*ErrorObj)->TryGetStringField(TEXT("type"), ErrorType);
				(*ErrorObj)->TryGetStringField(TEXT("message"), ErrorMessage);
			}
			UE_LOG(LogAgentZet, Error, TEXT("ClaudeClient: SSE Error: %s -- %s"), *ErrorType, *ErrorMessage);

			FAgentZetHTTPError Err;
			Err.Type = EAgentZetHTTPErrorType::ServerError;
			Err.RawMessage = ErrorMessage;
			Err.UserFriendlyMessage = FString::Printf(TEXT("Claude API error: %s"), *ErrorMessage);
			ErrorReceivedDelegate.Broadcast(Err);
		}
		break;

	case EAgentZetSSEEventType::Ping:
		break;

	default:
		break;
	}
}

void FAgentZetClaudeClient::ExtractTokenUsage(const TSharedPtr<FJsonObject>& JsonData)
{
	if (!JsonData.IsValid()) return;

	const TSharedPtr<FJsonObject>* MessageObj = nullptr;
	if (JsonData->TryGetObjectField(TEXT("message"), MessageObj))
	{
		const TSharedPtr<FJsonObject>* UsageObj = nullptr;
		if ((*MessageObj)->TryGetObjectField(TEXT("usage"), UsageObj))
		{
			int32 InputTokens = 0;
			(*UsageObj)->TryGetNumberField(TEXT("input_tokens"), InputTokens);
			if (InputTokens > 0) LastTokenUsage.InputTokens = InputTokens;

			int32 CacheCreation = 0, CacheRead = 0;
			(*UsageObj)->TryGetNumberField(TEXT("cache_creation_input_tokens"), CacheCreation);
			(*UsageObj)->TryGetNumberField(TEXT("cache_read_input_tokens"), CacheRead);
			LastTokenUsage.CacheCreationInputTokens = CacheCreation;
			LastTokenUsage.CacheReadInputTokens = CacheRead;
		}
	}

	const TSharedPtr<FJsonObject>* UsageObj = nullptr;
	if (JsonData->TryGetObjectField(TEXT("usage"), UsageObj))
	{
		int32 OutputTokens = 0;
		(*UsageObj)->TryGetNumberField(TEXT("output_tokens"), OutputTokens);
		if (OutputTokens > 0) LastTokenUsage.OutputTokens = OutputTokens;
	}
}

void FAgentZetClaudeClient::FinalizeResponse()
{
	FAgentZetMessage AssistantMsg(EAgentZetMessageRole::Assistant, CurrentAssistantContent);
	AssistantMsg.MessageId = CurrentMessageId;

	// Build structured content blocks JSON for proper API round-tripping.
	// This preserves tool_use blocks alongside text blocks so the conversation
	// can be replayed, exported, or re-sent with full structural fidelity.
	TArray<TSharedPtr<FJsonValue>> ContentBlocks;

	// Add text block if there's any text content
	if (!CurrentAssistantContent.IsEmpty())
	{
		TSharedPtr<FJsonObject> TextBlock = MakeShared<FJsonObject>();
		TextBlock->SetStringField(TEXT("type"), TEXT("text"));
		TextBlock->SetStringField(TEXT("text"), CurrentAssistantContent);
		ContentBlocks.Add(MakeShared<FJsonValueObject>(TextBlock));
	}

	// Add tool_use blocks
	for (const FAgentZetToolCall& ToolCall : PendingToolCalls)
	{
		TSharedPtr<FJsonObject> ToolBlock = MakeShared<FJsonObject>();
		ToolBlock->SetStringField(TEXT("type"), TEXT("tool_use"));
		ToolBlock->SetStringField(TEXT("id"), ToolCall.ToolUseId);
		ToolBlock->SetStringField(TEXT("name"), ToolCall.ToolName);
		if (ToolCall.InputParams.IsValid())
		{
			ToolBlock->SetObjectField(TEXT("input"), ToolCall.InputParams);
		}
		ContentBlocks.Add(MakeShared<FJsonValueObject>(ToolBlock));
	}

	// Serialize content blocks to JSON string
	if (ContentBlocks.Num() > 0)
	{
		FString BlocksStr;
		TSharedRef<TJsonWriter<>> BlocksWriter = TJsonWriterFactory<>::Create(&BlocksStr);
		FJsonSerializer::Serialize(ContentBlocks, BlocksWriter);
		AssistantMsg.ContentBlocksJson = BlocksStr;
	}

	MessageCompleteDelegate.Broadcast(AssistantMsg);

	for (const FAgentZetToolCall& ToolCall : PendingToolCalls)
	{
		ToolCallReceivedDelegate.Broadcast(ToolCall);
	}

	if (LastTokenUsage.TotalTokens() > 0)
	{
		TokenUsageUpdatedDelegate.Broadcast(LastTokenUsage);
	}
}
