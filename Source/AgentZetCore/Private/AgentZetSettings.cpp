// Copyright AgentZet. All Rights Reserved.

#include "AgentZetSettings.h"
#include "AgentZetCoreModule.h"
#include "Misc/MessageDialog.h"

FAgentZetSettingsChangedDelegate UAgentZetDeveloperSettings::OnSettingsChanged;

static const FString DefaultApiEndpoint = TEXT("https://api.anthropic.com/v1/messages");

UAgentZetDeveloperSettings::UAgentZetDeveloperSettings()
{
	// Provider selection — defaults to Anthropic (backward compat)
	ActiveProvider = EAgentZetProvider::Anthropic;

	// --- Anthropic defaults ---
	ApiEndpoint = DefaultApiEndpoint;
	ClaudeModel = EAgentZetClaudeModel::Sonnet_4_6;
	ContextWindow = EAgentZetContextWindow::Standard_200K;
	MaxResponseTokens = 8192;
	RequestTimeoutSeconds = 120;
	bEnableExtendedThinking = false;
	ThinkingBudgetTokens = 3000;

	// --- OpenAI defaults (from Roo Code openAiNativeDefaultModelId) ---
	OpenAiModelId = TEXT("gpt-5.1-codex-max");
	OpenAiBaseUrl = TEXT("");  // empty = official https://api.openai.com/v1
	OpenAiReasoningEffort = EAgentZetReasoningEffort::Medium;

	// --- Azure OpenAI defaults (ported from Roo Code azureOpenAiDefaultApiVersion) ---
	// Azure auth/URL model is fundamentally different from official OpenAI:
	// - Header: 'api-key: {key}' NOT 'Authorization: Bearer {key}'
	// - URL: https://{resource}.openai.azure.com/openai/deployments/{deployment}/chat/completions
	// - Query: ?api-version=2024-02-01
	// - Uses Chat Completions only (no Responses API)
	AzureApiKey = TEXT("");
	AzureDeploymentName = TEXT("");  // User must set this to their deployment name
	AzureBaseUrl = TEXT("");         // User must set: https://{resource}.openai.azure.com
	AzureApiVersion = TEXT("2024-02-01");  // Stable GA version (matches Roo Code azureOpenAiDefaultApiVersion)

	// --- Google Gemini defaults (from Roo Code geminiDefaultModelId) ---
	GeminiModelId = TEXT("gemini-3.1-pro-preview");
	GeminiBaseUrl = TEXT("");  // empty = official generativelanguage.googleapis.com
	GeminiThinkingBudgetTokens = 0;  // 0 = disabled by default. User must opt-in for thinking models.
	GeminiReasoningEffort = EAgentZetReasoningEffort::Disabled;  // Disabled by default. Prevents sending unknown fields to non-thinking models.

	// --- DeepSeek defaults (from Zoo-Code-main deepSeekDefaultModelId) ---
	DeepSeekModelId = TEXT("deepseek-v4-flash");
	DeepSeekBaseUrl = TEXT("https://api.deepseek.com/v1");
	DeepSeekReasoningEffort = EAgentZetReasoningEffort::High;  // Default to 'high' — matches V4 default reasoning effort

	// --- Mistral defaults (from Roo Code mistralDefaultModelId) ---
	MistralModelId = TEXT("codestral-latest");

	// --- xAI defaults (from Roo Code xaiDefaultModelId) ---
	xAIModelId = TEXT("grok-code-fast-1");

	// --- OpenRouter defaults (from Roo Code openRouterDefaultModelId) ---
	OpenRouterModelId = TEXT("anthropic/claude-sonnet-4.5");

	// --- Ollama defaults (from Roo Code ollamaDefaultModelId) ---
	OllamaBaseUrl = TEXT("http://localhost:11434");
	OllamaModelId = TEXT("devstral:24b");
	OllamaContextSize = 32768;

	// --- LM Studio defaults (from Roo Code lMStudioDefaultModelId) ---
	LMStudioBaseUrl = TEXT("http://localhost:1234");
	LMStudioModelId = TEXT("mistralai/devstral-small-2505");

	// --- Custom endpoint defaults ---
	CustomBaseUrl = TEXT("");
	CustomApiKey = TEXT("");
	CustomEndpointModelId = TEXT("");

	// Safety defaults -- Marketplace-safe: Sandbox by default, opt-in for power features
	SecurityMode = EAgentZetSecurityMode::Sandbox;
	MaxAutoRetries = 3;
	bAutoApproveLowRisk = true;
	bAutoApproveAllTools = false;  // Must be explicitly enabled by user
	bAutoApproveReadOnlyTools = true; // Safe: read-only tools never mutate project
	MaxConsecutiveAutoApprovedRequests = 25; // Prevent runaway loops
	MaxAutoApprovedCostDollars = 5.0f; // $5 default cost cap
	bRequireTypedConfirmation = true;
	bEnableAutoBackup = true;
	MaxBackupCount = 50;
	bAutoCheckout = true;
	bAllowExternalProcessExecution = false; // Must be explicitly enabled by user

	// Privacy
	bHasAcceptedPrivacyDisclosure = false;

	// Cost defaults
	DailyTokenLimit = 0; // Unlimited
	bShowCostEstimates = true;
	bShowPerRequestCost = true; // Show per-request cost in header

	// Context defaults
	ContextTokenBudget = 30000;
	bIncludeSourceTree = true;
	bIncludeAssetSummary = true;
	bIncludeSettingsSnapshot = false;
	bIncludeClassHierarchy = true;

	// Context Management defaults (auto-condense)
	bAutoCondenseContext = true;
	AutoCondenseThresholdPercent = 80; // Trigger at 80% context usage

	// UI defaults
	ChatFontSize = 12;
	bShowTimestamps = true;
	bEnableStreamingDisplay = true;

	// Tool defaults -- all enabled
	bEnableBlueprintTools = true;
	bEnableCppTools = true;
	bEnableMaterialTools = true;
	bEnableImportTools = true;
	bEnableLevelTools = true;
	bEnableSettingsTools = true;
	bEnableBuildTools = true;
	bEnablePerformanceTools = true;

	// New tool defaults (v1.1)
	bEnablePythonTools = false;        // Opt-in: requires Developer mode, powerful but risky
	bEnableViewportCapture = true;     // Safe: read-only viewport capture
	bEnableDataTableTools = true;      // Safe: asset creation
	bEnableBehaviorTreeTools = true;   // Safe: asset creation + level modification
	bEnableSequencerTools = true;      // Safe: asset creation + level modification
	bEnablePIETools = false;           // Opt-in: requires Developer mode, can crash editor
	bEnableGASTools = true;            // Safe: asset creation + C++ generation
}

