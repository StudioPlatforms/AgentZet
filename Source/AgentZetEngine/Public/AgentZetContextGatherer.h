// Copyright AgentZet. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentZetInterfaces.h"
#include "AgentZetProjectContext.h"

class AGENTZETENGINE_API FAgentZetContextGatherer : public IAgentZetContextGatherer
{
public:
	FAgentZetContextGatherer();
	virtual ~FAgentZetContextGatherer();

	virtual FString BuildContextString() override;
	virtual FString GetFileTreeString() override;
	virtual FString GetAssetSummaryString() override;
	virtual FString GetSettingsSnapshotString() override;
	virtual FString GetClassHierarchyString() override;
	virtual int32 EstimateTokenCount() const override;

	/** Build a full project context object */
	FAgentZetProjectContext BuildProjectContext();

private:
	FAgentZetProjectContext CachedContext;
	FDateTime LastCaptureTime;
	static const int32 CacheValiditySeconds = 30;
};
