// 踏みつけ加速メカゲーム — プレイ中の画面表示

#include "UI/ImpactHUDWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Collision/ImpactResolver.h"
#include "Collision/ImpactTypes.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/ImpactGameState.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Score/ImpactScoreSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Tuning/ImpactTuningDataAsset.h"
#include "Tuning/ScoreTuningDataAsset.h"
#include "Tuning/VehicleTuningDataAsset.h"
#include "UI/ImpactWidgetHelpers.h"
#include "Vehicle/ImpactLockOnComponent.h"
#include "Vehicle/ImpactVehiclePawn.h"

#define LOCTEXT_NAMESPACE "ImpactHUD"

namespace
{
	/** 表示を隠している状態。前回表示した値（FShownValues）の記録に使う。値としては現れない。 */
	constexpr int32 HiddenValue = TNumericLimits<int32>::Max();

	/** 残り時間の欄: 制限時間なし（FREE PLAY）の表示。 */
	constexpr int32 FreePlayClock = -1;

	/** カウントダウンの欄: 開始の合図（GO!）の表示。 */
	constexpr int32 GoSignal = -1;

	/** 速度ゲージの塗り色の区分。 */
	constexpr int32 FillNone = 0;
	constexpr int32 FillSmall = 1;
	constexpr int32 FillLarge = 2;
	constexpr int32 FillOverdrive = 3;

	/** レティクルの表示の区分。非表示は HiddenValue で表す。 */
	constexpr int32 ReticleIdle = 0;
	constexpr int32 ReticleActive = 1;

	/** 前回表示した値と比べ、異なれば記録を更新して true を返す。 */
	bool UpdateShown(int32& Shown, int32 Value)
	{
		if (Shown == Value)
		{
			return false;
		}

		Shown = Value;
		return true;
	}

	/**
	 * 子をキャンバスの Anchor（0〜1）の点に、同じ点を基準として置く。
	 * 例えば (1, 0) なら、子の右上を画面の右上へ合わせる。
	 */
	void AddAnchored(UCanvasPanel& Canvas, UWidget& Child, const FVector2D& Anchor, const FVector2D& Offset)
	{
		UCanvasPanelSlot* CanvasSlot = Canvas.AddChildToCanvas(&Child);
		CanvasSlot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
		CanvasSlot->SetAlignment(Anchor);
		CanvasSlot->SetAutoSize(true);
		CanvasSlot->SetPosition(Offset);
	}

	void AddToVerticalBox(UVerticalBox& Box, UWidget& Child, EHorizontalAlignment Alignment)
	{
		UVerticalBoxSlot* BoxSlot = Box.AddChildToVerticalBox(&Child);
		BoxSlot->SetHorizontalAlignment(Alignment);
	}
}

void UImpactHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	BuildLayout();

	// 画面全体に重なるが、操作は受け取らない。
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UImpactHUDWidget::BuildLayout()
{
	if (!WidgetTree)
	{
		UE_LOG(LogImpactUI, Error, TEXT("%s has no widget tree; the HUD is not built"), *GetName());
		return;
	}

	if (WidgetTree->RootWidget)
	{
		UE_LOG(LogImpactUI, Warning, TEXT("%s: the designer layout is replaced by the C++ layout"), *GetName());
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HUDRoot"));
	WidgetTree->RootWidget = Root;

	// 上中央: 残り時間と得点。
	UVerticalBox* TopCenter = WidgetTree->ConstructWidget<UVerticalBox>();
	TimeText = ImpactWidget::MakeText(*WidgetTree, TimeFontSize, TextColor, ETextJustify::Center);
	ScoreText = ImpactWidget::MakeText(*WidgetTree, ScoreFontSize, TextColor, ETextJustify::Center);
	AddToVerticalBox(*TopCenter, *TimeText, HAlign_Center);
	AddToVerticalBox(*TopCenter, *ScoreText, HAlign_Center);
	AddAnchored(*Root, *TopCenter, FVector2D(0.5f, 0.0f), FVector2D(0.0f, ScreenMargin.Y));

	// 右上: ゴール耐久値と自滅回数。
	UVerticalBox* TopRight = WidgetTree->ConstructWidget<UVerticalBox>();
	GoalText = ImpactWidget::MakeText(*WidgetTree, InfoFontSize, TextColor, ETextJustify::Right);
	CrashText = ImpactWidget::MakeText(*WidgetTree, InfoFontSize, TextColor, ETextJustify::Right);
	AddToVerticalBox(*TopRight, *GoalText, HAlign_Right);
	AddToVerticalBox(*TopRight, *CrashText, HAlign_Right);
	AddAnchored(*Root, *TopRight, FVector2D(1.0f, 0.0f), FVector2D(-ScreenMargin.X, ScreenMargin.Y));

	// 中央: カウントダウンと開始の合図。
	CountdownText = ImpactWidget::MakeText(*WidgetTree, CountdownFontSize, TextColor, ETextJustify::Center);
	CountdownText->SetVisibility(ESlateVisibility::Collapsed);
	AddAnchored(*Root, *CountdownText, FVector2D(0.5f, 0.5f), FVector2D(0.0f, CountdownOffsetY));

	// 下中央: 速度ゲージ。破壊閾値の位置に目印を重ね、「あといくつで何を壊せるか」を示す。
	UVerticalBox* Bottom = WidgetTree->ConstructWidget<UVerticalBox>();
	OverdriveText = ImpactWidget::MakeText(*WidgetTree, SpeedFontSize, GaugeOverdriveColor, ETextJustify::Center);
	OverdriveText->SetText(LOCTEXT("Overdrive", "OVERDRIVE"));
	OverdriveText->SetVisibility(ESlateVisibility::Collapsed);
	AddToVerticalBox(*Bottom, *OverdriveText, HAlign_Center);

	SpeedText = ImpactWidget::MakeText(*WidgetTree, SpeedFontSize, TextColor, ETextJustify::Center);
	AddToVerticalBox(*Bottom, *SpeedText, HAlign_Center);

	USizeBox* GaugeBox = WidgetTree->ConstructWidget<USizeBox>();
	GaugeBox->SetWidthOverride(GaugeWidth);
	GaugeBox->SetHeightOverride(GaugeHeight);
	UOverlay* Gauge = WidgetTree->ConstructWidget<UOverlay>();
	GaugeBox->AddChild(Gauge);

	SpeedBar = WidgetTree->ConstructWidget<UProgressBar>();
	UOverlaySlot* BarSlot = Gauge->AddChildToOverlay(SpeedBar);
	BarSlot->SetHorizontalAlignment(HAlign_Fill);
	BarSlot->SetVerticalAlignment(VAlign_Fill);

	SmallMarker = AddGaugeMarker(*Gauge);
	LargeMarker = AddGaugeMarker(*Gauge);
	OverdriveMarker = AddGaugeMarker(*Gauge);
	OverdriveMarker->SetColorAndOpacity(GaugeOverdriveColor);
	AddToVerticalBox(*Bottom, *GaugeBox, HAlign_Center);

	USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>();
	LabelBox->SetWidthOverride(GaugeWidth);
	UOverlay* LabelRow = WidgetTree->ConstructWidget<UOverlay>();
	LabelBox->AddChild(LabelRow);
	SmallMarkerLabel = AddMarkerLabel(*LabelRow);
	LargeMarkerLabel = AddMarkerLabel(*LabelRow);
	OverdriveMarkerLabel = AddMarkerLabel(*LabelRow);
	OverdriveMarkerLabel->SetColorAndOpacity(GaugeOverdriveColor);
	AddToVerticalBox(*Bottom, *LabelBox, HAlign_Center);

	AddAnchored(*Root, *Bottom, FVector2D(0.5f, 1.0f), FVector2D(0.0f, -ScreenMargin.Y));
	SpeedGauge = Bottom;
	if (!bShowSpeedGauge)
	{
		SpeedGauge->SetVisibility(ESlateVisibility::Collapsed);
	}

	// 画面中央の周り: 狙われている方向。印を方向へ回し、文字は位置を変えずに出す。
	LockWarningMarker = WidgetTree->ConstructWidget<UImage>();
	LockWarningMarker->SetColorAndOpacity(LockWarningColor);
	LockWarningMarker->SetDesiredSizeOverride(LockWarningSize);
	LockWarningMarker->SetVisibility(ESlateVisibility::Collapsed);
	AddAnchored(*Root, *LockWarningMarker, FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);

	LockWarningText = ImpactWidget::MakeText(*WidgetTree, InfoFontSize, LockWarningColor, ETextJustify::Center);
	LockWarningText->SetText(LOCTEXT("LockedOn", "LOCKED ON"));
	LockWarningText->SetVisibility(ESlateVisibility::Collapsed);
	AddAnchored(*Root, *LockWarningText, FVector2D(0.5f, 0.5f), FVector2D(0.0f, LockWarningRadius + ScreenMargin.Y));

	// 捕捉している相手: 四隅のカギ括弧のレティクル。画像アセットを使わず線で組み立てる。
	LockOnReticle = WidgetTree->ConstructWidget<USizeBox>();
	LockOnReticle->SetWidthOverride(LockOnReticleSize);
	LockOnReticle->SetHeightOverride(LockOnReticleSize);
	UOverlay* ReticleFrame = WidgetTree->ConstructWidget<UOverlay>();
	LockOnReticle->AddChild(ReticleFrame);

	const FVector2D HorizontalBar(LockOnReticleCornerLength, LockOnReticleThickness);
	const FVector2D VerticalBar(LockOnReticleThickness, LockOnReticleCornerLength);
	for (const EHorizontalAlignment Horizontal : { HAlign_Left, HAlign_Right })
	{
		for (const EVerticalAlignment Vertical : { VAlign_Top, VAlign_Bottom })
		{
			AddReticleBar(*ReticleFrame, Horizontal, Vertical, HorizontalBar);
			AddReticleBar(*ReticleFrame, Horizontal, Vertical, VerticalBar);
		}
	}

	// 画面左上を原点に、レティクルの中心を相手の画面位置へ合わせる。
	LockOnReticle->SetVisibility(ESlateVisibility::Collapsed);
	UCanvasPanelSlot* ReticleSlot = Root->AddChildToCanvas(LockOnReticle);
	ReticleSlot->SetAnchors(FAnchors(0.0f, 0.0f));
	ReticleSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	ReticleSlot->SetAutoSize(true);

	UE_LOG(LogImpactUI, Log, TEXT("%s: HUD layout built"), *GetName());
}

UImage* UImpactHUDWidget::AddGaugeMarker(UOverlay& Gauge)
{
	UImage* Marker = WidgetTree->ConstructWidget<UImage>();
	Marker->SetColorAndOpacity(MarkerColor);
	Marker->SetDesiredSizeOverride(FVector2D(MarkerWidth, GaugeHeight));

	UOverlaySlot* MarkerSlot = Gauge.AddChildToOverlay(Marker);
	MarkerSlot->SetHorizontalAlignment(HAlign_Left);
	MarkerSlot->SetVerticalAlignment(VAlign_Fill);
	return Marker;
}

void UImpactHUDWidget::AddReticleBar(
	UOverlay& Frame, EHorizontalAlignment Horizontal, EVerticalAlignment Vertical, const FVector2D& Size)
{
	UImage* Bar = WidgetTree->ConstructWidget<UImage>();
	Bar->SetColorAndOpacity(LockOnReticleIdleColor);
	Bar->SetDesiredSizeOverride(Size);

	UOverlaySlot* BarSlot = Frame.AddChildToOverlay(Bar);
	BarSlot->SetHorizontalAlignment(Horizontal);
	BarSlot->SetVerticalAlignment(Vertical);
	ReticleBars.Add(Bar);
}

UTextBlock* UImpactHUDWidget::AddMarkerLabel(UOverlay& LabelRow)
{
	UTextBlock* Label = ImpactWidget::MakeText(*WidgetTree, MarkerLabelFontSize, MarkerColor, ETextJustify::Center);

	UOverlaySlot* LabelSlot = LabelRow.AddChildToOverlay(Label);
	LabelSlot->SetHorizontalAlignment(HAlign_Left);
	return Label;
}

void UImpactHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 組み立てに失敗している場合は何も更新しない（原因は BuildLayout がログに残す）。
	if (!TimeText)
	{
		return;
	}

	const UWorld* World = GetWorld();
	const AImpactGameState* State = World ? World->GetGameState<AImpactGameState>() : nullptr;
	const AImpactVehiclePawn* Vehicle = Cast<AImpactVehiclePawn>(GetOwningPlayerPawn());
	UpdateMatchInfo(State);
	UpdateScore(State);
	if (bShowSpeedGauge)
	{
		UpdateSpeedGauge(Vehicle);
	}
	UpdateLockWarning(Vehicle);
	UpdateLockOnReticle(Vehicle);
}

