// 踏みつけ加速メカゲーム — 得点の型定義

#pragma once

#include "CoreMinimal.h"
#include "ImpactScoreTypes.generated.h"

/** 試合進行（得点・ゴール）に関するログカテゴリ。 */
DECLARE_LOG_CATEGORY_EXTERN(LogImpactMatch, Log, All);

/**
 * 得点の元になる集計値。
 *
 * 総得点そのものではなく内訳を持つ。配点は UScoreTuningDataAsset にあり、
 * リザルト画面（Phase 9）で項目別に表示するためにも内訳が必要になる。
 */
USTRUCT(BlueprintType)
struct FImpactScoreTally
{
	GENERATED_BODY()

	/** ゴールへの命中回数。 */
	UPROPERTY(BlueprintReadOnly, Category = "Score")
	int32 GoalHits = 0;

	/** ゴールへ与えたダメージの合計。 */
	UPROPERTY(BlueprintReadOnly, Category = "Score")
	int32 GoalDamage = 0;

	/** 撃破した敵機の数。 */
	UPROPERTY(BlueprintReadOnly, Category = "Score")
	int32 EnemiesDefeated = 0;

	/** 破壊した壁・建物の数。 */
	UPROPERTY(BlueprintReadOnly, Category = "Score")
	int32 ObstaclesLarge = 0;

	/** 破壊した小物の数。 */
	UPROPERTY(BlueprintReadOnly, Category = "Score")
	int32 ObstaclesSmall = 0;

	/** 踏みつけの回数。デバッグ操作による加速は含まない。 */
	UPROPERTY(BlueprintReadOnly, Category = "Score")
	int32 Stomps = 0;

	/** 自滅の回数（減点）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Score")
	int32 Crashes = 0;

	/** 最高到達速度（uu/s）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Score")
	float MaxSpeed = 0.0f;
};

/** 加点・減点の出来事。HUD のポップアップ表示に用いる。 */
USTRUCT(BlueprintType)
struct FImpactScoreEvent
{
	GENERATED_BODY()

	/** 表示名。 */
	UPROPERTY(BlueprintReadOnly, Category = "Score")
	FString Label;

	/** 加減された得点。減点は負の値。 */
	UPROPERTY(BlueprintReadOnly, Category = "Score")
	int32 Points = 0;

	/** 発生時刻（ワールド時間、秒）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Score")
	double Time = 0.0;
};
