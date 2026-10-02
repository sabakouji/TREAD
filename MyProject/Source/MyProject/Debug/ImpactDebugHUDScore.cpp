// 踏みつけ加速メカゲーム — デバッグ HUD（ゴール・得点）
//
// AImpactDebugHUD のうち、ゴールの耐久値・得点の内訳・加点時のポップアップを
// このファイルに分けて実装する。得点・ゴールへの依存をここに閉じ込める。

#include "Debug/ImpactDebugHUD.h"

#include "Core/ImpactGameMode.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Field/GoalComponent.h"
#include "Score/ImpactScoreSubsystem.h"
#include "Tuning/ScoreTuningDataAsset.h"

// 配色は AImpactDebugHUD のメンバ（Debug|Style）にあり、Blueprint 派生から調整できる。
namespace
{
	/** ポップアップ同士の縦の間隔（px）。 */
	constexpr float PopupSpacing = 4.0f;
}

float AImpactDebugHUD::DrawScoreDebug(float PosY)
{
	PosY = DrawDebugLine(TEXT("--- Score ---"), PosY, ScoreHeaderColor);

	const UWorld* World = GetWorld();
	const AImpactGameMode* GameMode = World ? World->GetAuthGameMode<AImpactGameMode>() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UImpactScoreSubsystem* Score = GameInstance ? GameInstance->GetSubsystem<UImpactScoreSubsystem>() : nullptr;
	const UScoreTuningDataAsset* ScoreTuning = GameMode ? GameMode->GetScoreTuning() : nullptr;

	if (!Score || !ScoreTuning)
	{
		return DrawDebugLine(TEXT("Score          : (score tuning not assigned)"), PosY, WarnColor);
	}

	const FImpactScoreTally& Tally = Score->GetTally();
	PosY = DrawDebugLine(
		FString::Printf(TEXT("Total Score    : %d"), ScoreTuning->ComputeTotal(Tally)),
		PosY, ValueColor);

	// 内訳。総得点がおかしいときに、どの項目が効いているかを追えるようにする。
	PosY = DrawDebugLine(
		FString::Printf(TEXT("  goal dmg %d  enemies %d  large %d  small %d  stomps %d  crashes %d  top %.0f"),
			Tally.GoalDamage, Tally.EnemiesDefeated, Tally.ObstaclesLarge, Tally.ObstaclesSmall,
			Tally.Stomps, Tally.Crashes, Tally.MaxSpeed),
		PosY, ValueColor);

	if (const UGoalComponent* Goal = GameMode->GetGoal())
	{
		const bool bDestroyed = Goal->IsGoalDestroyed();
		PosY = DrawDebugLine(
			FString::Printf(TEXT("Goal           : %d / %d%s"),
				Goal->GetDurability(), Goal->GetMaxDurability(), bDestroyed ? TEXT("  DESTROYED") : TEXT("")),
			PosY, bDestroyed ? WarnColor : ValueColor);
	}
	else
	{
		PosY = DrawDebugLine(TEXT("Goal           : (none on this map)"), PosY, ValueColor);
	}

	return PosY;
}

void AImpactDebugHUD::DrawScoreFeedback()
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UImpactScoreSubsystem* Score = GameInstance ? GameInstance->GetSubsystem<UImpactScoreSubsystem>() : nullptr;
	if (Score)
	{
		DrawScorePopups(*Score, World->GetTimeSeconds());
	}
}

void AImpactDebugHUD::DrawScorePopups(const UImpactScoreSubsystem& Score, double Now)
{
	if (!Canvas)
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;
	float PosY = Canvas->ClipY * ScorePopupTopRatio;

	// 新しいものを上に積む。表示時間を過ぎたものは描かない。
	const TArray<FImpactScoreEvent>& Events = Score.GetRecentEvents();
	for (int32 Index = Events.Num() - 1; Index >= 0; --Index)
	{
		const FImpactScoreEvent& Event = Events[Index];
		const double Age = Now - Event.Time;
		if (Age < 0.0 || Age > ScorePopupDuration)
		{
			continue;
		}

		// 時間とともに薄くして、古い加点が画面に残り続けないようにする。
		const float Opacity = 1.0f - static_cast<float>(Age / ScorePopupDuration);
		const FLinearColor Color = (Event.Points >= 0 ? PopupPositiveColor : PopupNegativeColor).CopyWithNewOpacity(Opacity);
		const FString Text = FString::Printf(TEXT("%s  %+d"), *Event.Label, Event.Points);

		float TextWidth = 0.0f;
		float TextHeight = 0.0f;
		GetTextSize(Text, TextWidth, TextHeight, Font);

		DrawText(Text, Color, (Canvas->ClipX - TextWidth) * 0.5f, PosY, Font);
		PosY += TextHeight + PopupSpacing;
	}
}
