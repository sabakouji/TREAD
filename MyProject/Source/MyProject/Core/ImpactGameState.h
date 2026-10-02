// 踏みつけ加速メカゲーム — 試合の状態

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "ImpactGameState.generated.h"

class UScoreTuningDataAsset;

/** 試合の進行段階。 */
UENUM(BlueprintType)
enum class EImpactMatchState : uint8
{
	/** 開始前。 */
	WaitingToStart,

	/** 開始前のカウントダウン。自機・敵機とも動けない。 */
	Countdown,

	/** 試合中。 */
	InProgress,

	/** 終了。リザルトを表示している。 */
	Finished
};

/**
 * 試合の結果。
 *
 * ソロモードは NPC を相手にしたスコアアタックであり、残機の概念がないため、
 * 試合は制限時間に達したときにのみ終わる（ゴール破壊や自滅回数では終わらない）。
 */
UENUM(BlueprintType)
enum class EImpactMatchResult : uint8
{
	/** まだ決まっていない。 */
	None,

	/** 制限時間に達した。 */
	TimeUp
};

/** 進行段階を表示用の文字列に変換する。 */
inline const TCHAR* LexToDisplayString(EImpactMatchState State)
{
	switch (State)
	{
	case EImpactMatchState::WaitingToStart:
		return TEXT("WaitingToStart");
	case EImpactMatchState::Countdown:
		return TEXT("Countdown");
	case EImpactMatchState::InProgress:
		return TEXT("InProgress");
	case EImpactMatchState::Finished:
		return TEXT("Finished");
	default:
		return TEXT("Unknown");
	}
}

/** 結果を表示用の文字列に変換する。リザルト画面の見出しにも用いる。 */
inline const TCHAR* LexToDisplayString(EImpactMatchResult Result)
{
	switch (Result)
	{
	case EImpactMatchResult::TimeUp:
		return TEXT("TIME UP");
	default:
		return TEXT("-");
	}
}

/** 進行段階が変わったことの通知。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchStateChanged, EImpactMatchState, NewState);

/**
 * 試合の進行状態（段階・残り時間・自滅回数・結果）を保持する。
 *
 * いつ始まり、いつ終わるかの判断は GameMode が行い、本クラスはその結果を保持して通知する。
 * 画面（HUD・リザルト）は GameMode ではなく本クラスを読む。
 * 将来ネットワーク対応した際、クライアントに存在するのは GameState のみであるため。
 */
