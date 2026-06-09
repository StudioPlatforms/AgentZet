// Copyright AgentZet. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentZetTypes.h"

/**
 * Parser for Server-Sent Events (SSE) from Claude's streaming API.
 * Handles chunked data, partial events, and converts raw SSE lines
 * into typed FAgentZetSSEEvent objects.
 */
class AGENTZETLLM_API FAgentZetSSEParser
{
public:
	FAgentZetSSEParser();
	~FAgentZetSSEParser();

	/** Reset the parser state for a new stream */
	void Reset();

	/**
	 * Feed raw data into the parser.
	 * May produce zero or more events.
	 * @param RawData - Raw bytes from the HTTP response chunk
	 * @param OutEvents - Array of parsed events produced from this chunk
	 */
	void ProcessChunk(const FString& RawData, TArray<FAgentZetSSEEvent>& OutEvents);

	/** Parse a single SSE data line into a typed event */
	static FAgentZetSSEEvent ParseEvent(const FString& EventType, const FString& DataPayload);

	/** Map an event type string to the enum */
	static EAgentZetSSEEventType StringToEventType(const FString& EventTypeString);

private:
	/** Buffer for incomplete SSE lines across chunks */
	FString LineBuffer;

	/** Current event type being built (from "event:" lines) */
	FString CurrentEventType;

	/** Current data being accumulated (from "data:" lines) */
	FString CurrentData;

	/** Process a single complete SSE line */
	void ProcessLine(const FString& Line, TArray<FAgentZetSSEEvent>& OutEvents);

	/** Flush the current event if we have accumulated data */
	void FlushCurrentEvent(TArray<FAgentZetSSEEvent>& OutEvents);
};