void UImpactHUDWidget::UpdateMatchInfo(const AImpactGameState* State)
{
	const EImpactMatchState MatchState = State ? State->GetMatchState() : EImpactMatchState::WaitingToStart;

	// 残り時間。表示は秒単位（切り上げ）なので、秒が変わったときだけ書き換える。
	int32 ClockSeconds = HiddenValue;
	if (State)
	{
		ClockSeconds = State->HasTimeLimit() ? FMath::CeilToInt(State->GetRemainingTime()) : FreePlayClock;
	}

	if (UpdateShown(Shown.ClockSeconds, ClockSeconds))
	{
		if (ClockSeconds == HiddenValue)
		{
			TimeText->SetText(FText::GetEmpty());
		}
		else if (ClockSeconds == FreePlayClock)
		{
			TimeText->SetText(LOCTEXT("FreePlay", "FREE PLAY"));
		}
		else
		{
			TimeText->SetText(ImpactWidget::FormatClock(static_cast<float>(ClockSeconds), true));
		}
	}

	const bool bLowTime = State && State->HasTimeLimit()
		&& MatchState == EImpactMatchState::InProgress
		&& State->GetRemainingTime() <= LowTimeWarningSeconds;
	if (UpdateShown(Shown.TimeWarning, bLowTime ? 1 : 0))
	{
		TimeText->SetColorAndOpacity(FSlateColor(bLowTime ? WarnColor : TextColor));
	}

	// 自滅回数。
	const int32 CrashCount = State ? State->GetCrashCount() : HiddenValue;
	if (UpdateShown(Shown.CrashCount, CrashCount))
	{
		CrashText->SetText(CrashCount == HiddenValue
			? FText::GetEmpty()
			: FText::Format(LOCTEXT("Crash", "CRASH  {0}"), FText::AsNumber(CrashCount)));
	}

	// カウントダウンの数字、開始直後の合図、またはどちらもなし。
	int32 Countdown = HiddenValue;
	if (MatchState == EImpactMatchState::Countdown)
	{
		Countdown = FMath::Max(FMath::CeilToInt(State->GetCountdownRemaining()), 1);
	}
	else if (MatchState == EImpactMatchState::InProgress && State->HasTimeLimit() && State->GetElapsedTime() < GoSignalDuration)
	{
		Countdown = GoSignal;
	}

	if (UpdateShown(Shown.Countdown, Countdown))
	{
		if (Countdown == HiddenValue)
		{
			CountdownText->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			CountdownText->SetText(Countdown == GoSignal ? LOCTEXT("Go", "GO!") : FText::AsNumber(Countdown));
			CountdownText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
}

void UImpactHUDWidget::UpdateScore(const AImpactGameState* State)
{
	// 配点とゴールの状態は GameState から読む。GameMode はサーバにしか存在しないため画面からは参照しない。
	const UGameInstance* GameInstance = GetGameInstance();
	const UImpactScoreSubsystem* Score = GameInstance ? GameInstance->GetSubsystem<UImpactScoreSubsystem>() : nullptr;
	const UScoreTuningDataAsset* ScoreTuning = State ? State->GetScoreTuning() : nullptr;

	// 得点は減点で負にもなるため、隠している状態は値の範囲外（HiddenValue）で表す。
	const int32 TotalScore = (Score && ScoreTuning) ? ScoreTuning->ComputeTotal(Score->GetTally()) : HiddenValue;
	if (UpdateShown(Shown.Score, TotalScore))
	{
		if (TotalScore == HiddenValue)
		{
			ScoreText->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			ScoreText->SetText(FText::Format(LOCTEXT("Score", "SCORE  {0}"), FText::AsNumber(TotalScore)));
			ScoreText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}

	const bool bHasGoal = State && State->HasGoal();
	const int32 Durability = bHasGoal ? State->GetGoalDurability() : HiddenValue;
	const int32 MaxDurability = bHasGoal ? State->GetGoalMaxDurability() : HiddenValue;
	const bool bDurabilityChanged = UpdateShown(Shown.GoalDurability, Durability);
	const bool bMaxDurabilityChanged = UpdateShown(Shown.GoalMaxDurability, MaxDurability);
	if (bDurabilityChanged || bMaxDurabilityChanged)
	{
		if (Durability == HiddenValue)
		{
			GoalText->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			GoalText->SetText(FText::Format(LOCTEXT("Goal", "GOAL  {0} / {1}"),
				FText::AsNumber(Durability), FText::AsNumber(MaxDurability)));
			GoalText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
}

void UImpactHUDWidget::UpdateSpeedGauge(const AImpactVehiclePawn* Vehicle)
{
	const UVehicleTuningDataAsset* Tuning = Vehicle ? Vehicle->GetTuning() : nullptr;
	if (!Tuning || Tuning->MaxSpeed <= 0.0f)
	{
		if (UpdateShown(Shown.Speed, HiddenValue))
		{
			SpeedText->SetText(FText::GetEmpty());
		}
		SpeedBar->SetPercent(0.0f);
		return;
	}

	// バーの割合は連続的に変わるため毎フレーム更新する。
	const float Speed = Vehicle->GetCurrentSpeed();
	SpeedBar->SetPercent(FMath::Clamp(Speed / Tuning->MaxSpeed, 0.0f, 1.0f));

	const int32 RoundedSpeed = FMath::RoundToInt(Speed);
	if (UpdateShown(Shown.Speed, RoundedSpeed))
	{
		SpeedText->SetText(FText::Format(LOCTEXT("Speed", "{0} uu/s"), FText::AsNumber(RoundedSpeed)));
	}

	// 今の速度で何を壊せるかをゲージの色で示す。色は区分が変わったときだけ塗り替える。
	const UImpactTuningDataAsset* Rules = Vehicle->GetImpactTuning();
	const bool bOverdrive = Rules && Rules->IsOverdrive(Speed);

	EDestructionRank Rank = EDestructionRank::Small;
	const bool bCanDestroy = FImpactResolver::GetDestroyableRank(Speed, *Tuning, Rank);
	int32 FillRank = !bCanDestroy ? FillNone : (Rank == EDestructionRank::Large ? FillLarge : FillSmall);
	if (bOverdrive)
	{
		FillRank = FillOverdrive;
	}

	if (UpdateShown(Shown.FillRank, FillRank))
	{
		FLinearColor FillColor = GaugeNoneColor;
		if (FillRank == FillOverdrive)
		{
			FillColor = GaugeOverdriveColor;
		}
		else if (FillRank == FillLarge)
		{
			FillColor = GaugeLargeColor;
		}
		else if (FillRank == FillSmall)
		{
			FillColor = GaugeSmallColor;
		}
		SpeedBar->SetFillColorAndOpacity(FillColor);
	}

	// 超加速は相手の機体を破壊できる状態そのもの。速度の数値だけでは読み取れないため文字でも示す。
	if (UpdateShown(Shown.Overdrive, bOverdrive ? 1 : 0))
	{
		OverdriveText->SetVisibility(bOverdrive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// 閾値は PIE 中の調整で変わりうるため、比較は毎フレーム行い、変わったときだけ数値を書き換える。
	const int32 SmallThreshold = FMath::RoundToInt(Tuning->SmallDestructionSpeed);
	if (UpdateShown(Shown.SmallThreshold, SmallThreshold))
	{
		SmallMarkerLabel->SetText(FText::AsNumber(SmallThreshold));
	}

	const int32 LargeThreshold = FMath::RoundToInt(Tuning->LargeDestructionSpeed);
	if (UpdateShown(Shown.LargeThreshold, LargeThreshold))
	{
		LargeMarkerLabel->SetText(FText::AsNumber(LargeThreshold));
	}

	const float OverdriveSpeed = Rules ? Rules->OverdriveSpeed : 0.0f;
	if (UpdateShown(Shown.OverdriveThreshold, FMath::RoundToInt(OverdriveSpeed)))
	{
		OverdriveMarkerLabel->SetText(FText::AsNumber(FMath::RoundToInt(OverdriveSpeed)));
	}

	// 目印の位置は前フレームの描画幅を使って中央をそろえるため、差分化せず毎フレーム置き直す。
	PlaceMarker(*SmallMarker, *SmallMarkerLabel, Tuning->SmallDestructionSpeed / Tuning->MaxSpeed);
	PlaceMarker(*LargeMarker, *LargeMarkerLabel, Tuning->LargeDestructionSpeed / Tuning->MaxSpeed);
	PlaceMarker(*OverdriveMarker, *OverdriveMarkerLabel, OverdriveSpeed / Tuning->MaxSpeed);
}

void UImpactHUDWidget::UpdateLockWarning(const AImpactVehiclePawn* Vehicle)
{
	const UImpactLockOnComponent* LockOn = Vehicle ? Vehicle->GetLockOn() : nullptr;

	// 複数から狙われている場合は、まず1機分だけ示す（同時被ロックの表示は企画書 4章の未決定事項）。
	const AImpactVehiclePawn* Locker = nullptr;
	if (LockOn)
	{
		for (const TWeakObjectPtr<AImpactVehiclePawn>& Entry : LockOn->GetLockedOnBy())
		{
			if (Entry.IsValid())
			{
				Locker = Entry.Get();
				break;
			}
		}
	}

	const bool bLocked = Locker != nullptr;
	if (UpdateShown(Shown.LockWarning, bLocked ? 1 : 0))
	{
		const ESlateVisibility NewVisibility = bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
		LockWarningMarker->SetVisibility(NewVisibility);
		LockWarningText->SetVisibility(NewVisibility);
	}

	if (!bLocked)
	{
		return;
	}

	// 画面中央から見た方向へ印を回す。機首ではなく視点の向きを基準にしないと、表示が画面とずれる。
	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	if (const APlayerController* Controller = GetOwningPlayer())
	{
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}

	const FVector ViewForward = ViewRotation.Vector().GetSafeNormal2D();
	const FVector ToLocker = (Locker->GetActorLocation() - Vehicle->GetActorLocation()).GetSafeNormal2D();
	if (ViewForward.IsNearlyZero() || ToLocker.IsNearlyZero())
	{
		return;
	}

	const float BearingDegrees = FMath::RadiansToDegrees(FMath::Atan2(
		FVector::CrossProduct(ViewForward, ToLocker).Z, FVector::DotProduct(ViewForward, ToLocker)));
	const float BearingRadians = FMath::DegreesToRadians(BearingDegrees);

	LockWarningMarker->SetRenderTranslation(
		FVector2D(FMath::Sin(BearingRadians), -FMath::Cos(BearingRadians)) * LockWarningRadius);
	LockWarningMarker->SetRenderTransformAngle(BearingDegrees);
}

void UImpactHUDWidget::PlaceMarker(UWidget& Marker, UTextBlock& Label, float Ratio) const
{
	const float PositionX = GaugeWidth * FMath::Clamp(Ratio, 0.0f, 1.0f);

	if (UOverlaySlot* MarkerSlot = Cast<UOverlaySlot>(Marker.Slot))
	{
		MarkerSlot->SetPadding(FMargin(PositionX - MarkerWidth * 0.5f, 0.0f, 0.0f, 0.0f));
	}

	// 数値は目印の真下に中央をそろえる。幅は描画後に確定するため、前フレームの幅を用いる。
	if (UOverlaySlot* LabelSlot = Cast<UOverlaySlot>(Label.Slot))
	{
		LabelSlot->SetPadding(FMargin(PositionX - Label.GetDesiredSize().X * 0.5f, 0.0f, 0.0f, 0.0f));
	}
}

void UImpactHUDWidget::UpdateLockOnReticle(const AImpactVehiclePawn* Vehicle)
{
	const UImpactLockOnComponent* LockOn = Vehicle ? Vehicle->GetLockOn() : nullptr;
	const AImpactVehiclePawn* Target = LockOn ? LockOn->GetTarget() : nullptr;

	// 相手の画面位置。背後にいる場合は投影に失敗し、画面外にいる場合は範囲外になる。どちらも出さない。
	FVector2D ScreenPosition = FVector2D::ZeroVector;
	bool bOnScreen = false;
	if (Target)
	{
		bOnScreen = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
			GetOwningPlayer(), Target->GetActorLocation(), ScreenPosition, false);

		const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this).GetLocalSize();
		bOnScreen = bOnScreen
			&& ScreenPosition.X >= 0.0f && ScreenPosition.X <= ViewportSize.X
			&& ScreenPosition.Y >= 0.0f && ScreenPosition.Y <= ViewportSize.Y;
	}

	int32 ReticleState = HiddenValue;
	if (bOnScreen)
	{
		ReticleState = LockOn->GetAssistState() == EImpactAssistState::Active ? ReticleActive : ReticleIdle;
	}

	// 表示の切り替えと色は、状態が変わったときだけ書き換える。
	if (UpdateShown(Shown.LockOnReticle, ReticleState))
	{
		if (ReticleState == HiddenValue)
		{
			LockOnReticle->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			const FLinearColor& Color = ReticleState == ReticleActive ? LockOnReticleActiveColor : LockOnReticleIdleColor;
			for (UImage* Bar : ReticleBars)
			{
				Bar->SetColorAndOpacity(Color);
			}
			LockOnReticle->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}

	// 位置は相手の動きに合わせて連続的に変わるため、毎フレーム置き直す。
	if (bOnScreen)
	{
		if (UCanvasPanelSlot* ReticleSlot = Cast<UCanvasPanelSlot>(LockOnReticle->Slot))
		{
			ReticleSlot->SetPosition(ScreenPosition);
		}
	}
}

#undef LOCTEXT_NAMESPACE
