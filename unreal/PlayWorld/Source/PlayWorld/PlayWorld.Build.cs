// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class PlayWorld : ModuleRules
{
	public PlayWorld(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "HTTP", "Json", "JsonUtilities", "UMG", "Slate", "SlateCore" });

		PrivateDependencyModuleNames.AddRange(new string[] { "WebSockets" });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true

		StageSampleServer();
	}

	private void StageSampleServer()
	{
		string SampleServerDir = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "..", "Hackathon_Sample"));
		if (!Directory.Exists(SampleServerDir))
		{
			return;
		}

		foreach (string SourceFile in Directory.GetFiles(SampleServerDir, "*", SearchOption.AllDirectories))
		{
			string FileName = Path.GetFileName(SourceFile);
			if (FileName.EndsWith(".log"))
			{
				continue;
			}

			string RelativePath = SourceFile.Substring(SampleServerDir.Length).TrimStart(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
			string StagedPath = "$(TargetOutputDir)/Hackathon_Sample/" + RelativePath.Replace('\\', '/');
			RuntimeDependencies.Add(StagedPath, SourceFile, StagedFileType.NonUFS);
		}
	}
}
