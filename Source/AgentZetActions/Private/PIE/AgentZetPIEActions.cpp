// Copyright AgentZet. All Rights Reserved.

#include "PIE/AgentZetPIEActions.h"
#include "AgentZetCoreModule.h"
#include "AgentZetSettings.h"
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/InputSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/App.h"

#define LOCTEXT_NAMESPACE "AgentZetPIEActions"

// ============================================================================
// Lifecycle
// ============================================================================

FAgentZetPIEActions::FAgentZetPIEActions() {}
FAgentZetPIEActions::~FAgentZetPIEActions() {}

// ============================================================================
// IAgentZetActionExecutor Interface
// ============================================================================

FName FAgentZetPIEActions::GetActionName() const { return FName(TEXT("PIE")); }
FText FAgentZetPIEActions::GetDisplayName() const { return LOCTEXT("DisplayName", "Play-In-Editor Testing"); }
EAgentZetActionCategory FAgentZetPIEActions::GetCategory() const { return EAgentZetActionCategory::General; }
EAgentZetRiskLevel FAgentZetPIEActions::GetDefaultRiskLevel() const { return EAgentZetRiskLevel::High; }
bool FAgentZetPIEActions::CanUndo() const { return false; }
bool FAgentZetPIEActions::UndoAction() { return false; }

TArray<FString> FAgentZetPIEActions::GetSupportedToolNames() const
{
	return {
		TEXT("start_pie_session"),
		TEXT("simulate_input"),
		TEXT("stop_pie_session")
	};
}

bool FAgentZetPIEActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	return true;
}

FAgentZetActionPlan FAgentZetPIEActions::PreviewAction(const TSharedRef<FJsonObject>& Params)
{
	FAgentZetActionPlan Plan;
	FString Action;
	Params->TryGetStringField(TEXT("action"), Action);
	if (Action.IsEmpty()) Params->TryGetStringField(TEXT("tool_name"), Action);

	if (Action == TEXT("start_pie_session"))
		Plan.Summary = TEXT("Launch Play-In-Editor session");
	else if (Action == TEXT("simulate_input"))
		Plan.Summary = TEXT("Simulate keyboard input during PIE");
	else if (Action == TEXT("stop_pie_session"))
		Plan.Summary = TEXT("Stop the current PIE session");
	else
		Plan.Summary = TEXT("PIE operation");

	Plan.MaxRiskLevel = EAgentZetRiskLevel::High;
	FAgentZetAction A;
	A.Description = Plan.Summary;
	A.Category = EAgentZetActionCategory::General;
	A.RiskLevel = EAgentZetRiskLevel::High;
	Plan.Actions.Add(A);
	return Plan;
}

FAgentZetActionResult FAgentZetPIEActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FAgentZetActionResult Result;
	Result.bSuccess = false;

	// Security gate: require Developer mode
	const UAgentZetDeveloperSettings* Settings = UAgentZetDeveloperSettings::Get();
	if (!Settings || Settings->SecurityMode != EAgentZetSecurityMode::Developer)
	{
		Result.Errors.Add(TEXT("PIE automation requires Developer security mode."));
		return Result;
	}

	FString Action;
	if (!Params->TryGetStringField(TEXT("action"), Action) || Action.IsEmpty())
		Params->TryGetStringField(TEXT("tool_name"), Action);

	if (Action == TEXT("start_pie_session"))
		return ExecuteStartPIE(Params, Result);
	else if (Action == TEXT("simulate_input"))
		return ExecuteSimulateInput(Params, Result);
	else if (Action == TEXT("stop_pie_session"))
		return ExecuteStopPIE(Params, Result);

	Result.Errors.Add(TEXT("Unknown PIE action. Use start_pie_session, simulate_input, or stop_pie_session."));
	return Result;
}

// ============================================================================
// start_pie_session
// ============================================================================

FAgentZetActionResult FAgentZetPIEActions::ExecuteStartPIE(
	const TSharedRef<FJsonObject>& Params, FAgentZetActionResult& Result)
{
	if (IsPIERunning())
	{
		Result.Warnings.Add(TEXT("PIE is already running."));
		Result.bSuccess = true;
		Result.ResultMessage = TEXT("PIE session is already active. Use simulate_input to interact, or stop_pie_session to end it.");
		return Result;
	}

	if (!GUnrealEd)
	{
		Result.Errors.Add(TEXT("GUnrealEd is not available."));
		return Result;
	}

	// Configure PIE settings
	FRequestPlaySessionParams PlayParams;

	// Optional: set the start location
	FString StartMode;
	Params->TryGetStringField(TEXT("start_mode"), StartMode);

	if (StartMode.Equals(TEXT("current_camera"), ESearchCase::IgnoreCase))
	{
		PlayParams.EditorPlaySettings = NewObject<ULevelEditorPlaySettings>();
		PlayParams.EditorPlaySettings->LastExecutedPlayModeLocation = PlayLocation_CurrentCameraLocation;
	}

	// Request PIE session
	GUnrealEd->RequestPlaySession(PlayParams);

	UE_LOG(LogAgentZet, Log, TEXT("PIEActions: Requested PIE session start."));

	Result.bSuccess = true;
	Result.ResultMessage = TEXT("PIE session started. The game is now running in the editor. "
		"Use read_message_log to check for runtime errors. "
		"Use simulate_input to inject key presses. "
		"Use stop_pie_session to end the session.");
	return Result;
}