const UAgentZetDeveloperSettings* UAgentZetDeveloperSettings::Get()
{
	return GetDefault<UAgentZetDeveloperSettings>();
}

bool UAgentZetDeveloperSettings::IsApiKeySet() const
{
	return !ApiKey.IsEmpty() && ApiKey.Len() > 10;
}

FString UAgentZetDeveloperSettings::GetEffectiveEndpoint() const
{
	switch (ActiveProvider)
	{
	case EAgentZetProvider::Anthropic:
		return ApiEndpoint.IsEmpty() ? DefaultApiEndpoint : ApiEndpoint;
	case EAgentZetProvider::OpenAI:
		// Official OpenAI API only — for Azure, use the Azure provider.
		// If user accidentally put an Azure URL here, OpenAICompatClient will
		// auto-detect it via _isAzureUrl() and apply Azure wire format anyway.
		return OpenAiBaseUrl.IsEmpty() ? TEXT("https://api.openai.com/v1") : OpenAiBaseUrl;
	case EAgentZetProvider::Azure:
		// Azure base URL: https://{resource}.openai.azure.com
		// The OpenAICompatClient will append the deployment path + api-version.
		// If the user left BaseUrl empty, return empty so the client can validate
		// and give a clear error instead of sending a request to nowhere.
		return AzureBaseUrl;
	case EAgentZetProvider::Google:
		return GeminiBaseUrl.IsEmpty() ? TEXT("https://generativelanguage.googleapis.com") : GeminiBaseUrl;
	case EAgentZetProvider::DeepSeek:
		return DeepSeekBaseUrl.IsEmpty() ? TEXT("https://api.deepseek.com/v1") : DeepSeekBaseUrl;
	case EAgentZetProvider::Mistral:
		return TEXT("https://api.mistral.ai/v1");
	case EAgentZetProvider::xAI:
		return TEXT("https://api.x.ai/v1");
	case EAgentZetProvider::OpenRouter:
		return TEXT("https://openrouter.ai/api/v1");
	case EAgentZetProvider::Ollama:
	{
		// Roo Code uses Ollama's native SDK (/api/chat), but we use the OpenAI-compatible
		// endpoint (/v1/chat/completions). The OpenAICompatClient appends /chat/completions,
		// so the base URL MUST end with /v1.
		//
		// Users commonly set "http://localhost:11434" without /v1 → 404.
		// Always normalize: strip trailing slash, then ensure /v1 suffix.
		FString Base = OllamaBaseUrl.IsEmpty() ? TEXT("http://localhost:11434") : OllamaBaseUrl;
		while (Base.EndsWith(TEXT("/"))) Base.RemoveAt(Base.Len() - 1);
		if (!Base.EndsWith(TEXT("/v1")))
		{
			Base += TEXT("/v1");
		}
		return Base;
	}
	case EAgentZetProvider::LMStudio:
	{
		// Roo Code lm-studio.ts: baseURL = (baseUrl || "http://localhost:1234") + "/v1"
		// Same pattern: always normalize to include /v1.
		FString Base = LMStudioBaseUrl.IsEmpty() ? TEXT("http://localhost:1234") : LMStudioBaseUrl;
		while (Base.EndsWith(TEXT("/"))) Base.RemoveAt(Base.Len() - 1);
		if (!Base.EndsWith(TEXT("/v1")))
		{
			Base += TEXT("/v1");
		}
		return Base;
	}
	case EAgentZetProvider::Custom:
		return CustomBaseUrl;
	default:
		return DefaultApiEndpoint;
	}
}

