// 踏みつけ加速メカゲーム — 拠点（ゴール）

#include "Field/GoalComponent.h"

#include "Collision/ImpactResolver.h"
#include "Components/PrimitiveComponent.h"
#include "Field/FieldPointComponent.h"
#include "Field/FieldSubsystem.h"
#include "Field/TeamComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Score/ImpactScoreTypes.h"
#include "Tuning/VehicleTuningDataAsset.h"

UGoalComponent::UGoalComponent()
{
	// 耐久値は衝突を受けたときだけ変わる。毎フレームの処理を持たない。
	PrimaryComponentTick.bCanEverTick = false;
}

void UGoalComponent::BeginPlay()
{
	Super::BeginPlay();

	Durability = MaxDurability;

	if (UFieldSubsystem* Field = UFieldSubsystem::Get(this))
	{
		Field->RegisterGoal(this);
	}
}

void UGoalComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UFieldSubsystem* Field = UFieldSubsystem::Get(this))
	{
		Field->UnregisterGoal(this);
	}

	Super::EndPlay(EndPlayReason);
}

int32 UGoalComponent::ReceiveImpact(float ImpactSpeed, const UVehicleTuningDataAsset& Tuning)
{
	if (IsGoalDestroyed())
	{
		return 0;
	}

	// 速度を積まない限りダメージは入らない。
	const float RequiredSpeed = FImpactResolver::GetRequiredSpeed(RequiredRank, Tuning);
	if (ImpactSpeed < RequiredSpeed)
	{
		return 0;
	}

	const int32 ExtraDamage = FMath::FloorToInt((ImpactSpeed - RequiredSpeed) / ExtraDamageSpeedStep);
	const int32 Damage = FMath::Min(1 + ExtraDamage, Durability);
	Durability -= Damage;

	AActor* Owner = GetOwner();
	UE_LOG(LogImpactMatch, Log, TEXT("%s took %d damage at impact speed %.0f (durability %d / %d)"),
		*GetNameSafe(Owner), Damage, ImpactSpeed, Durability, MaxDurability);

	if (IsGoalDestroyed())
	{
		// 破壊後は当たり判定と表示を切る。試合をどう扱うかは通知を受けた側（GameMode）が決める。
		if (Owner)
		{
			Owner->SetActorEnableCollision(false);
			Owner->SetActorHiddenInGame(true);
		}

		UE_LOG(LogImpactMatch, Log, TEXT("%s destroyed"), *GetNameSafe(Owner));
		OnGoalDestroyed.Broadcast(this);
	}

	return Damage;
}

FImpactReceiveResult UGoalComponent::ReceiveVehicleImpact_Implementation(const FImpactReceiveContext& Context)
{
	FImpactReceiveResult Result;

	// ダメージを与えられるのは敵対する陣営の機体だけ。自陣の拠点を守る NPC が当たっても削れない。
	// この規則はここ1箇所に置き、機体側は相手が拠点であることを知らない。
	const bool bCanDamage = Context.Instigator && UTeamComponent::AreHostile(Context.Instigator, GetOwner());
	if (!bCanDamage || !Context.Tuning)
	{
		return Result;
	}

	const int32 Damage = ReceiveImpact(Context.ImpactSpeed, *Context.Tuning);
	if (Damage > 0)
	{
		Result.Outcome = EImpactReceiveOutcome::Damaged;
		Result.Damage = Damage;
	}

	return Result;
}

FTransform UGoalComponent::GetGuardTransform() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return FTransform::Identity;
	}

	// 防衛ラインのマーカーは地面に置く想定のため、マーカーの高さを地面とみなす。
	const UFieldSubsystem* Field = UFieldSubsystem::Get(this);
	const int32 TeamId = UTeamComponent::GetTeamIdOf(Owner);
	if (const UFieldPointComponent* Point = Field
		? Field->FindNearestFieldPoint(EFieldPointRole::DefenseLine, TeamId, Owner->GetActorLocation())
		: nullptr)
	{
		const FVector Location = Point->GetComponentLocation() + FVector(0.0f, 0.0f, GuardSpawnHeight);
		return FTransform(FRotator(0.0f, Point->GetComponentRotation().Yaw, 0.0f), Location);
	}

	const FVector Forward = Owner->GetActorForwardVector().GetSafeNormal2D();
	const FVector Center = Owner->GetActorLocation() + Forward * GuardDistance;
	const FVector Location(Center.X, Center.Y, GetGroundZ() + GuardSpawnHeight);
	return FTransform(FRotator(0.0f, Owner->GetActorRotation().Yaw, 0.0f), Location);
}

float UGoalComponent::GetGroundZ() const
{
	const AActor* Owner = GetOwner();
	const UPrimitiveComponent* Root = Owner ? Cast<UPrimitiveComponent>(Owner->GetRootComponent()) : nullptr;
	if (!Root)
	{
		return Owner ? Owner->GetActorLocation().Z : 0.0f;
	}

	// 拠点の底面を地面とみなす。
	return Root->Bounds.Origin.Z - Root->Bounds.BoxExtent.Z;
}
