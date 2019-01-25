// Copyright Michael Galetzka, 2017

#include "SOutputLog.h"
#include "Widgets/Layout/SScrollBorder.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/LocalPlayer.h"
#include "Widgets/Input/SSearchBox.h"
#include "Styling/SlateTypes.h"
#include "AssetRegistryModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "BlueprintEditorModule.h"
#include "BlueprintEditor.h"
#include "SourceCodeNavigation.h"
#include "EditorStyleSet.h"
#include "Classes/EditorStyleSettings.h"

using namespace std;

#define LOCTEXT_NAMESPACE "SOutputLog"

/** Expression context to test the given messages against the current text filter */
class FLogFilter_TextFilterExpressionContext : public ITextFilterExpressionContext
{
public:
    FLogFilter_TextFilterExpressionContext(const FLogMessage& InMessage) : Message(&InMessage) {       
    }

    /** Test the given value against the strings extracted from the current item */
    virtual bool TestBasicStringExpression(const FTextFilterString& InValue, const ETextFilterTextComparisonMode InTextComparisonMode) const override {
        return TextFilterUtils::TestBasicStringExpression(*Message->Message, InValue, InTextComparisonMode); 
    }

    /**
    * Perform a complex expression test for the current item
    * No complex expressions in this case - always returns false
    */
    virtual bool TestComplexExpression(const FName& InKey, const FTextFilterString& InValue, const ETextFilterComparisonOperation InComparisonOperation, const ETextFilterTextComparisonMode InTextComparisonMode) const override { return false; }

private:
    /** Message that is being filtered */
    const FLogMessage* Message;
};

/** Custom console editable text box whose only purpose is to prevent some keys from being typed */
class SConsoleEditableTextBox : public SEditableTextBox
{
public:
    SLATE_BEGIN_ARGS(SConsoleEditableTextBox) {}

    /** Hint text that appears when there is no text in the text box */
    SLATE_ATTRIBUTE(FText, HintText)

        /** Called whenever the text is changed interactively by the user */
        SLATE_EVENT(FOnTextChanged, OnTextChanged)

        /** Called whenever the text is committed.  This happens when the user presses enter or the text box loses focus. */
        SLATE_EVENT(FOnTextCommitted, OnTextCommitted)

        SLATE_END_ARGS()


        void Construct(const FArguments& InArgs)
    {
        SetStyle(&FCoreStyle::Get().GetWidgetStyle< FEditableTextBoxStyle >("NormalEditableTextBox"));

        SBorder::Construct(SBorder::FArguments()
            .BorderImage(this, &SConsoleEditableTextBox::GetConsoleBorder)
            .BorderBackgroundColor(Style->BackgroundColor)
            .ForegroundColor(Style->ForegroundColor)
            .Padding(Style->Padding)
            [
                SAssignNew(EditableText, SConsoleEditableText)
                .HintText(InArgs._HintText)
            .OnTextChanged(InArgs._OnTextChanged)
            .OnTextCommitted(InArgs._OnTextCommitted)
            ]);
    }

private:
    class SConsoleEditableText : public SEditableText
    {
    public:
        SLATE_BEGIN_ARGS(SConsoleEditableText) {}
        /** The text that appears when there is nothing typed into the search box */
        SLATE_ATTRIBUTE(FText, HintText)
            /** Called whenever the text is changed interactively by the user */
            SLATE_EVENT(FOnTextChanged, OnTextChanged)

            /** Called whenever the text is committed.  This happens when the user presses enter or the text box loses focus. */
            SLATE_EVENT(FOnTextCommitted, OnTextCommitted)
            SLATE_END_ARGS()

            void Construct(const FArguments& InArgs)
        {
            SEditableText::Construct
            (
                SEditableText::FArguments()
                .HintText(InArgs._HintText)
                .OnTextChanged(InArgs._OnTextChanged)
                .OnTextCommitted(InArgs._OnTextCommitted)
                .ClearKeyboardFocusOnCommit(false)
                .IsCaretMovedWhenGainFocus(false)
                .MinDesiredWidth(400.0f)
            );
        }

        virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
        {
            // Special case handling.  Intercept the tilde key.  It is not suitable for typing in the console
            if (InKeyEvent.GetKey() == EKeys::Tilde)
            {
                return FReply::Unhandled();
            }
            else
            {
                return SEditableText::OnKeyDown(MyGeometry, InKeyEvent);
            }
        }

        virtual FReply OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent)
        {
            // Special case handling.  Intercept the tilde key.  It is not suitable for typing in the console
            if (InCharacterEvent.GetCharacter() != 0x60)
            {
                return SEditableText::OnKeyChar(MyGeometry, InCharacterEvent);
            }
            else
            {
                return FReply::Unhandled();
            }
        }

    };

    /** @return Border image for the text box based on the hovered and focused state */
    const FSlateBrush* GetConsoleBorder() const
    {
        if (EditableText->HasKeyboardFocus())
        {
            return &Style->BackgroundImageFocused;
        }
        else
        {
            if (EditableText->IsHovered())
            {
                return &Style->BackgroundImageHovered;
            }
            else
            {
                return &Style->BackgroundImageNormal;
            }
        }
    }

};

SConsoleInputBox::SConsoleInputBox()
    : SelectedSuggestion(-1)
    , bIgnoreUIUpdate(false)
{
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SConsoleInputBox::Construct(const FArguments& InArgs)
{
    OnConsoleCommandExecuted = InArgs._OnConsoleCommandExecuted;
    ConsoleCommandCustomExec = InArgs._ConsoleCommandCustomExec;

    ChildSlot
        [
            SAssignNew(SuggestionBox, SMenuAnchor)
            .Placement(InArgs._SuggestionListPlacement)
        [
            SAssignNew(InputText, SConsoleEditableTextBox)
            .OnTextCommitted(this, &SConsoleInputBox::OnTextCommitted)
        .HintText(NSLOCTEXT("ConsoleInputBox", "TypeInConsoleHint", "Enter console command"))
        .OnTextChanged(this, &SConsoleInputBox::OnTextChanged)
        ]
    .MenuContent
    (
        SNew(SBorder)
        .BorderImage(FEditorStyle::GetBrush("Menu.Background"))
        .Padding(FMargin(2))
        [
            SNew(SBox)
            .HeightOverride(250) // avoids flickering, ideally this would be adaptive to the content without flickering
        [
            SAssignNew(SuggestionListView, SListView< TSharedPtr<FString> >)
            .ListItemsSource(&Suggestions)
        .SelectionMode(ESelectionMode::Single)							// Ideally the mouse over would not highlight while keyboard controls the UI
        .OnGenerateRow(this, &SConsoleInputBox::MakeSuggestionListItemWidget)
        .OnSelectionChanged(this, &SConsoleInputBox::SuggestionSelectionChanged)
        .ItemHeight(18)
        ]
        ]
    )
        ];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SConsoleInputBox::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
    if (!GIntraFrameDebuggingGameThread && !IsEnabled())
    {
        SetEnabled(true);
    }
    else if (GIntraFrameDebuggingGameThread && IsEnabled())
    {
        SetEnabled(false);
    }
}


void SConsoleInputBox::SuggestionSelectionChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
    if (bIgnoreUIUpdate)
    {
        return;
    }

    for (int32 i = 0; i < Suggestions.Num(); ++i)
    {
        if (NewValue == Suggestions[i])
        {
            SelectedSuggestion = i;
            MarkActiveSuggestion();

            // If the user selected this suggestion by clicking on it, then go ahead and close the suggestion
            // box as they've chosen the suggestion they're interested in.
            if (SelectInfo == ESelectInfo::OnMouseClick)
            {
                SuggestionBox->SetIsOpen(false);
            }
            break;
        }
    }
}


