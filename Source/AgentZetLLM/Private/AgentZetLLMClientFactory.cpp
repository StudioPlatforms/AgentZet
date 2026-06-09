// Copyright AgentZet. All Rights Reserved.

#include "AgentZetLLMClientFactory.h"
#include "AgentZetCoreModule.h"
#include "AgentZetSettings.h"
#include "AgentZetClaudeClient.h"
#include "AgentZetOpenAICompatClient.h"
#include "AgentZetCopilotClient.h"
#include "AgentZetGeminiClient.h"
#include "AgentZetModelRegistry.h"

TSharedPtr<IAgentZetLLMClient> FAgentZetLLMClientFactory::CreateClient()
{
	const UAgentZetDeveloperSettings* Settings = UAgentZetDeveloperSettings::Get();
	if (!Settings)
	{
		UE_LOG(LogAgentZet, Error, TEXT("LLMClientFactory: Could not load UAgentZetDeveloperSettings."));
		return nullptr;
	}
	return CreateClientForProvider(Settings->ActiveProvider, Settings);
}

TSharedPtr<IAgentZetLLMClient> FAgentZetLLMClientFactory::CreateClientForProvider(
	EAgentZetProvider Provider,
	const UAgentZetDeveloperSettings* Settings)
{
	if (!Settings)
	{
		UE_LOG(LogAgentZet, Error, TEXT("LLMClientFactory: Null settings passed to CreateClientForProvider."));
		return nullptr;
	}

	FString ModelId = Settings->GetEffectiveModel();
	FString Endpoint = Settings->GetEffectiveEndpoint();
	FString ApiKey = Settings->GetActiveApiKey();

	UE_LOG(LogAgentZet, Log, TEXT("LLMClientFactory: Creating client for provider=%d model=%s"),
		(int32)Provider, *ModelId);

	// -------------------------------------------------------------------------
	// Anthropic (Claude) — uses AgentZetClaudeClient (Anthropic Messages API)
	// -------------------------------------------------------------------------
	if (Provider == EAgentZetProvider::Anthropic)
	{
		TSharedPtr<FAgentZetClaudeClient> Client = MakeShared<FAgentZetClaudeClient>();
		Client->SetEndpoint(Endpoint);
		Client->SetApiKey(ApiKey);
		Client->SetModel(ModelId);
		Client->SetMaxTokens(Settings->MaxResponseTokens);
		return Client;
	}

	// -------------------------------------------------------------------------
	// Google Gemini — uses AgentZetGeminiClient (Google Generative Language API)
	// -------------------------------------------------------------------------
	if (Provider == EAgentZetProvider::Google)
	{
		TSharedPtr<FAgentZetGeminiClient> Client = MakeShared<FAgentZetGeminiClient>();
		Client->SetApiKey(ApiKey);
		Client->SetModel(ModelId);
		Client->SetBaseUrl(Settings->GeminiBaseUrl);
		Client->SetMaxTokens(Settings->MaxResponseTokens);
		Client->SetThinkingBudget(Settings->GeminiThinkingBudgetTokens);
		Client->SetReasoningEffort(Settings->GeminiReasoningEffort);
		return Client;
	}

	// -------------------------------------------------------------------------
	// GitHub Copilot
	// -------------------------------------------------------------------------
	if (Provider == EAgentZetProvider::GitHubCopilot)
	{
		TSharedPtr<FAgentZetCopilotClient> Client = MakeShared<FAgentZetCopilotClient>();
		Client->SetModel(Settings->CopilotModelId);
		Client->SetMaxTokens(Settings->MaxResponseTokens);
		return Client;
	}

	// -------------------------------------------------------------------------
	// All other providers — use AgentZetOpenAICompatClient
	// -------------------------------------------------------------------------
	TSharedPtr<FAgentZetOpenAICompatClient> Client = MakeShared<FAgentZetOpenAICompatClient>();
	Client->SetProvider(Provider);
	Client->SetEndpoint(Endpoint);
	Client->SetApiKey(ApiKey);
	Client->SetModel(ModelId);
	Client->SetMaxTokens(Settings->MaxResponseTokens);

	// =========================================================================
	// FIX: Auto-disable streaming for local providers (Ollama, LM Studio, Custom)
	//
	// Local model servers (Ollama, LiteLLM, LM Studio) do NOT reliably support
	// OpenAI-style SSE streaming for tool calls. Symptoms:
	//   - Requests hang indefinitely
	//   - Partial/malformed JSON tool call arguments
	//   - Raw JSON returned instead of proper SSE events
	//   - "Payload is incomplete" warnings from UE HTTP module
	//
	// The fix: disable streaming for local providers and use synchronous
	// /v1/chat/completions responses instead. The non-streaming code path in
	// HandleRequestComplete parses the full JSON response body directly.
	//
	// Users can override by explicitly enabling streaming in the UI setting.
	// =========================================================================
	const bool bIsLocalProvider = (Provider == EAgentZetProvider::Ollama ||
	                               Provider == EAgentZetProvider::LMStudio);
	if (bIsLocalProvider)
	{
		Client->SetStreamingEnabled(false);
		UE_LOG(LogAgentZet, Log, TEXT("LLMClientFactory: Streaming auto-disabled for local provider %s (non-SSE compatible)."),
			*ModelId);

		// Pass Ollama context size (num_ctx) — defaults to 8192 in settings.
		// Ollama's internal default is only 2048, which is far too small for AgentZet's
		// system prompt + 93 tool schemas + project context.
		// Roo Code native-ollama.ts: passes options.num_ctx when ollamaNumCtx is set.
		if (Settings->OllamaContextSize > 0)
		{
			Client->SetOllamaContextSize(Settings->OllamaContextSize);
			UE_LOG(LogAgentZet, Log, TEXT("LLMClientFactory: Ollama num_ctx = %d"), Settings->OllamaContextSize);
		}
	}

	switch (Provider)
	{
	case EAgentZetProvider::OpenAI:
		Client->SetReasoningEffort(Settings->OpenAiReasoningEffort);
		break;

	case EAgentZetProvider::Azure:
		// Azure OpenAI Service wire format (ported from Roo Code openai.ts AzureOpenAI client):
		//   - Auth:  'api-key' header (NOT 'Authorization: Bearer')
		//   - URL:   https://{resource}.openai.azure.com/openai/deployments/{deployment}/chat/completions?api-version=...
		//   - API:   Chat Completions only (Responses API NOT supported by Azure)
		// The client auto-activates Azure wire format when AzureApiVersion is set OR
		// when the BaseUrl contains ".azure.com" (see IsAzureUrl() in the client).
		Client->SetAzureApiVersion(Settings->AzureApiVersion.IsEmpty() ? TEXT("2024-02-01") : Settings->AzureApiVersion);
		Client->SetReasoningEffort(EAgentZetReasoningEffort::Disabled);  // Azure does not support reasoning_effort
		UE_LOG(LogAgentZet, Log, TEXT("LLMClientFactory: Azure OpenAI — resource=%s deployment=%s api-version=%s"),
			*Settings->AzureBaseUrl, *Settings->AzureDeploymentName,
			*(Settings->AzureApiVersion.IsEmpty() ? TEXT("2024-02-01") : Settings->AzureApiVersion));
		break;

	case EAgentZetProvider::DeepSeek:
	{
		// DeepSeek V4 models (v4-flash, v4-pro) support a thinking toggle:
		//   - When reasoning effort is not Disabled → thinking: enabled + reasoning_effort
		//   - When reasoning effort is Disabled → thinking: disabled (non-thinking mode)
		// Legacy models:
		//   - deepseek-reasoner: always thinking, reasoning_effort supported
		//   - deepseek-chat: never thinking, reasoning_effort ignored
		// The client itself handles the V4 vs legacy branching in BuildChatCompletionsBody()
		EAgentZetReasoningEffort Effort = Settings->DeepSeekReasoningEffort;
		Client->SetReasoningEffort(Effort);
		UE_LOG(LogAgentZet, Log, TEXT("LLMClientFactory: DeepSeek model=%s reasoning_effort=%d"),
			*ModelId, (int32)Effort);
		break;
	}
	case EAgentZetProvider::OpenRouter:
	case EAgentZetProvider::Ollama:
	case EAgentZetProvider::LMStudio:
	case EAgentZetProvider::Mistral:
	case EAgentZetProvider::xAI:
	case EAgentZetProvider::Custom:
	default:
		Client->SetReasoningEffort(EAgentZetReasoningEffort::Disabled);
		break;
	}

	return Client;
}

