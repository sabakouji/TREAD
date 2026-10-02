// 踏みつけ加速メカゲーム — 試合進行の調整アセット

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MatchTuningDataAsset.generated.h"

class UWorld;

/**
 * 試合の進行（カウントダウン・制限時間）と画面の遷移先を集約する DataAsset。
 *
 * ソロモードは NPC を相手にしたスコアアタックで、残機の概念がないため、試合は制限時間でのみ終わる。
 * エディタでは本アセットを開き、詳細パネルから PIE 中にも調整できる。
 * 時間制限が働くのはゴールのあるマップ（ソロモード）のみで、
 * ゴールのないテストコースは制限なしのフリープレイになる。
 */
UCLASS(BlueprintType)
class MYPROJECT_API UMatchTuningDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 開始前のカウントダウン（秒）。この間は自機・敵機とも動けない。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CountdownSeconds = 3.0f;

	/** 制限時間（秒）。カウントダウンの終了から数える。到達するとタイムアップ。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float MatchDurationSeconds = 180.0f;

	/** タイトル画面のマップ。リザルトの「タイトルへ戻る」の遷移先。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Maps")
	TSoftObjectPtr<UWorld> TitleMap;

	/** ソロモードのマップ。タイトルの「開始」の遷移先。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Maps")
	TSoftObjectPtr<UWorld> SoloMap;
};
