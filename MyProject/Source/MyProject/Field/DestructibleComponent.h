// 踏みつけ加速メカゲーム — 破壊可能オブジェクト

#pragma once

#include "CoreMinimal.h"
#include "Collision/ImpactReceiver.h"
#include "Collision/ImpactTypes.h"
#include "Components/ActorComponent.h"
#include "DestructibleComponent.generated.h"

class UDestructibleComponent;
class UFeedbackTuningDataAsset;
class UGeometryCollection;
class UGeometryCollectionComponent;
class UNiagaraSystem;
class UStaticMesh;
class UStaticMeshComponent;
class UVehicleTuningDataAsset;

/** 破壊されたことの通知。スコア・NPC・演出が購読する。ImpactVelocity は破壊した機体の速度ベクトル。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDestructibleBroken, UDestructibleComponent*, Destructible, FVector, ImpactVelocity);

/**
 * 一定以上の速度で衝突された場合にのみ壊れる、建物などの振る舞い。任意の Actor に付けるだけで破壊可能になる。
 *
 * 「加速しないと壊せない」を表すため、破壊の本体はダメージ量ではなく速度の閾値（ランクごとに DA_VehicleTuning で定義）。
 * 閾値を超えた衝突1回で耐久値が1減り、0 で破壊される（既定の MaxHP 1 では一撃で壊れる）。
 *
 * ロジックは「壊れた / 壊れていない」と損傷段階の状態だけを持ち、見た目は演出として扱う（フィールド設計 3-3）。
 * 破壊の瞬間に当たり判定を消して瓦礫へ差し替え、ここで「道が開いた」がゲーム上確定する。
 * Chaos の破片（DestructionGC）は演出専用で、機体・踏み台と当たらず、決めた秒数で消える。
 * フィールド上の建物はすべて分割する方針のため（FIELD-02）、DestructionGC が未設定なら開始時とデータ検証で警告する。
 * 分割しない小物（街灯など）は、この部品ではなく UBreakablePropComponent で作る。
 * Actor は破棄せずに残す。瓦礫を残して遠目にも通れることを示すため。
 */
UCLASS(ClassGroup = (Impact), meta = (BlueprintSpawnableComponent))
class MYPROJECT_API UDestructibleComponent : public UActorComponent, public IImpactReceiver
{
	GENERATED_BODY()

public:
	UDestructibleComponent();

	/**
	 * 機体の衝突に反応する（IImpactReceiver）。
	 * 衝突時の生速度がランクの破壊閾値以上なら耐久値を減らし、0 になれば衝突処理の中で即座に破壊する。
	 * 破壊を遅らせると当たり判定が残る時間が生まれ、機体が引っかかるため即時に行う。
	 */
	virtual FImpactReceiveResult ReceiveVehicleImpact_Implementation(const FImpactReceiveContext& Context) override;

	/** 破壊難度ランク。 */
	UFUNCTION(BlueprintPure, Category = "Destructible")
	EDestructionRank GetRank() const { return Rank; }

	/** 破壊済みか。 */
	UFUNCTION(BlueprintPure, Category = "Destructible")
	bool IsBroken() const { return bBroken; }

	/** 残り耐久値。 */
	UFUNCTION(BlueprintPure, Category = "Destructible")
	int32 GetRemainingHP() const { return FMath::Max(MaxHP - HitsTaken, 0); }

	/** 破壊で開く経路の名前。NPC が「この建物を壊すと近道になる」と判断するために用いる。無ければ None。 */
	UFUNCTION(BlueprintPure, Category = "Destructible")
	FName GetOpenedRouteTag() const { return OpenedRouteTag; }

	/** 破壊されたことの通知。 */
	UPROPERTY(BlueprintAssignable, Category = "Destructible|Events")
	FOnDestructibleBroken OnBroken;

	//~ 判定規則（状態を持たない。自動テストから直接検証する）

	/**
	 * 衝突の向きを受け付けるか。bFrontOnly のとき、対象の正面（前方）側から当たった場合のみ受け付ける。
	 * @param OwnerForward     対象の前方向
	 * @param ImpactDirection  機体から当たった面へ向かう向き
	 */
	static bool IsDirectionAccepted(bool bInFrontOnly, float InFrontOnlyAngle, const FVector& OwnerForward, const FVector& ImpactDirection);

	/** 閾値を超えた衝突を1回受けた結果。耐久値が尽きれば Destroyed、残れば Weakened。 */
	static EImpactReceiveOutcome ResolveHit(int32 HitsTakenBefore, int32 InMaxHP);

	/** 損傷段階に対応する損傷メッシュの番号。損傷していない、またはメッシュが無ければ INDEX_NONE。 */
	static int32 GetDamagedMeshIndex(int32 InHitsTaken, int32 DamagedMeshCount);

#if WITH_EDITOR
	/** DestructionGC が未設定なら警告する（エディタのデータ検証）。 */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

protected:
	virtual void BeginPlay() override;

