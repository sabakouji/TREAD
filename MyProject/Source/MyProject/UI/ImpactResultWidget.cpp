// 踏みつけ加速メカゲーム — リザルト画面

#include "UI/ImpactResultWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/ImpactPlayerController.h"
#include "Tuning/ScoreTuningDataAsset.h"
#include "UI/ImpactWidgetHelpers.h"

#define LOCTEXT_NAMESPACE "ImpactResult"

namespace
{
	/** 子を縦に積み、上に Spacing の間隔を空ける。 */
	UVerticalBoxSlot* AddSection(UVerticalBox& Box, UWidget& Child, float Spacing)
	{
		UVerticalBoxSlot* BoxSlot = Box.AddChildToVerticalBox(&Child);
		BoxSlot->SetHorizontalAlignment(HAlign_Center);
		BoxSlot->SetPadding(FMargin(0.0f, Spacing, 0.0f, 0.0f));
		return BoxSlot;
	}

	/** 固定幅の列を作る。 */
	USizeBox* MakeColumn(UWidgetTree& Tree, UWidget& Content, float Width)
	{
		USizeBox* Column = Tree.ConstructWidget<USizeBox>();
		Column->SetWidthOverride(Width);
		Column->AddChild(&Content);
		return Column;
	}
}

void UImpactResultWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	BuildLayout();
}

void UImpactResultWidget::BuildLayout()
{
	if (!WidgetTree)
	{
		UE_LOG(LogImpactUI, Error, TEXT("%s has no widget tree; the result screen is not built"), *GetName());
		return;
	}

	if (WidgetTree->RootWidget)
	{
		UE_LOG(LogImpactUI, Warning, TEXT("%s: the designer layout is replaced by the C++ layout"), *GetName());
	}

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ResultRoot"));
	WidgetTree->RootWidget = Root;

	// 一時停止したゲーム画面の上に、半透明の背景を敷く。
	UImage* Backdrop = WidgetTree->ConstructWidget<UImage>();
	Backdrop->SetColorAndOpacity(BackdropColor);
	UOverlaySlot* BackdropSlot = Root->AddChildToOverlay(Backdrop);
	BackdropSlot->SetHorizontalAlignment(HAlign_Fill);
	BackdropSlot->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
	UOverlaySlot* ContentSlot = Root->AddChildToOverlay(Content);
	ContentSlot->SetHorizontalAlignment(HAlign_Center);
	ContentSlot->SetVerticalAlignment(VAlign_Center);

	ResultText = ImpactWidget::MakeText(*WidgetTree, TitleFontSize, TextColor, ETextJustify::Center);
	AddSection(*Content, *ResultText, 0.0f);

	RowsBox = WidgetTree->ConstructWidget<UVerticalBox>();
	AddSection(*Content, *MakeColumn(*WidgetTree, *RowsBox, PanelWidth), SectionSpacing);

	FinalText = ImpactWidget::MakeText(*WidgetTree, FinalFontSize, TextColor, ETextJustify::Center);
	AddSection(*Content, *FinalText, SectionSpacing);

	InfoText = ImpactWidget::MakeText(*WidgetTree, InfoFontSize, SubTextColor, ETextJustify::Center);
	AddSection(*Content, *InfoText, 0.0f);

	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
	RetryButton = ImpactWidget::MakeButton(*WidgetTree, LOCTEXT("Retry", "RETRY"), ButtonFontSize, TextColor, ButtonColor);
	TitleButton = ImpactWidget::MakeButton(*WidgetTree, LOCTEXT("Title", "TITLE"), ButtonFontSize, TextColor, ButtonColor);
	Buttons->AddChildToHorizontalBox(RetryButton);
	UHorizontalBoxSlot* TitleSlot = Buttons->AddChildToHorizontalBox(TitleButton);
	TitleSlot->SetPadding(FMargin(ButtonSpacing, 0.0f, 0.0f, 0.0f));
	AddSection(*Content, *Buttons, SectionSpacing);

	RetryButton->OnClicked.AddDynamic(this, &UImpactResultWidget::HandleRetryClicked);
	TitleButton->OnClicked.AddDynamic(this, &UImpactResultWidget::HandleTitleClicked);

	UE_LOG(LogImpactUI, Log, TEXT("%s: result layout built"), *GetName());
}

