// Copyright AgentZet. All Rights Reserved.

#include "AgentZetUIModule.h"
#include "AgentZetCoreModule.h"
#include "AgentZetStyle.h"
#include "AgentZetCommands.h"
#include "Widgets/SAgentZetMainPanel.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "FAgentZetUIModule"

const FName FAgentZetUIModule::AgentZetTabName(TEXT("AgentZetPanel"));

void FAgentZetUIModule::StartupModule()
{
	FAgentZetStyle::Initialize();
	FAgentZetStyle::ReloadTextures();
	FAgentZetCommands::Register();

	RegisterTabSpawner();
	RegisterMenuExtensions();

	UE_LOG(LogAgentZet, Log, TEXT("AgentZetUI module started."));
}

void FAgentZetUIModule::ShutdownModule()
{
	UnregisterTabSpawner();

	FAgentZetCommands::Unregister();
	FAgentZetStyle::Shutdown();

	UE_LOG(LogAgentZet, Log, TEXT("AgentZetUI module shut down."));
}

FAgentZetUIModule& FAgentZetUIModule::Get()
{
	return FModuleManager::LoadModuleChecked<FAgentZetUIModule>("AgentZetUI");
}

bool FAgentZetUIModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded("AgentZetUI");
}

void FAgentZetUIModule::RegisterTabSpawner()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		AgentZetTabName,
		FOnSpawnTab::CreateRaw(this, &FAgentZetUIModule::OnSpawnAgentZetTab))
		.SetDisplayName(LOCTEXT("AgentZetTabTitle", "AgentZet"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FAgentZetUIModule::UnregisterTabSpawner()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(AgentZetTabName);
}

void FAgentZetUIModule::RegisterMenuExtensions()
{
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		if (Menu)
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddEntry(FToolMenuEntry::InitMenuEntry(
				"OpenAgentZet",
				LOCTEXT("OpenAgentZetLabel", "AgentZet"),
				LOCTEXT("OpenAgentZetTooltip", "Open AgentZet AI Assistant"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([]()
				{
					FGlobalTabmanager::Get()->TryInvokeTab(FName("AgentZetPanel"));
				}))
			));
		}
	}));
}

TSharedRef<SDockTab> FAgentZetUIModule::OnSpawnAgentZetTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SAgentZetMainPanel)
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAgentZetUIModule, AgentZetUI)
