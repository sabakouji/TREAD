// 踏みつけ加速メカゲーム — 破壊演出の調整アセット

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Vehicle/ImpactVehicleTypes.h"
#include "FeedbackTuningDataAsset.generated.h"

class UCameraShakeBase;

/** 1種類の出来事に対する手応えの演出（ヒットストップとカメラシェイク）。 */
USTRUCT(BlueprintType)
struct FImpactFeedbackPreset
{
	GENERATED_BODY()

	FImpactFeedbackPreset() = default;

	FImpactFeedbackPreset(float InHitStopDuration, float InHitStopTimeDilation, float InShakeScale)
		: HitStopDuration(InHitStopDuration)
		, HitStopTimeDilation(InHitStopTimeDilation)
		, ShakeScale(InShakeScale)
	{
	}

	/** ヒットストップの長さ（実時間の秒）。0 ならヒットストップしない。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float HitStopDuration = 0.0f;

	/** ヒットストップ中の時間の進み方（1 が通常。小さいほど止まって見える）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback", meta = (ClampMin = "0.01", ClampMax = "1.0", UIMin = "0.01", UIMax = "1.0"))
	float HitStopTimeDilation = 1.0f;

	/** カメラシェイクの強さ（ShakeClass に掛ける倍率）。0 なら揺らさない。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ShakeScale = 0.0f;
};

/**
 * 破壊の手応え（フィールド設計 3.4）を集約する DataAsset。
 *
 * 見た目の演出だけを扱い、ゲームの判定（破壊・減速・得点）には一切関与しない。
 * ヒットストップとカメラシェイクは UImpactFeedbackComponent、破片は UDestructibleComponent が読む。
 */
UCLASS(BlueprintType)
class MYPROJECT_API UFeedbackTuningDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UFeedbackTuningDataAsset();

	/** 出来事に対応する演出。演出を持たない出来事（踏みつけ・自滅）は nullptr。 */
	const FImpactFeedbackPreset* FindPreset(EVehicleGameplayEvent Event) const;

	/**
	 * ヒットストップを解除するタイマーの遅延（ゲーム時間の秒）。
	 * ワールドのタイマーは時間の遅さの影響を受けるため、実時間の長さに時間の進み方を掛けて換算する。
	 */
	static float ToDilatedTimerDelay(float RealDuration, float TimeDilation);

	/**
	 * 破片に与える初速（uu/s）。機体の水平速度の向きに、速さ × DebrisSpeedRatio で飛ばす。
	 * 機体がほぼ止まっていれば 0。
	 */
	FVector ComputeDebrisVelocity(const FVector& ImpactVelocity) const;

	//~ 手応え（ヒットストップ・カメラシェイク）

	/** 揺らし方の定義。既定は UImpactCameraShake（コンストラクタで設定）。Blueprint で派生させて差し替えられる。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
	TSubclassOf<UCameraShakeBase> ShakeClass;

	/** 小物（Small）を破壊したとき。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
	FImpactFeedbackPreset SmallDestroyed = { 0.04f, 0.1f, 0.4f };

	/** 壁・建物（Large）を破壊したとき。最も重い手応えにする。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
	FImpactFeedbackPreset LargeDestroyed = { 0.09f, 0.05f, 1.0f };

	/** 段階破壊の建物を損傷させて弾かれたとき。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
	FImpactFeedbackPreset StructureDamaged = { 0.03f, 0.2f, 0.5f };

	/** ゴールにダメージを与えたとき。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
	FImpactFeedbackPreset GoalHit = { 0.07f, 0.1f, 0.8f };

	//~ 破片（Chaos、演出専用）

	/** 破片の初速の、機体の速さに対する比率。突進方向へ吹き飛ばして「ぶち抜いた」感を出す。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debris", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DebrisSpeedRatio = 0.6f;

	/** 衝突点から細かく崩す範囲の半径（uu）。この外側は大きな塊のまま遅れて崩れる。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debris", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DebrisHoleRadius = 250.0f;

	/** 衝突点に与える歪み。Geometry Collection の損傷閾値（Damage Threshold）を上回る値にする。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debris", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DebrisHoleStrain = 1000000.0f;

	/** 初速を与える範囲の半径（uu）。衝突点から離れた破片ほど初速が小さくなる。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debris", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DebrisPushRadius = 600.0f;

	/** 破片が消えるまでの秒数。破片はゲームに関与しないため、見せ終えたら消す。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debris", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float DebrisLifetime = 4.0f;
};