TSharedRef<ITableRow> SConsoleInputBox::MakeSuggestionListItemWidget(TSharedPtr<FString> Text, const TSharedRef<STableViewBase>& OwnerTable)
{
    check(Text.IsValid());

    FString Left, Mid, Right, TempRight, Combined;

    if (Text->Split(TEXT("\t"), &Left, &TempRight))
    {
        if (TempRight.Split(TEXT("\t"), &Mid, &Right))
        {
            Combined = Left + Mid + Right;
        }
        else
        {
            Combined = Left + Right;
        }
    }
    else
    {
        Combined = *Text;
    }

    FText HighlightText = FText::FromString(Mid);

    return
        SNew(STableRow< TSharedPtr<FString> >, OwnerTable)
        [
            SNew(SBox)
            .WidthOverride(300)			// to enforce some minimum width, ideally we define the minimum, not a fixed width
        [
            SNew(STextBlock)
            .Text(FText::FromString(Combined))
        .TextStyle(FEditorStyle::Get(), "Log.Normal")
        .HighlightText(HighlightText)
        ]
        ];
}

class FConsoleVariableAutoCompleteVisitor
{
public:
    // @param Name must not be 0
    // @param CVar must not be 0
    static void OnConsoleVariable(const TCHAR *Name, IConsoleObject* CVar, TArray<FString>& Sink)
    {
#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
        if (CVar->TestFlags(ECVF_Cheat))
        {
            return;
        }
#endif // (UE_BUILD_SHIPPING || UE_BUILD_TEST)
        if (CVar->TestFlags(ECVF_Unregistered))
        {
            return;
        }

        Sink.Add(Name);
    }
};

void SConsoleInputBox::OnTextChanged(const FText& InText)
{
    if (bIgnoreUIUpdate)
    {
        return;
    }

    const FString& InputTextStr = InputText->GetText().ToString();
    if (!InputTextStr.IsEmpty())
    {
        TArray<FString> AutoCompleteList;

        // console variables
        {
            IConsoleManager::Get().ForEachConsoleObjectThatContains(
                FConsoleObjectVisitor::CreateStatic< TArray<FString>& >(
                    &FConsoleVariableAutoCompleteVisitor::OnConsoleVariable,
                    AutoCompleteList), *InputTextStr);
        }

        AutoCompleteList.Sort();

        for (uint32 i = 0; i < (uint32)AutoCompleteList.Num(); ++i)
        {
            FString &ref = AutoCompleteList[i];
            int32 Start = ref.Find(InputTextStr);

            if (Start != INDEX_NONE)
            {
                ref = ref.Left(Start) + TEXT("\t") + ref.Mid(Start, InputTextStr.Len()) + TEXT("\t") + ref.RightChop(Start + InputTextStr.Len());
            }
        }

        SetSuggestions(AutoCompleteList, false);
    }
    else
    {
        ClearSuggestions();
    }
}

void SConsoleInputBox::OnTextCommitted(const FText& InText, ETextCommit::Type CommitInfo)
{
    if (CommitInfo == ETextCommit::OnEnter)
    {
        if (!InText.IsEmpty())
        {
            IConsoleManager::Get().AddConsoleHistoryEntry(TEXT(""), *InText.ToString());

            // Copy the exec text string out so we can clear the widget's contents.  If the exec command spawns
            // a new window it can cause the text box to lose focus, which will result in this function being
            // re-entered.  We want to make sure the text string is empty on re-entry, so we'll clear it out
            const FString ExecString = InText.ToString();

            // Clear the console input area
            bIgnoreUIUpdate = true;
            InputText->SetText(FText::GetEmpty());
            bIgnoreUIUpdate = false;

            // Exec!
            if (ConsoleCommandCustomExec.IsBound())
            {
                ConsoleCommandCustomExec.Execute(ExecString);
            }
            else
            {
                bool bWasHandled = false;
                UWorld* World = NULL;
                UWorld* OldWorld = NULL;

                // The play world needs to handle these commands if it exists
                if (GIsEditor && GEditor->PlayWorld && !GIsPlayInEditorWorld)
                {
                    World = GEditor->PlayWorld;
                    OldWorld = SetPlayInEditorWorld(GEditor->PlayWorld);
                }

                ULocalPlayer* Player = GEngine->GetDebugLocalPlayer();
                if (Player)
                {
                    UWorld* PlayerWorld = Player->GetWorld();
                    if (!World)
                    {
                        World = PlayerWorld;
                    }
                    bWasHandled = Player->Exec(PlayerWorld, *ExecString, *GLog);
                }

                if (!World)
                {
                    World = GEditor->GetEditorWorldContext().World();
                }
                if (World)
                {
                    if (!bWasHandled)
                    {
                        AGameModeBase* const GameMode = World->GetAuthGameMode();
                        AGameStateBase* const GameState = World->GetGameState();
                        if (GameMode && GameMode->ProcessConsoleExec(*ExecString, *GLog, NULL))
                        {
                            bWasHandled = true;
                        }
                        else if (GameState && GameState->ProcessConsoleExec(*ExecString, *GLog, NULL))
                        {
                            bWasHandled = true;
                        }
                    }

                    if (!bWasHandled && !Player)
                    {
                        if (GIsEditor)
                        {
                            bWasHandled = GEditor->Exec(World, *ExecString, *GLog);
                        }
                        else
                        {
                            bWasHandled = GEngine->Exec(World, *ExecString, *GLog);
                        }
                    }
                }
                // Restore the old world of there was one
                if (OldWorld)
                {
                    RestoreEditorWorld(OldWorld);
                }
            }
        }

        ClearSuggestions();

        OnConsoleCommandExecuted.ExecuteIfBound();
    }
}

FReply SConsoleInputBox::OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent)
{
    if (SuggestionBox->IsOpen())
    {
        if (KeyEvent.GetKey() == EKeys::Up || KeyEvent.GetKey() == EKeys::Down)
        {
            if (KeyEvent.GetKey() == EKeys::Up)
            {
                if (SelectedSuggestion < 0)
                {
                    // from edit control to end of list
                    SelectedSuggestion = Suggestions.Num() - 1;
                }
                else
                {
                    // got one up, possibly back to edit control
                    --SelectedSuggestion;
                }
            }

            if (KeyEvent.GetKey() == EKeys::Down)
            {
                if (SelectedSuggestion < Suggestions.Num() - 1)
                {
                    // go one down, possibly from edit control to top
                    ++SelectedSuggestion;
                }
                else
                {
                    // back to edit control
                    SelectedSuggestion = -1;
                }
            }

            MarkActiveSuggestion();

            return FReply::Handled();
        }
        else if (KeyEvent.GetKey() == EKeys::Tab)
        {
            if (Suggestions.Num())
            {
                if (SelectedSuggestion >= 0 && SelectedSuggestion < Suggestions.Num())
                {
                    MarkActiveSuggestion();
                    OnTextCommitted(InputText->GetText(), ETextCommit::OnEnter);
                }
                else
                {
                    SelectedSuggestion = 0;
                    MarkActiveSuggestion();
                }
            }

            return FReply::Handled();
        }
    }
    else
    {
        if (KeyEvent.GetKey() == EKeys::Up)
        {
            TArray<FString> History;

            IConsoleManager::Get().GetConsoleHistory(TEXT(""), History);

            SetSuggestions(History, true);

            if (Suggestions.Num())
            {
                SelectedSuggestion = Suggestions.Num() - 1;
                MarkActiveSuggestion();
            }

            return FReply::Handled();
        }
    }

    return FReply::Unhandled();
}

