// Copyright AgentZet. All Rights Reserved.

#include "AgentZetBackupManager.h"
#include "AgentZetCoreModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

FAgentZetBackupManager::FAgentZetBackupManager() {}
FAgentZetBackupManager::~FAgentZetBackupManager() {}

FString FAgentZetBackupManager::GetBackupDirectory() const
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("AgentZet"), TEXT("Backups"));
}

FString FAgentZetBackupManager::BackupFile(const FString& FilePath)
{
	FString BackupDir = FPaths::Combine(GetBackupDirectory(), FDateTime::UtcNow().ToString());
	FString FileName = FPaths::GetCleanFilename(FilePath);
	FString BackupPath = FPaths::Combine(BackupDir, FileName);
	IFileManager::Get().MakeDirectory(*BackupDir, true);
	IFileManager::Get().Copy(*BackupPath, *FilePath);
	UE_LOG(LogAgentZet, Log, TEXT("BackupManager: Backed up %s -> %s"), *FilePath, *BackupPath);
	return BackupPath;
}

TArray<FString> FAgentZetBackupManager::BackupFiles(const TArray<FString>& FilePaths)
{
	TArray<FString> BackupPaths;
	for (const FString& Path : FilePaths) { BackupPaths.Add(BackupFile(Path)); }
	return BackupPaths;
}

bool FAgentZetBackupManager::RestoreFile(const FString& BackupPath) { /* Stub */ return false; }
bool FAgentZetBackupManager::RestoreUndoGroup(const FString& UndoGroupName) { /* Stub */ return false; }
void FAgentZetBackupManager::PruneOldBackups() { /* Stub */ }
