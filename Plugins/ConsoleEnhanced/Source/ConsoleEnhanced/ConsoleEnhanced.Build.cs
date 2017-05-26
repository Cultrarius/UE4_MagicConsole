// Copyright Michael Galetzka, 2017

using UnrealBuildTool;

public class ConsoleEnhanced : ModuleRules
{
    public ConsoleEnhanced(ReadOnlyTargetRules Target) : base(Target)
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
