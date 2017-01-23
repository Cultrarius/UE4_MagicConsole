// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ConsoleEnhancedPrivatePCH.h"
#include "ConsoleEnhancedEdMode.h"
#include "ConsoleEnhancedEdModeToolkit.h"
#include "Toolkits/ToolkitManager.h"

const FEditorModeID FConsoleEnhancedEdMode::EM_ConsoleEnhancedEdModeId = TEXT("EM_ConsoleEnhancedEdMode");

FConsoleEnhancedEdMode::FConsoleEnhancedEdMode()
{

}

FConsoleEnhancedEdMode::~FConsoleEnhancedEdMode()
{

}

void FConsoleEnhancedEdMode::Enter()
{
	FEdMode::Enter();

	if (!Toolkit.IsValid() && UsesToolkits())
	{
		Toolkit = MakeShareable(new FConsoleEnhancedEdModeToolkit);
		Toolkit->Init(Owner->GetToolkitHost());
	}
}

void FConsoleEnhancedEdMode::Exit()
{
	if (Toolkit.IsValid())
	{
		FToolkitManager::Get().CloseToolkit(Toolkit.ToSharedRef());
		Toolkit.Reset();
	}

	// Call base Exit method to ensure proper cleanup
	FEdMode::Exit();
}

bool FConsoleEnhancedEdMode::UsesToolkits() const
{
	return true;
}