void SConsoleInputBox::SetSuggestions(TArray<FString>& Elements, bool bInHistoryMode)
{
    FString SelectionText;
    if (SelectedSuggestion >= 0 && SelectedSuggestion < Suggestions.Num())
    {
        SelectionText = *Suggestions[SelectedSuggestion];
    }

    SelectedSuggestion = -1;
    Suggestions.Empty();
    SelectedSuggestion = -1;

    for (uint32 i = 0; i < (uint32)Elements.Num(); ++i)
    {
        Suggestions.Add(MakeShareable(new FString(Elements[i])));

        if (Elements[i] == SelectionText)
        {
            SelectedSuggestion = i;
        }
    }

    if (Suggestions.Num())
    {
        // Ideally if the selection box is open the output window is not changing it's window title (flickers)
        SuggestionBox->SetIsOpen(true, false);
        SuggestionListView->RequestScrollIntoView(Suggestions.Last());
    }
    else
    {
        SuggestionBox->SetIsOpen(false);
    }
}

void SConsoleInputBox::OnFocusLost(const FFocusEvent& InFocusEvent)
{
    //	SuggestionBox->SetIsOpen(false);
}

void SConsoleInputBox::MarkActiveSuggestion()
{
    bIgnoreUIUpdate = true;
    if (SelectedSuggestion >= 0)
    {
        SuggestionListView->SetSelection(Suggestions[SelectedSuggestion]);
        SuggestionListView->RequestScrollIntoView(Suggestions[SelectedSuggestion]);	// Ideally this would only scroll if outside of the view

        InputText->SetText(FText::FromString(GetSelectionText()));
    }
    else
    {
        SuggestionListView->ClearSelection();
    }
    bIgnoreUIUpdate = false;
}

void SConsoleInputBox::ClearSuggestions()
{
    SelectedSuggestion = -1;
    SuggestionBox->SetIsOpen(false);
    Suggestions.Empty();
}

FString SConsoleInputBox::GetSelectionText() const
{
    FString ret = *Suggestions[SelectedSuggestion];

    ret = ret.Replace(TEXT("\t"), TEXT(""));

    return ret;
}

TSharedRef< FOutputLogTextLayoutMarshaller > FOutputLogTextLayoutMarshaller::Create(TArray< TSharedPtr<FLogMessage> > InMessages, FLogFilter* InFilter)
{
    return MakeShareable(new FOutputLogTextLayoutMarshaller(MoveTemp(InMessages), InFilter));
}

FOutputLogTextLayoutMarshaller::~FOutputLogTextLayoutMarshaller()
{
}

void FOutputLogTextLayoutMarshaller::SetText(const FString& SourceString, FTextLayout& TargetTextLayout)
{
    TextLayout = (FCustomTextLayout*)&TargetTextLayout;
    AppendMessagesToTextLayout(Messages);
}

void FOutputLogTextLayoutMarshaller::GetText(FString& TargetString, const FTextLayout& SourceTextLayout)
{
    SourceTextLayout.GetAsText(TargetString);
}

bool FOutputLogTextLayoutMarshaller::AppendMessage(const TCHAR* InText, const ELogVerbosity::Type InVerbosity, const double Time, const FName& InCategory)
{
    TArray< TSharedPtr<FLogMessage> > NewMessages;
    if (SOutputLog::CreateLogMessages(InText, InVerbosity, InCategory, Time, NewMessages))
    {
        const bool bWasEmpty = Messages.Num() == 0;

        if (Filter->bCollapsedMode && NewMessages.Num() == 1 && Messages.Num() > 0) {
            auto PrevMessage = Messages.Last();
            if (NewMessages[0]->Message->Equals(*PrevMessage->Message)) {
                PrevMessage->Count += 1;
                if (Filter->IsMessageAllowed(PrevMessage))
                {
                    TextLayout->RemoveLine(TextLayout->GetLineModels().Num() - 1);
                }
                NewMessages[0] = Messages.Pop(false);
            }
        }
        Messages.Append(NewMessages);

        if (TextLayout)
        {
            // If we were previously empty, then we'd have inserted a dummy empty line into the document
            // We need to remove this line now as it would cause the message indices to get out-of-sync with the line numbers, which would break auto-scrolling
            if (bWasEmpty)
            {
                TextLayout->ClearLines();
            }

            // If we've already been given a text layout, then append these new messages rather than force a refresh of the entire document
            AppendMessagesToTextLayout(NewMessages);

            if (TextLayout->GetLineModels().Num() == 0) {
                TextLayout->AddEmptyRun();
            }
        }
        else
        {
            MarkMessagesCacheAsDirty();
            MakeDirty();
        }

        return true;
    }

    return false;
}

void FOutputLogTextLayoutMarshaller::AppendMessageToTextLayout(const TSharedPtr<FLogMessage>& InMessage)
{
    TArray<TSharedPtr<FLogMessage>> MessagesList;
    MessagesList.Add(InMessage);
    AppendMessagesToTextLayout(MessagesList);
}

struct RichTextHelper {
    static void JumpToGraph(IBlueprintEditor& bpEditor, UEdGraph* Graph) {
        bpEditor.JumpToHyperlink(Graph, false);
    }

    static void OpenBlueprint(const FSlateHyperlinkRun::FMetadata& Metadata, UBlueprint* Blueprint, UEdGraph* Graph)
    {
        if (Blueprint && Blueprint->IsValidLowLevel()) {

            // check to see if the blueprint is already opened in one of the editors
            auto editors = FAssetEditorManager::Get().FindEditorsForAsset(Blueprint);
            for (IAssetEditorInstance* editor : editors) {
                FBlueprintEditor* bpEditor = static_cast<FBlueprintEditor*>(editor);
                if (bpEditor && bpEditor->GetBlueprintObj() == Blueprint) {
                    bpEditor->BringToolkitToFront();
                    if (Graph && Graph->IsValidLowLevel()) {
                        JumpToGraph(*bpEditor, Graph);
                    }
                }
                return;
            }

            // open a new editor
            FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");
            TSharedRef<IBlueprintEditor> NewKismetEditor = BlueprintEditorModule.CreateBlueprintEditor(EToolkitMode::Standalone, TSharedPtr<IToolkitHost>(), Blueprint);
            if (Graph && Graph->IsValidLowLevel()) {
                JumpToGraph(NewKismetEditor.Get(), Graph);
            }
        }
        else {
            // TODO: log that blueprint reference is no longer valid
        }
    }

    static void OpenUrl(const FSlateHyperlinkRun::FMetadata& Metadata)
    {
        const FString* url = Metadata.Find(TEXT("href"));
        if (url)
        {
            FPlatformProcess::LaunchURL(**url, nullptr, nullptr);
        }
    }

