// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ConsoleEnhancedPrivatePCH.h"
#include "ConsoleEnhancedEdMode.h"
#include "SOutputLog.h"
#include "SDebugConsole.h"
#include "SDeviceOutputLog.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#include "SDockTab.h"

#define LOCTEXT_NAMESPACE "FConsoleEnhancedModule"

namespace OutputLogModule
{
    static const FName OutputLogTabName = FName(TEXT("OutputLogPlus"));
    static const FName DeviceOutputLogTabName = FName(TEXT("DeviceOutputLogPlus"));
}

/** This class is to capture all log output even if the log window is closed */
class FOutputLogHistory : public FOutputDevice
{
public:

    FOutputLogHistory()
    {
        GLog->AddOutputDevice(this);
        GLog->SerializeBacklog(this);
    }

    ~FOutputLogHistory()
    {
        // At shutdown, GLog may already be null
        if (GLog != NULL)
        {
            GLog->RemoveOutputDevice(this);
        }
    }

    /** Gets all captured messages */
    const TArray< TSharedPtr<FLogMessage> >& GetMessages() const
    {
        return Messages;
    }

protected:

    virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override
    {
        // Capture all incoming messages and store them in history
        SOutputLog::CreateLogMessages(V, Verbosity, Category, -1, Messages);
    }

    virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category, const double Time) override
    {
        // Capture all incoming messages and store them in history
        SOutputLog::CreateLogMessages(V, Verbosity, Category, Time, Messages);
    }    

private:

    /** All log messsges since this module has been started */
    TArray< TSharedPtr<FLogMessage> > Messages;
};

/** Our global output log app spawner */
static TSharedPtr<FOutputLogHistory> OutputLogHistory;

TSharedRef<SDockTab> SpawnOutputLog(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        .Icon(FEditorStyle::GetBrush("Log.TabIcon"))
        .TabRole(ETabRole::NomadTab)
        .Label(NSLOCTEXT("OutputLogPlus", "TabTitle", "Enhanced Output Log"))
        [
            SNew(SOutputLog).Messages(OutputLogHistory->GetMessages())
        ];
}

TSharedRef<SDockTab> SpawnDeviceOutputLog(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        .Icon(FEditorStyle::GetBrush("Log.TabIcon"))
        .TabRole(ETabRole::NomadTab)
        .Label(NSLOCTEXT("OutputLogPlus", "DeviceTabTitle", "Enhanced Device Output Log"))
        [
            SNew(SDeviceOutputLog)
        ];
}

void FConsoleEnhancedModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	FEditorModeRegistry::Get().RegisterMode<FConsoleEnhancedEdMode>(FConsoleEnhancedEdMode::EM_ConsoleEnhancedEdModeId, LOCTEXT("ConsoleEnhancedEdModeName", "ConsoleEnhancedEdMode"), FSlateIcon(), true);

    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(OutputLogModule::OutputLogTabName, FOnSpawnTab::CreateStatic(&SpawnOutputLog))
        .SetDisplayName(NSLOCTEXT("UnrealEditor", "OutputLogPlusTab", "Enhanced Output Log"))
        .SetTooltipText(NSLOCTEXT("UnrealEditor", "OutputLogPlusTooltipText", "Open the Output Log tab."))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsLogCategory())
        .SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "Log.TabIcon"));

    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(OutputLogModule::DeviceOutputLogTabName, FOnSpawnTab::CreateStatic(&SpawnDeviceOutputLog))
        .SetDisplayName(NSLOCTEXT("UnrealEditor", "DeviceOutputLogPlusTab", "Enhanced Device Output Log"))
        .SetTooltipText(NSLOCTEXT("UnrealEditor", "DeviceOutputLogPlusTooltipText", "Open the Device Output Log tab."))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsLogCategory())
        .SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "Log.TabIcon"))
        .SetAutoGenerateMenuEntry(false); // remove once not Experimental

    OutputLogHistory = MakeShareable(new FOutputLogHistory);
}

void FConsoleEnhancedModule::ShutdownModule()
{
    if (FSlateApplication::IsInitialized())
    {
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(OutputLogModule::OutputLogTabName);
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(OutputLogModule::DeviceOutputLogTabName);
    }

	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	FEditorModeRegistry::Get().UnregisterMode(FConsoleEnhancedEdMode::EM_ConsoleEnhancedEdModeId);
}

TSharedRef< SWidget > FConsoleEnhancedModule::MakeConsoleInputBox(TSharedPtr< SEditableTextBox >& OutExposedEditableTextBox) const
{
    TSharedRef< SConsoleInputBox > NewConsoleInputBox = SNew(SConsoleInputBox);
    OutExposedEditableTextBox = NewConsoleInputBox->GetEditableTextBox();
    return NewConsoleInputBox;
}


void FConsoleEnhancedModule::ToggleDebugConsoleForWindow(const TSharedRef< SWindow >& Window, const EDebugConsoleStyle::Type InStyle, const FDebugConsoleDelegates& DebugConsoleDelegates)
{
    bool bShouldOpen = true;
    // Close an existing console box, if there is one
    TSharedPtr< SWidget > PinnedDebugConsole(DebugConsole.Pin());
    if (PinnedDebugConsole.IsValid())
    {
        // If the console is already open close it unless it is in a different window.  In that case reopen it on that window
        bShouldOpen = false;
        TSharedPtr< SWindow > WindowForExistingConsole = FSlateApplication::Get().FindWidgetWindow(PinnedDebugConsole.ToSharedRef());
        if (WindowForExistingConsole.IsValid())
        {
            WindowForExistingConsole->RemoveOverlaySlot(PinnedDebugConsole.ToSharedRef());
            DebugConsole.Reset();
        }

        if (WindowForExistingConsole != Window)
        {
            // Console is being opened on another window
            bShouldOpen = true;
        }
    }

    TSharedPtr<SDockTab> ActiveTab = FGlobalTabmanager::Get()->GetActiveTab();
    if (ActiveTab.IsValid() && ActiveTab->GetLayoutIdentifier() == FTabId(OutputLogModule::OutputLogTabName))
    {
        FGlobalTabmanager::Get()->DrawAttention(ActiveTab.ToSharedRef());
        bShouldOpen = false;
    }

    if (bShouldOpen)
    {
        const EDebugConsoleStyle::Type DebugConsoleStyle = InStyle;
        TSharedRef< SDebugConsole > DebugConsoleRef = SNew(SDebugConsole, DebugConsoleStyle, this, &DebugConsoleDelegates);
        DebugConsole = DebugConsoleRef;

        const int32 MaximumZOrder = MAX_int32;
        Window->AddOverlaySlot(MaximumZOrder)
            .VAlign(VAlign_Bottom)
            .HAlign(HAlign_Center)
            .Padding(10.0f)
            [
                DebugConsoleRef
            ];

        // Force keyboard focus
        DebugConsoleRef->SetFocusToEditableText();
    }
}


void FConsoleEnhancedModule::CloseDebugConsole()
{
    TSharedPtr< SWidget > PinnedDebugConsole(DebugConsole.Pin());

    if (PinnedDebugConsole.IsValid())
    {
        TSharedPtr< SWindow > WindowForExistingConsole = FSlateApplication::Get().FindWidgetWindow(PinnedDebugConsole.ToSharedRef());
        if (WindowForExistingConsole.IsValid())
        {
            WindowForExistingConsole->RemoveOverlaySlot(PinnedDebugConsole.ToSharedRef());
            DebugConsole.Reset();
        }
    }
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FConsoleEnhancedModule, ConsoleEnhanced)