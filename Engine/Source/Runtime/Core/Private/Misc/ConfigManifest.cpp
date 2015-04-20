
#pragma once

#include "CorePrivatePCH.h"

#include "ConfigManifest.h"
#include "RemoteConfigIni.h"
#include "EngineVersion.h"

#include "Runtime/Launch/Resources/Version.h"

enum class EConfigManifestVersion
{
	/******* DO NOT REMOVE OLD VERSIONS ********/
	Initial,
	RenameEditorAgnosticSettings,
	MigrateProjectSpecificInisToAgnostic,

	// ^ Add new versions above here ^
	NumOfVersions
};

bool IsDirectoryEmpty(const TCHAR* InDirectory)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	struct FVisitor : IPlatformFile::FDirectoryVisitor
	{
		FVisitor() : bHasFiles(false) {}

		bool bHasFiles;
		virtual bool Visit(const TCHAR*, bool bIsDir) override
		{
			if (!bIsDir)
			{
				bHasFiles = true;
				return false;
			}
			return true;
		}

	} Visitor;

	PlatformFile.IterateDirectory(InDirectory, Visitor);

	return !Visitor.bHasFiles;
}

FString ProjectSpecificIniPath(const TCHAR* InLeaf)
{
	return FPaths::GeneratedConfigDir() / ANSI_TO_TCHAR(FPlatformProperties::PlatformName()) / InLeaf;
}

FString ProjectAgnosticIniPath(const TCHAR* InLeaf)
{
	return FPaths::GameAgnosticSavedDir() / TEXT("Config") / ANSI_TO_TCHAR(FPlatformProperties::PlatformName()) / InLeaf;
}

/** Migrates config files from a previous version of the engine. Does nothing on non-installed versions */
void MigratePreviousEngineInis()
{
	if (!FPaths::ShouldSaveToUserDir() && !FApp::IsEngineInstalled())
	{
		// We can't do this in non-installed engines or where we haven't saved to a user directory
		return;
	}

	FEngineVersion PreviousVersion(GEngineVersion.GetMajor(), GEngineVersion.GetMinor() - 1, GEngineVersion.GetPatch(), GEngineVersion.GetChangelist(), GEngineVersion.GetBranch());

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	while(PreviousVersion.GetMinor() >= 0)
	{
		const FString Directory = FString(FPlatformProcess::UserSettingsDir()) / ENGINE_VERSION_TEXT(EPIC_PRODUCT_IDENTIFIER) / PreviousVersion.ToString(EVersionComponent::Minor) / TEXT("Saved") / TEXT("Config") / ANSI_TO_TCHAR(FPlatformProperties::PlatformName());
		if (FPaths::DirectoryExists(Directory))
		{
			const FString DestDir = ProjectAgnosticIniPath(TEXT(""));
			if (PlatformFile.CreateDirectoryTree(*DestDir))
			{
				PlatformFile.CopyDirectoryTree(*DestDir, *Directory, false);
			}

			// If we failed to create the directory tree anyway we don't want to allow the possibility of upgrading from even older versions, so early return regardless
			return;
		}

		PreviousVersion = FEngineVersion(PreviousVersion.GetMajor(), PreviousVersion.GetMinor() - 1, PreviousVersion.GetPatch(), PreviousVersion.GetChangelist(), PreviousVersion.GetBranch());
	}
}

void FConfigManifest::UpgradeFromPreviousVersions()
{
	// First off, load the manifest config if it exists
	FConfigFile Manifest;

	const FString ManifestFilename = ProjectAgnosticIniPath(TEXT("Manifest.ini"));

	if (!FPaths::FileExists(ManifestFilename) && IsDirectoryEmpty(*FPaths::GetPath(ManifestFilename)))
	{
		// Copy files from previous versions of the engine, if possible
		MigratePreviousEngineInis();
	}

	const EConfigManifestVersion LatestVersion = (EConfigManifestVersion)((int32)EConfigManifestVersion::NumOfVersions - 1);
	EConfigManifestVersion CurrentVersion = EConfigManifestVersion::Initial;

	if (FPaths::FileExists(ManifestFilename))
	{
		// Load the manifest from the file
		Manifest.Read(*ManifestFilename);

		int64 Version = 0;
		if (Manifest.GetInt64(TEXT("Manifest"), TEXT("Version"), Version) && Version < (int64)EConfigManifestVersion::NumOfVersions)
		{
			CurrentVersion = (EConfigManifestVersion)Version;
		}
	}

	if (CurrentVersion == LatestVersion)
	{
		return;
	}

	CurrentVersion = UpgradeFromVersion(CurrentVersion);

	// Set the version in the manifest, and write it out
	Manifest.SetInt64(TEXT("Manifest"), TEXT("Version"), (int64)CurrentVersion);
	Manifest.Write(ManifestFilename);
}