    static void OpenPath(const FSlateHyperlinkRun::FMetadata& Metadata, FString Path, bool IsSourceFile)
    {
        const FString AbsolutePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*Path);
        if (IsSourceFile && FSourceCodeNavigation::IsCompilerAvailable()) {
            FSourceCodeNavigation::OpenSourceFile(AbsolutePath);
        }
        else {
            FPlatformProcess::LaunchFileInDefaultExternalApplication(*AbsolutePath);
        }
    }
};

void FOutputLogTextLayoutMarshaller::AppendMessagesToTextLayout(const TArray<TSharedPtr<FLogMessage>>& InMessages)
{
    TArray<FTextLayout::FNewLineData> LinesToAdd;
    LinesToAdd.Reserve(InMessages.Num());
    TArray<UBlueprint*> blueprints;
    for (TObjectIterator<UBlueprint> Itr; Itr; ++Itr)
    {
        UBlueprint* bp = *Itr;
        if (bp->GeneratedClass) {
            blueprints.Add(bp);
        }
    }

    for (const auto& CurrentMessage : InMessages)
    {
        if (!Filter->IsMessageAllowed(CurrentMessage))
        {
            continue;
        }
        MarkMessagesCacheAsDirty();

        auto StyleSettings = GetDefault<ULogDisplaySettings>();
        TArray<TSharedRef<IRun>> Runs;
        TSharedRef<FString> LineText = CurrentMessage->Message;
        const FTextBlockStyle& MessageTextStyle = GetStyle(CurrentMessage, StyleSettings);
        int32 startOffset = 0;
        if (CurrentMessage->Count > 1) {
            FString* newLine = new FString("{");
            newLine->AppendInt(CurrentMessage->Count);
            newLine->Append("} ");
            startOffset = newLine->Len();
            newLine->Append(*LineText);
            LineText = MakeShareable(newLine);
            const FTextBlockStyle& CountTextStyle = FTextBlockStyle(MessageTextStyle).SetColorAndOpacity(FSlateColor(StyleSettings->CollapsedLineCounterColor));
            TSharedRef<FSlateTextRun> textRun = FSlateTextRun::Create(FRunInfo(), LineText, CountTextStyle, FTextRange(0, startOffset));
            Runs.Add(textRun);
        }

        FHyperlinkStyle linkStyle = FEditorStyle::Get().GetWidgetStyle<FHyperlinkStyle>(FName(TEXT("NavigationHyperlink")));
        linkStyle.SetTextStyle(MessageTextStyle);

        TSet<FTextRange> foundLinkRanges;
        map<int32, TSharedRef<FSlateHyperlinkRun>> hyperlinkRuns;
        if (StyleSettings->bParseBlueprintLinks) {
            CreateBlueprintHyperlinks(blueprints, foundLinkRanges, LineText, linkStyle, hyperlinkRuns);
        }
        if (StyleSettings->bParseFilePaths) {
            CreateFilepathHyperlinks(LineText, foundLinkRanges, linkStyle, hyperlinkRuns);
        }
        if (StyleSettings->bParseHyperlinks) {
            CreateUrlHyperlinks(LineText, foundLinkRanges, linkStyle, hyperlinkRuns);
        }

        if (hyperlinkRuns.empty()) {
            Runs.Add(FSlateTextRun::Create(FRunInfo(), LineText, MessageTextStyle, FTextRange(startOffset, LineText->Len())));
        }
        else {
            int32 lastIndex = startOffset;
            for (auto run : hyperlinkRuns) {
                if (lastIndex < run.first) {
                    TSharedRef<FSlateTextRun> textRun = FSlateTextRun::Create(FRunInfo(), LineText, MessageTextStyle, FTextRange(lastIndex, run.first));
                    Runs.Add(textRun);
                }
                Runs.Add(run.second);
                lastIndex = run.second->GetTextRange().EndIndex;
            }
            if (lastIndex < LineText->Len()) {
                TSharedRef<FSlateTextRun> textRun = FSlateTextRun::Create(FRunInfo(), LineText, MessageTextStyle, FTextRange(lastIndex, LineText->Len()));
                Runs.Add(textRun);
            }
        }

        LinesToAdd.Emplace(MoveTemp(LineText), MoveTemp(Runs));
    }

    TextLayout->AddLines(LinesToAdd);
}

void FOutputLogTextLayoutMarshaller::CreateUrlHyperlinks(TSharedRef<FString> LineText, TSet<FTextRange> &foundLinkRanges, FHyperlinkStyle linkStyle, map<int32, TSharedRef<FSlateHyperlinkRun>> &hyperlinkRuns) const
{
    FRegexMatcher urlMatcher(UrlPattern, *LineText);
    while (urlMatcher.FindNext()) {
        int32 matchStart = urlMatcher.GetMatchBeginning();
        FTextRange newRange(matchStart, urlMatcher.GetMatchEnding());
        if (overlapping(newRange, foundLinkRanges)) {
            continue;
        }

        FRunInfo RunInfo(TEXT("a"));
        RunInfo.MetaData.Add(TEXT("href"), urlMatcher.GetCaptureGroup(0));

        FSlateHyperlinkRun::FOnClick OnHyperlinkClicked = FSlateHyperlinkRun::FOnClick::CreateStatic(&RichTextHelper::OpenUrl);
        TSharedRef<FSlateHyperlinkRun> HyperlinkRun = FSlateHyperlinkRun::Create(
            RunInfo,
            LineText,
            linkStyle,
            OnHyperlinkClicked,
            FSlateHyperlinkRun::FOnGenerateTooltip(),
            FSlateHyperlinkRun::FOnGetTooltipText(),
            newRange
        );
        hyperlinkRuns.insert(make_pair(matchStart, HyperlinkRun));
        foundLinkRanges.Add(newRange);
    }
}

void FOutputLogTextLayoutMarshaller::CreateFilepathHyperlinks(TSharedRef<FString> LineText, TSet<FTextRange> &foundLinkRanges, FHyperlinkStyle linkStyle, map<int32, TSharedRef<FSlateHyperlinkRun>> &hyperlinkRuns) const
{
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    FRegexMatcher pathMatcher(FilePathPattern, *LineText);
    while (pathMatcher.FindNext()) {
        int32 matchStart = pathMatcher.GetMatchBeginning();
        FTextRange newRange(matchStart, pathMatcher.GetMatchEnding());
        if (overlapping(newRange, foundLinkRanges)) {
            continue;
        }

        FString foundPath = pathMatcher.GetCaptureGroup(0);
        foundPath.ReplaceInline(ANSI_TO_TCHAR("\""), ANSI_TO_TCHAR(""));
        foundPath.ReplaceInline(ANSI_TO_TCHAR("'"), ANSI_TO_TCHAR(""));
        if (!(PlatformFile.DirectoryExists(*foundPath) || PlatformFile.FileExists(*foundPath))) {
            continue;
        }
        bool isSourceFile = foundPath.EndsWith(FString(".cpp")) || foundPath.EndsWith(FString(".h"));

        FRunInfo RunInfo(TEXT("a"));
        RunInfo.MetaData.Add(TEXT("href"), isSourceFile ? FString("Open source file") : FString("Open file or directory"));

        FSlateHyperlinkRun::FOnClick OnHyperlinkClicked = FSlateHyperlinkRun::FOnClick::CreateStatic(&RichTextHelper::OpenPath, foundPath, isSourceFile);
        TSharedRef<FSlateHyperlinkRun> HyperlinkRun = FSlateHyperlinkRun::Create(
            RunInfo,
            LineText,
            linkStyle,
            OnHyperlinkClicked,
            FSlateHyperlinkRun::FOnGenerateTooltip(),
            FSlateHyperlinkRun::FOnGetTooltipText(),
            newRange
        );
        hyperlinkRuns.insert(make_pair(matchStart, HyperlinkRun));
        foundLinkRanges.Add(newRange);
    }
}

