// Copyright Michael Galetzka, 2017

#include "ConsoleEnhanced.h"
#include "SOutputLog.h"
#include "SDebugConsole.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructure.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#include "Widgets/Docking/SDockTab.h"
#include "LogDisplaySettings.h"
#include "ISettingsModule.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "FConsoleEnhancedModule"

namespace OutputLogModule
{
    static const FName OutputLogTabName = FName(TEXT("OutputLogPlus"));
    static const FName OutputLogTabName2 = FName(TEXT("OutputLogPlus2"));
    static const FName OutputLogTabName3 = FName(TEXT("OutputLogPlus3"));
    static const FName OutputLogTabName4 = FName(TEXT("OutputLogPlus4"));
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

    /** All log messages since this module has been started */
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

void FConsoleEnhancedModule::StartupModule()
{
    ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
    if (SettingsModule)
    {
        auto SavedSettings = GetMutableDefault<ULogDisplaySettings>();
        SettingsModule->RegisterSettings("Project", "Plugins", "OutputLog Pro",
            LOCTEXT("RuntimeSettingsName", "OutputLog Pro"),
            LOCTEXT("RuntimeSettingsDescription", "Dsiplay settings for the enchanced ouput log."),
            SavedSettings);
    }

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(OutputLogModule::OutputLogTabName, FOnSpawnTab::CreateStatic(&SpawnOutputLog))
        .SetDisplayName(NSLOCTEXT("UnrealEditor", "OutputLogPlusTab", "Enhanced Output Log"))
        .SetTooltipText(NSLOCTEXT("UnrealEditor", "OutputLogPlusTooltipText", "Open the Output Log tab."))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsLogCategory())
        .SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "Log.TabIcon"));

    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(OutputLogModule::OutputLogTabName2, FOnSpawnTab::CreateStatic(&SpawnOutputLog))
        .SetDisplayName(NSLOCTEXT("UnrealEditor", "OutputLogPlusTab2", "Enhanced Output Log (Window 2)"))
        .SetTooltipText(NSLOCTEXT("UnrealEditor", "OutputLogPlusTooltipText", "Open the Output Log tab."))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsLogCategory())
        .SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "Log.TabIcon"));

    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(OutputLogModule::OutputLogTabName3, FOnSpawnTab::CreateStatic(&SpawnOutputLog))
        .SetDisplayName(NSLOCTEXT("UnrealEditor", "OutputLogPlusTab3", "Enhanced Output Log (Window 3)"))
        .SetTooltipText(NSLOCTEXT("UnrealEditor", "OutputLogPlusTooltipText", "Open the Output Log tab."))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsLogCategory())
        .SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "Log.TabIcon"));

    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(OutputLogModule::OutputLogTabName4, FOnSpawnTab::CreateStatic(&SpawnOutputLog))
        .SetDisplayName(NSLOCTEXT("UnrealEditor", "OutputLogPlusTab4", "Enhanced Output Log (Window 4)"))
        .SetTooltipText(NSLOCTEXT("UnrealEditor", "OutputLogPlusTooltipText", "Open the Output Log tab."))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsLogCategory())
        .SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "Log.TabIcon"));

    OutputLogHistory = MakeShareable(new FOutputLogHistory);
}

void FConsoleEnhancedModule::ShutdownModule()
{
    if (FSlateApplication::IsInitialized())
    {
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(OutputLogModule::OutputLogTabName);
    }

    ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
    if (SettingsModule)
    {
        SettingsModule->UnregisterSettings("Project", "Plugins", "OutputLog Pro");
    }
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