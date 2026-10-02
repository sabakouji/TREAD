// 踏みつけ加速メカゲーム — 分割しない小物の Actor

#include "World/BreakablePropActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Field/BreakablePropComponent.h"

namespace
{
	/** 小物の当たり判定サイズ（半径、uu）。街灯を模した細長い柱とする。 */
	const FVector PropExtent(20.0f, 20.0f, 200.0f);
}

ABreakablePropActor::ABreakablePropActor()
{
	PrimaryActorTick.bCanEverTick = false;

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBox->SetBoxExtent(PropExtent);

	// 機体とは重なるだけにし、機体の進路を止めない。止めると移動の衝突判定に回り、壁として減速・自滅してしまう。
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionBox->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionBox->SetGenerateOverlapEvents(true);
	SetRootComponent(CollisionBox);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(CollisionBox);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Breakable = CreateDefaultSubobject<UBreakablePropComponent>(TEXT("Breakable"));
}
