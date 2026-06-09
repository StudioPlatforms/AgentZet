// Copyright AgentZet. All Rights Reserved.

#include "AgentZetErrorFeedback.h"

FAgentZetErrorFeedback::FAgentZetErrorFeedback() {}
FAgentZetErrorFeedback::~FAgentZetErrorFeedback() {}

FString FAgentZetErrorFeedback::FormatCompilationErrors(const TArray<FString>& Errors)
{
	FString Result = TEXT("Compilation Errors:\n");
	for (const FString& Error : Errors) { Result += FString::Printf(TEXT("- %s\n"), *Error); }
	return Result;
}

FString FAgentZetErrorFeedback::FormatBuildErrors(const FString& BuildOutput) { return FString::Printf(TEXT("Build Output:\n%s"), *BuildOutput); }
bool FAgentZetErrorFeedback::ShouldRetry(const FGuid& ActionId) const { return GetRetryCount(ActionId) < MaxRetries; }
void FAgentZetErrorFeedback::RecordRetry(const FGuid& ActionId) { RetryCountMap.FindOrAdd(ActionId)++; }
void FAgentZetErrorFeedback::ResetRetries(const FGuid& ActionId) { RetryCountMap.Remove(ActionId); }
int32 FAgentZetErrorFeedback::GetRetryCount(const FGuid& ActionId) const { const int32* Count = RetryCountMap.Find(ActionId); return Count ? *Count : 0; }