	/** 破壊難度ランク。必要な速度は DA_VehicleTuning 側のランクの閾値で決まる。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destructible")
	EDestructionRank Rank = EDestructionRank::Small;

	/** 耐久値。閾値を超えた衝突をこの回数受けると破壊される。1 なら一撃で壊れる。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destructible", meta = (ClampMin = "1"))
	int32 MaxHP = 1;

	/** 対象の正面側からの衝突のみ有効にするか。裏から壊せない壁などに使う。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destructible")
	bool bFrontOnly = false;

	/** bFrontOnly のとき、正面とみなす角度（deg）。対象の前方向からこの角度以内で当たれば受け付ける。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destructible", meta = (EditCondition = "bFrontOnly", ClampMin = "0.0", ClampMax = "180.0"))
	float FrontOnlyAngle = 60.0f;

	/**
	 * 見た目を差し替えるスタティックメッシュの部品名。None なら所有 Actor の最初のスタティックメッシュを使う。
	 * メッシュが複数ある Actor では指定すること（指定がないとどれが差し替わるかが定まらない）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destructible|Visual")
	FName VisualMeshName;

	/** 損傷段階ごとのメッシュ。1回目の損傷で [0]、2回目で [1] … に差し替える。足りなければ最後のものを使い続ける。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destructible|Visual")
	TArray<TObjectPtr<UStaticMesh>> DamagedMeshes;

	/**
	 * 破壊後の瓦礫（膝下程度の低い残骸）。当たり判定は持たない。
	 * 未設定なら破壊時に見た目を非表示にする。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destructible|Visual")
	TObjectPtr<UStaticMesh> RubbleMesh;

	/** 瓦礫へ差し替える際の見た目のスケール。本体のメッシュに掛けていたスケールを瓦礫の実寸に戻すために用いる。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destructible|Visual")
	FVector RubbleRelativeScale = FVector::OneVector;

	/** 破壊時の粉塵・火花（任意）。衝突点から突進方向へ向けて出す。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destructible|Visual")
	TObjectPtr<UNiagaraSystem> DestructionFX;

	/**
	 * 破壊時に飛び散らせる Chaos の破片（任意）。エディタの Fracture モードで作った Geometry Collection を割り当てる。
	 * 衝突点の周りだけを細かく崩し、突進方向へ初速を与える。未設定なら瓦礫への差し替えと DestructionFX だけで壊れる。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destructible|Visual")
	TObjectPtr<UGeometryCollection> DestructionGC;

	/** 破片の初速・崩す範囲・消えるまでの秒数。Blueprint 側で DA_FeedbackTuning を割り当てる。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destructible|Visual")
	TObjectPtr<UFeedbackTuningDataAsset> FeedbackTuning;

	/** DestructionFX へ機体の速度ベクトルを渡すユーザーパラメータ名。破片を突進方向へ飛ばすために使う。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destructible|Visual")
	FName FXVelocityParameter = TEXT("ImpactVelocity");

	/** 破壊で開く経路の名前（NPC 判断用）。GameState に「開いた経路」として記録される。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destructible")
	FName OpenedRouteTag;

private:
	/** 損傷段階に応じて見た目を差し替える。 */
	void ApplyDamagedMesh();

	/** 破壊を確定させる（当たり判定・見た目・演出・通知）。 */
	void Break(const FImpactReceiveContext& Context);

	/**
	 * Chaos の破片を生成する（演出専用）。建物の Actor は当たり判定を切っているため、破片は別の Actor として出す。
	 * 物理側の初期化（生成の次の物理ステップ）を待ち、次のフレームで最上位の塊を割り、
	 * その次のフレームで衝突点の近くを細かく崩して突進方向へ飛ばす（2段階）。
	 * @param MeshTransform  差し替え前の見た目の位置・向き（スケールは使わない）
	 */
	void SpawnDebris(const FTransform& MeshTransform, const FImpactReceiveContext& Context) const;

	/** 衝突点の周りを細かく崩す歪みと、突進方向への初速を破片に与える（2段目）。 */
	static void ApplyDebrisImpact(
		UGeometryCollectionComponent& Pieces, const UFeedbackTuningDataAsset& Tuning, const FVector& ImpactPoint, const FVector& ImpactVelocity);

	/** 見た目の差し替え先を探す。VisualMeshName が合う部品、指定が無ければ最初のスタティックメッシュ。 */
	UStaticMeshComponent* FindVisualMesh() const;

	/** 見た目の差し替え先。BeginPlay で決める。 */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	int32 HitsTaken = 0;
	bool bBroken = false;
};