FString FAgentZetLLMClientFactory::GetActiveProviderDisplayName()
{
	const UAgentZetDeveloperSettings* Settings = UAgentZetDeveloperSettings::Get();
	if (!Settings) return TEXT("Unknown");

	FString ModelId = Settings->GetEffectiveModel();

	switch (Settings->ActiveProvider)
	{
	case EAgentZetProvider::Anthropic:
		return FString::Printf(TEXT("Claude | %s"), *ModelId);
	case EAgentZetProvider::OpenAI:
		return FString::Printf(TEXT("OpenAI | %s"), *ModelId);
	case EAgentZetProvider::Azure:
		// Show deployment name — it's the "model" from Azure's perspective
		return ModelId.IsEmpty()
			? TEXT("Azure OpenAI | (no deployment)")
			: FString::Printf(TEXT("Azure OpenAI | %s"), *ModelId);
	case EAgentZetProvider::Google:
		return FString::Printf(TEXT("Gemini | %s"), *ModelId);
	case EAgentZetProvider::DeepSeek:
		return FString::Printf(TEXT("DeepSeek | %s"), *ModelId);
	case EAgentZetProvider::Mistral:
		return FString::Printf(TEXT("Mistral | %s"), *ModelId);
	case EAgentZetProvider::xAI:
		return FString::Printf(TEXT("xAI | %s"), *ModelId);
	case EAgentZetProvider::OpenRouter:
		return FString::Printf(TEXT("OpenRouter | %s"), *ModelId);
	case EAgentZetProvider::Ollama:
		return FString::Printf(TEXT("Ollama (local) | %s"), *ModelId);
	case EAgentZetProvider::LMStudio:
		return FString::Printf(TEXT("LM Studio (local) | %s"), *ModelId);
	case EAgentZetProvider::Custom:
		return FString::Printf(TEXT("Custom | %s"), *ModelId);
	case EAgentZetProvider::GitHubCopilot:
		return FString::Printf(TEXT("GitHub Copilot | %s"), *ModelId);
	default:
		return ModelId;
	}
}