void FOutputLogTextLayoutMarshaller::CreateBlueprintHyperlinks(const TArray<UBlueprint*>& blueprints, TSet<FTextRange>& foundLinkRanges, TSharedRef<FString> LineText, FHyperlinkStyle linkStyle, map<int32, TSharedRef<FSlateHyperlinkRun>> &hyperlinkRuns) const
{
    for (UBlueprint* bp : blueprints) {
        auto pathName = bp->GeneratedClass->GetPathName();

        TArray<UEdGraph*> allGraphs;
        bp->GetAllGraphs(allGraphs);
        // adds event graph, but only the generated intermediate representation, not the start nodes
        // allGraphs.Append(bp->EventGraphs);
        TMap<int32, UEdGraph*> graphIndices;
        for (auto graph : allGraphs) {
            auto fullGraphName = pathName + ":" + graph->GetName();
            int32 index = LineText->Find(fullGraphName);
            if (index > 0) {
                graphIndices.Add(index, graph);
                while (index >= 0 && index < LineText->Len()) {
                    index = LineText->Find(fullGraphName, ESearchCase::IgnoreCase, ESearchDir::FromStart, index + 1);
                    if (index > 0) {
                        graphIndices.Add(index, graph);
                    }
                }
                break;
            }
        }

        TArray<int32> indices;
        int32 foundIndex = -1;
        do {
            foundIndex = LineText->Find(pathName, ESearchCase::IgnoreCase, ESearchDir::FromStart, foundIndex + 1);
            if (foundIndex > 0) {
                indices.Add(foundIndex);
            }
        } while (foundIndex >= 0 && foundIndex < LineText->Len());

        for (int32 index : indices) {
            UEdGraph* foundGraph = graphIndices.Contains(index) ? graphIndices[index] : nullptr;
            int32 graphNameOffset = foundGraph ? foundGraph->GetName().Len() + 1 : 0;
            FTextRange newRange(index, index + pathName.Len() + graphNameOffset);
            if (overlapping(newRange, foundLinkRanges)) {
                continue;
            }
            FRunInfo RunInfo(TEXT("a"));
            RunInfo.MetaData.Add(TEXT("href"), FString("Open in Blueprint Editor"));

            FSlateHyperlinkRun::FOnClick OnHyperlinkClicked = FSlateHyperlinkRun::FOnClick::CreateStatic(&RichTextHelper::OpenBlueprint, bp, foundGraph);
            TSharedRef<FSlateHyperlinkRun> HyperlinkRun = FSlateHyperlinkRun::Create(
                RunInfo,
                LineText,
                linkStyle,
                OnHyperlinkClicked,
                FSlateHyperlinkRun::FOnGenerateTooltip(),
                FSlateHyperlinkRun::FOnGetTooltipText(),
                newRange
            );
            hyperlinkRuns.insert(make_pair(newRange.BeginIndex, HyperlinkRun));
            foundLinkRanges.Add(newRange);
        }
    }
}


FTextBlockStyle FOutputLogTextLayoutMarshaller::GetStyle(const TSharedPtr<FLogMessage>& Message, const ULogDisplaySettings* StyleSettings) const
{
    auto style = FTextBlockStyle(FEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>(Message->Style));
    if (StyleSettings->bDisplayTextShadow) {
        style
            .SetShadowColorAndOpacity(StyleSettings->ShadowColor)
            .SetShadowOffset(StyleSettings->ShadowOffset);
    }
    else {
        style
            .SetShadowColorAndOpacity(FLinearColor::Transparent)
            .SetShadowOffset(FVector2D::ZeroVector);
    }
    if (StyleSettings->bDisplayOutline) {
        style.Font.OutlineSettings.OutlineColor = StyleSettings->OutlineColor;
        style.Font.OutlineSettings.OutlineSize = StyleSettings->OutlineSize;
    }
    for (const FLogCategorySetting& logCategory : StyleSettings->LogCategories) {
        if (logCategory.CategorySearchString.IsEmpty()) {
            continue;
        }
        bool isMatch = false;
        if (logCategory.SearchAsRegex) {
            try {
                regex searchPattern(TCHAR_TO_ANSI(*logCategory.CategorySearchString), regex_constants::nosubs | regex_constants::icase);
                smatch matcher;
                isMatch = regex_search(Message->CString, matcher, searchPattern);
            }
            catch (std::regex_error&) {
                // just ignore the log category
            }
        }
        else {
            FTextFilterExpressionEvaluator evaluator(ETextFilterExpressionEvaluatorMode::BasicString);
            evaluator.SetFilterText(FText::FromString(logCategory.CategorySearchString));
            isMatch = evaluator.TestTextFilter(FLogFilter_TextFilterExpressionContext(*Message));
        }

        if (isMatch) {
            style.ColorAndOpacity = FSlateColor(logCategory.TextColor);
            if (StyleSettings->bDisplayTextShadow) {
                style.ShadowColorAndOpacity = logCategory.ShadowColor;
            }
            break;
        }
    }
    return style;
}

bool FOutputLogTextLayoutMarshaller::overlapping(const FTextRange& testRange, const TSet<FTextRange>& ranges)
{
    for (FTextRange range : ranges) {
        if ((range.BeginIndex <= testRange.BeginIndex && range.EndIndex >= testRange.BeginIndex) ||
            (range.BeginIndex <= testRange.EndIndex   && range.EndIndex >= testRange.EndIndex)) {
            return true;
        }
    }
    return false;
}

void FOutputLogTextLayoutMarshaller::ClearMessages()
{
    Messages.Empty();
    MakeDirty();
}

void FOutputLogTextLayoutMarshaller::CountMessages()
{
    // Do not re-count if not dirty
    if (!bNumMessagesCacheDirty)
    {
        return;
    }

    CachedNumMessages = 0;

    for (const auto& CurrentMessage : Messages)
    {
        if (Filter->IsMessageAllowed(CurrentMessage))
        {
            CachedNumMessages++;
        }
    }

    // Cache re-built, remove dirty flag
    bNumMessagesCacheDirty = false;
}

int32 FOutputLogTextLayoutMarshaller::GetNumMessages() const
{
    return Messages.Num();
}

int32 FOutputLogTextLayoutMarshaller::GetNumFilteredMessages()
{
    // No need to filter the messages if the filter is not set
    if (!Filter->IsFilterSet())
    {
        return GetNumMessages();
    }

    // Re-count messages if filter changed before we refresh
    if (bNumMessagesCacheDirty)
    {
        CountMessages();
    }

    return CachedNumMessages;
}

void FOutputLogTextLayoutMarshaller::MarkMessagesCacheAsDirty()
{
    bNumMessagesCacheDirty = true;
}

void FOutputLogTextLayoutMarshaller::MarkMessagesFilterAsDirty()
{
    for (const auto& CurrentMessage : Messages)
    {
        CurrentMessage->filter = UNKNOWN;
    }
}

