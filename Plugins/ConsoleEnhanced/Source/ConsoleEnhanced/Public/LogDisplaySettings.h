// Copyright Michael Galetzka, 2017

#pragma once

#include "LogDisplaySettings.generated.h"

USTRUCT()
struct FLogCategorySetting
{
    GENERATED_BODY()

    // What to search for to match this category.
    UPROPERTY(EditAnywhere, config, Category = "Log Categories")
        FString CategorySearchString;

    // The text color for lines that match the specified search term of this category
    UPROPERTY(EditAnywhere, config, Category = "Log Categories")
        FLinearColor TextColor = FLinearColor(0.402, 0.402, 0.402);

    // The shadow color for this log category
    UPROPERTY(EditAnywhere, config, Category = "Log Categories")
        FLinearColor ShadowColor = FLinearColor(0, 0, 0, 0.5);

    // If true then the category search string will be interpreted as regular expression
    UPROPERTY(EditAnywhere, config, Category = "Log Categories", AdvancedDisplay)
        bool SearchAsRegex = true;
};

/**
 * Implements the settings for the log plugin.
 */
UCLASS(config = Engine, defaultconfig)
class ULogDisplaySettings : public UObject
{
	GENERATED_BODY()

public:
        // Displays a shadow drop behind the log text for better readability
        UPROPERTY(EditAnywhere, config, Category = "Output Log")
            bool bDisplayTextShadow = true;

        // Recognizes blueprint paths from you project (e.g. in a stack trace) and makes them into clickable links
        UPROPERTY(EditAnywhere, config, Category = "Output Log")
            bool bParseBlueprintLinks = true;

        // Displays all valid file paths as clickable links
        UPROPERTY(EditAnywhere, config, Category = "Output Log")
            bool bParseFilePaths = true;

        // Displays all hyperlinks in the log as clickable links
        UPROPERTY(EditAnywhere, config, Category = "Output Log")
            bool bParseHyperlinks = true;

        // How to display the counter in the "Collapsed Mode"
        UPROPERTY(EditAnywhere, config, Category = "Output Log")
            FLinearColor CollapsedLineCounterColor = FLinearColor(0.9, 0.1, 0.7);

        // The pixel offset for the shadow drop
        UPROPERTY(EditAnywhere, config, Category = "Output Log", AdvancedDisplay)
            FVector2D ShadowOffset = FVector2D(1, 1);

        // The color for the displayed shadow drop
        UPROPERTY(EditAnywhere, config, Category = "Output Log", AdvancedDisplay)
            FLinearColor ShadowColor = FLinearColor(0, 0, 0, 0.5);

        // If the texts should have an outline applied to it
        UPROPERTY(EditAnywhere, config, Category = "Output Log", AdvancedDisplay)
            bool bDisplayOutline = false;

        // The color of the text outline
        UPROPERTY(EditAnywhere, config, Category = "Output Log", AdvancedDisplay)
            FLinearColor OutlineColor;

        // The size of the outline in pixels
        UPROPERTY(EditAnywhere, config, Category = "Output Log", AdvancedDisplay)
            float OutlineSize;

        // The regex to determine if a log message is filtered out in "spam" mode. Caution! Only edit when you know what you are doing!
        UPROPERTY(EditAnywhere, config, Category = "Output Log", AdvancedDisplay, meta = (ConfigRestartRequired = true))
            FString AntiSpamRegex = FString(TEXT("(last play command: )|(No blueprints needed recompiling)|(PIE: )|(Creating play world package)|(LoadErrors: New page)|(Finished looking for orphan)|(Missing cached shader map)|(MapCheck: New page)|(Deleted Actor: )|(Deleted \\d* Actors)|(LogSavePackage: Save=)|(Finished SavePackage)|(LogFileHelpers: Saving map)|(Reallocating scene render targets)|(Native class hierarchy)|(MaterialEditorStats: )|(seconds spent updating \\d+ materials)|(Quitting Cascade)|(LogSavePackage: Moving)|(Creating AISystem)|(LogInit: )|(level for play took)|(LogEditorViewport: Clicking on Actor)|(New page: Lighting Build)"));

        // Allows to define custom log categories by search string. The first matching category is applied to each line.
        UPROPERTY(EditAnywhere, config, Category = "Log Categories")
            TArray<FLogCategorySetting> LogCategories;
};