FString UAgentZetDeveloperSettings::GetActiveApiKey() const
{
	switch (ActiveProvider)
	{
	case EAgentZetProvider::Anthropic:  return ApiKey;
	case EAgentZetProvider::OpenAI:     return OpenAiApiKey;
	case EAgentZetProvider::Azure:      return AzureApiKey;   // 'api-key' header, not 'Authorization: Bearer'
	case EAgentZetProvider::Google:     return GeminiApiKey;
	case EAgentZetProvider::DeepSeek:   return DeepSeekApiKey;
	case EAgentZetProvider::Mistral:    return MistralApiKey;
	case EAgentZetProvider::xAI:        return xAIApiKey;
	case EAgentZetProvider::OpenRouter: return OpenRouterApiKey;
	case EAgentZetProvider::Ollama:     return TEXT("");   // no auth for local
	case EAgentZetProvider::LMStudio:   return TEXT("");   // no auth for local
	case EAgentZetProvider::Custom:     return CustomApiKey;
	default: return ApiKey;
	}
}

FString UAgentZetDeveloperSettings::GetEffectiveModel() const
{
	switch (ActiveProvider)
	{
	case EAgentZetProvider::Anthropic:
	{
		if (ClaudeModel == EAgentZetClaudeModel::Custom)
			return CustomModelId.IsEmpty() ? ModelEnumToApiString(EAgentZetClaudeModel::Sonnet_4_5) : CustomModelId;
		return ModelEnumToApiString(ClaudeModel);
	}
	case EAgentZetProvider::OpenAI:
		return OpenAiModelId.IsEmpty() ? TEXT("gpt-5.1-codex-max") : OpenAiModelId;
	case EAgentZetProvider::Azure:
		// For Azure, the "model" is the deployment name, NOT the base model ID.
		// Returning the deployment name is what the API expects in the URL and body.
		return AzureDeploymentName;
	case EAgentZetProvider::Google:
		return GeminiModelId.IsEmpty() ? TEXT("gemini-3.1-pro-preview") : GeminiModelId;
	case EAgentZetProvider::DeepSeek:
		return DeepSeekModelId.IsEmpty() ? TEXT("deepseek-v4-flash") : DeepSeekModelId;
	case EAgentZetProvider::Mistral:
		return MistralModelId.IsEmpty() ? TEXT("codestral-latest") : MistralModelId;
	case EAgentZetProvider::xAI:
		return xAIModelId.IsEmpty() ? TEXT("grok-code-fast-1") : xAIModelId;
	case EAgentZetProvider::OpenRouter:
		return OpenRouterModelId.IsEmpty() ? TEXT("anthropic/claude-sonnet-4.5") : OpenRouterModelId;
	case EAgentZetProvider::Ollama:
		return OllamaModelId.IsEmpty() ? TEXT("devstral:24b") : OllamaModelId;
	case EAgentZetProvider::LMStudio:
		return LMStudioModelId.IsEmpty() ? TEXT("mistralai/devstral-small-2505") : LMStudioModelId;
	case EAgentZetProvider::Custom:
		return CustomEndpointModelId;
	case EAgentZetProvider::GitHubCopilot:
		return CopilotModelId.IsEmpty() ? TEXT("gpt-4") : CopilotModelId;
	default:
		return ModelEnumToApiString(EAgentZetClaudeModel::Sonnet_4_5);
	}
}

