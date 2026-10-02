// 踏みつけ加速メカゲーム — 配点アセット

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Score/ImpactScoreTypes.h"
#include "ScoreTuningDataAsset.generated.h"

/**
 * 得点の配点を集約する DataAsset。
 *
 * 総得点 = ゴールダメージ × PointsPerGoalDamage
 *        + 敵機撃破 × PointsPerEnemyDefeated
 *        + 壁・建物破壊 × PointsPerObstacleLarge
 *        + 小物破壊 × PointsPerObstacleSmall
 *        + 踏みつけ × PointsPerStomp
 *        + 最高到達速度 ÷ MaxSpeedBonusDivisor
 *        - 自滅 × PenaltyPerCrash
 */
UCLASS(BlueprintType)
class MYPROJECT_API UScoreTuningDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** ゴールへ与えたダメージ 1 あたりの得点。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Score")
	int32 PointsPerGoalDamage = 1000;

	/** 敵機を 1 機撃破するごとの得点。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Score")
	int32 PointsPerEnemyDefeated = 500;

	/** 壁・建物を 1 つ破壊するごとの得点。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Score")
	int32 PointsPerObstacleLarge = 100;

	/** 小物を 1 つ破壊するごとの得点。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Score")
	int32 PointsPerObstacleSmall = 30;

	/** 踏みつけ 1 回ごとの得点。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Score")
	int32 PointsPerStomp = 10;

	/** 最高到達速度（uu/s）をこの値で割ったものをボーナスとする。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Score", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float MaxSpeedBonusDivisor = 10.0f;

	/** 自滅 1 回ごとの減点。正の値で指定する。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Score", meta = (ClampMin = "0"))
	int32 PenaltyPerCrash = 200;

	/** 最高到達速度によるボーナスを返す。 */
	UFUNCTION(BlueprintPure, Category = "Score")
	int32 ComputeMaxSpeedBonus(float MaxSpeed) const;

	/** 集計値から総得点を求める。 */
	UFUNCTION(BlueprintPure, Category = "Score")
	int32 ComputeTotal(const FImpactScoreTally& Tally) const;
};