FOutputLogTextLayoutMarshaller::FOutputLogTextLayoutMarshaller(TArray< TSharedPtr<FLogMessage> > InMessages, FLogFilter* InFilter)
    : Messages(MoveTemp(InMessages))
    , Filter(InFilter)
    , TextLayout(nullptr)
    , UrlPattern(FRegexPattern(FString("\\b(((https?://)?www\\d{0,3}[.]|(https?://))([^\\s()<>]+|\\(([^\\s()<>]+|(\\([^\\s()<>]+\\)))*\\))+(\\(([^\\s()<>]+|(\\([^\\s()<>]+\\)))*\\)|[^\\s`!()\\[\\]{};:'\"., <>?«»“”‘’]))")))
#if PLATFORM_MAC || PLATFORM_LINUX
    , FilePathPattern(FRegexPattern(FString("\"((?:/[^/]*)+)/?\"|((?:/[^/ \\n]*)+/?)")))
#else
    , FilePathPattern(FRegexPattern(FString("\"((?:[a-zA-Z]:|\\\\\\\\\\w[ \\w\\.]*)(?:[\\\\/]\\w[^\"\\n] * ) + )\"|'((?:[a-zA-Z]:|\\\\\\\\\\w[ \\w\\.]*)(?:[\\\\/]\\w[^'\\n]*)+)'|((?:[a-zA-Z]:|\\\\\\\\\\w[\\w\\.]*)(?:[\\\\/]\\w[^ \\n]*)+)")))
#endif
{
}

#include <iostream>



TSharedRef<FSlateTextLayout> FCustomTextLayout::CreateLayout(SWidget* InWidget, const FTextBlockStyle& InDefaultTextStyle)
{
    TSharedRef< FCustomTextLayout > Layout = MakeShareable(new FCustomTextLayout(InWidget, InDefaultTextStyle));
    Layout->AggregateChildren();
    return Layout;
}


FCustomTextLayout::FCustomTextLayout(SWidget* InWidget, FTextBlockStyle InDefaultTextStyle) : FSlateTextLayout(InWidget, MoveTemp(InDefaultTextStyle)) {

}

void FCustomTextLayout::RemoveSingleLineFromLayout()
{
    if (LineViews.Num() == 0) {
        return;
    }
    TextLayoutSize.Height -= LineViews.Last().Size.Y;
    if (TextLayoutSize.Height < 0) {
        TextLayoutSize.Height = 0;
    }
}

void FCustomTextLayout::AddEmptyRun()
{
    TSharedRef<FString> LineText = MakeShareable(new FString());
    TArray<TSharedRef<IRun>> Runs;
    Runs.Add(FSlateTextRun::Create(FRunInfo(), LineText, GetDefaultTextStyle()));

    AddLine(FTextLayout::FNewLineData(MoveTemp(LineText), MoveTemp(Runs)));
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SOutputLog::Construct(const FArguments& InArgs)
{
    MessagesTextMarshaller = FOutputLogTextLayoutMarshaller::Create(InArgs._Messages, &Filter);

    MessagesTextBox = SNew(SMultiLineEditableTextBox)
        .Style(FEditorStyle::Get(), "Log.TextBox")
        .TextStyle(FEditorStyle::Get(), "Log.Normal")
        .ForegroundColor(FLinearColor::Gray)
        .Marshaller(MessagesTextMarshaller)
        .IsReadOnly(true)
        .AlwaysShowScrollbars(true)
        .OnVScrollBarUserScrolled(this, &SOutputLog::OnUserScrolled)
        .CreateSlateTextLayout(FCreateSlateTextLayout::CreateStatic(&FCustomTextLayout::CreateLayout))
        .ContextMenuExtender(this, &SOutputLog::ExtendTextBoxMenu);

    ChildSlot
	[
		SNew(SVerticalBox)

		// Console output and filters
		+SVerticalBox::Slot()
		[
			SNew(SBorder)
			.Padding(3)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)

				// Output Log Filter
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 4.0f))
				[
					SNew(SHorizontalBox)
			
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SComboButton)
						.ComboButtonStyle(FEditorStyle::Get(), "GenericFilters.ComboButtonStyle")
						.ForegroundColor(FLinearColor::White)
						.ContentPadding(0)
						.ToolTipText(LOCTEXT("AddFilterToolTip", "Add an output log filter."))
						.OnGetMenuContent(this, &SOutputLog::MakeAddFilterMenu)
						.HasDownArrow(true)
						.ContentPadding(FMargin(1, 0))
						.ButtonContent()
						[
							SNew(SHorizontalBox)

							+SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(STextBlock)
								.TextStyle(FEditorStyle::Get(), "GenericFilters.TextStyle")
								.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.9"))
								.Text(FText::FromString(FString(TEXT("\xf0b0"))) /*fa-filter*/)
							]

							+SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(2, 0, 0, 0)
							[
								SNew(STextBlock)
								.TextStyle(FEditorStyle::Get(), "GenericFilters.TextStyle")
								.Text(LOCTEXT("Filters", "Filters"))
							]
						]
					]

					+SHorizontalBox::Slot()
					.Padding(4, 1, 0, 0)
					[
						SAssignNew(FilterTextBox, SSearchBox)
						.HintText(LOCTEXT("SearchLogHint", "Search Log"))
						.OnTextChanged(this, &SOutputLog::OnFilterTextChanged)
                        .OnTextCommitted(this, &SOutputLog::OnFilterTextCommitted)
						.DelayChangeNotificationsWhileTyping(true)
					]
				]

				// Output log area
				+SVerticalBox::Slot()
				.FillHeight(1)
				[
					MessagesTextBox.ToSharedRef()
				]
			]
		]

		// The console input box
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
		[
			SNew(SConsoleInputBox)
			.OnConsoleCommandExecuted(this, &SOutputLog::OnConsoleCommandExecuted)

			// Always place suggestions above the input line for the output log widget
			.SuggestionListPlacement(MenuPlacement_AboveAnchor)
		]
	];

    GLog->AddOutputDevice(this);

    bIsUserScrolled = false;
    RequestForceScroll();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

SOutputLog::~SOutputLog()
{
    if (GLog != NULL)
    {
        GLog->RemoveOutputDevice(this);
    }
}

