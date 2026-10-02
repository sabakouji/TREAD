// 踏みつけ加速メカゲーム — 拠点（ゴール）の Actor

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GoalActor.generated.h"

class UBoxComponent;
class UGoalComponent;
class UStaticMeshComponent;
class UTeamComponent;

/**
 * 拠点（ゴール）の Actor。当たり判定・見た目・陣営・拠点の振る舞いを組み合わせるだけで、規則は持たない。
 *
 * 耐久値・ダメージ・破壊の通知・守備位置は UGoalComponent に、所属陣営は UTeamComponent にある。
 * ソロモードの拠点は NPC 側の陣営（ImpactTeam::Opponent）に属し、プレイヤーだけが削れる。
 */
UCLASS()
class MYPROJECT_API AGoalActor : public AActor
{
	GENERATED_BODY()

public:
	AGoalActor();

	UGoalComponent* GetGoalComponent() const { return Goal; }

protected:
	/** 当たり判定。ルートコンポーネント。機体を止める。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> CollisionBox;

	/** 見た目。メッシュは Blueprint 側で割り当てる。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** 所属陣営。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTeamComponent> Team;

	/** 拠点の振る舞い（耐久値・ダメージ・破壊の通知・守備位置）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UGoalComponent> Goal;
};
