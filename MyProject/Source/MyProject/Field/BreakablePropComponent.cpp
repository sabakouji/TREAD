// 踏みつけ加速メカゲーム — 分割しない小物（街灯など）

#include "Field/BreakablePropComponent.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "NiagaraFunctionLibrary.h"

UBreakablePropComponent::UBreakablePropComponent()
{
	// 重なりの通知で壊れるだけで、毎フレームの処理は持たない。
	PrimaryComponentTick.bCanEverTick = false;
}

void UBreakablePropComponent::BeginPlay()
{
	Super::BeginPlay();

	// 所有 Actor のルートの当たり判定に機体が重なったら壊れる。当たり判定は機体と「重なる」設定にしておくこと
	// （ABreakablePropActor が設定済み）。機体を止める設定だと、機体は壁として扱って減速・自滅してしまう。
	if (UPrimitiveComponent* Root = GetOwner() ? Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent()) : nullptr)
	{
		Root->OnComponentBeginOverlap.AddUniqueDynamic(this, &UBreakablePropComponent::HandleBeginOverlap);
	}
}

void UBreakablePropComponent::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 壊すのは機体（Pawn）だけ。踏み台など他の動く物では壊れない。
	if (Cast<APawn>(OtherActor))
	{
		Break(OtherActor);
	}
}

void UBreakablePropComponent::Break(AActor* Breaker)
{
	AActor* Owner = GetOwner();
	if (bBroken || !Owner)
	{
		return;
	}

	bBroken = true;
	Owner->SetActorEnableCollision(false);
	Owner->SetActorHiddenInGame(true);

	if (BreakFX)
	{
		const FVector BreakerVelocity = Breaker ? Breaker->GetVelocity() : FVector::ZeroVector;
		const FRotator Facing = BreakerVelocity.IsNearlyZero() ? Owner->GetActorRotation() : BreakerVelocity.Rotation();
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, BreakFX, Owner->GetActorLocation(), Facing);
	}

	OnBroken.Broadcast(this, Breaker);
}
