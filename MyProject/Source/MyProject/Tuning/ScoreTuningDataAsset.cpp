// 踏みつけ加速メカゲーム — 配点アセット

#include "Tuning/ScoreTuningDataAsset.h"

int32 UScoreTuningDataAsset::ComputeMaxSpeedBonus(float MaxSpeed) const
{
	return FMath::FloorToInt(FMath::Max(MaxSpeed, 0.0f) / MaxSpeedBonusDivisor);
}

int32 UScoreTuningDataAsset::ComputeTotal(const FImpactScoreTally& Tally) const
{
	return Tally.GoalDamage * PointsPerGoalDamage
		+ Tally.EnemiesDefeated * PointsPerEnemyDefeated
		+ Tally.ObstaclesLarge * PointsPerObstacleLarge
		+ Tally.ObstaclesSmall * PointsPerObstacleSmall
		+ Tally.Stomps * PointsPerStomp
		+ ComputeMaxSpeedBonus(Tally.MaxSpeed)
		- Tally.Crashes * PenaltyPerCrash;
}
