// 踏みつけ加速メカゲーム — ゲームモード

#pragma once

#include "CoreMinimal.h"
#include "Core/ImpactGameState.h"
#include "Field/TeamComponent.h"
#include "GameFramework/GameModeBase.h"
#include "Vehicle/ImpactVehicleTypes.h"
#include "ImpactGameMode.generated.h"

class AEnemyVehiclePawn;
class AImpactVehiclePawn;
class UDestructibleComponent;
class UGoalComponent;
class UImpactScoreSubsystem;
class UMatchTuningDataAsset;
class UScoreTuningDataAsset;

/**
 * 既定の Pawn / PlayerController / HUD クラスを束ね、試合のルール（進行・終了条件・敵機の出現・得点）を担うゲームモード。
 *
 * 具体的なアセット（BP_ImpactVehicle 等）の割り当ては Blueprint 派生側で行い、
 * C++ 側は既定として C++ クラスを指定する。
 *
 * 機体からは「何が起きたか」の通知だけを受け取り、それを何点とするか、試合を終えるかはここで判断する。
 * 判断の結果（進行段階・残り時間・自滅回数）は AImpactGameState に置き、画面はそちらを読む。
 *
 * ゴールのあるマップはソロモードとしてカウントダウンと制限時間つきで進行し、制限時間に達すると終わる。
 * ゴールのないマップ（テストコース）は制限なしのフリープレイになる。
 */
UCLASS()
class MYPROJECT_API AImpactGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AImpactGameMode();

	virtual void StartPlay() override;
	virtual void RestartPlayer(AController* NewPlayer) override;

	/** プレイヤーの陣営と一致する陣営付きの開始地点（ATeamPlayerStart）を優先して選ぶ。無ければ既定の選び方に任せる。 */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void Tick(float DeltaSeconds) override;

	/** 敵機が撃破されたことを受け取り、得点に反映してリスポーンを予約する。 */
	void NotifyEnemyDefeated(AEnemyVehiclePawn* Enemy);

	/** 同じマップで試合をやり直す。 */
	UFUNCTION(BlueprintCallable, Category = "Match")
	void RestartMatch();

	/** タイトル画面へ戻る。 */
	UFUNCTION(BlueprintCallable, Category = "Match")
	void ReturnToTitle();

	/**
	 * プレイヤーの機体。未出現なら nullptr。
	 * 「プレイヤー機体はどれか」の決定はここ1箇所に集める。ソロモードでは唯一のプレイヤーの機体を返す。
	 * 対戦化の際は、ここを攻撃対象の選定（最寄りの敵プレイヤーなど）に置き換える。
	 */
	UFUNCTION(BlueprintPure, Category = "Match")
	AImpactVehiclePawn* GetPlayerVehicle() const;

	/** 現在の敵機。未出現・撃破済みなら nullptr。 */
	AEnemyVehiclePawn* GetCurrentEnemy() const;

	/** プレイヤーが攻める拠点（プレイヤーと敵対する陣営の拠点）。拠点のないマップでは nullptr。 */
	UGoalComponent* GetGoal() const;

	/** プレイヤーの陣営。 */
	UFUNCTION(BlueprintPure, Category = "Match")
	int32 GetPlayerTeamId() const { return PlayerTeamId; }

	/** 配点。未設定なら nullptr。 */
	const UScoreTuningDataAsset* GetScoreTuning() const { return ScoreTuning; }

	/** 試合進行の調整値。未設定なら nullptr。 */
	const UMatchTuningDataAsset* GetMatchTuning() const { return MatchTuning; }

	/** 撃破した敵機の累計。 */
	UFUNCTION(BlueprintPure, Category = "Enemy")
	int32 GetEnemiesDefeated() const { return EnemiesDefeated; }

