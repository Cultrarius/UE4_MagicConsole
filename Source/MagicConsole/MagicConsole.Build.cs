// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class MagicConsole : ModuleRules
{
	public MagicConsole(ReadOnlyTargetRules Target) : base(Target)
	{
        MinFilesUsingPrecompiledHeaderOverride = 1;
        bFasterWithoutUnity = true;
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });
	}
}
