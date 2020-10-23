// Copyright Michael Galetzka, 2017

using UnrealBuildTool;

public class ConsoleEnhanced : ModuleRules
{
    public ConsoleEnhanced(ReadOnlyTargetRules Target) : base(Target)
    {
        MinFilesUsingPrecompiledHeaderOverride = 1;
        bFasterWithoutUnity = true;
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        if (Target.Type == TargetType.Editor)
        {
            PrivateDependencyModuleNames.Add("UnrealEd");
        }

        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
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
