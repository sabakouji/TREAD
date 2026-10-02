// 踏みつけ加速メカゲーム — 破壊可能オブジェクトの Actor

#include "World/DestructibleObstacleActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Field/DestructibleComponent.h"

namespace
{
	/** 破壊可能オブジェクトの当たり判定サイズ（半径、uu）。 */
	const FVector ObstacleExtent(100.0f, 100.0f, 150.0f);
}

ADestructibleObstacleActor::ADestructibleObstacleActor()
{
	PrimaryActorTick.bCanEverTick = false;

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBox->SetBoxExtent(ObstacleExtent);

	// 踏み台と異なり、こちらは機体を止める。破壊できるかどうかは衝突時の速度で決まる。
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionBox->SetCollisionObjectType(ECC_WorldStatic);
	CollisionBox->SetCollisionResponseToAllChannels(ECR_Block);
	CollisionBox->SetSimulatePhysics(false);
	SetRootComponent(CollisionBox);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(CollisionBox);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 損傷・破壊時にプレイ中のメッシュ差し替え（UDestructibleComponent）を行うため、動かせる設定にしておく。
	BodyMesh->SetMobility(EComponentMobility::Movable);

	Destructible = CreateDefaultSubobject<UDestructibleComponent>(TEXT("Destructible"));
}
