// Copyright AgentZet. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentZetInterfaces.h"

/**
 * Context-as-tools executor.
 * 
 * Instead of front-loading the entire project context into the system prompt
 * (which can blow past token limits on large projects), this executor provides
 * on-demand tools that Claude calls to explore the project:
 * 
 * - list_directory: List files/folders in a project-relative path
 * - search_assets: Search the asset registry by class, name, or path
 * - read_file_snippet: Read a section of a source file (with line range)
 * 
 * These tools are always available regardless of security mode (read-only).
 */
class AGENTZETACTIONS_API FAgentZetContextActions : public IAgentZetActionExecutor
{
public:
    FAgentZetContextActions();
    virtual ~FAgentZetContextActions();

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
    FAgentZetActionResult ExecuteListDirectory(const TSharedRef<FJsonObject>& Params, FAgentZetActionResult& Result);
    FAgentZetActionResult ExecuteSearchAssets(const TSharedRef<FJsonObject>& Params, FAgentZetActionResult& Result);
    FAgentZetActionResult ExecuteReadFileSnippet(const TSharedRef<FJsonObject>& Params, FAgentZetActionResult& Result);

    /** Validate that a path doesn't escape the project directory */
    bool IsPathSafe(const FString& RelativePath) const;
};
