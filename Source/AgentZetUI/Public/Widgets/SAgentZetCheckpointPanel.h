// Copyright AgentZet. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "AgentZetCheckpointManager.h"

DECLARE_DELEGATE_OneParam(FOnAgentZetRestoreCheckpoint, const FString& /*CommitHash*/);
DECLARE_DELEGATE_OneParam(FOnAgentZetViewCheckpointDiff, const FString& /*CommitHash*/);

/**
 * Checkpoint timeline panel.
 * Ported from Roo Code's CheckpointRestoreDialog.tsx.
 *
 * Shows a scrollable list of git checkpoints for the current session.
 * Each entry: timestamp, description, modified file count.
 * Buttons: "Restore" and "View Diff".
 */
class AGENTZETUI_API SAgentZetCheckpointPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAgentZetCheckpointPanel) {}
		SLATE_EVENT(FOnAgentZetRestoreCheckpoint, OnRestoreCheckpoint)
		SLATE_EVENT(FOnAgentZetViewCheckpointDiff, OnViewDiff)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Refresh the checkpoint list from the checkpoint manager */
	void RefreshCheckpoints(const TArray<FAgentZetCheckpoint>& Checkpoints);

private:
	TSharedPtr<class SScrollBox> CheckpointList;
	FOnAgentZetRestoreCheckpoint OnRestoreCheckpoint;
	FOnAgentZetViewCheckpointDiff OnViewDiff;

	TSharedRef<SWidget> BuildCheckpointRow(const FAgentZetCheckpoint& Checkpoint);
};
