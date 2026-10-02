// 踏みつけ加速メカゲーム — 踏み台の供給拠点

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StompTargetSpawner.generated.h"

class UArrowComponent;
class UTankSpawnerComponent;

/**
 * 踏み台を一定間隔で供給する拠点。向きを示す矢印と供給の振る舞い（UTankSpawnerComponent）を組み合わせるだけの Actor。
 *
 * 企画書の「拠点から一定間隔で戦車が供給され、敵拠点に向かって行進する」に相当する。
 * 供給間隔・ウェーブ数・上限・流すレーンは UTankSpawnerComponent に設定する。
 * レーンを指定しなければ、矢印の方向へ直進させる。
 */
UCLASS()
class MYPROJECT_API AStompTargetSpawner : public AActor
{
	GENERATED_BODY()

public:
	AStompTargetSpawner();

	UTankSpawnerComponent* GetTankSpawner() const { return TankSpawner; }

protected:
	/** 行進方向を示す矢印。ルートコンポーネント。レーンが無い場合はこの向きへ直進させる。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UArrowComponent> DirectionArrow;

	/** 踏み台の供給。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTankSpawnerComponent> TankSpawner;
};
