// 踏みつけ加速メカゲーム — 拠点（ゴール）の Actor

#include "World/GoalActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Field/GoalComponent.h"
#include "Field/TeamComponent.h"

namespace
{
	/** ゴールの当たり判定サイズ（半径、uu）。機体より十分大きく、正面から狙いやすい壁状にする。 */
	const FVector GoalExtent(300.0f, 600.0f, 400.0f);
}

AGoalActor::AGoalActor()
{
	PrimaryActorTick.bCanEverTick = false;

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBox->SetBoxExtent(GoalExtent);

	// 機体を止める。障害物探索（WorldStatic）にもかかるため、守備側の NPC は自陣ゴールを避けて走る。
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionBox->SetCollisionObjectType(ECC_WorldStatic);
	CollisionBox->SetCollisionResponseToAllChannels(ECR_Block);
	CollisionBox->SetSimulatePhysics(false);
	SetRootComponent(CollisionBox);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(CollisionBox);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// ソロモードの拠点は NPC 側に属する。守備の NPC と同じ陣営のため、NPC が当たっても削れない。
	Team = CreateDefaultSubobject<UTeamComponent>(TEXT("Team"));
	Team->SetTeamId(ImpactTeam::Opponent);

	Goal = CreateDefaultSubobject<UGoalComponent>(TEXT("Goal"));
}