bool UAgentZetDeveloperSettings::IsActiveProviderApiKeySet() const
{
	FString Key = GetActiveApiKey();
	// Local providers and Copilot don't need an API key configured here
	if (ActiveProvider == EAgentZetProvider::Ollama || 
		ActiveProvider == EAgentZetProvider::LMStudio || 
		ActiveProvider == EAgentZetProvider::GitHubCopilot)
	{
		return true;
	}
	// Azure: also requires AzureBaseUrl and AzureDeploymentName to be usable
	if (ActiveProvider == EAgentZetProvider::Azure)
		return !Key.IsEmpty() && Key.Len() > 10 && !AzureBaseUrl.IsEmpty() && !AzureDeploymentName.IsEmpty();
	return !Key.IsEmpty() && Key.Len() > 10;
}

FString UAgentZetDeveloperSettings::GetModelDisplayName() const
{
	// Return a provider-qualified display name for all providers, not just Anthropic.
	const FString ModelId = GetEffectiveModel();
	switch (ActiveProvider)
	{
	case EAgentZetProvider::Anthropic:
	{
		switch (ClaudeModel)
		{
		case EAgentZetClaudeModel::Sonnet_4_6:	return TEXT("Claude Sonnet 4.6");
		case EAgentZetClaudeModel::Sonnet_4_5:	return TEXT("Claude Sonnet 4.5");
		case EAgentZetClaudeModel::Opus_4_6:	return TEXT("Claude Opus 4.6");
		case EAgentZetClaudeModel::Opus_4_5:	return TEXT("Claude Opus 4.5");
		case EAgentZetClaudeModel::Haiku_4:	return TEXT("Claude Haiku 4");
		case EAgentZetClaudeModel::Custom:		return FString::Printf(TEXT("Custom (%s)"), *CustomModelId);
		default: return ModelId;
		}
	}
	case EAgentZetProvider::OpenAI:
		return FString::Printf(TEXT("OpenAI: %s"), *ModelId);
	case EAgentZetProvider::Azure:
		// Show deployment name with Azure label so users know they're in Azure mode
		return ModelId.IsEmpty()
			? TEXT("Azure OpenAI (no deployment set)")
			: FString::Printf(TEXT("Azure: %s"), *ModelId);
	case EAgentZetProvider::Google:
		return FString::Printf(TEXT("Gemini: %s"), *ModelId);
	case EAgentZetProvider::DeepSeek:
		return FString::Printf(TEXT("DeepSeek: %s"), *ModelId);
	case EAgentZetProvider::Mistral:
		return FString::Printf(TEXT("Mistral: %s"), *ModelId);
	case EAgentZetProvider::xAI:
		return FString::Printf(TEXT("xAI: %s"), *ModelId);
	case EAgentZetProvider::OpenRouter:
		return FString::Printf(TEXT("OpenRouter: %s"), *ModelId);
	case EAgentZetProvider::Ollama:
		return FString::Printf(TEXT("Ollama (local): %s"), *ModelId);
	case EAgentZetProvider::LMStudio:
		return FString::Printf(TEXT("LM Studio (local): %s"), *ModelId);
	case EAgentZetProvider::Custom:
		return FString::Printf(TEXT("Custom: %s"), *ModelId);
	default:
		return ModelId;
	}
}

