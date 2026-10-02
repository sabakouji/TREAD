// 踏みつけ加速メカゲーム — ロックオン（ステアリングアシスト）

#include "Vehicle/ImpactLockOnComponent.h"

#include "Collision/ImpactResolver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Field/TeamComponent.h"
#include "Tuning/ImpactTuningDataAsset.h"
#include "Tuning/VehicleTuningDataAsset.h"
#include "UI/HUD/TreadHUDDataComponent.h"
#include "UI/HUD/TreadHUDTags.h"
#include "UI/HUD/TreadHUDTypes.h"
#include "Vehicle/ImpactVehicleMovementComponent.h"
#include "Vehicle/ImpactVehiclePawn.h"

namespace
{
	/** これ未満の速さでは到達時間を推定できない（uu/s）。 */
	constexpr float MinMeaningfulSpeed = 1.0f;
}

UImpactLockOnComponent::UImpactLockOnComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UImpactLockOnComponent::BeginPlay()
{
	Super::BeginPlay();

	// 判断の結果を同じフレームの移動で使うため、移動より先に回す。
	// Tick の順序は既定では保証されないため、前提条件として明示する。
	if (const AImpactVehiclePawn* Vehicle = GetOwnerVehicle())
	{
		if (UImpactVehicleMovementComponent* Movement = Vehicle->GetVehicleMovement())
		{
			Movement->AddTickPrerequisiteComponent(this);
		}
	}
}

void UImpactLockOnComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 捕捉したまま消えると、相手の被ロック一覧に無効な参照が残る。
	SetTarget(nullptr);

	Super::EndPlay(EndPlayReason);
}

void UImpactLockOnComponent::TickComponent(
	float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AImpactVehiclePawn* Vehicle = GetOwnerVehicle();
	UImpactVehicleMovementComponent* Movement = Vehicle ? Vehicle->GetVehicleMovement() : nullptr;
	const UImpactTuningDataAsset* Rules = Vehicle ? Vehicle->GetImpactTuning() : nullptr;
	const UVehicleTuningDataAsset* Tuning = Vehicle ? Vehicle->GetTuning() : nullptr;
	if (!Movement || !Rules || !Tuning)
	{
		return;
	}

	SetTarget(FindTarget(*Rules));

	const AImpactVehiclePawn* CurrentTarget = Target.Get();
	if (!CurrentTarget)
	{
		LeadPoint = FVector::ZeroVector;
		AimErrorDegrees = 0.0f;
		TimeToContact = -1.0f;
		CameraYawOffset = 0.0f;
		AssistState = EImpactAssistState::NoTarget;
		AssistYawRate = 0.0f;
		Movement->SetAssistYawRate(0.0f);
		PublishHUDValue(*Vehicle);
		return;
	}

	LeadPoint = ComputeLeadPoint(*CurrentTarget, *Rules);
	TimeToContact = ComputeTimeToContact(*CurrentTarget, *Tuning);

	// カメラは相手そのものの方向へ寄せる。リード点へ向けると、相手の先の何もない場所を向いてしまう。
	const FVector Forward = Vehicle->GetActorForwardVector().GetSafeNormal2D();
	const FVector ToTarget = (CurrentTarget->GetActorLocation() - Vehicle->GetActorLocation()).GetSafeNormal2D();
	const float TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(
		FVector::CrossProduct(Forward, ToTarget).Z, FVector::DotProduct(Forward, ToTarget)));
	CameraYawOffset = TargetYaw * Rules->LockOnCameraYawBlend;

	AssistYawRate = ComputeAssistYawRate(DeltaTime, *Rules);
	Movement->SetAssistYawRate(AssistYawRate);
	PublishHUDValue(*Vehicle);
}

void UImpactLockOnComponent::PublishHUDValue(const AImpactVehiclePawn& Vehicle) const
{
	UTreadHUDDataComponent* HUDData = Vehicle.GetHUDData();
	if (!HUDData)
	{
		return;
	}

	float State = TreadHUDLockOn::Locked;
	if (AssistState == EImpactAssistState::NoTarget)
	{
		State = TreadHUDLockOn::None;
	}
	else if (AssistState == EImpactAssistState::Active)
	{
		State = TreadHUDLockOn::Assisting;
	}
	HUDData->SetValue(TreadHUDTags::LockOn, State, TreadHUDLockOn::None, TreadHUDLockOn::Assisting);
}