/** Combine 2 config files together, putting the result in a third */
void CombineConfig(const TCHAR* Base, const TCHAR* Other, const TCHAR* Output)
{
	FConfigFile Config;

	Config.Read(Base);
	Config.Combine(Other);

	Config.Write(Output, false /*bDoRemoteWrite*/);
}

/** Migrate a project specific ini to be a project agnostic one */
void MigrateToAgnosticIni(const TCHAR* SrcIniName, const TCHAR* DstIniName)
{
	const FString OldIni = ProjectSpecificIniPath(SrcIniName);
	const FString NewIni = ProjectAgnosticIniPath(DstIniName);

	if (FPaths::FileExists(*OldIni))
	{
		if (!FPaths::FileExists(*NewIni))
		{
			IFileManager::Get().Move(*NewIni, *OldIni);
		}
		else
		{
			CombineConfig(*NewIni, *OldIni, *NewIni);
		}
	}
}

/** Migrate a project specific ini to be a project agnostic one */
void MigrateToAgnosticIni(const TCHAR* IniName)
{
	MigrateToAgnosticIni(IniName, IniName);
}

/** Rename an ini file, dealing with the case where the destination already exists */
void RenameIni(const TCHAR* OldIni, const TCHAR* NewIni)
{
	if (FPaths::FileExists(OldIni))
	{
		if (!FPaths::FileExists(NewIni))
		{
			IFileManager::Get().Move(NewIni, OldIni);
		}
		else
		{
			CombineConfig(NewIni, OldIni, NewIni);
		}
	}
}

void FConfigManifest::MigrateEditorUserSettings()
{
	const FString EditorUserSettingsFilename = ProjectSpecificIniPath(TEXT("EditorUserSettings.ini"));
	if (!FPaths::FileExists(EditorUserSettingsFilename))
	{
		return;
	}

	// Handle upgrading editor user settings to the new path
	FConfigFile EditorUserSettingsConfig;
	EditorUserSettingsConfig.NoSave = true;
	FConfigCacheIni::LoadLocalIniFile(EditorUserSettingsConfig, TEXT("EditorUserSettings"), true);
	
	FConfigFile EditorPerProjectUserSettingsConfig;
	FConfigCacheIni::LoadLocalIniFile(EditorPerProjectUserSettingsConfig, TEXT("EditorPerProjectUserSettings"), true);

	EditorPerProjectUserSettingsConfig.AddMissingProperties(EditorUserSettingsConfig);
	if (EditorPerProjectUserSettingsConfig.Write(ProjectSpecificIniPath(TEXT("EditorPerProjectUserSettings.ini")), false))
	{
		FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*EditorUserSettingsFilename);
	}
}

EConfigManifestVersion FConfigManifest::UpgradeFromVersion(EConfigManifestVersion FromVersion)
{
	// Perform upgrades sequentially...

	if (FromVersion < EConfigManifestVersion::RenameEditorAgnosticSettings)
	{
		// First off, rename the Editor game agnostic ini config to EditorSettings
		RenameIni(*ProjectAgnosticIniPath(TEXT("EditorGameAgnostic.ini")), *ProjectAgnosticIniPath(TEXT("EditorSettings.ini")));

		FromVersion = EConfigManifestVersion::RenameEditorAgnosticSettings;
	}

	if (FromVersion < EConfigManifestVersion::MigrateProjectSpecificInisToAgnostic)
	{
		if (!FApp::HasGameName())
		{
			// We can't upgrade game settings if there is no game.
			return FromVersion;
		}

		// The initial versioning made the following changes:

		// 1. Move Editor.ini from Game/Saved/Config to Engine/Saved/Config, thus making it project-agnostic
		// 2. Move EditorLayout.ini from Game/Saved/Config to Engine/Saved/Config, thus making it project-agnostic
		// 3. Move EditorKeyBindings.ini from Game/Saved/Config to Engine/Saved/Config, thus making it project-agnostic

		MigrateToAgnosticIni(TEXT("Editor.ini"));
		MigrateToAgnosticIni(TEXT("EditorLayout.ini"));
		MigrateToAgnosticIni(TEXT("EditorKeyBindings.ini"));
		
		FromVersion = EConfigManifestVersion::MigrateProjectSpecificInisToAgnostic;
	}

	return FromVersion;
}