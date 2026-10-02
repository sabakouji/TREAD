// 踏みつけ加速メカゲーム — 得点の集計

#include "Score/ImpactScoreSubsystem.h"

#include "Tuning/ScoreTuningDataAsset.h"

DEFINE_LOG_CATEGORY(LogImpactMatch);

namespace
{
	/** 表示用に保持する直近の加点・減点の件数。ポップアップが画面を埋めない程度にする。 */
	constexpr int32 MaxRecentEvents = 8;
}

void UImpactScoreSubsystem::ResetScore()
{
	Tally = FImpactScoreTally();
	RecentEvents.Reset();

	UE_LOG(LogImpactMatch, Log, TEXT("score reset"));
}

void UImpactScoreSubsystem::RecordVehicleEvent(
	EVehicleGameplayEvent Event, int32 Amount, const UScoreTuningDataAsset& ScoreTuning, double WorldTime)
{
	switch (Event)
	{
	case EVehicleGameplayEvent::Stomp:
		++Tally.Stomps;
		PushEvent(TEXT("Stomp"), ScoreTuning.PointsPerStomp, WorldTime);
		return;

	case EVehicleGameplayEvent::DestroyedSmall:
		++Tally.ObstaclesSmall;
		PushEvent(TEXT("Small Destroyed"), ScoreTuning.PointsPerObstacleSmall, WorldTime);
		return;

	case EVehicleGameplayEvent::DestroyedLarge:
		++Tally.ObstaclesLarge;
		PushEvent(TEXT("Large Destroyed"), ScoreTuning.PointsPerObstacleLarge, WorldTime);
		return;

	case EVehicleGameplayEvent::Crash:
		++Tally.Crashes;
		PushEvent(TEXT("Crash"), -ScoreTuning.PenaltyPerCrash, WorldTime);
		return;

	case EVehicleGameplayEvent::GoalHit:
		++Tally.GoalHits;
		Tally.GoalDamage += Amount;
		PushEvent(FString::Printf(TEXT("Goal Hit x%d"), Amount), Amount * ScoreTuning.PointsPerGoalDamage, WorldTime);
		return;

	case EVehicleGameplayEvent::StructureDamaged:
		// 建物を損傷させただけでは得点にならない（破壊して初めて数える）。
		return;
	}
}

void UImpactScoreSubsystem::RecordEnemyDefeated(const UScoreTuningDataAsset& ScoreTuning, double WorldTime)
{
	++Tally.EnemiesDefeated;
	PushEvent(TEXT("Enemy Defeated"), ScoreTuning.PointsPerEnemyDefeated, WorldTime);
}

void UImpactScoreSubsystem::RecordSpeed(float Speed)
{
	Tally.MaxSpeed = FMath::Max(Tally.MaxSpeed, Speed);
}

void UImpactScoreSubsystem::PushEvent(const FString& Label, int32 Points, double WorldTime)
{
	FImpactScoreEvent& Added = RecentEvents.AddDefaulted_GetRef();
	Added.Label = Label;
	Added.Points = Points;
	Added.Time = WorldTime;

	if (RecentEvents.Num() > MaxRecentEvents)
	{
		RecentEvents.RemoveAt(0, RecentEvents.Num() - MaxRecentEvents);
	}

	UE_LOG(LogImpactMatch, Log, TEXT("score event: %s %+d"), *Label, Points);
}