AImpactVehiclePawn* UImpactLockOnComponent::GetOwnerVehicle() const
{
	return Cast<AImpactVehiclePawn>(GetOwner());
}

AImpactVehiclePawn* UImpactLockOnComponent::FindTarget(const UImpactTuningDataAsset& Rules) const
{
	const AImpactVehiclePawn* Vehicle = GetOwnerVehicle();
	UWorld* World = GetWorld();
	if (!Vehicle || !World)
	{
		return nullptr;
	}

	const FVector Location = Vehicle->GetActorLocation();
	const FVector Forward = Vehicle->GetActorForwardVector().GetSafeNormal2D();

	AImpactVehiclePawn* Nearest = nullptr;
	float NearestDistance = TNumericLimits<float>::Max();

	// 機体は同時に数機しかいないため素直に走査する。増える場合は GameMode 側に一覧を持たせる。
	for (TActorIterator<AImpactVehiclePawn> It(World); It; ++It)
	{
		AImpactVehiclePawn* Candidate = *It;

		// 撃破されて当たり判定を切った機体と、味方（同じ陣営・中立）は捕捉しない。
		if (!Candidate || Candidate == Vehicle || !Candidate->GetActorEnableCollision()
			|| !UTeamComponent::AreHostile(Vehicle, Candidate))
		{
			continue;
		}

		const FVector ToCandidate = Candidate->GetActorLocation() - Location;
		const float Distance = ToCandidate.Size2D();
		if (Distance > Rules.LockOnRange || Distance >= NearestDistance)
		{
			continue;
		}

		const FVector Direction = ToCandidate.GetSafeNormal2D();
		if (Direction.IsNearlyZero())
		{
			continue;
		}

		// クロスヘア方式（企画書 3-2）を踏襲し、前方の範囲から外れれば捕捉は解除される。
		const float Angle = FMath::RadiansToDegrees(
			FMath::Acos(FMath::Clamp(FVector::DotProduct(Forward, Direction), -1.0f, 1.0f)));
		if (Angle > Rules.LockOnConeAngle)
		{
			continue;
		}

		Nearest = Candidate;
		NearestDistance = Distance;
	}

	return Nearest;
}

void UImpactLockOnComponent::SetTarget(AImpactVehiclePawn* NewTarget)
{
	AImpactVehiclePawn* Previous = Target.Get();
	if (Previous == NewTarget)
	{
		return;
	}

	AImpactVehiclePawn* Vehicle = GetOwnerVehicle();
	if (!Vehicle)
	{
		return;
	}

	if (Previous)
	{
		if (UImpactLockOnComponent* PreviousLockOn = Previous->GetLockOn())
		{
			PreviousLockOn->RemoveLocker(*Vehicle);
		}
	}

	if (NewTarget)
	{
		if (UImpactLockOnComponent* NextLockOn = NewTarget->GetLockOn())
		{
			NextLockOn->AddLocker(*Vehicle);
		}
	}

	Target = NewTarget;
}

void UImpactLockOnComponent::AddLocker(AImpactVehiclePawn& Locker)
{
	LockedOnBy.RemoveAll([](const TWeakObjectPtr<AImpactVehiclePawn>& Entry) { return !Entry.IsValid(); });
	LockedOnBy.AddUnique(&Locker);
}

void UImpactLockOnComponent::RemoveLocker(AImpactVehiclePawn& Locker)
{
	LockedOnBy.RemoveAll([&Locker](const TWeakObjectPtr<AImpactVehiclePawn>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == &Locker;
	});
}

FVector UImpactLockOnComponent::ComputeLeadPoint(
	const AImpactVehiclePawn& InTarget, const UImpactTuningDataAsset& Rules) const
{
	const AImpactVehiclePawn* Vehicle = GetOwnerVehicle();
	const UImpactVehicleMovementComponent* TargetMovement = InTarget.GetVehicleMovement();
	if (!Vehicle || !TargetMovement)
	{
		return InTarget.GetActorLocation();
	}

	const FVector TargetLocation = InTarget.GetActorLocation();
	const FVector TargetVelocity = TargetMovement->GetHorizontalVelocity();
	const float SelfSpeed = FMath::Max(Vehicle->GetCurrentSpeed(), MinMeaningfulSpeed);

	// 到達時間と予測点は互いに依存する。一度だけ推定を修正すれば、この用途には十分な精度になる。
	FVector Lead = TargetLocation;
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		const float TravelTime = FMath::Clamp(
			(Lead - Vehicle->GetActorLocation()).Size2D() / SelfSpeed, 0.0f, Rules.LockOnMaxLeadTime);
		Lead = TargetLocation + TargetVelocity * TravelTime;
	}

	return Lead;
}

