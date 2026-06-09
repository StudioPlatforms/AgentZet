// Copyright AgentZet. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentZetTypes.h"
#include "AgentZetProjectContext.generated.h"

/**
 * Serializable snapshot of the current project state.
 * Sent to the AI as context with every request.
 */
USTRUCT(BlueprintType)
struct AGENTZETCORE_API FAgentZetProjectContext
{
	GENERATED_BODY()

	/** Name of the Unreal project */
	UPROPERTY(BlueprintReadOnly, Category = "AgentZet")
	FString ProjectName;

	/** Engine version string (e.g. "5.5.0") */
	UPROPERTY(BlueprintReadOnly, Category = "AgentZet")
	FString EngineVersion;

	/** Absolute path to the project root */
	UPROPERTY(BlueprintReadOnly, Category = "AgentZet")
	FString ProjectRootPath;

	/** Content directory tree entries */
	UPROPERTY(BlueprintReadOnly, Category = "AgentZet")
	TArray<FAgentZetFileEntry> ContentTree;

	/** Source directory tree entries */
	UPROPERTY(BlueprintReadOnly, Category = "AgentZet")
	TArray<FAgentZetFileEntry> SourceTree;

	/** Config directory tree entries */
	UPROPERTY(BlueprintReadOnly, Category = "AgentZet")
	TArray<FAgentZetFileEntry> ConfigTree;

	/** All registered assets in the project */
	UPROPERTY(BlueprintReadOnly, Category = "AgentZet")
	TArray<FAgentZetAssetEntry> Assets;

	/** Summary counts by asset type */
	UPROPERTY(BlueprintReadOnly, Category = "AgentZet")
	TMap<FString, int32> AssetCountsByClass;

	/** Currently loaded/open level path */
	UPROPERTY(BlueprintReadOnly, Category = "AgentZet")
	FString CurrentLevelPath;

	/** List of modules in the project (from .uproject) */
	UPROPERTY(BlueprintReadOnly, Category = "AgentZet")
	TArray<FString> ProjectModules;

	/** List of enabled plugins */
	UPROPERTY(BlueprintReadOnly, Category = "AgentZet")
	TArray<FString> EnabledPlugins;

	/** Target platform(s) configured */
	UPROPERTY(BlueprintReadOnly, Category = "AgentZet")
	TArray<FString> TargetPlatforms;

	/** Timestamp of when this context was captured */
	UPROPERTY(BlueprintReadOnly, Category = "AgentZet")
	FDateTime CaptureTimestamp;

	FAgentZetProjectContext()
		: CaptureTimestamp(FDateTime::UtcNow())
	{
	}

	/** Serialize the entire context to a compressed string for the AI system prompt */
	FString ToContextString() const;

	/** Estimate the approximate token count of the serialized context */
	int32 EstimateTokenCount() const;
};
