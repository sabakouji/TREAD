// 踏みつけ加速メカゲーム — ロックオン（ステアリングアシスト）

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ImpactLockOnComponent.generated.h"

class AImpactVehiclePawn;
class UImpactTuningDataAsset;
class UVehicleTuningDataAsset;

/** ステアリングアシストの作動状態。デバッグ表示で作動条件を追えるようにする。 */
UENUM(BlueprintType)
enum class EImpactAssistState : uint8
{
	/** 捕捉している相手がいない。 */
	NoTarget,

	/** 行動不能中・弾かれ中・操作が止められている。 */
	Disabled,

	/** 角度誤差が AssistConeAngle を超えている。 */
	OutOfCone,

	/** 命中直前のためアシストを切っている。 */
	Cutoff,

	/** 作動中。 */
	Active
};

/** アシストの作動状態を表示用の文字列に変換する。 */
inline const TCHAR* LexToDisplayString(EImpactAssistState State)
{
	switch (State)
	{
	case EImpactAssistState::NoTarget:
		return TEXT("no target");
	case EImpactAssistState::Disabled:
		return TEXT("disabled");
	case EImpactAssistState::OutOfCone:
		return TEXT("out of cone");
	case EImpactAssistState::Cutoff:
		return TEXT("cutoff");
	case EImpactAssistState::Active:
		return TEXT("ACTIVE");
	default:
		return TEXT("Unknown");
	}
}

/**
 * ロックオンとステアリングアシスト（仕様 3章）。
 *
 * 攻撃入力が存在しないパッシブ方式のため、ロックオンは照準機構ではなく微調整機構として働く。
 * 前方の最近接の機体を自動で捕捉し、相手の予測到達点（リード点）との角度誤差が狭い窓に入っている間だけ、
 * 速度に依存しない固定枠の角速度を通常の旋回に加算する。大きな軌道はプレイヤーが作り、
 * 最後の数度だけ機械が詰める。命中直前はアシストを切り、避けた・避けられなかったの手応えを残す。
 *
 * アシストは移動コンポーネントへ「入力」として渡し、挙動そのものには手を加えない。
 * 「入力 → シミュレーション → 結果適用」の構造を崩さないため、本コンポーネントは移動より先に Tick する。
 */
UCLASS(ClassGroup = (Vehicle), meta = (BlueprintSpawnableComponent))
class MYPROJECT_API UImpactLockOnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UImpactLockOnComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 現在捕捉している相手。捕捉していなければ nullptr。 */
	UFUNCTION(BlueprintPure, Category = "Lock On")
	AImpactVehiclePawn* GetTarget() const { return Target.Get(); }

	/** 相手の予測到達点（リード点）。捕捉していないときは意味を持たない。 */
	UFUNCTION(BlueprintPure, Category = "Lock On")
	FVector GetLeadPoint() const { return LeadPoint; }

	/** アシストの作動状態。 */
	UFUNCTION(BlueprintPure, Category = "Lock On")
	EImpactAssistState GetAssistState() const { return AssistState; }

	/** 今フレームのアシスト角速度（deg/s）。正で右へ曲がる。 */
	UFUNCTION(BlueprintPure, Category = "Lock On")
	float GetAssistYawRate() const { return AssistYawRate; }

	/** 機首方向とリード点の角度差（deg）。正で右。 */
	UFUNCTION(BlueprintPure, Category = "Lock On")
	float GetAimErrorDegrees() const { return AimErrorDegrees; }

	/** 接触までの予測時間（秒）。近づいていない場合は負値。 */
	UFUNCTION(BlueprintPure, Category = "Lock On")
	float GetTimeToContact() const { return TimeToContact; }

	/** カメラを対象方向へ寄せる量（機体基準のヨー、deg）。捕捉していなければ 0。 */
	UFUNCTION(BlueprintPure, Category = "Lock On")
	float GetCameraYawOffset() const { return CameraYawOffset; }

	/** この機体を捕捉している機体の一覧。被ロック方向の表示に使う。 */
	const TArray<TWeakObjectPtr<AImpactVehiclePawn>>& GetLockedOnBy() const { return LockedOnBy; }

private:
	/** 捕捉条件（前方 LockOnConeAngle 以内・LockOnRange 以内）を満たす最近接の機体を返す。 */
	AImpactVehiclePawn* FindTarget(const UImpactTuningDataAsset& Rules) const;

	/** 捕捉対象を切り替え、相手側の被ロック一覧を更新する。 */
	void SetTarget(AImpactVehiclePawn* NewTarget);

	/** 相手の速度から予測到達点を求める。 */
	FVector ComputeLeadPoint(const AImpactVehiclePawn& InTarget, const UImpactTuningDataAsset& Rules) const;

	/** 接触までの予測時間（秒）を求める。近づいていない場合は負値を返す。 */
	float ComputeTimeToContact(const AImpactVehiclePawn& InTarget, const UVehicleTuningDataAsset& Tuning) const;

	/** アシスト角速度を決める。作動条件を満たさなければ 0。 */
	float ComputeAssistYawRate(float DeltaTime, const UImpactTuningDataAsset& Rules);

	/** 被ロック一覧への登録・解除。捕捉している側から呼ぶ。 */
	void AddLocker(AImpactVehiclePawn& Locker);
	void RemoveLocker(AImpactVehiclePawn& Locker);

	/** 所有機体。 */
	AImpactVehiclePawn* GetOwnerVehicle() const;

	/** 捕捉とアシストの状態（HUD.LockOn）を機体の HUD の値の置き場へ書き込む。 */
	void PublishHUDValue(const AImpactVehiclePawn& Vehicle) const;

	TWeakObjectPtr<AImpactVehiclePawn> Target;

	/** この機体を捕捉している機体。消滅した参照は参照時に取り除く。 */
	TArray<TWeakObjectPtr<AImpactVehiclePawn>> LockedOnBy;

	FVector LeadPoint = FVector::ZeroVector;
	EImpactAssistState AssistState = EImpactAssistState::NoTarget;
	float AssistYawRate = 0.0f;
	float AimErrorDegrees = 0.0f;
	float TimeToContact = -1.0f;
	float CameraYawOffset = 0.0f;
};
