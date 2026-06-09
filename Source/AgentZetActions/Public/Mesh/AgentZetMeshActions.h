// Copyright AgentZet. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentZetInterfaces.h"

class AGENTZETACTIONS_API FAgentZetMeshActions : public IAgentZetActionExecutor
{
public:
    FAgentZetMeshActions();
    virtual ~FAgentZetMeshActions();

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
};
