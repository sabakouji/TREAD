// 踏みつけ加速メカゲーム — 拠点（ゴール）

#pragma once

#include "CoreMinimal.h"
#include "Collision/ImpactReceiver.h"
#include "Collision/ImpactTypes.h"
#include "Components/ActorComponent.h"
#include "GoalComponent.generated.h"

class UGoalComponent;
class UVehicleTuningDataAsset;

/** 拠点が破壊されたことの通知。GameMode が記録し、Blueprint からも購読できる。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGoalDestroyed, UGoalComponent*, Goal);

/**
 * 突撃して破壊する対象となる拠点の振る舞い。企画書 3-1 の「ゴールへ突撃・破壊する」を表す。
 * 拠点の Actor に付け、同じ Actor の UTeamComponent で所属陣営を決める。
 *
 * 企画書 4章の未決定事項「一撃必殺か、耐久値の削り合いか」に対し、プロトタイプでは
 * 耐久値の削り合いを採用している。一撃必殺ではプレイ時間が短くなりすぎ、得点の積み上げで評価できないため。
 *
 * 守備側の NPC は、同じ陣営の防衛ラインのマーカーがあればそこに、無ければ拠点の正面に出現し、そこを起点に迎撃する。
 */
UCLASS(ClassGroup = (Impact), meta = (BlueprintSpawnableComponent))
class MYPROJECT_API UGoalComponent : public UActorComponent, public IImpactReceiver
{
	GENERATED_BODY()

public:
	UGoalComponent();

	/**
	 * 機体の衝突に反応する（IImpactReceiver）。
	 * ダメージを与えられるのは拠点と敵対する陣営の機体だけで、自陣の拠点を守る NPC が当たっても影響を受けない
	 * （機体側では破壊不能の壁として扱われる）。ダメージの計算は ReceiveImpact に委ねる。
	 */
	virtual FImpactReceiveResult ReceiveVehicleImpact_Implementation(const FImpactReceiveContext& Context) override;

	/**
	 * 機体の衝突を受ける。衝突時の生速度が RequiredRank の破壊閾値を超えていればダメージを受ける。
	 * 閾値を超えた一撃で 1、さらに超過速度 ExtraDamageSpeedStep ごとに 1 を加える。
	 *
	 * @return 実際に与えられたダメージ。閾値未満、または既に破壊済みなら 0。
	 */
	int32 ReceiveImpact(float ImpactSpeed, const UVehicleTuningDataAsset& Tuning);

	/** 残り耐久値。 */
	UFUNCTION(BlueprintPure, Category = "Goal")
	int32 GetDurability() const { return Durability; }

	/** 耐久値の上限。 */
	UFUNCTION(BlueprintPure, Category = "Goal")
	int32 GetMaxDurability() const { return MaxDurability; }

	/** 破壊済みか。 */
	UFUNCTION(BlueprintPure, Category = "Goal")
	bool IsGoalDestroyed() const { return Durability <= 0; }

	/**
	 * 守備側の NPC が出現・待機する位置と向き。
	 * 同じ陣営の防衛ラインのマーカー（EFieldPointRole::DefenseLine）があれば拠点に最も近いものを、
	 * 無ければ拠点の正面方向に GuardDistance 離れた地点を使う。高さは地面から GuardSpawnHeight 上げる。
	 */
	UFUNCTION(BlueprintPure, Category = "Goal")
	FTransform GetGuardTransform() const;

	/** 拠点が破壊されたことの通知。 */
	UPROPERTY(BlueprintAssignable, Category = "Goal|Events")
	FOnGoalDestroyed OnGoalDestroyed;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 耐久値の上限。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Goal", meta = (ClampMin = "1"))
	int32 MaxDurability = 5;

	/** ダメージに必要な速度のランク。閾値は DA_VehicleTuning 側で定義する。拠点は壁・建物と同じ閾値を要求する。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Goal")
	EDestructionRank RequiredRank = EDestructionRank::Large;

	/**
	 * 破壊閾値を超えた速度が、この値ごとに追加ダメージ 1 を与える（uu/s）。
	 * 踏みつけ1回分（600）にしておくと、速度を積むほど一撃が重くなる。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Goal", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float ExtraDamageSpeedStep = 600.0f;

	/** 防衛ラインのマーカーが無い場合の、拠点の正面から守備位置までの距離（uu）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Goal|Guard", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float GuardDistance = 1500.0f;

	/** 守備位置の地面からの高さ（uu）。機体の出現時に床へ埋まらないようにする。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Goal|Guard", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float GuardSpawnHeight = 200.0f;

private:
	/** 拠点の底面の高さ。ルートの当たり判定の境界から求める。 */
	float GetGroundZ() const;

	int32 Durability = 0;
};
