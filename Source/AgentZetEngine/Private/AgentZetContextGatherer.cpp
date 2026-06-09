// Copyright AgentZet. All Rights Reserved.

#include "AgentZetContextGatherer.h"
#include "AgentZetCoreModule.h"
#include "AgentZetSettings.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/EngineVersion.h"
#include "HAL/PlatformFileManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "UObject/UObjectIterator.h"
#include "Engine/World.h"
#include "Editor.h"

FAgentZetContextGatherer::FAgentZetContextGatherer()
	: LastCaptureTime(FDateTime::MinValue())
{
}

FAgentZetContextGatherer::~FAgentZetContextGatherer()
{
}

// ============================================================================
// Directory Visitor with hard-ignore exclusions (Gemini fix)
// ============================================================================

/** Directories that must NEVER be iterated — they can freeze the editor and blow tokens */
static const TArray<FString> HardIgnoreDirs = {
	TEXT("Saved"),
	TEXT("Intermediate"),
	TEXT("Binaries"),
	TEXT("DerivedDataCache"),
	TEXT("DDC"),
	TEXT(".git"),
	TEXT(".svn"),
	TEXT("__ExternalActors__"),
	TEXT("__ExternalObjects__")
};

static bool ShouldIgnoreDirectory(const FString& DirPath)
{
	FString DirName = FPaths::GetCleanFilename(DirPath);
	for (const FString& Ignored : HardIgnoreDirs)
	{
		if (DirName.Equals(Ignored, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

class FAgentZetDirectoryVisitor : public IPlatformFile::FDirectoryVisitor
{
public:
	TArray<FAgentZetFileEntry>& Entries;
	FString BasePath;
	TSet<FString> AllowedExtensions;
	int32 MaxEntries;

	FAgentZetDirectoryVisitor(TArray<FAgentZetFileEntry>& InEntries, const FString& InBasePath,
		const TSet<FString>& InAllowedExts, int32 InMaxEntries = 2000)
		: Entries(InEntries), BasePath(InBasePath), AllowedExtensions(InAllowedExts), MaxEntries(InMaxEntries) {}

	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
	{
		// Hard cap on entries to prevent token explosion
		if (Entries.Num() >= MaxEntries) return false;

		FString FullPath = FString(FilenameOrDirectory);
		FString RelativePath = FullPath;
		FPaths::MakePathRelativeTo(RelativePath, *BasePath);

		if (bIsDirectory)
		{
			// CRITICAL FIX (Gemini): Hard-ignore Saved/Intermediate/Binaries/DerivedDataCache
			// Iterating Intermediate alone can freeze the editor and blow past 200K tokens
			if (ShouldIgnoreDirectory(FullPath))
			{
				return true; // Continue but skip this directory's contents
			}

			FAgentZetFileEntry Entry;
			Entry.Path = RelativePath;
			Entry.bIsDirectory = true;
			Entries.Add(Entry);
		}
		else
		{
			FString Extension = FPaths::GetExtension(FullPath).ToLower();
			if (AllowedExtensions.Num() == 0 || AllowedExtensions.Contains(Extension))
			{
				FAgentZetFileEntry Entry;
				Entry.Path = RelativePath;
				Entry.Extension = Extension;
				Entry.bIsDirectory = false;
				Entry.FileSize = IFileManager::Get().FileSize(*FullPath);
				Entries.Add(Entry);
			}
		}
		return true; // Continue iteration
	}
};

// ============================================================================
// Public API
// ============================================================================

FString FAgentZetContextGatherer::BuildContextString()
{
	FAgentZetProjectContext Ctx = BuildProjectContext();

	// CRITICAL FIX (ChatGPT): Enforce token cap on context string
	// For 1000+ asset projects this could exceed 200k quickly
	const UAgentZetDeveloperSettings* Settings = UAgentZetDeveloperSettings::Get();
	int32 TokenBudget = Settings ? Settings->ContextTokenBudget : 30000;

	FString FullContext = Ctx.ToContextString();
	int32 EstTokens = FullContext.Len() / 4; // ~4 chars per token

	if (EstTokens > TokenBudget)
	{
		// Truncate to fit budget with a clear truncation notice
		int32 MaxChars = TokenBudget * 4;
		FullContext = FullContext.Left(MaxChars);
		FullContext += TEXT("\n\n[CONTEXT TRUNCATED — project has more files/assets than token budget allows. Consider enabling Context-as-Tools for large projects.]");

		UE_LOG(LogAgentZet, Warning, TEXT("ContextGatherer: Context truncated from ~%d to ~%d tokens (budget: %d)"),
			EstTokens, TokenBudget, TokenBudget);
	}

	return FullContext;
}

FString FAgentZetContextGatherer::GetFileTreeString()
{
	FAgentZetProjectContext Ctx = BuildProjectContext();
	FString Result = TEXT("=== Content Directory ===\n");
	for (const FAgentZetFileEntry& Entry : Ctx.ContentTree)
	{
		if (Entry.bIsDirectory)
			Result += FString::Printf(TEXT("[DIR] %s\n"), *Entry.Path);
		else
			Result += FString::Printf(TEXT("  %s (%lld bytes)\n"), *Entry.Path, Entry.FileSize);
	}

	Result += TEXT("\n=== Source Directory ===\n");
	for (const FAgentZetFileEntry& Entry : Ctx.SourceTree)
	{
		if (Entry.bIsDirectory)
			Result += FString::Printf(TEXT("[DIR] %s\n"), *Entry.Path);
		else
			Result += FString::Printf(TEXT("  %s\n"), *Entry.Path);
	}

	return Result;
}

FString FAgentZetContextGatherer::GetAssetSummaryString()
{
	FAgentZetProjectContext Ctx = BuildProjectContext();
	FString Result = TEXT("=== Asset Summary ===\n");
	Result += FString::Printf(TEXT("Total assets: %d\n"), Ctx.Assets.Num());

	for (const auto& Pair : Ctx.AssetCountsByClass)
	{
		Result += FString::Printf(TEXT("  %s: %d\n"), *Pair.Key, Pair.Value);
	}

	return Result;
}

FString FAgentZetContextGatherer::GetSettingsSnapshotString()
{
	return TEXT("=== Settings Snapshot ===\n(Will be populated in Phase 13)");
}

FString FAgentZetContextGatherer::GetClassHierarchyString()
{
	FString Result = TEXT("=== Project Class Hierarchy ===\n");

	FString ProjectModuleName = FApp::GetProjectName();

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (!Class || !Class->GetPackage()) continue;

		FString PackageName = Class->GetPackage()->GetName();

		if (PackageName.StartsWith(TEXT("/Game")) || PackageName.Contains(ProjectModuleName))
		{
			FString ParentName = Class->GetSuperClass() ? Class->GetSuperClass()->GetName() : TEXT("None");
			Result += FString::Printf(TEXT("  %s : %s\n"), *Class->GetName(), *ParentName);
		}
	}

	return Result;
}

int32 FAgentZetContextGatherer::EstimateTokenCount() const
{
	return CachedContext.EstimateTokenCount();
}

FAgentZetProjectContext FAgentZetContextGatherer::BuildProjectContext()
{
	// Check cache validity
	FDateTime Now = FDateTime::UtcNow();
	if ((Now - LastCaptureTime).GetTotalSeconds() < CacheValiditySeconds)
	{
		return CachedContext;
	}

	FAgentZetProjectContext Ctx;
	Ctx.CaptureTimestamp = Now;

	// ---- Project Info ----
	Ctx.ProjectName = FApp::GetProjectName();
	Ctx.EngineVersion = FEngineVersion::Current().ToString();
	Ctx.ProjectRootPath = FPaths::ProjectDir();

	// ---- Current Level ----
	if (GEditor && GEditor->GetEditorWorldContext().World())
	{
		UWorld* World = GEditor->GetEditorWorldContext().World();
		Ctx.CurrentLevelPath = World->GetMapName();
	}

	// ---- File Trees (with hard-ignore directories) ----
	{
		TSet<FString> ContentExts;
		ContentExts.Add(TEXT("uasset"));
		ContentExts.Add(TEXT("umap"));
		FAgentZetDirectoryVisitor ContentVisitor(Ctx.ContentTree, FPaths::ProjectContentDir(), ContentExts);
		IFileManager::Get().IterateDirectoryRecursively(*FPaths::ProjectContentDir(), ContentVisitor);
	}
	{
		TSet<FString> SourceExts;
		SourceExts.Add(TEXT("h"));
		SourceExts.Add(TEXT("cpp"));
		SourceExts.Add(TEXT("cs"));
		FAgentZetDirectoryVisitor SourceVisitor(Ctx.SourceTree, FPaths::GameSourceDir(), SourceExts);
		if (FPaths::DirectoryExists(FPaths::GameSourceDir()))
		{
			IFileManager::Get().IterateDirectoryRecursively(*FPaths::GameSourceDir(), SourceVisitor);
		}
	}
	{
		TSet<FString> ConfigExts;
		ConfigExts.Add(TEXT("ini"));
		FAgentZetDirectoryVisitor ConfigVisitor(Ctx.ConfigTree, FPaths::ProjectConfigDir(), ConfigExts);
		IFileManager::Get().IterateDirectoryRecursively(*FPaths::ProjectConfigDir(), ConfigVisitor);
	}

	// ---- Asset Registry ----
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AllAssets;
	AssetRegistry.GetAllAssets(AllAssets, true);

	for (const FAssetData& AssetData : AllAssets)
	{
		FString AssetPath = AssetData.GetObjectPathString();
		if (!AssetPath.StartsWith(TEXT("/Game/"))) continue;

		FAgentZetAssetEntry Entry;
		Entry.AssetPath = AssetPath;
		Entry.AssetName = AssetData.AssetName.ToString();
		Entry.AssetClass = AssetData.AssetClassPath.GetAssetName().ToString();
		Ctx.Assets.Add(Entry);

		int32& Count = Ctx.AssetCountsByClass.FindOrAdd(Entry.AssetClass);
		Count++;
	}

	// ---- Modules ----
	TArray<FString> ModuleDirs;
	if (FPaths::DirectoryExists(FPaths::GameSourceDir()))
	{
		IFileManager::Get().FindFiles(ModuleDirs, *FPaths::GameSourceDir(), false, true);
		for (const FString& Dir : ModuleDirs)
		{
			Ctx.ProjectModules.Add(Dir);
		}
	}

	// Cache result
	CachedContext = Ctx;
	LastCaptureTime = Now;

	UE_LOG(LogAgentZet, Log, TEXT("ContextGatherer: Built context — %d content files, %d source files, %d assets, %d modules"),
		Ctx.ContentTree.Num(), Ctx.SourceTree.Num(), Ctx.Assets.Num(), Ctx.ProjectModules.Num());

	return Ctx;
}
