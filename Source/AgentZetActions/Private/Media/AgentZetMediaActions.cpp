// Copyright AgentZet. All Rights Reserved.

#include "Media/AgentZetMediaActions.h"
#include "AgentZetCoreModule.h"

FAgentZetMediaActions::FAgentZetMediaActions() {}
FAgentZetMediaActions::~FAgentZetMediaActions() {}
FName FAgentZetMediaActions::GetActionName() const { return FName(TEXT("Media")); }
FText FAgentZetMediaActions::GetDisplayName() const { return FText::FromString(TEXT("Media Actions")); }
EAgentZetActionCategory FAgentZetMediaActions::GetCategory() const { return EAgentZetActionCategory::Texture; }
EAgentZetRiskLevel FAgentZetMediaActions::GetDefaultRiskLevel() const { return EAgentZetRiskLevel::Medium; }
FAgentZetActionPlan FAgentZetMediaActions::PreviewAction(const TSharedRef<FJsonObject>& Params) { return FAgentZetActionPlan(); }
FAgentZetActionResult FAgentZetMediaActions::ExecuteAction(const TSharedRef<FJsonObject>& Params) { FAgentZetActionResult R; R.ResultMessage = TEXT("Stub: not yet implemented"); return R; }
bool FAgentZetMediaActions::CanUndo() const { return false; }
bool FAgentZetMediaActions::UndoAction() { return false; }
TArray<FString> FAgentZetMediaActions::GetSupportedToolNames() const { return TArray<FString>(); }
bool FAgentZetMediaActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const { return true; }