protected:
	/** 出現させる敵機。Blueprint 側で BP_EnemyVehicle を割り当てる。未設定なら敵機は出現しない。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy")
	TSubclassOf<AEnemyVehiclePawn> EnemyVehicleClass;

	/** 敵機の出現位置と向き。マップにゴールがある場合は、ゴールの守備位置を優先する。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy")
	FTransform EnemySpawnTransform;

	/** 撃破からリスポーンまでの時間（秒）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float EnemyRespawnDelay = 3.0f;

	/** 配点。Blueprint 側で DA_ScoreTuning を割り当てる。未設定なら得点は集計されない。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Score")
	TObjectPtr<UScoreTuningDataAsset> ScoreTuning;

	/**
	 * 試合進行（カウントダウン・制限時間・遷移先）。Blueprint 側で DA_MatchTuning を割り当てる。
	 * 未設定ならゴールのあるマップでもフリープレイになる。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match")
	TObjectPtr<UMatchTuningDataAsset> MatchTuning;

	/** プレイヤーの陣営。出現時に機体へ割り当てる。攻める拠点はこの陣営と敵対する陣営の拠点になる。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "0", ClampMax = "1"))
	int32 PlayerTeamId = ImpactTeam::Player;

private:
	/** マップに応じて、ソロモードのカウントダウンかフリープレイを始める。 */
	void BeginMatchFlow();

	/** 進行段階を変え、自機の操作可否をそれに合わせる。 */
	void SetMatchState(EImpactMatchState NewState);

	/** 自機の操作可否を、現在の進行段階に合わせる。 */
	void ApplyControlLock(AImpactVehiclePawn& Vehicle) const;

	/** 終了条件を調べ、満たしていれば試合を終える。 */
	void CheckEndConditions(const AImpactGameState& State);

	/** 結果を確定して試合を終え、リザルトの表示のためにゲームを一時停止する。 */
	void FinishMatch(EImpactMatchResult Result);

	/** 自機の現在の速度を最高到達速度の記録へ渡す。 */
	void RecordPlayerSpeed();

	/** ゴールの状態（有無・耐久値）を GameState へ反映する。画面は GameState の値を表示する。 */
	void PublishGoalState();

	/** 敵機を出現させる。 */
	void SpawnEnemy();

	/**
	 * 自機の走行中の出来事を得点と自滅回数へ反映する。
	 * dynamic デリゲートの受け手のため UFUNCTION が必須（無いとビルドは通るが購読に失敗する）。
	 */
	UFUNCTION()
	void HandlePlayerVehicleEvent(EVehicleGameplayEvent Event, int32 Amount);

	/** ゴールが破壊されたことを受け取る。試合は終わらず、制限時間まで続く。dynamic デリゲートの受け手。 */
	UFUNCTION()
	void HandleGoalDestroyed(UGoalComponent* DestroyedGoal);

	/**
	 * 拠点の登録を受け取り、プレイヤーと敵対する陣営の拠点なら攻める拠点として採用する。dynamic デリゲートの受け手。
	 * 拠点の BeginPlay と GameMode の初期化順に依存しないよう、検索ではなく通知で受け取る。
	 * 試合の開始後に採用した場合は GameState の拠点の状態だけを更新し、試合の形式は切り替えない。
	 */
	UFUNCTION()
	void HandleGoalRegistered(UGoalComponent* Goal);

	/** フィールドの破壊可能オブジェクトが破壊されたことを GameState へ反映する。dynamic デリゲートの受け手。 */
	UFUNCTION()
	void HandleFieldObjectBroken(UDestructibleComponent* Destructible);

	/** 得点の集計先。 */
	UImpactScoreSubsystem* GetScoreSubsystem() const;

	/** 試合の状態。GameStateClass が AImpactGameState 派生でない場合は nullptr。 */
	AImpactGameState* GetImpactGameState() const;

	TWeakObjectPtr<AEnemyVehiclePawn> CurrentEnemy;
	TWeakObjectPtr<UGoalComponent> CachedGoal;
	FTimerHandle EnemyRespawnTimer;
	int32 EnemiesDefeated = 0;

	/**
	 * StartPlay を終えたか（試合の形式が決まったか）。これより後に登録された拠点は「試合開始後の登録」として扱う。
	 * GameMode 自身の BeginPlay の完了（HasActorBegunPlay）は、他の Actor の BeginPlay と前後するため目安にならない。
	 */
	bool bMatchStartedUp = false;
};
