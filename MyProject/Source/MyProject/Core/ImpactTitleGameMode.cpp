// 踏みつけ加速メカゲーム — タイトル画面のゲームモード

#include "Core/ImpactTitleGameMode.h"

#include "Core/ImpactTitlePlayerController.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Score/ImpactScoreSubsystem.h"
#include "Score/ImpactScoreTypes.h"
#include "Tuning/MatchTuningDataAsset.h"

AImpactTitleGameMode::AImpactTitleGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = AImpactTitlePlayerController::StaticClass();
}

void AImpactTitleGameMode::StartPlay()
{
	Super::StartPlay();

	// 得点の集計は GameInstance に属し、レベル遷移では消えない。
	// タイトルへ戻った時点で前の試合の集計を確実に捨てる。
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (UImpactScoreSubsystem* Score = GameInstance->GetSubsystem<UImpactScoreSubsystem>())
		{
			Score->ResetScore();
		}
	}

	UE_LOG(LogImpactMatch, Log, TEXT("title: ready (%s)"), *GetName());

	if (!MatchTuning || MatchTuning->SoloMap.IsNull())
	{
		UE_LOG(LogImpactMatch, Error, TEXT("SoloMap is not set on %s; START will not work"), *GetName());
	}
}

bool AImpactTitleGameMode::PlayerCanRestart_Implementation(APlayerController* Player)
{
	return false;
}

void AImpactTitleGameMode::StartSoloGame()
{
	if (!MatchTuning || MatchTuning->SoloMap.IsNull())
	{
		UE_LOG(LogImpactMatch, Error, TEXT("cannot start: SoloMap is not set on %s"), *GetName());
		return;
	}

	UE_LOG(LogImpactMatch, Log, TEXT("title: starting %s"), *MatchTuning->SoloMap.ToString());
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, MatchTuning->SoloMap);
}