bool SOutputLog::CreateLogMessages(const TCHAR* message, ELogVerbosity::Type Verbosity, const class FName& Category, const double Time, TArray< TSharedPtr<FLogMessage> >& OutMessages)
{
    if (Verbosity == ELogVerbosity::SetColor)
    {
        // Skip Color Events
        return false;
    }

    FName Style;
    if (Category == NAME_Cmd)
    {
        Style = FName(TEXT("Log.Command"));
    }
    else if (Verbosity == ELogVerbosity::Error)
    {
        Style = FName(TEXT("Log.Error"));
    }
    else if (Verbosity == ELogVerbosity::Warning)
    {
        Style = FName(TEXT("Log.Warning"));
    }
    else
    {
        Style = FName(TEXT("Log.Normal"));
    }

    // Determine how to format timestamps
    static ELogTimes::Type LogTimestampMode = ELogTimes::SinceGStartTime;
    if (UObjectInitialized() && !GExitPurge)
    {
        // Logging can happen very late during shutdown, even after the UObject system has been torn down, hence the init check above
        LogTimestampMode = GetDefault<UEditorStyleSettings>()->LogTimestampMode;
    }

    const int32 OldNumMessages = OutMessages.Num();

    // handle multiline strings by breaking them apart by line
    TArray<FTextRange> LineRanges;
    FString CurrentLogDump = message;
    FTextRange::CalculateLineRangesFromString(CurrentLogDump, LineRanges);

    bool bIsFirstLineInMessage = true;
    for (const FTextRange& LineRange : LineRanges)
    {
        if (!LineRange.IsEmpty())
        {
            FString Line = CurrentLogDump.Mid(LineRange.BeginIndex, LineRange.Len());
            Line = Line.ConvertTabsToSpaces(4);

            // Hard-wrap lines to avoid them being too long
            static const int32 HardWrapLen = 360;
            for (int32 CurrentStartIndex = 0; CurrentStartIndex < Line.Len();)
            {
                int32 HardWrapLineLen = 0;
                if (bIsFirstLineInMessage)
                {
                    FString MessagePrefix = FOutputDeviceHelper::FormatLogLine(Verbosity, Category, nullptr, LogTimestampMode, Time);

                    HardWrapLineLen = FMath::Min(HardWrapLen - MessagePrefix.Len(), Line.Len() - CurrentStartIndex);
                    FString HardWrapLine = Line.Mid(CurrentStartIndex, HardWrapLineLen);

                    OutMessages.Add(MakeShareable(new FLogMessage(MakeShareable(new FString(MessagePrefix + HardWrapLine)), Verbosity, Style, Category)));
                }
                else
                {
                    HardWrapLineLen = FMath::Min(HardWrapLen, Line.Len() - CurrentStartIndex);
                    FString HardWrapLine = Line.Mid(CurrentStartIndex, HardWrapLineLen);

                    OutMessages.Add(MakeShareable(new FLogMessage(MakeShareable(new FString(MoveTemp(HardWrapLine))), Verbosity, Style, Category)));
                }

                bIsFirstLineInMessage = false;
                CurrentStartIndex += HardWrapLineLen;
            }
        }
    }

    return OldNumMessages != OutMessages.Num();
}

void SOutputLog::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category)
{
    Serialize(V, Verbosity, Category, -1);
}

void SOutputLog::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category, const double Time)
{
    if (MessagesTextMarshaller->AppendMessage(V, Verbosity, Time, Category))
    {
        // Don't scroll to the bottom automatically when the user is scrolling the view or has scrolled it away from the bottom.
        if (!bIsUserScrolled)
        {
            RequestForceScroll();
        }
    }
}

void SOutputLog::ExtendTextBoxMenu(FMenuBuilder& Builder)
{
    FUIAction ClearOutputLogAction(
        FExecuteAction::CreateRaw(this, &SOutputLog::OnClearLog),
        FCanExecuteAction::CreateSP(this, &SOutputLog::CanClearLog)
    );

    Builder.AddMenuEntry(
        NSLOCTEXT("OutputLog", "ClearLogLabel", "Clear Log"),
        NSLOCTEXT("OutputLog", "ClearLogTooltip", "Clears all log messages"),
        FSlateIcon(),
        ClearOutputLogAction
    );
}

void SOutputLog::OnClearLog()
{
    // Make sure the cursor is back at the start of the log before we clear it
    MessagesTextBox->GoTo(FTextLocation(0));

    MessagesTextMarshaller->ClearMessages();
    MessagesTextBox->Refresh();
    bIsUserScrolled = false;
}

void SOutputLog::OnUserScrolled(float ScrollOffset)
{
    bIsUserScrolled = ScrollOffset < 1.0 && !FMath::IsNearlyEqual(ScrollOffset, 1.0f);
}

bool SOutputLog::CanClearLog() const
{
    return MessagesTextMarshaller->GetNumMessages() > 0;
}

void SOutputLog::OnConsoleCommandExecuted()
{
    RequestForceScroll();
}

void SOutputLog::RequestForceScroll()
{
    if (MessagesTextMarshaller->GetNumFilteredMessages() > 0)
    {
        MessagesTextBox->ScrollTo(FTextLocation(MessagesTextMarshaller->GetNumFilteredMessages() - 1));
        bIsUserScrolled = false;
    }
}

void SOutputLog::Refresh()
{
    // Re-count messages if filter changed before we refresh
    MessagesTextMarshaller->CountMessages();

    MessagesTextBox->GoTo(FTextLocation(0));
    MessagesTextMarshaller->MakeDirty();
    MessagesTextBox->Refresh();
    RequestForceScroll();
}

void SOutputLog::OnFilterTextChanged(const FText& InFilterText)
{
    // Flag the messages count as dirty
    MessagesTextMarshaller->MarkMessagesFilterAsDirty();
    MessagesTextMarshaller->MarkMessagesCacheAsDirty();

    // Set filter phrases
    Filter.SetFilterText(InFilterText);

    // Report possible syntax errors back to the user
    FilterTextBox->SetError(Filter.GetSyntaxErrors());

    // Repopulate the list to show only what has not been filtered out.
    Refresh();
}

void SOutputLog::OnFilterTextCommitted(const FText& InFilterText, ETextCommit::Type InCommitType)
{
    OnFilterTextChanged(InFilterText);
}

TSharedRef<SWidget> SOutputLog::MakeAddFilterMenu()
{
    FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, nullptr);
    FillVerbosityEntries(MenuBuilder);

    MenuBuilder.BeginSection("OutputLogSettingEntries");
    {
        MenuBuilder.AddMenuEntry(
            LOCTEXT("SearchRegex", "/Regex/ search"),
            LOCTEXT("SearchRegex_Tooltip", "The search function supports regular expressions"),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateSP(this, &SOutputLog::MenuRegex_Execute),
                FCanExecuteAction::CreateSP(this, &SOutputLog::Menu_CanExecute),
                FIsActionChecked::CreateSP(this, &SOutputLog::MenuRegex_IsChecked)),
            NAME_None,
            EUserInterfaceActionType::ToggleButton
        );

        MenuBuilder.AddMenuEntry(
            LOCTEXT("CollapseMessages", "Collapse Mode"),
            LOCTEXT("CollapseMessages_Tooltip", "Collapses adjacent identical messages into a single line"),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateSP(this, &SOutputLog::MenuCollapsed_Execute),
                FCanExecuteAction::CreateSP(this, &SOutputLog::Menu_CanExecute),
                FIsActionChecked::CreateSP(this, &SOutputLog::MenuCollapsed_IsChecked)),
            NAME_None,
            EUserInterfaceActionType::ToggleButton
        );

        MenuBuilder.AddMenuEntry(
            LOCTEXT("AntiSpamMessages", "Anti Spam Mode"),
            LOCTEXT("AntiSpamMessages_Tooltip", "Filters out common and uninteresting log messages"),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateSP(this, &SOutputLog::MenuAntiSpam_Execute),
                FCanExecuteAction::CreateSP(this, &SOutputLog::Menu_CanExecute),
                FIsActionChecked::CreateSP(this, &SOutputLog::MenuAntiSpam_IsChecked)),
            NAME_None,
            EUserInterfaceActionType::ToggleButton
        );

        MenuBuilder.AddMenuEntry(
            LOCTEXT("ShowCommandsMessages", "Show Commands"),
            LOCTEXT("ShowCommandsMessages_Tooltip", "If unchecked, all console command calls are filtered out"),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateSP(this, &SOutputLog::MenuShowCommands_Execute),
                FCanExecuteAction::CreateSP(this, &SOutputLog::Menu_CanExecute),
                FIsActionChecked::CreateSP(this, &SOutputLog::MenuShowCommands_IsChecked)),
            NAME_None,
            EUserInterfaceActionType::ToggleButton
        );
    }
    MenuBuilder.EndSection();

    return MenuBuilder.MakeWidget();
}

