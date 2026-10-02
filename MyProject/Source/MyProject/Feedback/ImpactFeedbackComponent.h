// 踏みつけ加速メカゲーム — 衝突の手応えの演出

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Vehicle/ImpactVehicleTypes.h"
#include "ImpactFeedbackComponent.generated.h"

class APawn;
class UFeedbackTuningDataAsset;
class UImpactVehicleMovementComponent;

/** 実行中のヒットストップ。終わる時刻は実時間（時間の遅さの影響を受けない）で持つ。 */
struct FHitStopState
{
	/** ヒットストップ中の時間の進み方。 */
	float TimeDilation = 1.0f;

	/** 終わる時刻（ワールドの実時間の秒）。 */
	double EndRealTime = 0.0;

	/** 指定時刻に実行中か。 */
	bool IsActiveAt(double NowRealTime) const { return NowRealTime < EndRealTime; }
};

/**
 * 自機の破壊・命中の瞬間に、ヒットストップとカメラシェイクで手応えを出す部品。PlayerController に付ける。
 *
 * 破壊の手応えは見た目よりこれで決まる部分が大きい（フィールド設計 3.4）。
 * 移動コンポーネントの出来事の通知（OnGameplayEvent）を受け取るだけで、判定や移動の計算には関与しない。
 *
 * ヒットストップはゲーム全体の時間を一瞬遅くする（Global Time Dilation）。ソロモード専用の方式で、
 * オンライン化の際は各クライアントの見た目だけの演出に作り直す必要がある（CLAUDE.md §7）。
 */
UCLASS(ClassGroup = (Impact), meta = (BlueprintSpawnableComponent))
class MYPROJECT_API UImpactFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UImpactFeedbackComponent();

	/** 出来事に応じた手応えを出す。Blueprint から任意の出来事で呼ぶこともできる。 */
	UFUNCTION(BlueprintCallable, Category = "Feedback")
	void PlayFeedback(EVehicleGameplayEvent Event);

	/** ヒットストップ中か。 */
	UFUNCTION(BlueprintPure, Category = "Feedback")
	bool IsHitStopActive() const;

	/**
	 * ヒットストップを始める。実行中なら MergeHitStop の規則で合成し、弱い出来事が強いヒットストップを縮めないようにする。
	 * @param RealDuration  長さ（実時間の秒）
	 * @param TimeDilation  時間の進み方（1 未満）
	 */
	UFUNCTION(BlueprintCallable, Category = "Feedback")
	void StartHitStop(float RealDuration, float TimeDilation);

	/** ヒットストップを終え、時間の進み方を元に戻す。 */
	UFUNCTION(BlueprintCallable, Category = "Feedback")
	void EndHitStop();

	/**
	 * 実行中のヒットストップに新しい要求を合成する。
	 * 実行中なら「時間の進み方はより遅いほう」「終わる時刻はより遅いほう」を採る。実行中でなければ新しい要求そのもの。
	 */
	static FHitStopState MergeHitStop(const FHitStopState& Current, double NowRealTime, float RealDuration, float TimeDilation);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 演出の調整値。Blueprint 側で DA_FeedbackTuning を割り当てる。未設定なら演出しない。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback")
	TObjectPtr<UFeedbackTuningDataAsset> FeedbackTuning;

private:
	/** 操作する機体が変わったら、出来事の購読を付け替える。dynamic デリゲートの受け手。 */
	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	/** 機体の出来事を受け取る。dynamic デリゲートの受け手。 */
	UFUNCTION()
	void HandleVehicleEvent(EVehicleGameplayEvent Event, int32 Amount);

	/** 機体の出来事を購読する。 */
	void BindToPawn(APawn* Pawn);

	/** 購読中の機体の移動コンポーネント。 */
	TWeakObjectPtr<UImpactVehicleMovementComponent> BoundMovement;

	FTimerHandle HitStopTimer;

	/** 実行中のヒットストップ。 */
	FHitStopState HitStop;
};
