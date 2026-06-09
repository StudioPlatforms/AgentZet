// Copyright AgentZet. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentZetInterfaces.h"

/**
 * FAgentZetBehaviorTreeActions
 *
 * Gameplay AI tools for creating and configuring Behavior Trees:
 *   - create_blackboard: Create Blackboard assets with typed keys
 *   - create_behavior_tree: Create Behavior Tree assets and assign Blackboards
 *   - inject_bt_nodes: Add Selectors, Sequences, Decorators, Services programmatically
 *   - configure_navmesh: Spawn NavMeshBoundsVolumes and trigger NavMesh builds
 *
 * Behavior Trees are Unreal's primary AI decision-making system. Combined with
 * the Blackboard (shared memory), they let the AI create NPC behavior patterns
 * like patrol → investigate → attack → flee.
 */
class AGENTZETACTIONS_API FAgentZetBehaviorTreeActions : public IAgentZetActionExecutor
{
public:
	FAgentZetBehaviorTreeActions();
	virtual ~FAgentZetBehaviorTreeActions();

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
	/** Create a Blackboard asset with typed keys (Bool, Int, Float, String, Vector, Object, Enum, Class). */
	FAgentZetActionResult ExecuteCreateBlackboard(const TSharedRef<FJsonObject>& Params, FAgentZetActionResult& Result);

	/** Create a Behavior Tree asset and optionally assign a Blackboard. */
	FAgentZetActionResult ExecuteCreateBehaviorTree(const TSharedRef<FJsonObject>& Params, FAgentZetActionResult& Result);

	/** Inject nodes into an existing BT (Selectors, Sequences, Tasks, Decorators, Services). */
	FAgentZetActionResult ExecuteInjectBTNodes(const TSharedRef<FJsonObject>& Params, FAgentZetActionResult& Result);

	/** Spawn a NavMeshBoundsVolume, scale to level, and trigger NavMesh rebuild. */
	FAgentZetActionResult ExecuteConfigureNavMesh(const TSharedRef<FJsonObject>& Params, FAgentZetActionResult& Result);
};
