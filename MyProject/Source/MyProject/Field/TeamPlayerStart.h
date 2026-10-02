// 踏みつけ加速メカゲーム — 陣営付きの開始地点

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerStart.h"
#include "TeamPlayerStart.generated.h"

class UTeamComponent;

/**
 * 陣営付きの開始地点。GameMode はプレイヤーの陣営と一致するものを優先して選ぶ
 * （AImpactGameMode::ChoosePlayerStart_Implementation）。
 * 陣営の付いていない通常の PlayerStart しか無いマップでも、従来どおり出現できる。
 */
UCLASS()
class MYPROJECT_API ATeamPlayerStart : public APlayerStart
{
	GENERATED_BODY()

public:
	ATeamPlayerStart(const FObjectInitializer& ObjectInitializer);

protected:
	/** 陣営。既定はプレイヤーの陣営。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTeamComponent> Team;
};
