// Copyright AgentZet. All Rights Reserved.

#include "AgentZetSSEParser.h"
#include "AgentZetCoreModule.h"
#include "Serialization/JsonSerializer.h"

FAgentZetSSEParser::FAgentZetSSEParser()
{
}

FAgentZetSSEParser::~FAgentZetSSEParser()
{
}

void FAgentZetSSEParser::Reset()
{
	LineBuffer.Empty();
	CurrentEventType.Empty();
	CurrentData.Empty();
}

void FAgentZetSSEParser::ProcessChunk(const FString& RawData, TArray<FAgentZetSSEEvent>& OutEvents)
{
	// Append to existing buffer
	LineBuffer += RawData;

	// Process complete lines
	FString RemainingBuffer;
	FString Line;

	while (LineBuffer.Split(TEXT("\n"), &Line, &RemainingBuffer))
	{
		Line.TrimEndInline();
		ProcessLine(Line, OutEvents);
		LineBuffer = RemainingBuffer;
	}
}

void FAgentZetSSEParser::ProcessLine(const FString& Line, TArray<FAgentZetSSEEvent>& OutEvents)
{
	if (Line.IsEmpty())
	{
		// Empty line = end of event, flush
		FlushCurrentEvent(OutEvents);
		return;
	}

	if (Line.StartsWith(TEXT("event:")))
	{
		CurrentEventType = Line.Mid(6).TrimStartAndEnd();
	}
	else if (Line.StartsWith(TEXT("data:")))
	{
		FString DataValue = Line.Mid(5).TrimStartAndEnd();
		if (!CurrentData.IsEmpty())
		{
			CurrentData += TEXT("\n");
		}
		CurrentData += DataValue;
	}
	else if (Line.StartsWith(TEXT(":")))
	{
		// Comment line, ignore (SSE spec)
	}
}

void FAgentZetSSEParser::FlushCurrentEvent(TArray<FAgentZetSSEEvent>& OutEvents)
{
	if (CurrentData.IsEmpty() && CurrentEventType.IsEmpty())
	{
		return;
	}

	FAgentZetSSEEvent Event = ParseEvent(CurrentEventType, CurrentData);
	OutEvents.Add(Event);

	CurrentEventType.Empty();
	CurrentData.Empty();
}

FAgentZetSSEEvent FAgentZetSSEParser::ParseEvent(const FString& EventType, const FString& DataPayload)
{
	FAgentZetSSEEvent Event;
	Event.Type = StringToEventType(EventType);
	Event.RawData = DataPayload;

	// Try to parse JSON
	if (!DataPayload.IsEmpty())
	{
		TSharedPtr<FJsonObject> JsonObj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(DataPayload);
		if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
		{
			Event.JsonData = JsonObj;

			// Extract content block index if present
			if (JsonObj->HasField(TEXT("index")))
			{
				Event.ContentBlockIndex = JsonObj->GetIntegerField(TEXT("index"));
			}
		}
	}

	return Event;
}

EAgentZetSSEEventType FAgentZetSSEParser::StringToEventType(const FString& EventTypeString)
{
	if (EventTypeString == TEXT("message_start")) return EAgentZetSSEEventType::MessageStart;
	if (EventTypeString == TEXT("content_block_start")) return EAgentZetSSEEventType::ContentBlockStart;
	if (EventTypeString == TEXT("content_block_delta")) return EAgentZetSSEEventType::ContentBlockDelta;
	if (EventTypeString == TEXT("content_block_stop")) return EAgentZetSSEEventType::ContentBlockStop;
	if (EventTypeString == TEXT("message_delta")) return EAgentZetSSEEventType::MessageDelta;
	if (EventTypeString == TEXT("message_stop")) return EAgentZetSSEEventType::MessageStop;
	if (EventTypeString == TEXT("ping")) return EAgentZetSSEEventType::Ping;
	if (EventTypeString == TEXT("error")) return EAgentZetSSEEventType::Error;
	return EAgentZetSSEEventType::Unknown;
}