void SOutputLog::FillVerbosityEntries(FMenuBuilder& MenuBuilder)
{
    MenuBuilder.BeginSection("OutputLogVerbosityEntries");
    {
        MenuBuilder.AddMenuEntry(
            LOCTEXT("ShowMessages", "Messages"),
            LOCTEXT("ShowMessages_Tooltip", "Filter the Output Log to show messages"),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateSP(this, &SOutputLog::MenuLogs_Execute),
                FCanExecuteAction::CreateSP(this, &SOutputLog::Menu_CanExecute),
                FIsActionChecked::CreateSP(this, &SOutputLog::MenuLogs_IsChecked)),
            NAME_None,
            EUserInterfaceActionType::ToggleButton
        );

        MenuBuilder.AddMenuEntry(
            LOCTEXT("ShowWarnings", "Warnings"),
            LOCTEXT("ShowWarnings_Tooltip", "Filter the Output Log to show warnings"),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateSP(this, &SOutputLog::MenuWarnings_Execute),
                FCanExecuteAction::CreateSP(this, &SOutputLog::Menu_CanExecute),
                FIsActionChecked::CreateSP(this, &SOutputLog::MenuWarnings_IsChecked)),
            NAME_None,
            EUserInterfaceActionType::ToggleButton
        );

        MenuBuilder.AddMenuEntry(
            LOCTEXT("ShowErrors", "Errors"),
            LOCTEXT("ShowErrors_Tooltip", "Filter the Output Log to show errors"),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateSP(this, &SOutputLog::MenuErrors_Execute),
                FCanExecuteAction::CreateSP(this, &SOutputLog::Menu_CanExecute),
                FIsActionChecked::CreateSP(this, &SOutputLog::MenuErrors_IsChecked)),
            NAME_None,
            EUserInterfaceActionType::ToggleButton
        );
    }
    MenuBuilder.EndSection();
}

bool SOutputLog::Menu_CanExecute() const
{
    return true;
}

bool SOutputLog::MenuLogs_IsChecked() const
{
    return Filter.bShowLogs;
}

bool SOutputLog::MenuWarnings_IsChecked() const
{
    return Filter.bShowWarnings;
}

bool SOutputLog::MenuErrors_IsChecked() const
{
    return Filter.bShowErrors;
}

bool SOutputLog::MenuRegex_IsChecked() const
{
    return Filter.bUseRegex;
}

bool SOutputLog::MenuCollapsed_IsChecked() const
{
    return Filter.bCollapsedMode;
}

bool SOutputLog::MenuAntiSpam_IsChecked() const
{
    return Filter.bAntiSpamMode;
}

bool SOutputLog::MenuShowCommands_IsChecked() const
{
    return Filter.bShowCommands;
}

void SOutputLog::MenuShowCommands_Execute()
{
    Filter.bShowCommands = !Filter.bShowCommands;

    // Flag the messages count as dirty
    MessagesTextMarshaller->MarkMessagesFilterAsDirty();
    MessagesTextMarshaller->MarkMessagesCacheAsDirty();
    Refresh();
}

void SOutputLog::MenuAntiSpam_Execute()
{
    Filter.bAntiSpamMode = !Filter.bAntiSpamMode;

    // Flag the messages count as dirty
    MessagesTextMarshaller->MarkMessagesFilterAsDirty();
    MessagesTextMarshaller->MarkMessagesCacheAsDirty();
    Refresh();
}

void SOutputLog::MenuCollapsed_Execute()
{
    Filter.bCollapsedMode = !Filter.bCollapsedMode;

    // Flag the messages count as dirty
    MessagesTextMarshaller->MarkMessagesFilterAsDirty();
    MessagesTextMarshaller->MarkMessagesCacheAsDirty();
    Refresh();
}

void SOutputLog::MenuRegex_Execute()
{
    Filter.bUseRegex = !Filter.bUseRegex;

    // Flag the messages count as dirty
    MessagesTextMarshaller->MarkMessagesFilterAsDirty();
    MessagesTextMarshaller->MarkMessagesCacheAsDirty();
    Refresh();
}

void SOutputLog::MenuLogs_Execute()
{
    Filter.bShowLogs = !Filter.bShowLogs;

    // Flag the messages count as dirty
    MessagesTextMarshaller->MarkMessagesFilterAsDirty();
    MessagesTextMarshaller->MarkMessagesCacheAsDirty();
    Refresh();
}

void SOutputLog::MenuWarnings_Execute()
{
    Filter.bShowWarnings = !Filter.bShowWarnings;

    // Flag the messages count as dirty
    MessagesTextMarshaller->MarkMessagesFilterAsDirty();
    MessagesTextMarshaller->MarkMessagesCacheAsDirty();
    Refresh();
}

void SOutputLog::MenuErrors_Execute()
{
    Filter.bShowErrors = !Filter.bShowErrors;

    // Flag the messages count as dirty
    MessagesTextMarshaller->MarkMessagesFilterAsDirty();
    MessagesTextMarshaller->MarkMessagesCacheAsDirty();
    Refresh();
}

bool FLogFilter::IsMessageAllowed(const TSharedPtr<FLogMessage>& Message)
{
    if (Message->filter != UNKNOWN) {
        return Message->filter == VISIBLE;
    }
    bool visible = checkMessage(Message);
    Message->filter = visible ? VISIBLE : HIDDEN;
    return visible;
}

bool FLogFilter::checkMessage(const TSharedPtr<FLogMessage>& Message)
{
    // Filter Verbosity
    {
        if (Message->Verbosity == ELogVerbosity::Error && !bShowErrors)
        {
            Message->filter = HIDDEN;
            return false;
        }

        if (Message->Verbosity == ELogVerbosity::Warning && !bShowWarnings)
        {
            Message->filter = HIDDEN;
            return false;
        }

        if (Message->Verbosity != ELogVerbosity::Error && Message->Verbosity != ELogVerbosity::Warning && !bShowLogs)
        {
            Message->filter = HIDDEN;
            return false;
        }

        if (!bShowCommands && Message->Category == NAME_Cmd) {
            Message->filter = HIDDEN;
            return false;
        }
    }

    // AntiSpam filter
    if (bAntiSpamMode && Message->Verbosity != ELogVerbosity::Warning && Message->Verbosity != ELogVerbosity::Error) {
        smatch matcher;
        bool found = regex_search(Message->CString, matcher, antiSpamRegex);
        if (found) {
            return false;
        }
    }

    // Filter search phrase
    if (bUseRegex) {
        if (TextFilterExpressionEvaluator.GetFilterText().IsEmpty()) {
            return true;
        }
        smatch matcher;
        if (!regex_search(Message->CString, matcher, bIsRegexValid ? searchRegex : lastValidRegex)) {
            return false;
        }
    }
    else if (!TextFilterExpressionEvaluator.TestTextFilter(FLogFilter_TextFilterExpressionContext(*Message)))
    {
        return false;
    }

    return true;
}

FText FLogFilter::getInValidRegexText()
{
    return LOCTEXT("InvalidRegex", "Invalid regex");
}

#undef LOCTEXT_NAMESPACE

