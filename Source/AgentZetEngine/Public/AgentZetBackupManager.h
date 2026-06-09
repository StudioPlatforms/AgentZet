// Copyright AgentZet. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentZetInterfaces.h"

class AGENTZETENGINE_API FAgentZetBackupManager : public IAgentZetBackupManager
{
public:
	FAgentZetBackupManager();
	virtual ~FAgentZetBackupManager();

	virtual FString BackupFile(const FString& FilePath) override;
	virtual TArray<FString> BackupFiles(const TArray<FString>& FilePaths) override;
	virtual bool RestoreFile(const FString& BackupPath) override;
	virtual bool RestoreUndoGroup(const FString& UndoGroupName) override;
	virtual FString GetBackupDirectory() const override;
	virtual void PruneOldBackups() override;

	int32 MaxBackupCount = 50;

private:
	TMap<FString, TArray<FString>> UndoGroupBackups;
};
