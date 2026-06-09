// Copyright AgentZet. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentZetInterfaces.h"

class AGENTZETACTIONS_API FAgentZetCppActions : public IAgentZetActionExecutor
{
public:
    FAgentZetCppActions();
    virtual ~FAgentZetCppActions();

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
    FAgentZetActionResult ExecuteCreateCppClass(const TSharedRef<FJsonObject>& Params, FAgentZetActionResult& Result);
    FAgentZetActionResult ExecuteModifyCppFile(const TSharedRef<FJsonObject>& Params, FAgentZetActionResult& Result);
    FAgentZetActionResult ExecuteTriggerCompile(FAgentZetActionResult& Result);
    FAgentZetActionResult ExecuteRegenerateProjectFiles(FAgentZetActionResult& Result);

    /** Validate generated code for dangerous patterns */
    bool ValidateCodeSafety(const FString& Code, TArray<FString>& OutViolations) const;

    /** Write a file to disk with backup */
    bool WriteFileWithBackup(const FString& FilePath, const FString& Content);
};
