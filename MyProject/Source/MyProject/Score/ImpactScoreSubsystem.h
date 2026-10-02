// 踏みつけ加速メカゲーム — 得点の集計

#pragma once

#include "CoreMinimal.h"
#include "Score/ImpactScoreTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Vehicle/ImpactVehicleTypes.h"
#include "ImpactScoreSubsystem.generated.h"

class UScoreTuningDataAsset;

/**
 * 試合中の得点の内訳を集計する。
 *
 * レベル遷移をまたいでリザルト画面（Phase 9）へ持ち越すため GameInstance に属させる。
 * どの出来事を何点とするかの判断は GameMode から配点アセットとともに渡され、
 * 本クラスは集計と記録に専念する。
 */
UCLASS()
class MYPROJECT_API UImpactScoreSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** 集計を初期化する。試合開始時に呼ぶ。 */
	void ResetScore();

	/** 自機の走行中の出来事を集計に反映する。 */
	void RecordVehicleEvent(EVehicleGameplayEvent Event, int32 Amount, const UScoreTuningDataAsset& ScoreTuning, double WorldTime);

	/** 敵機の撃破を集計に反映する。 */
	void RecordEnemyDefeated(const UScoreTuningDataAsset& ScoreTuning, double WorldTime);

	/** 現在の速度を受け取り、最高到達速度を更新する。 */
	void RecordSpeed(float Speed);

	/** 得点の内訳。 */
	const FImpactScoreTally& GetTally() const { return Tally; }

	/** 直近の加点・減点。新しいものほど後ろにある。 */
	const TArray<FImpactScoreEvent>& GetRecentEvents() const { return RecentEvents; }

private:
	/** 加点・減点を記録する。表示用に直近の一定件数だけを保持する。 */
	void PushEvent(const FString& Label, int32 Points, double WorldTime);

	FImpactScoreTally Tally;
	TArray<FImpactScoreEvent> RecentEvents;
};
