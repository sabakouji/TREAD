// 踏みつけ加速メカゲーム — タイトル画面のゲームモード

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ImpactTitleGameMode.generated.h"

class UMatchTuningDataAsset;

/**
 * タイトル画面のゲームモード。機体を出現させず、画面の表示はプレイヤーコントローラに任せる。
 *
 * マップ名が "Title" で始まるマップで使われる（DefaultEngine.ini の GameModeMapPrefixes）。
 * マップごとの World Settings は Python から書き込めないため、ini で割り当てている。
 */
UCLASS()
class MYPROJECT_API AImpactTitleGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AImpactTitleGameMode();

	virtual void StartPlay() override;

	/** タイトル画面では機体を出現させない。 */
	virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override;

	/** ソロモードのマップへ遷移する。 */
	void StartSoloGame();

protected:
	/** 遷移先のマップ。Blueprint 側で DA_MatchTuning を割り当てる。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match")
	TObjectPtr<UMatchTuningDataAsset> MatchTuning;
};