UCLASS()
class MYPROJECT_API AImpactGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	/**
	 * 試合の条件を設定し、経過時間と自滅回数を初期化する。
	 * 制限時間に 0 を渡すと時間制限なし（フリープレイ）になる。
	 */
	void ConfigureMatch(float InCountdownSeconds, float InMatchDuration);

	/** 進行段階を変え、変わった場合は通知する。 */
	void SetMatchState(EImpactMatchState NewState);

	/** 結果を確定する。段階を Finished にする前に呼ぶ。 */
	void SetMatchResult(EImpactMatchResult NewResult) { MatchResult = NewResult; }

	/** カウントダウンを進める。 */
	void AdvanceCountdown(float DeltaSeconds);

	/** 試合の経過時間を進める。 */
	void AdvanceMatchTime(float DeltaSeconds);

	/** 自滅を 1 回数える。終了条件ではなく、表示と記録のために数える。 */
	void AddCrash() { ++CrashCount; }

	UFUNCTION(BlueprintPure, Category = "Match")
	EImpactMatchState GetMatchState() const { return MatchState; }

	UFUNCTION(BlueprintPure, Category = "Match")
	EImpactMatchResult GetMatchResult() const { return MatchResult; }

	/** 自機・敵機を動かしてよい段階か。 */
	UFUNCTION(BlueprintPure, Category = "Match")
	bool IsControlAllowed() const { return MatchState == EImpactMatchState::InProgress; }

	/** 制限時間があるか。フリープレイでは false。 */
	UFUNCTION(BlueprintPure, Category = "Match")
	bool HasTimeLimit() const { return MatchDuration > 0.0f; }

	/** 残り時間（秒）。制限時間がなければ 0。 */
	UFUNCTION(BlueprintPure, Category = "Match")
	float GetRemainingTime() const;

	/** カウントダウン終了からの経過時間（秒）。 */
	UFUNCTION(BlueprintPure, Category = "Match")
	float GetElapsedTime() const { return ElapsedTime; }

	/** カウントダウンの残り（秒）。 */
	UFUNCTION(BlueprintPure, Category = "Match")
	float GetCountdownRemaining() const { return CountdownRemaining; }

	/** 試合中の自滅回数。 */
	UFUNCTION(BlueprintPure, Category = "Match")
	int32 GetCrashCount() const { return CrashCount; }

	/** 配点を記録する。画面が総得点を計算するために読む。GameMode が試合開始時に設定する。 */
	void SetScoreTuning(UScoreTuningDataAsset* InScoreTuning) { ScoreTuning = InScoreTuning; }

	/** 配点。未設定なら nullptr（得点を表示しない）。 */
	UFUNCTION(BlueprintPure, Category = "Match")
	UScoreTuningDataAsset* GetScoreTuning() const { return ScoreTuning; }

	/**
	 * ゴールの状態を記録する。GameMode が試合開始時・命中時・破壊時に呼ぶ。
	 * @param bInHasGoal  このマップにゴールがあるか。false の場合、耐久値は無視される。
	 */
	void SetGoalState(bool bInHasGoal, int32 InDurability, int32 InMaxDurability);

	/** このマップにゴールがあるか。 */
	UFUNCTION(BlueprintPure, Category = "Match")
	bool HasGoal() const { return bHasGoal; }

	/** ゴールの残り耐久値。 */
	UFUNCTION(BlueprintPure, Category = "Match")
	int32 GetGoalDurability() const { return GoalDurability; }

	/** ゴールの耐久値の上限。 */
	UFUNCTION(BlueprintPure, Category = "Match")
	int32 GetGoalMaxDurability() const { return GoalMaxDurability; }

	/**
	 * フィールドの破壊可能オブジェクトが破壊されたことを記録する。GameMode が破壊の通知を受けて呼ぶ。
	 * @param OpenedRouteTag  破壊で開いた経路の名前。None なら経路は記録しない。
	 */
	void RecordFieldObjectBroken(FName OpenedRouteTag);

	/** 破壊されたフィールドの破壊可能オブジェクトの数。 */
	UFUNCTION(BlueprintPure, Category = "Field")
	int32 GetBrokenFieldObjectCount() const { return BrokenFieldObjectCount; }

	/** 建物の破壊によって経路が開いたか。NPC が近道を使えるかの判断に用いる。 */
	UFUNCTION(BlueprintPure, Category = "Field")
	bool IsRouteOpened(FName RouteTag) const { return OpenedRoutes.Contains(RouteTag); }

	/** 進行段階が変わったことの通知。 */
	UPROPERTY(BlueprintAssignable, Category = "Match|Events")
	FOnMatchStateChanged OnMatchStateChanged;

private:
	EImpactMatchState MatchState = EImpactMatchState::WaitingToStart;
	EImpactMatchResult MatchResult = EImpactMatchResult::None;
	float CountdownRemaining = 0.0f;
	float MatchDuration = 0.0f;
	float ElapsedTime = 0.0f;
	int32 CrashCount = 0;

	/** 配点。アセットへの参照のみで、保存対象ではない。 */
	UPROPERTY(Transient)
	TObjectPtr<UScoreTuningDataAsset> ScoreTuning;

	bool bHasGoal = false;
	int32 GoalDurability = 0;
	int32 GoalMaxDurability = 0;

	/** 破壊済みのフィールドオブジェクトの数と、開いた経路。対戦化の際に同期する対象。 */
	int32 BrokenFieldObjectCount = 0;
	TSet<FName> OpenedRoutes;
};
