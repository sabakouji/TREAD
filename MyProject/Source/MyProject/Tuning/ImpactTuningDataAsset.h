// 踏みつけ加速メカゲーム — 追突・ロックオンの規則の調整値アセット

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ImpactTuningDataAsset.generated.h"

/**
 * 追突処理・面判定・ロックオンの規則を集約する DataAsset（DA_ImpactTuning）。
 *
 * 機体ごとの性質（寸法・速度・旋回）は UVehicleTuningDataAsset に置き、
 * 本アセットには試合に参加する全機体で共通の「規則」を置く。
 * 全機体が同じアセットを参照すること（機体同士の衝突は先に検出した側のアセットで解決する）。
 *
 * 仕様: Plan/踏みつけ加速メカゲーム_追突・ロックオン仕様.md
 */
UCLASS(BlueprintType)
class MYPROJECT_API UImpactTuningDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	//~ 面判定（仕様 2-4）

	/** 正面と判定する角度の上限（deg）。これ未満が Front。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Facing", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float FrontAngleThreshold = 60.0f;

	/** 側面と背面の境界角度（deg）。これ未満が Side、以上が Back。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Facing", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float SideAngleThreshold = 135.0f;

	/** 正面で接触を受けたときのエネルギー補正。基準。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Facing", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FacingPowerFront = 1.0f;

	/** 側面で接触を受けたときのエネルギー補正。側面を取られたら明確に不利にする。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Facing", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FacingPowerSide = 0.35f;

	/** 背面で接触を受けたときのエネルギー補正。背面を取られたら実質確定で負ける。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Facing", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FacingPowerBack = 0.10f;

	//~ 追突（仕様 2-2）

	/**
	 * 破壊が発生する「超加速」の入口（uu/s）。勝者の生速度がこれ以上なら相手を破壊する。
	 * Large の破壊閾値（3200）より上に置き、特別な状態とする。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clash", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float OverdriveSpeed = 3600.0f;

	/**
	 * 拮抗とみなすエネルギー差の上限（uu/s 相当）。
	 * 踏みつけ1回分（600）の差では決着しない値にする。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clash", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ClashThreshold = 800.0f;

	/** 弾き時の反発係数。バンパー（正面）で相手を押し返すときの跳ね返りの強さ。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clash", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Restitution = 0.7f;

	/** 相手を破壊した勝者に残る速度の割合。破壊しても無傷では通れない。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clash", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ClashWinnerSpeedRatio = 0.8f;

	/**
	 * 機体同士の衝突を解決した後、再び解決しない時間（秒）。
	 * 双方の移動とバンパーの接触検出が同じ衝突を拾うため、これがないと1回の衝突が二重に解決される。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clash", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ClashCooldown = 0.3f;

	//~ 拮抗スタン（仕様 2-6）

	/** 拮抗時に両者へ与えるスタン時間（秒）。長いと膠着する。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clash|Stun", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ClashStunDuration = 0.6f;

	/** 拮抗時に相互に離れる速度（uu/s）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clash|Stun", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ClashKnockbackSpeed = 1200.0f;

	/**
	 * 拮抗時に離れる方向を真後ろから横へずらす角度（deg）。
	 * 両者を同じ回転方向へずらすため、互いに反対側の横へ抜け、スタン明けに正面同士の膠着が続かない。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clash|Stun", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float ClashKnockbackSideAngle = 20.0f;

	//~ 弾かれ（仕様 2-5）

	/** 弾かれて操作できなくなる時間（秒）。弾かれた軌道は操作できない（CLAUDE.md §7 の暫定判断）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clash|Knockback", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float KnockbackRecoveryTime = 0.4f;

	/** 弾かれ中・拮抗スタン中の減速（uu/s^2）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clash|Knockback", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float KnockbackDeceleration = 2500.0f;

	/**
	 * 弾かれ状態にする速度変化の下限（uu/s）。
	 * これ未満の変化（かすった場合）は軽く逸れるだけで、操作を奪わない。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clash|Knockback", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float KnockbackMinDeltaSpeed = 400.0f;

	//~ ロックオン（仕様 3章）

	/** 捕捉角（deg）。機首方向からこの角度以内の相手を捕捉する。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LockOn", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float LockOnConeAngle = 25.0f;

	/** 捕捉距離（uu）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LockOn", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float LockOnRange = 10000.0f;

	/** アシストが作動する、機首とリード点の角度誤差の上限（deg）。狭い窓にして「曲がる」用途に使えないようにする。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LockOn", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float AssistConeAngle = 12.0f;

	/** アシストの角速度の上限（deg/s）。速度に依存しない固定枠で、通常旋回に加算する。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LockOn", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AssistTurnRate = 25.0f;

	/** 命中の予測時間がこれ以下になったらアシストを切る（秒）。被弾側に「避けられた」の手応えを残すため。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LockOn", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AssistCutoffTime = 0.2f;

	/** リード点を予測する時間の上限（秒）。遠い相手の予測点が現実離れしないようにする。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LockOn", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float LockOnMaxLeadTime = 1.5f;

	/** 捕捉中にカメラを対象方向へ寄せる割合（0..1）。0 で追従しない。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LockOn|Camera", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LockOnCameraYawBlend = 0.35f;

	/** カメラが対象方向へ寄る速さ。小さいほど緩やかに追従する。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LockOn|Camera", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float LockOnCameraInterpSpeed = 2.0f;

	//~ 導出値

	/** 超加速（OverdriveSpeed 以上）の状態か。 */
	UFUNCTION(BlueprintPure, Category = "Impact Tuning")
	bool IsOverdrive(float Speed) const { return Speed >= OverdriveSpeed; }
};
