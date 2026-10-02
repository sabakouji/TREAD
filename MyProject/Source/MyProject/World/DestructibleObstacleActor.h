// 踏みつけ加速メカゲーム — 破壊可能オブジェクトの Actor

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DestructibleObstacleActor.generated.h"

class UBoxComponent;
class UDestructibleComponent;
class UStaticMeshComponent;

/**
 * 一定以上の速度で衝突された場合にのみ破壊される、箱型の障害物。
 * 当たり判定・見た目・破壊の振る舞い（UDestructibleComponent）を組み合わせるだけで、規則は持たない。
 *
 * 企画書 2-2 の「加速が一定以上ある場合のみ建物などを破壊できる」を表す。
 * 破壊に必要な速度はランクごとに DA_VehicleTuning 側で定義し、ランクは UDestructibleComponent に設定する。
 *
 * 破壊可能な部品を持たないブロック判定のアクタ（床・外周壁など）は破壊不能として扱われる。
 */
UCLASS()
class MYPROJECT_API ADestructibleObstacleActor : public AActor
{
	GENERATED_BODY()

public:
	ADestructibleObstacleActor();

	UBoxComponent* GetCollisionBox() const { return CollisionBox; }
	UStaticMeshComponent* GetBodyMesh() const { return BodyMesh; }
	UDestructibleComponent* GetDestructible() const { return Destructible; }

protected:
	/** 当たり判定。ルートコンポーネント。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> CollisionBox;

	/** 見た目。メッシュは Blueprint 側で割り当てる。破壊時は瓦礫へ差し替えられる。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** 破壊の振る舞い（ランク・耐久値・瓦礫・演出）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UDestructibleComponent> Destructible;
};
