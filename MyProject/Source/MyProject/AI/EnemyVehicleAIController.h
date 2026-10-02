// 踏みつけ加速メカゲーム — 敵機の AI コントローラ

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "EnemyVehicleAIController.generated.h"

class UGoalComponent;
class UAITuningDataAsset;

/** NPC の判断・出現に関するログカテゴリ。 */
DECLARE_LOG_CATEGORY_EXTERN(LogImpactAI, Log, All);

/**
 * 敵機の行動状態。
 *
 * 計画書では Behavior Tree を想定していたが、状態が少数に限定されていること、
 * および BT/BB アセットは Python からの組み立てで検証が難しいことから、C++ の状態機械で実装する。
 */
UENUM(BlueprintType)
enum class EEnemyAIState : uint8
{
	/** 速度が足りない。踏み台へ向かって加速する。 */
	Accelerating,

	/** プレイヤーへ突進する。ゴールを守る場合は迎撃。 */
	Attacking,

	/** 進路上に今の速度では壊せない障害物がある。ブレーキターンで回避する。 */
	Avoiding,

	/** 守るべきゴールがあり、差し迫った脅威もない。守備位置へ戻って待機する。 */
	Guarding
};

/** 行動状態を表示用の文字列に変換する。 */
inline const TCHAR* LexToDisplayString(EEnemyAIState State)
{
	switch (State)
	{
	case EEnemyAIState::Accelerating:
		return TEXT("Accelerating");
	case EEnemyAIState::Attacking:
		return TEXT("Attacking");
	case EEnemyAIState::Avoiding:
		return TEXT("Avoiding");
	case EEnemyAIState::Guarding:
		return TEXT("Guarding");
	default:
		return TEXT("Unknown");
	}
}

/**
 * 敵機を操作する AI。
 *
 * 自機と同じく移動コンポーネントへ入力（旋回・スロットル・ブレーキ）を与えるだけで、
 * 挙動そのものには手を加えない。「入力 → シミュレーション → 結果適用」の構造を AI でも崩さない。
 *
 * マップにゴールがあれば守備役として振る舞い（ゴールへ近づくプレイヤーを迎撃する）、
 * ゴールがなければプレイヤーへ攻めかかる。
 * 判断（状態と目標の決定）は反応遅延の間隔でのみ行い、入力は毎フレーム与える。
 */
UCLASS()
class MYPROJECT_API AEnemyVehicleAIController : public AAIController
{
	GENERATED_BODY()

public:
	AEnemyVehicleAIController();

	virtual void Tick(float DeltaSeconds) override;

	/** 現在の行動状態。 */
	UFUNCTION(BlueprintPure, Category = "Enemy AI")
	EEnemyAIState GetAIState() const { return AIState; }

protected:
	virtual void OnPossess(APawn* InPawn) override;

private:
	/** 状態と目標を決め直す。 */
	void Think(const UAITuningDataAsset& AITuning);

	/** ゴールがない場合の判断。十分な速度があればプレイヤーへ攻めかかる。 */
	void ThinkOffense(bool bFastEnough);

	/** ゴールを守る場合の判断。ゴールへ近づくプレイヤーを迎撃し、それ以外は守備位置の周囲で備える。 */
	void ThinkDefense(const UAITuningDataAsset& AITuning, const UGoalComponent& Goal, bool bFastEnough);

	/** 目標へ向けて入力を与える。 */
	void Drive(const UAITuningDataAsset& AITuning);

	/**
	 * 踏み台のうち、LeashCenter から LeashRadius 以内にあるものの中で、自機に最も近いものを探す。
	 * 範囲を制限しない場合は LeashRadius に十分大きな値を渡す。
	 */
	AActor* FindNearestStompTarget(const FVector& LeashCenter, float LeashRadius) const;

	/** プレイヤーの機体を探す。 */
	AActor* FindPlayerVehicle() const;

	/** 守るべきゴールを探す。破壊済みのものは対象にしない。 */
	UGoalComponent* FindGoal() const;

	/**
	 * 進路上に、今の速度では壊せない障害物があるかを調べる。
	 * @param OutAvoidDirection  ある場合の回避方向（面に沿って、今の進行方向に近い側）
	 */
	bool FindBlockingObstacle(const UAITuningDataAsset& AITuning, FVector& OutAvoidDirection) const;

	EEnemyAIState AIState = EEnemyAIState::Accelerating;

	/** 現在の目標（踏み台またはプレイヤー）。 */
	TWeakObjectPtr<AActor> TargetActor;

	/** 守るべきゴール。ゴールのないマップでは無効。 */
	TWeakObjectPtr<UGoalComponent> DefendedGoal;

	/** 守備位置。 */
	FVector GuardPoint = FVector::ZeroVector;

	/** 守備位置で向く方向（ゴールの正面）。 */
	FVector GuardFacing = FVector::ForwardVector;

	/** 回避中に向かう方向。 */
	FVector AvoidDirection = FVector::ForwardVector;

	/** 次の判断までの残り時間（秒）。 */
	float ThinkCooldown = 0.0f;

	/** 回避行動の残り時間（秒）。 */
	float AvoidRemaining = 0.0f;
};