float UImpactLockOnComponent::ComputeTimeToContact(
	const AImpactVehiclePawn& InTarget, const UVehicleTuningDataAsset& Tuning) const
{
	const AImpactVehiclePawn* Vehicle = GetOwnerVehicle();
	const UImpactVehicleMovementComponent* Movement = Vehicle ? Vehicle->GetVehicleMovement() : nullptr;
	const UImpactVehicleMovementComponent* TargetMovement = InTarget.GetVehicleMovement();
	const UVehicleTuningDataAsset* TargetTuning = InTarget.GetTuning();
	if (!Movement || !TargetMovement || !TargetTuning)
	{
		return -1.0f;
	}

	const FVector ToTarget = InTarget.GetActorLocation() - Vehicle->GetActorLocation();
	const FVector Direction = ToTarget.GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		return 0.0f;
	}

	const float ClosingSpeed = FVector::DotProduct(
		Movement->GetHorizontalVelocity() - TargetMovement->GetHorizontalVelocity(), Direction);
	if (ClosingSpeed <= 0.0f)
	{
		return -1.0f;
	}

	// 接触は双方の実効半径で起きる。中心までの距離で測ると、アシストを切るのが遅れる。
	const float ContactDistance =
		FImpactResolver::GetContactRadius(
			Vehicle->GetActorForwardVector(), Direction, Movement->GetCurrentSpeed(), Tuning)
		+ FImpactResolver::GetContactRadius(
			InTarget.GetActorForwardVector(), -Direction, TargetMovement->GetCurrentSpeed(), *TargetTuning);

	return FMath::Max(ToTarget.Size2D() - ContactDistance, 0.0f) / ClosingSpeed;
}

float UImpactLockOnComponent::ComputeAssistYawRate(float DeltaTime, const UImpactTuningDataAsset& Rules)
{
	const AImpactVehiclePawn* Vehicle = GetOwnerVehicle();
	const UImpactVehicleMovementComponent* Movement = Vehicle ? Vehicle->GetVehicleMovement() : nullptr;
	if (!Vehicle || !Movement || DeltaTime <= 0.0f)
	{
		AssistState = EImpactAssistState::Disabled;
		return 0.0f;
	}

	// 行動不能中・弾かれ中・操作が止められている間は機首を向けられない。
	const EVehicleDriveState DriveState = Movement->GetDriveState();
	if (Vehicle->AreControlsLocked()
		|| DriveState == EVehicleDriveState::Stunned
		|| DriveState == EVehicleDriveState::KnockedBack)
	{
		AssistState = EImpactAssistState::Disabled;
		return 0.0f;
	}

	const FVector Forward = Vehicle->GetActorForwardVector().GetSafeNormal2D();
	const FVector ToLead = (LeadPoint - Vehicle->GetActorLocation()).GetSafeNormal2D();
	if (Forward.IsNearlyZero() || ToLead.IsNearlyZero())
	{
		AssistState = EImpactAssistState::Disabled;
		return 0.0f;
	}

	AimErrorDegrees = FMath::RadiansToDegrees(FMath::Atan2(
		FVector::CrossProduct(Forward, ToLead).Z, FVector::DotProduct(Forward, ToLead)));

	// 作動範囲は狭いままにする。広げると「曲がる」用途に使えてしまい、
	// 速いほど曲がれないという設計が崩れる（仕様 3-3）。
	if (FMath::Abs(AimErrorDegrees) > Rules.AssistConeAngle)
	{
		AssistState = EImpactAssistState::OutOfCone;
		return 0.0f;
	}

	// 命中直前は切る。張り付きが強すぎると、被弾側に避けられた・避けられなかったの手応えが残らない。
	if (TimeToContact >= 0.0f && TimeToContact <= Rules.AssistCutoffTime)
	{
		AssistState = EImpactAssistState::Cutoff;
		return 0.0f;
	}

	AssistState = EImpactAssistState::Active;

	// 固定枠を上限としつつ、今フレームで誤差を詰めきる以上には回さない。
	const float Rate = FMath::Min(Rules.AssistTurnRate, FMath::Abs(AimErrorDegrees) / DeltaTime);
	return FMath::Sign(AimErrorDegrees) * Rate;
}