FString UAgentZetDeveloperSettings::ModelEnumToApiString(EAgentZetClaudeModel Model)
{
	switch (Model)
	{
	case EAgentZetClaudeModel::Sonnet_4_6:	return TEXT("claude-sonnet-4-6");
	case EAgentZetClaudeModel::Sonnet_4_5:	return TEXT("claude-sonnet-4-5");
	case EAgentZetClaudeModel::Opus_4_8:	return TEXT("claude-opus-4-8");
	case EAgentZetClaudeModel::Opus_4_7:	return TEXT("claude-opus-4-7");
	case EAgentZetClaudeModel::Opus_4_6:	return TEXT("claude-opus-4-6");
	case EAgentZetClaudeModel::Opus_4_5:	return TEXT("claude-opus-4-5-20251101");
	case EAgentZetClaudeModel::Haiku_4:	return TEXT("claude-haiku-4-5-20251001");
	default: return TEXT("claude-sonnet-4-6");
	}
}

bool UAgentZetDeveloperSettings::IsToolCategoryAllowed(EAgentZetActionCategory Category) const
{
	switch (SecurityMode)
	{
	case EAgentZetSecurityMode::Sandbox:
		switch (Category)
		{
		case EAgentZetActionCategory::Cpp:
		case EAgentZetActionCategory::Build:
		case EAgentZetActionCategory::Settings:
		case EAgentZetActionCategory::SourceControl:
		case EAgentZetActionCategory::FileSystem:
			return false;
		default:
			return true;
		}
	case EAgentZetSecurityMode::Advanced:
		switch (Category)
		{
		case EAgentZetActionCategory::Build:
			return false;
		default:
			return true;
		}
	case EAgentZetSecurityMode::Developer:
		return true;
	default:
		return false;
	}
}

bool UAgentZetDeveloperSettings::IsIniSectionAllowed(const FString& Section) const
{
	if (SecurityMode == EAgentZetSecurityMode::Advanced)
	{
		return Section.StartsWith(TEXT("/Script/AgentZet"));
	}
	return true;
}

FName UAgentZetDeveloperSettings::GetContainerName() const
{
	return TEXT("Project");
}

FName UAgentZetDeveloperSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

FName UAgentZetDeveloperSettings::GetSectionName() const
{
	return TEXT("AgentZet");
}

#if WITH_EDITOR
FText UAgentZetDeveloperSettings::GetSectionText() const
{
	return FText::FromString(TEXT("AgentZet AI Assistant"));
}

FText UAgentZetDeveloperSettings::GetSectionDescription() const
{
	return FText::FromString(TEXT("Configure the AgentZet AI assistant plugin -- API keys, model selection, safety settings, context, and tool availability."));
}

void UAgentZetDeveloperSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!PropertyChangedEvent.Property) return;

	FName PropName = PropertyChangedEvent.Property->GetFName();

	// ====================================================================
	// CRITICAL (Gemini): Extended Thinking token constraint validation
	// Anthropic API requires: budget_tokens >= 1024, max_tokens > budget_tokens
	// ====================================================================
	if (PropName == GET_MEMBER_NAME_CHECKED(UAgentZetDeveloperSettings, ThinkingBudgetTokens))
	{
		ThinkingBudgetTokens = FMath::Max(1024, ThinkingBudgetTokens);

		if (bEnableExtendedThinking && MaxResponseTokens <= ThinkingBudgetTokens)
		{
			MaxResponseTokens = ThinkingBudgetTokens + 1024;
			UE_LOG(LogAgentZet, Warning,
				TEXT("AgentZet: MaxResponseTokens auto-adjusted to %d (must be > ThinkingBudgetTokens %d)"),
				MaxResponseTokens, ThinkingBudgetTokens);
		}
	}
	else if (PropName == GET_MEMBER_NAME_CHECKED(UAgentZetDeveloperSettings, MaxResponseTokens))
	{
		if (bEnableExtendedThinking && MaxResponseTokens <= ThinkingBudgetTokens)
		{
			MaxResponseTokens = ThinkingBudgetTokens + 1024;
			UE_LOG(LogAgentZet, Warning,
				TEXT("AgentZet: MaxResponseTokens clamped to %d (must be > ThinkingBudgetTokens %d)"),
				MaxResponseTokens, ThinkingBudgetTokens);
		}
	}
	else if (PropName == GET_MEMBER_NAME_CHECKED(UAgentZetDeveloperSettings, bEnableExtendedThinking))
	{
		if (bEnableExtendedThinking)
		{
			ThinkingBudgetTokens = FMath::Max(1024, ThinkingBudgetTokens);
			if (MaxResponseTokens <= ThinkingBudgetTokens)
			{
				MaxResponseTokens = ThinkingBudgetTokens + 1024;
			}
		}
	}

	// ====================================================================
	// CRITICAL (ChatGPT): Developer mode escalation confirmation dialog
	// Switching to Developer mode requires explicit user confirmation.
	// ====================================================================
	if (PropName == GET_MEMBER_NAME_CHECKED(UAgentZetDeveloperSettings, SecurityMode))
	{
		if (SecurityMode == EAgentZetSecurityMode::Developer)
		{
			FText Title = FText::FromString(TEXT("Enable Developer Mode?"));
			FText Message = FText::FromString(
				TEXT("WARNING: Developer Mode allows:\n\n")
				TEXT("- C++ file writes and compilation\n")
				TEXT("- Full INI/config modification\n")
				TEXT("- External process execution (UAT builds)\n")
				TEXT("- Source control operations\n")
				TEXT("- Project-wide mutation\n\n")
				TEXT("This gives the AI full power over your project.\n")
				TEXT("Only enable if you understand the risks.\n\n")
				TEXT("Continue?")
			);

			EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNo, Message, Title);

			if (Result != EAppReturnType::Yes)
			{
				// User declined -- revert to Advanced
				SecurityMode = EAgentZetSecurityMode::Advanced;
				UE_LOG(LogAgentZet, Log, TEXT("AgentZet: Developer mode switch declined. Reverting to Advanced."));
			}
		}
	}

	OnSettingsChanged.Broadcast(PropName);
}
#endif

// ============================================================================
// Model dropdown options (UFUNCTION used by UPROPERTY GetOptions meta)
// Model lists sourced from Roo Code packages/types/src/providers/*.ts
// ============================================================================

// Complete model lists sourced from Roo Code packages/types/src/providers/*.ts
// Every model ID matches exactly what Roo Code defines in its type files.

TArray<FString> UAgentZetDeveloperSettings::GetOpenAIModelOptions() const
{
	// Source: Roo-Code-main/packages/types/src/providers/openai.ts (openAiNativeModels)
	return {
		// GPT-5.x flagship series
		TEXT("gpt-5.4"),
		TEXT("gpt-5.3-codex"),
		TEXT("gpt-5.3-chat-latest"),
		TEXT("gpt-5.2"),
		TEXT("gpt-5.2-codex"),
		TEXT("gpt-5.2-chat-latest"),
		TEXT("gpt-5.1-codex-max"),
		TEXT("gpt-5.1"),
		TEXT("gpt-5.1-codex"),
		TEXT("gpt-5.1-codex-mini"),
		TEXT("gpt-5"),
		TEXT("gpt-5-mini"),
		TEXT("gpt-5-codex"),
		TEXT("gpt-5-nano"),
		TEXT("gpt-5-chat-latest"),
		// GPT-4.x series
		TEXT("gpt-4.1"),
		TEXT("gpt-4.1-mini"),
		TEXT("gpt-4.1-nano"),
		TEXT("gpt-4o"),
		TEXT("gpt-4o-mini"),
		// o-series (reasoning models)
		TEXT("o3"),
		TEXT("o3-high"),
		TEXT("o3-low"),
		TEXT("o4-mini"),
		TEXT("o4-mini-high"),
		TEXT("o4-mini-low"),
		TEXT("o3-mini"),
		TEXT("o3-mini-high"),
		TEXT("o3-mini-low"),
		TEXT("o1"),
		TEXT("o1-preview"),
		TEXT("o1-mini"),
		// Codex agent
		TEXT("codex-mini-latest"),
		// Dated snapshots (backward compatibility)
		TEXT("gpt-5-2025-08-07"),
		TEXT("gpt-5-mini-2025-08-07"),
		TEXT("gpt-5-nano-2025-08-07"),
	};
}

