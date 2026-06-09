// Copyright AgentZet. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentZetInterfaces.h"

/**
 * FAgentZetSequencerActions
 *
 * Provides tools for creating and editing Level Sequences (cinematics/timelines):
 *   - create_level_sequence: Create a LevelSequence asset and optionally spawn it
 *   - add_sequencer_track: Add Actor, Camera, Transform, or Audio tracks
 *   - add_sequencer_keyframe: Animate properties over time (transforms, floats, etc.)
 *
 * Level Sequences are used for cutscenes, camera animations, gameplay timelines,
 * and any property animation that needs precise timing control.
 */
class AGENTZETACTIONS_API FAgentZetSequencerActions : public IAgentZetActionExecutor
{
public:
	FAgentZetSequencerActions();
	virtual ~FAgentZetSequencerActions();

	virtual FName GetActionName() const override;
	virtual FText GetDisplayName() const override;
	virtual EAgentZetActionCategory GetCategory() const override;
	virtual EAgentZetRiskLevel GetDefaultRiskLevel() const override;
	virtual FAgentZetActionPlan PreviewAction(const TSharedRef<FJsonObject>& Params) override;
	virtual FAgentZetActionResult ExecuteAction(const TSharedRef<FJsonObject>& Params) override;
	virtual bool CanUndo() const override;
	virtual bool UndoAction() override;
	virtual TArray<FString> GetSupportedToolNames() const override;
	virtual bool ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const override;

private:
	/** Create a new LevelSequence asset. */
	FAgentZetActionResult ExecuteCreateLevelSequence(const TSharedRef<FJsonObject>& Params, FAgentZetActionResult& Result);

	/** Add a track to an existing LevelSequence (Actor binding, Camera cut, Audio, etc.). */
	FAgentZetActionResult ExecuteAddSequencerTrack(const TSharedRef<FJsonObject>& Params, FAgentZetActionResult& Result);

	/** Add a keyframe to a track property at a specific time. */
	FAgentZetActionResult ExecuteAddSequencerKeyframe(const TSharedRef<FJsonObject>& Params, FAgentZetActionResult& Result);
};
