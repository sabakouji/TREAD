// 踏みつけ加速メカゲーム — 踏み台（戦車）アクタ

#include "Stomp/StompTargetActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Field/LaneActor.h"

namespace
{
	/** 踏み台の当たり判定サイズ（半径、uu）。戦車を模した横長の箱とする。 */
	const FVector StompTargetExtent(120.0f, 70.0f, 40.0f);

	/** これ以上上を向いた面は床・緩い斜面とみなし、行進を塞がない（法線の Z 成分）。 */
	constexpr float WalkableNormalZ = 0.7f;
}

AStompTargetActor::AStompTargetActor()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBox->SetBoxExtent(StompTargetExtent);
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionBox->SetCollisionObjectType(ECC_WorldDynamic);

	// 機体を物理的に押し止めないこと。Block にすると、行進してくる踏み台が機体を
	// 壁のように拘束し、貫通状態から復帰できなくなる。
	// 踏みつけは重なりで検出し、機体は踏み台を踏み潰して走り抜ける。
	CollisionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionBox->SetGenerateOverlapEvents(true);
	CollisionBox->SetSimulatePhysics(false);
	SetRootComponent(CollisionBox);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(CollisionBox);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AStompTargetActor::SetMarchDirection(const FVector& InDirection)
{
	const FVector Horizontal = InDirection.GetSafeNormal2D();
	if (Horizontal.IsNearlyZero())
	{
		return;
	}

	MarchDirection = Horizontal;
	SetActorRotation(MarchDirection.Rotation());
}

void AStompTargetActor::SetLane(ALaneActor* InLane, float StartDistance)
{
	if (!InLane)
	{
		return;
	}

	Lane = InLane;
	LaneDistance = FMath::Clamp(StartDistance, 0.0f, InLane->GetLength());

	// レーンの終端で消えるため、直進用の寿命は外す。塞がれて待つ間に寿命で消えないようにする。
	SetLifeSpan(0.0f);
	PlaceOnLane();
}

void AStompTargetActor::BeginPlay()
{
	Super::BeginPlay();

	// 到達点を越えて無限に進み続けないよう、一定時間で消す。時間の計測はエンジンの寿命機構に任せる。
	SetLifeSpan(LifeSpanSeconds);
}

void AStompTargetActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// レーンが途中で削除された場合は、その時点の向きのまま直進に切り替え、寿命で消えるようにする。
	if (Lane.IsStale())
	{
		Lane.Reset();
		SetLifeSpan(LifeSpanSeconds);
	}

	// 直進の踏み台は従来どおり壁をすり抜けて進み、寿命で消える。
	if (!Lane.IsValid())
	{
		bMarchBlocked = false;
		AddActorWorldOffset(MarchDirection * MarchSpeed * DeltaSeconds, true);
		return;
	}

	// レーンでは壁や建物の前で詰まって待つ（フィールド設計 5章の暫定判断）。建物が壊れて当たり判定が消えれば再開する。
	// 前の踏み台が詰まっていれば、重ならないよう車間を空けて後ろで待つ。
	bMarchBlocked = ProbeBlocked(MarchDirection) || ProbeTankAhead(MarchDirection);
	if (bMarchBlocked)
	{
		return;
	}

	AdvanceAlongLane(DeltaSeconds);
}

void AStompTargetActor::AdvanceAlongLane(float DeltaSeconds)
{
	LaneDistance += MarchSpeed * DeltaSeconds;
	if (LaneDistance >= Lane->GetLength())
	{
		Destroy();
		return;
	}

	PlaceOnLane();
}

void AStompTargetActor::PlaceOnLane()
{
	const ALaneActor* CurrentLane = Lane.Get();
	if (!CurrentLane)
	{
		return;
	}

	const FVector Direction = CurrentLane->GetDirectionAtDistance(LaneDistance);
	if (!Direction.IsNearlyZero())
	{
		MarchDirection = Direction;
	}

	// レーンは地面に描かれているため、車体の半分の高さだけ持ち上げて底面を地面に合わせる。
	const FVector Ground = CurrentLane->GetGroundLocationAtDistance(LaneDistance);
	const FVector Location = Ground + FVector(0.0f, 0.0f, CollisionBox->GetScaledBoxExtent().Z);
	SetActorLocationAndRotation(Location, MarchDirection.Rotation());
}

bool AStompTargetActor::ProbeBlocked(const FVector& Direction) const
{
	const UWorld* World = GetWorld();
	if (!World || Direction.IsNearlyZero())
	{
		return false;
	}

	// 車体の中心から、前端の先 BlockProbeDistance までを調べる。床は水平の線にかからない。
	const FVector Start = GetActorLocation();
	const FVector End = Start + Direction * (CollisionBox->GetScaledBoxExtent().X + BlockProbeDistance);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(StompTargetMarchProbe), false, this);
	FHitResult Hit;
	if (!World->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return false;
	}

	return Hit.ImpactNormal.Z < WalkableNormalZ;
}

bool AStompTargetActor::ProbeTankAhead(const FVector& Direction) const
{
	const UWorld* World = GetWorld();
	if (!World || Direction.IsNearlyZero())
	{
		return false;
	}

	const FVector Start = GetActorLocation();
	const FVector End = Start + Direction * (CollisionBox->GetScaledBoxExtent().X + FollowSpacing);

	// 踏み台は WorldDynamic。同じ種別の他の Actor も掛かるため、全件を受けて踏み台だけを見る。
	FCollisionQueryParams Params(SCENE_QUERY_STAT(StompTargetFollowProbe), false, this);
	TArray<FHitResult> Hits;
	World->LineTraceMultiByObjectType(Hits, Start, End, FCollisionObjectQueryParams(ECC_WorldDynamic), Params);

	return Hits.ContainsByPredicate([](const FHitResult& Hit)
	{
		return Cast<AStompTargetActor>(Hit.GetActor()) != nullptr;
	});
}

bool AStompTargetActor::ReceiveStomp_Implementation()
{
	if (Durability <= 0)
	{
		return false;
	}

	--Durability;
	if (Durability <= 0)
	{
		Destroy();
	}

	return true;
}
