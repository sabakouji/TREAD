// 踏みつけ加速メカゲーム — NPC の挙動調整値アセット

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AITuningDataAsset.generated.h"

/**
 * NPC（敵機）の判断に関わる調整値を集約する DataAsset。
 *
 * 機体性能そのものは企画書 3-5 のとおり自機と同一とし（DA_VehicleTuning を共用）、
 * 敵機との差は本アセットの「判断」のみで表現する。
 */
UCLASS(BlueprintType)
class MYPROJECT_API UAITuningDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** この速度に達したら攻撃フェーズへ移る（uu/s）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Decision", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AttackSpeedThreshold = 2500.0f;

	/**
	 * 攻撃フェーズから加速フェーズへ戻る際の速度の余裕（uu/s）。
	 * 攻撃は AttackSpeedThreshold 以上で開始し、(AttackSpeedThreshold - この値) 未満で終了する。
	 * 境界付近で状態が振動しないようにするため。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Decision", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AttackHysteresis = 400.0f;

	/** 状態と目標を判断し直す間隔（秒）。人間らしい反応遅延。0 にすると理不尽な挙動になる。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Decision", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ReactionDelay = 0.25f;

	/** 旋回入力が最大になる、目標との角度差（deg）。小さいほど機敏に向きを変えようとする。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Steering", meta = (ClampMin = "1.0", ClampMax = "180.0"))
	float SteerFullLockAngle = 30.0f;

	/** 進路上の障害物を探す距離（uu）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Avoidance", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ObstacleProbeDistance = 1500.0f;

	/** 回避行動（ブレーキターン）を続ける時間（秒）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Avoidance", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AvoidDuration = 0.8f;

	//~ ゴールの守備（マップにゴールがある場合のみ使う）

	/** プレイヤーがゴールからこの距離（uu）以内に入ったら迎撃に向かう。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defense", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float InterceptRadius = 6000.0f;

	/**
	 * 加速のために踏み台を探す範囲（守備位置からの距離、uu）。
	 * これより遠い踏み台は追わない。加速を優先してゴールを空けないようにするため。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defense", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float LeashRadius = 5000.0f;

	/** 守備位置に到着したとみなす距離（uu）。到着したら停止してゴールの正面を向く。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defense", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float GuardArrivalRadius = 400.0f;
};
