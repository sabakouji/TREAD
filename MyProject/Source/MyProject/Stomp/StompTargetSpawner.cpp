// 踏みつけ加速メカゲーム — 踏み台の供給拠点

#include "Stomp/StompTargetSpawner.h"

#include "Components/ArrowComponent.h"
#include "Field/TankSpawnerComponent.h"

AStompTargetSpawner::AStompTargetSpawner()
{
	// 供給は部品がタイマーで行うため、毎フレームの処理は持たない。
	PrimaryActorTick.bCanEverTick = false;

	DirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("DirectionArrow"));
	DirectionArrow->SetArrowColor(FLinearColor(0.2f, 0.8f, 1.0f, 1.0f));
	SetRootComponent(DirectionArrow);

	TankSpawner = CreateDefaultSubobject<UTankSpawnerComponent>(TEXT("TankSpawner"));
}