TArray<FString> UAgentZetDeveloperSettings::GetGeminiModelOptions() const
{
	// Source: Roo-Code-main/packages/types/src/providers/gemini.ts (geminiModels)
	return {
		// Gemini 3.x (reasoning effort models)
		TEXT("gemini-3.1-pro-preview"),
		TEXT("gemini-3.1-pro-preview-customtools"),
		TEXT("gemini-3-pro-preview"),
		TEXT("gemini-3-flash-preview"),
		// Gemini 2.5 Pro (thinking budget models)
		TEXT("gemini-2.5-pro"),
		TEXT("gemini-2.5-pro-preview-06-05"),
		TEXT("gemini-2.5-pro-preview-05-06"),
		TEXT("gemini-2.5-pro-preview-03-25"),
		// Gemini 2.5 Flash
		TEXT("gemini-flash-latest"),
		TEXT("gemini-2.5-flash-preview-09-2025"),
		TEXT("gemini-2.5-flash"),
		// Gemini 2.5 Flash Lite
		TEXT("gemini-flash-lite-latest"),
		TEXT("gemini-2.5-flash-lite-preview-09-2025"),
	};
}

TArray<FString> UAgentZetDeveloperSettings::GetDeepSeekModelOptions() const
{
	// Source: Zoo-Code-main/packages/types/src/providers/deepseek.ts (deepSeekModels)
	// V4 models are the primary offering; legacy aliases kept for compatibility.
	return {
		TEXT("deepseek-v4-flash"),
		TEXT("deepseek-v4-pro"),
		TEXT("deepseek-chat"),
		TEXT("deepseek-reasoner"),
	};
}

TArray<FString> UAgentZetDeveloperSettings::GetMistralModelOptions() const
{
	// Source: Roo-Code-main/packages/types/src/providers/mistral.ts (mistralModels)
	return {
		TEXT("magistral-medium-latest"),
		TEXT("devstral-medium-latest"),
		TEXT("mistral-medium-latest"),
		TEXT("codestral-latest"),
		TEXT("mistral-large-latest"),
		TEXT("ministral-8b-latest"),
		TEXT("ministral-3b-latest"),
		TEXT("mistral-small-latest"),
		TEXT("pixtral-large-latest"),
	};
}

TArray<FString> UAgentZetDeveloperSettings::GetxAIModelOptions() const
{
	// Source: Roo-Code-main/packages/types/src/providers/xai.ts (xaiModels)
	return {
		TEXT("grok-code-fast-1"),
		TEXT("grok-4-1-fast-reasoning"),
		TEXT("grok-4-1-fast-non-reasoning"),
		TEXT("grok-4-fast-reasoning"),
		TEXT("grok-4-fast-non-reasoning"),
		TEXT("grok-4-0709"),
		TEXT("grok-3-mini"),
		TEXT("grok-3"),
	};
}

TArray<FString> UAgentZetDeveloperSettings::GetCopilotModelOptions() const
{
	// Return dynamically cached models if available
	if (CopilotAvailableModels.Num() > 0)
	{
		return CopilotAvailableModels;
	}

	// Fallback to these mapped standard proxy IDs via GitHub's API if not yet cached.
	return {
		TEXT("gpt-4o"),
		TEXT("gpt-4"),
		TEXT("claude-3.5-sonnet"),
		TEXT("claude-3.7-sonnet"),
		TEXT("gemini-2.1-pro"),
		TEXT("o1"),
		TEXT("o1-preview"),
		TEXT("o1-mini"),
		TEXT("o3-mini")
	};
}