void UImpactResultWidget::ShowResult(EImpactMatchResult Result, const FImpactScoreTally& Tally,
	const UScoreTuningDataAsset* ScoreTuning, float ElapsedSeconds)
{
	if (!ResultText)
	{
		return;
	}

	ResultText->SetText(FText::FromString(LexToDisplayString(Result)));
	ResultText->SetColorAndOpacity(FSlateColor(GetResultColor(Result)));

	// 内訳の各行は「回数 × 配点 = 点数」。配点がなければ回数だけを示す。
	const bool bHasTuning = ScoreTuning != nullptr;
	const auto CountTimes = [bHasTuning](int32 Count, int32 PointsEach)
	{
		return bHasTuning
			? FText::Format(LOCTEXT("CountTimes", "{0} x {1}"), FText::AsNumber(Count), FText::AsNumber(PointsEach))
			: FText::AsNumber(Count);
	};

	RowsBox->ClearChildren();
	const int32 GoalEach = bHasTuning ? ScoreTuning->PointsPerGoalDamage : 0;
	const int32 EnemyEach = bHasTuning ? ScoreTuning->PointsPerEnemyDefeated : 0;
	const int32 LargeEach = bHasTuning ? ScoreTuning->PointsPerObstacleLarge : 0;
	const int32 SmallEach = bHasTuning ? ScoreTuning->PointsPerObstacleSmall : 0;
	const int32 StompEach = bHasTuning ? ScoreTuning->PointsPerStomp : 0;
	const int32 CrashEach = bHasTuning ? -ScoreTuning->PenaltyPerCrash : 0;

	AddRow(LOCTEXT("GoalDamage", "Goal Damage"), CountTimes(Tally.GoalDamage, GoalEach), Tally.GoalDamage * GoalEach, bHasTuning);
	AddRow(LOCTEXT("Enemies", "Enemies Defeated"), CountTimes(Tally.EnemiesDefeated, EnemyEach), Tally.EnemiesDefeated * EnemyEach, bHasTuning);
	AddRow(LOCTEXT("Large", "Large Destroyed"), CountTimes(Tally.ObstaclesLarge, LargeEach), Tally.ObstaclesLarge * LargeEach, bHasTuning);
	AddRow(LOCTEXT("Small", "Small Destroyed"), CountTimes(Tally.ObstaclesSmall, SmallEach), Tally.ObstaclesSmall * SmallEach, bHasTuning);
	AddRow(LOCTEXT("Stomps", "Stomps"), CountTimes(Tally.Stomps, StompEach), Tally.Stomps * StompEach, bHasTuning);
	AddRow(LOCTEXT("TopSpeed", "Top Speed Bonus"),
		FText::Format(LOCTEXT("SpeedValue", "{0} uu/s"), FText::AsNumber(FMath::RoundToInt(Tally.MaxSpeed))),
		bHasTuning ? ScoreTuning->ComputeMaxSpeedBonus(Tally.MaxSpeed) : 0, bHasTuning);
	AddRow(LOCTEXT("Crashes", "Crashes"), CountTimes(Tally.Crashes, CrashEach), Tally.Crashes * CrashEach, bHasTuning);

	FinalText->SetText(bHasTuning
		? FText::Format(LOCTEXT("Final", "FINAL SCORE  {0}"), FText::AsNumber(ScoreTuning->ComputeTotal(Tally)))
		: LOCTEXT("FinalUnavailable", "FINAL SCORE  -"));

	InfoText->SetText(FText::Format(LOCTEXT("Info", "TIME {0}    TOP SPEED {1} uu/s"),
		ImpactWidget::FormatClock(ElapsedSeconds, false), FText::AsNumber(FMath::RoundToInt(Tally.MaxSpeed))));

	UE_LOG(LogImpactUI, Log, TEXT("result shown: %s, final score %s"), LexToDisplayString(Result),
		bHasTuning ? *FString::FromInt(ScoreTuning->ComputeTotal(Tally)) : TEXT("-"));
}

void UImpactResultWidget::AddRow(const FText& Label, const FText& Detail, int32 Points, bool bShowPoints)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();

	UTextBlock* LabelText = ImpactWidget::MakeText(*WidgetTree, RowFontSize, TextColor);
	LabelText->SetText(Label);
	UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(LabelText);
	LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UTextBlock* DetailText = ImpactWidget::MakeText(*WidgetTree, RowFontSize, SubTextColor, ETextJustify::Right);
	DetailText->SetText(Detail);
	Row->AddChildToHorizontalBox(MakeColumn(*WidgetTree, *DetailText, DetailColumnWidth));

	UTextBlock* PointsText = ImpactWidget::MakeText(*WidgetTree, RowFontSize, Points < 0 ? PenaltyColor : TextColor, ETextJustify::Right);
	PointsText->SetText(bShowPoints ? FText::AsNumber(Points) : FText::GetEmpty());
	Row->AddChildToHorizontalBox(MakeColumn(*WidgetTree, *PointsText, PointsColumnWidth));

	RowsBox->AddChildToVerticalBox(Row);
}

FLinearColor UImpactResultWidget::GetResultColor(EImpactMatchResult Result) const
{
	return Result == EImpactMatchResult::TimeUp ? TimeUpColor : TextColor;
}

void UImpactResultWidget::HandleRetryClicked()
{
	DisableButtons();

	if (AImpactPlayerController* Controller = GetOwningPlayer<AImpactPlayerController>())
	{
		Controller->RequestRetry();
	}
}

void UImpactResultWidget::HandleTitleClicked()
{
	DisableButtons();

	if (AImpactPlayerController* Controller = GetOwningPlayer<AImpactPlayerController>())
	{
		Controller->RequestReturnToTitle();
	}
}

void UImpactResultWidget::DisableButtons()
{
	RetryButton->SetIsEnabled(false);
	TitleButton->SetIsEnabled(false);
}

#undef LOCTEXT_NAMESPACE
