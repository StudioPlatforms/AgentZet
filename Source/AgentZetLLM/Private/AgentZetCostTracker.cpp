// Copyright AgentZet. All Rights Reserved.

#include "AgentZetCostTracker.h"
#include "AgentZetCoreModule.h"

FAgentZetCostTracker::FAgentZetCostTracker() {}

// ============================================================================
// Claude Model Pricing (USD per 1M tokens, as of 2025)
// Based on Roo Code's cost.ts + Anthropic pricing page
// ============================================================================

FAgentZetModelPricing FAgentZetCostTracker::GetModelPricing(EAgentZetClaudeModel Model)
{
	FAgentZetModelPricing Pricing;

	switch (Model)
	{
	case EAgentZetClaudeModel::Sonnet_4_6:
		// Claude Sonnet 4.6 / Claude Sonnet 4 pricing
		Pricing.InputPricePerMillion = 3.0f;
		Pricing.OutputPricePerMillion = 15.0f;
		Pricing.CacheWritePricePerMillion = 3.75f;   // 1.25x input
		Pricing.CacheReadPricePerMillion = 0.30f;    // 0.1x input
		break;

	case EAgentZetClaudeModel::Sonnet_4_5:
		// Claude Sonnet 4.5 pricing
		Pricing.InputPricePerMillion = 3.0f;
		Pricing.OutputPricePerMillion = 15.0f;
		Pricing.CacheWritePricePerMillion = 3.75f;
		Pricing.CacheReadPricePerMillion = 0.30f;
		break;

	case EAgentZetClaudeModel::Opus_4_6:
		// Claude Opus 4.6 / Claude Opus 4 pricing
		Pricing.InputPricePerMillion = 15.0f;
		Pricing.OutputPricePerMillion = 75.0f;
		Pricing.CacheWritePricePerMillion = 18.75f;  // 1.25x input
		Pricing.CacheReadPricePerMillion = 1.50f;    // 0.1x input
		break;

	case EAgentZetClaudeModel::Opus_4_5:
		// Claude Opus 4.5 pricing
		Pricing.InputPricePerMillion = 15.0f;
		Pricing.OutputPricePerMillion = 75.0f;
		Pricing.CacheWritePricePerMillion = 18.75f;
		Pricing.CacheReadPricePerMillion = 1.50f;
		break;

	case EAgentZetClaudeModel::Haiku_4:
		// Claude Haiku 4 pricing (fastest, cheapest)
		Pricing.InputPricePerMillion = 0.80f;
		Pricing.OutputPricePerMillion = 4.0f;
		Pricing.CacheWritePricePerMillion = 1.0f;
		Pricing.CacheReadPricePerMillion = 0.08f;
		break;

	case EAgentZetClaudeModel::Custom:
	default:
		// Use Sonnet 4.6 as fallback estimate
		Pricing.InputPricePerMillion = 3.0f;
		Pricing.OutputPricePerMillion = 15.0f;
		Pricing.CacheWritePricePerMillion = 3.75f;
		Pricing.CacheReadPricePerMillion = 0.30f;
		break;
	}

	return Pricing;
}

FAgentZetRequestCost FAgentZetCostTracker::CalculateRequestCost(
	EAgentZetClaudeModel Model,
	const FAgentZetTokenUsage& Usage)
{
	FAgentZetModelPricing Pricing = GetModelPricing(Model);
	FAgentZetRequestCost Cost;

	// Anthropic billing model: InputTokens does NOT include cached tokens
	// Total input = base input + cache creation + cache reads
	// (matches Roo Code's calculateApiCostAnthropic)
	int32 BaseInputTokens = Usage.InputTokens;
	int32 CacheCreationTokens = Usage.CacheCreationInputTokens;
	int32 CacheReadTokens = Usage.CacheReadInputTokens;

	// Cost = (tokens / 1,000,000) * price_per_million
	Cost.InputCost = (BaseInputTokens / 1000000.0f) * Pricing.InputPricePerMillion;
	Cost.OutputCost = (Usage.OutputTokens / 1000000.0f) * Pricing.OutputPricePerMillion;
	Cost.CacheWriteCost = (CacheCreationTokens / 1000000.0f) * Pricing.CacheWritePricePerMillion;
	Cost.CacheReadCost = (CacheReadTokens / 1000000.0f) * Pricing.CacheReadPricePerMillion;

	Cost.TotalCost = Cost.InputCost + Cost.OutputCost + Cost.CacheWriteCost + Cost.CacheReadCost;

	return Cost;
}

FAgentZetRequestCost FAgentZetCostTracker::CalculateCost(
	EAgentZetProvider Provider,
	const FString& ModelSlug,
	const FAgentZetTokenUsage& Usage)
{
	// Convert slug to simple enum logic to use existing mechanism
	EAgentZetClaudeModel Model = EAgentZetClaudeModel::Custom;

	if (Provider == EAgentZetProvider::Anthropic)
	{
		if (ModelSlug.Contains("claude-3-7-sonnet")) Model = EAgentZetClaudeModel::Sonnet_4_6; // Mapping logic...
		else if (ModelSlug.Contains("claude-3-5-sonnet")) Model = EAgentZetClaudeModel::Sonnet_4_5;
		else if (ModelSlug.Contains("claude-3-opus")) Model = EAgentZetClaudeModel::Opus_4_5; // Approximate map
		else if (ModelSlug.Contains("claude-3-5-haiku")) Model = EAgentZetClaudeModel::Haiku_4; // Approximate map
	}

	return CalculateRequestCost(Model, Usage);
}

FString FAgentZetCostTracker::FormatCost(float Cost)
{
	if (Cost < 0.001f)
	{
		return TEXT("< $0.001");
	}
	else if (Cost < 0.01f)
	{
		return FString::Printf(TEXT("$%.4f"), Cost);
	}
	else if (Cost < 1.0f)
	{
		return FString::Printf(TEXT("$%.3f"), Cost);
	}
	else
	{
		return FString::Printf(TEXT("$%.2f"), Cost);
	}
}

void FAgentZetCostTracker::AddRequestCost(const FAgentZetRequestCost& RequestCost)
{
	SessionTotalCost += RequestCost.TotalCost;
	SessionRequestCount++;
	CostSinceLastReset += RequestCost.TotalCost;
	RequestCountSinceLastReset++;
}

void FAgentZetCostTracker::AddRequestCost(EAgentZetProvider Provider, const FString& ModelSlug, const FAgentZetTokenUsage& Usage)
{
	FAgentZetRequestCost ReqCost = CalculateCost(Provider, ModelSlug, Usage);
	AddRequestCost(ReqCost);
}

void FAgentZetCostTracker::Reset()
{
	SessionTotalCost = 0.0f;
	SessionRequestCount = 0;
	CostSinceLastReset = 0.0f;
	RequestCountSinceLastReset = 0;
}

void FAgentZetCostTracker::ResetTrackingBaseline()
{
	CostSinceLastReset = 0.0f;
	RequestCountSinceLastReset = 0;
}
