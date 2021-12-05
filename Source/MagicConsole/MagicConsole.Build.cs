// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class MagicConsole : ModuleRules
{
	public MagicConsole(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(new [] { "Core", "CoreUObject", "Engine", "InputCore" });
	}
}
