// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ConsoleEnhanced : ModuleRules
{
    public ConsoleEnhanced(TargetInfo Target)
    {

        PublicIncludePaths.AddRange(
            new string[] { "ConsoleEnhanced/Public" }
        );


        PrivateIncludePaths.AddRange(
            new string[] { "ConsoleEnhanced/Private", }
        );


        PublicDependencyModuleNames.AddRange(
            new string[] { "Core", }
        );


        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
                "UnrealEd",
                "Slate",
                "SlateCore",
                "EditorStyle",
                "TargetPlatform",
                "DesktopPlatform",
                "Kismet"
            }
        );
    }
}