// ============================================================================
// simulate_input
// ============================================================================

FAgentZetActionResult FAgentZetPIEActions::ExecuteSimulateInput(
	const TSharedRef<FJsonObject>& Params, FAgentZetActionResult& Result)
{
	if (!IsPIERunning())
	{
		Result.Errors.Add(TEXT("PIE is not running. Call start_pie_session first."));
		return Result;
	}

	FString KeyName;
	if (!Params->TryGetStringField(TEXT("key"), KeyName))
	{
		Result.Errors.Add(TEXT("Missing required field: 'key' (e.g., 'W', 'SpaceBar', 'LeftMouseButton', 'Gamepad_FaceButton_Bottom')"));
		return Result;
	}

	FString ActionType;
	if (!Params->TryGetStringField(TEXT("action_type"), ActionType))
	{
		ActionType = TEXT("press"); // Default: press and release
	}
	ActionType = ActionType.ToLower();

	float DurationSeconds = 0.0f;
	Params->TryGetNumberField(TEXT("duration"), DurationSeconds);
	DurationSeconds = FMath::Clamp(DurationSeconds, 0.0f, 10.0f);

	// Resolve the FKey
	FKey Key(*KeyName);
	if (!Key.IsValid())
	{
		Result.Errors.Add(FString::Printf(
			TEXT("Invalid key name: '%s'. Use UE key names like 'W', 'A', 'S', 'D', 'SpaceBar', 'LeftShift', "
				 "'LeftMouseButton', 'Gamepad_FaceButton_Bottom', 'Gamepad_LeftStick_Up', etc."),
			*KeyName));
		return Result;
	}

	FString Report;

	if (ActionType == TEXT("press") || ActionType == TEXT("tap"))
	{
		// Simulate press + release
		FSlateApplication& Slate = FSlateApplication::Get();
		FKeyEvent KeyEvent(Key, FModifierKeysState(), 0, false, 0, 0);

		Slate.ProcessKeyDownEvent(KeyEvent);

		if (DurationSeconds > 0.0f)
		{
			// For duration-based press, we'd need async. For now, just hold for one frame.
			// The AI should use multiple simulate_input calls for sustained input.
			Report = FString::Printf(TEXT("Pressed '%s' (note: sustained press for %.1fs requires multiple calls or PIE tick integration). "
				"Key was pressed and released in a single frame."), *KeyName, DurationSeconds);
		}
		else
		{
			Report = FString::Printf(TEXT("Tapped '%s' (press + release in one frame)."), *KeyName);
		}

		Slate.ProcessKeyUpEvent(KeyEvent);
	}
	else if (ActionType == TEXT("down") || ActionType == TEXT("hold"))
	{
		FSlateApplication& Slate = FSlateApplication::Get();
		FKeyEvent KeyEvent(Key, FModifierKeysState(), 0, false, 0, 0);
		Slate.ProcessKeyDownEvent(KeyEvent);
		Report = FString::Printf(TEXT("Pressed '%s' down (held). Use action_type='up' to release."), *KeyName);
	}
	else if (ActionType == TEXT("up") || ActionType == TEXT("release"))
	{
		FSlateApplication& Slate = FSlateApplication::Get();
		FKeyEvent KeyEvent(Key, FModifierKeysState(), 0, false, 0, 0);
		Slate.ProcessKeyUpEvent(KeyEvent);
		Report = FString::Printf(TEXT("Released '%s'."), *KeyName);
	}
	else
	{
		Result.Errors.Add(FString::Printf(TEXT("Unknown action_type '%s'. Use 'press', 'down', or 'up'."), *ActionType));
		return Result;
	}

	Result.bSuccess = true;
	Result.ResultMessage = Report;
	return Result;
}

// ============================================================================
// stop_pie_session
// ============================================================================

FAgentZetActionResult FAgentZetPIEActions::ExecuteStopPIE(
	const TSharedRef<FJsonObject>& Params, FAgentZetActionResult& Result)
{
	if (!IsPIERunning())
	{
		Result.bSuccess = true;
		Result.ResultMessage = TEXT("PIE is not running. Nothing to stop.");
		return Result;
	}

	if (GUnrealEd)
	{
		GUnrealEd->RequestEndPlayMap();
		UE_LOG(LogAgentZet, Log, TEXT("PIEActions: Requested PIE session stop."));
	}

	Result.bSuccess = true;
	Result.ResultMessage = TEXT("PIE session stopped. Use read_message_log to check for any runtime errors that occurred during play.");
	return Result;
}

// ============================================================================
// Helpers
// ============================================================================

bool FAgentZetPIEActions::IsPIERunning()
{
	if (!GEditor) return false;
	return GEditor->IsPlaySessionInProgress();
}

#undef LOCTEXT_NAMESPACE
