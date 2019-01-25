// Copyright Michael Galetzka, 2017

#pragma once

#include "Modules/ModuleInterface.h"
#include "Editor.h"
#include "SlateBasics.h"

/** Style of the debug console */
namespace EDebugConsoleStyle
{
    enum Type
    {
        /** Shows the debug console input line with tab completion only */
        Compact,

        /** Shows a scrollable log window with the input line on the bottom */
        WithLog,
    };
};

struct FDebugConsoleDelegates
{
    FSimpleDelegate OnFocusLost;
    FSimpleDelegate OnConsoleCommandExecuted;
};

class FConsoleEnhancedModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

    /** Generates a console input box widget.  Remember, this widget will become invalid if the
    output log DLL is unloaded on the fly. */
    virtual TSharedRef< SWidget > MakeConsoleInputBox(TSharedPtr< SEditableTextBox >& OutExposedEditableTextBox) const;

    /** Opens a debug console in the specified window, if not already open */
    virtual void ToggleDebugConsoleForWindow(const TSharedRef< SWindow >& Window, const EDebugConsoleStyle::Type InStyle, const FDebugConsoleDelegates& DebugConsoleDelegates);

    /** Closes the debug console for the specified window */
    virtual void CloseDebugConsole();

private:

    /** Weak pointer to a debug console that's currently open, if any */
    TWeakPtr< SWidget > DebugConsole;
};