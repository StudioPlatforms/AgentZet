// Copyright AgentZet. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentZetTypes.h"

/**
 * Append-only execution journal recording every tool action AgentZet performs.
 * Provides audit trail, replay capability, and debugging for AI mutations.
 * 
 * v2.2: PreStateHash/PostStateHash for deterministic state verification.
 * Uses SHA-1 via FSHA1::HashBuffer() (160-bit, sufficient for integrity tracking).
 * Writes to: Saved/AgentZet/ExecutionLog_YYYYMMDD.json
 */
class AGENTZETENGINE_API FAgentZetExecutionJournal
{
public:
	FAgentZetExecutionJournal();
	~FAgentZetExecutionJournal();

	/** Record an action execution to the journal */
	void RecordExecution(const FAgentZetActionExecutionRecord& Record);

	/** Get all records from the current session */
	const TArray<FAgentZetActionExecutionRecord>& GetSessionRecords() const { return SessionRecords; }

	/** Get the number of records in this session */
	int32 GetSessionRecordCount() const { return SessionRecords.Num(); }

	/** Flush the journal to disk */
	void FlushToDisk();

	/** Load historical records from disk */
	bool LoadFromDisk();

	/** Get the journal file path */
	FString GetJournalFilePath() const;

	/** Clear the current session (does NOT delete disk file) */
	void ClearSession();

	/** Build a summary string of the last N actions for AI context */
	FString BuildRecentActionsSummary(int32 Count = 10) const;

	/** Compute SHA-1 hash of a file for pre/post state comparison */
	static FString ComputeFileHash(const FString& FilePath);

	/** Compute SHA-1 hash of a UPackage (asset) by hashing its saved file on disk.
	 *  NOTE: Must be called AFTER the asset has been saved to disk to capture changes. */
	static FString ComputeAssetHash(const FString& AssetPath);

private:
	TArray<FAgentZetActionExecutionRecord> SessionRecords;
	bool bDirty;
	TSharedPtr<FJsonObject> RecordToJson(const FAgentZetActionExecutionRecord& Record) const;
};
