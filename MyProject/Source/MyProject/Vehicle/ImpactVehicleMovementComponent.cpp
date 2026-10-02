// 踏みつけ加速メカゲーム — 機体の移動コンポーネント

#include "Vehicle/ImpactVehicleMovementComponent.h"

#include "Collision/ImpactReceiver.h"
#include "Collision/ImpactResolver.h"
#include "Collision/Stompable.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Tuning/ImpactTuningDataAsset.h"
#include "Tuning/VehicleTuningDataAsset.h"
#include "UI/HUD/TreadHUDDataComponent.h"
#include "UI/HUD/TreadHUDTags.h"
#include "Vehicle/ImpactVehiclePawn.h"

namespace
{
	/** これ以上の速さを持つときのみ進行方向を意味のある値として扱う（uu/s）。 */
	constexpr float MinMeaningfulSpeed = 1.0f;

	/** 接地とみなすヒット法線の上向き成分。約45度以内の斜面を地面とする。 */
	constexpr float GroundNormalThreshold = 0.7f;

	/** 接地時に地面へ押し付けておく速度（uu/s）。斜面で浮かないようにする。 */
	constexpr float GroundedStickSpeed = -10.0f;

	/**
	 * 衝突の診断ログ。壁・建物への衝突と判定の結果、判定から漏れた衝突を LogImpactVehicle に出す。
	 * 既定では無効。調査するときにコンソールで `Impact.LogWallHits 1` とする（FIELD-02）。
	 */
	TAutoConsoleVariable<bool> CVarLogWallHits(
		TEXT("Impact.LogWallHits"),
		false,
		TEXT("Log every wall/obstacle hit of vehicles and how it was resolved (diagnostics)."));

	/** 速度に対してこの割合しか進めなかったフレームを「進めなかった」とみなす。 */
	constexpr float StuckProgressRatio = 0.1f;

	bool ShouldLogWallHits()
	{
		return CVarLogWallHits.GetValueOnGameThread();
	}

	const TCHAR* LexToDisplayString(EImpactReceiveOutcome Outcome)
	{
		switch (Outcome)
		{
		case EImpactReceiveOutcome::Unaffected:
			return TEXT("Unaffected");
		case EImpactReceiveOutcome::Damaged:
			return TEXT("Damaged");
		case EImpactReceiveOutcome::Destroyed:
			return TEXT("Destroyed");
		case EImpactReceiveOutcome::Weakened:
			return TEXT("Weakened");
		default:
			return TEXT("Unknown");
		}
	}

	/** 衝突1件の内訳をログに出す。 */
	void LogWallHit(const UActorComponent& Movement, const TCHAR* Source, const TCHAR* Handling, const FHitResult& Hit, float Speed)
	{
		if (!ShouldLogWallHits())
		{
			return;
		}

		UE_LOG(LogImpactVehicle, Log,
			TEXT("[HitLog] %s %s: %s -> %s (comp %s) normal %s time %.3f start-penetrating %d depth %.2f speed %.0f"),
			*GetNameSafe(Movement.GetOwner()), Source, Handling, *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()),
			*Hit.ImpactNormal.ToCompactString(), Hit.Time, Hit.bStartPenetrating ? 1 : 0, Hit.PenetrationDepth, Speed);
	}
}

UImpactVehicleMovementComponent::UImpactVehicleMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UImpactVehicleMovementComponent::SetMoveInput(const FVector2D& InMoveInput)
{
	MoveInput.X = FMath::Clamp(InMoveInput.X, -1.0f, 1.0f);
	MoveInput.Y = FMath::Clamp(InMoveInput.Y, -1.0f, 1.0f);
}

void UImpactVehicleMovementComponent::SetBrakeInput(bool bInBraking)
{
	bBrakeInput = bInBraking;
}

void UImpactVehicleMovementComponent::SetAssistYawRate(float DegreesPerSecond)
{
	AssistYawRate = DegreesPerSecond;
}

float UImpactVehicleMovementComponent::GetTurnRadius() const
{
	if (CurrentSpeed < MinMeaningfulSpeed || FMath::IsNearlyZero(MoveInput.X) || FMath::IsNearlyZero(TurnRate))
	{
		return 0.0f;
	}

	// 半径 = 速さ / 角速度。角速度は rad/s に直して用いる。
	const float AngularSpeedRadians = FMath::DegreesToRadians(TurnRate * FMath::Abs(MoveInput.X));
	return CurrentSpeed / AngularSpeedRadians;
}

float UImpactVehicleMovementComponent::GetMaxSpeed() const
{
	const UVehicleTuningDataAsset* Tuning = GetTuning();
	return Tuning ? Tuning->MaxSpeed : 0.0f;
}

const UVehicleTuningDataAsset* UImpactVehicleMovementComponent::GetTuning() const
{
	const AImpactVehiclePawn* VehiclePawn = Cast<AImpactVehiclePawn>(PawnOwner);
	return VehiclePawn ? VehiclePawn->GetTuning() : nullptr;
}

const UImpactTuningDataAsset* UImpactVehicleMovementComponent::GetImpactTuning() const
{
	const AImpactVehiclePawn* VehiclePawn = Cast<AImpactVehiclePawn>(PawnOwner);
	return VehiclePawn ? VehiclePawn->GetImpactTuning() : nullptr;
}

float UImpactVehicleMovementComponent::GetDriftAngleDegrees() const
{
	if (!UpdatedComponent || CurrentSpeed < MinMeaningfulSpeed)
	{
		return 0.0f;
	}

	const FVector Facing = UpdatedComponent->GetForwardVector().GetSafeNormal2D();
	const float Dot = FMath::Clamp(FVector::DotProduct(Facing, MoveDirection), -1.0f, 1.0f);
	return FMath::RadiansToDegrees(FMath::Acos(Dot));
}

void UImpactVehicleMovementComponent::UpdateUncontrolled(float DeltaTime, const UImpactTuningDataAsset& Rules)
{
	if (StunRemaining > 0.0f)
	{
		StunRemaining = FMath::Max(StunRemaining - DeltaTime, 0.0f);
		DriveState = EVehicleDriveState::Stunned;
	}
	else
	{
		KnockbackRemaining = FMath::Max(KnockbackRemaining - DeltaTime, 0.0f);
		DriveState = EVehicleDriveState::KnockedBack;
	}

	// 押しっぱなしの入力は Triggered で毎フレーム設定し直されるため、ここで捨てても操作可能に戻れば復帰する。
	MoveInput = FVector2D::ZeroVector;
	bBrakeInput = false;

	// 弾かれた方向・拮抗の離脱方向へ流されながら減速する。旋回と慣性ブレンドは行わない。
	// 自滅・撃破の軽いダウンは速度 0 から始まるため、その場で止まったままになる。
	CurrentSpeed = FMath::FInterpConstantTo(CurrentSpeed, 0.0f, DeltaTime, Rules.KnockbackDeceleration);
}

void UImpactVehicleMovementComponent::TickComponent(
	float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (ShouldSkipUpdate(DeltaTime) || !UpdatedComponent || !PawnOwner)
	{
		return;
	}

	const UVehicleTuningDataAsset* Tuning = GetTuning();
	const UImpactTuningDataAsset* Rules = GetImpactTuning();
	if (!Tuning || !Rules)
	{
		// 調整値が未割り当てでは挙動を決定できない。停止状態を保つ（未設定の警告は Pawn の BeginPlay で出す）。
		CurrentSpeed = 0.0f;
		UpdateComponentVelocity();
		return;
	}

	// 行動不能中・弾かれ中は入力を一切受け付けない。減速・落下と衝突処理のみ継続する。
	if (StunRemaining > 0.0f || KnockbackRemaining > 0.0f)
	{
		UpdateUncontrolled(DeltaTime, *Rules);
		UpdateVerticalSpeed(DeltaTime);
		ApplyMovement(DeltaTime);
		DetectBumperContacts(*Tuning, *Rules);
		PublishHUDValues(*Tuning, *Rules);
		return;
	}

	// 処理順はヘッダのコメントに記載した順序を厳守する。
	UpdateDriveState(DeltaTime, *Tuning);
	ApplyTurn(DeltaTime, *Tuning);
	ApplyInertiaBlend(*Tuning);
	UpdateSpeed(DeltaTime, *Tuning);
	UpdateVerticalSpeed(DeltaTime);
	ApplyMovement(DeltaTime);
	DetectBumperContacts(*Tuning, *Rules);
	PublishHUDValues(*Tuning, *Rules);

	// 入力はここでクリアしない。
	// Enhanced Input の処理は PlayerController の Tick 内で行われ、本コンポーネントの Tick と
	// 同じ Tick グループに属するため、両者の順序は保証されない。
	// Tick 末尾でクリアすると、コンポーネントが先に回った場合に入力が一度も読まれず破棄される。
	// 入力の解除は Completed イベント（キーを離した時）で行う。
}

void UImpactVehicleMovementComponent::PublishHUDValues(const UVehicleTuningDataAsset& Tuning, const UImpactTuningDataAsset& Rules) const
{
	const AImpactVehiclePawn* VehiclePawn = Cast<AImpactVehiclePawn>(PawnOwner);
	UTreadHUDDataComponent* HUDData = VehiclePawn ? VehiclePawn->GetHUDData() : nullptr;
	if (!HUDData)
	{
		return;
	}

	HUDData->SetValue(TreadHUDTags::Speed, CurrentSpeed, 0.0f, Tuning.MaxSpeed);

	// 行動不能は自滅（CrashStunDuration）と拮抗（ClashStunDuration）で長さが違う。長いほうを満了とする。
	const float LongestStun = FMath::Max(Tuning.CrashStunDuration, Rules.ClashStunDuration);
	HUDData->SetValue(TreadHUDTags::Stun, StunRemaining, 0.0f, LongestStun);
}

void UImpactVehicleMovementComponent::UpdateDriveState(float DeltaTime, const UVehicleTuningDataAsset& Tuning)
{
	const bool bTurning = !FMath::IsNearlyZero(MoveInput.X);

	EVehicleDriveState NewState = EVehicleDriveState::Cruising;
	if (bBrakeInput)
	{
		NewState = bTurning ? EVehicleDriveState::BrakeTurning : EVehicleDriveState::Braking;
	}

	const bool bWasBrakeTurning = DriveState == EVehicleDriveState::BrakeTurning;
	const bool bIsBrakeTurning = NewState == EVehicleDriveState::BrakeTurning;

	if (bIsBrakeTurning && !bWasBrakeTurning)
	{
		// ブレーキターン開始。この瞬間の勢いを退避する。
		StoredSpeed = CurrentSpeed;
		BrakeTurnElapsed = 0.0f;
	}
	else if (bIsBrakeTurning)
	{
		BrakeTurnElapsed += DeltaTime;

		// 保存できる時間には上限を設け、無限に溜められないようにする。
		if (BrakeTurnElapsed > Tuning.BrakeTurnMaxHoldTime)
		{
			StoredSpeed = FMath::FInterpConstantTo(StoredSpeed, 0.0f, DeltaTime, Tuning.NaturalDeceleration);
		}
	}
	else if (bWasBrakeTurning)
	{
		// ブレーキターン終了。保存した勢いの一部を取り戻して再加速する。
		CurrentSpeed = FMath::Max(CurrentSpeed, StoredSpeed * Tuning.BrakeTurnSpeedRetention);
		StoredSpeed = 0.0f;
		BrakeTurnElapsed = 0.0f;
	}

	DriveState = NewState;
}

void UImpactVehicleMovementComponent::ApplyTurn(float DeltaTime, const UVehicleTuningDataAsset& Tuning)
{
	// ブレーキターン中は速度依存の制限を外し、専用の角速度上限に切り替える。
	// この上限が最小旋回半径を生み、瞬間反転ではなく弧を描く挙動になる。
	TurnRate = (DriveState == EVehicleDriveState::BrakeTurning)
		? Tuning.MaxBrakeTurnAngularSpeed
		: Tuning.GetTurnRateForSpeed(CurrentSpeed);

	// 通常の旋回に、ロックオンのアシスト（速度非依存の固定枠）を加算する。
	const float DeltaYaw = (MoveInput.X * TurnRate + AssistYawRate) * DeltaTime;
	if (FMath::IsNearlyZero(DeltaYaw))
	{
		return;
	}

	UpdatedComponent->AddLocalRotation(FRotator(0.0f, DeltaYaw, 0.0f));
}

void UImpactVehicleMovementComponent::ApplyInertiaBlend(const UVehicleTuningDataAsset& Tuning)
{
	InertiaWeight = Tuning.GetInertiaWeightForSpeed(CurrentSpeed);

	const FVector Facing = UpdatedComponent->GetForwardVector().GetSafeNormal2D();
	if (Facing.IsNearlyZero())
	{
		return;
	}

	// 停止からの発進時は慣性の参照方向が無いため、機首方向をそのまま採用する。
	if (CurrentSpeed < MinMeaningfulSpeed || MoveDirection.IsNearlyZero())
	{
		MoveDirection = Facing;
		return;
	}

	// 方向の補間は線形補間だと逆向き付近で退化するため、回転として球面補間する。
	const FQuat FacingQuat = FRotationMatrix::MakeFromX(Facing).ToQuat();
	const FQuat InertiaQuat = FRotationMatrix::MakeFromX(MoveDirection).ToQuat();
	const FQuat BlendedQuat = FQuat::Slerp(FacingQuat, InertiaQuat, InertiaWeight);

	MoveDirection = BlendedQuat.GetForwardVector().GetSafeNormal2D();
	if (MoveDirection.IsNearlyZero())
	{
		MoveDirection = Facing;
	}
}

void UImpactVehicleMovementComponent::UpdateSpeed(float DeltaTime, const UVehicleTuningDataAsset& Tuning)
{
	// ブレーキ中・ブレーキターン中は 0 を目標に強く減速する。
	// ブレーキターンでは実速度が落ちても StoredSpeed に勢いが退避されている。
	if (DriveState != EVehicleDriveState::Cruising)
	{
		CurrentSpeed = FMath::FInterpConstantTo(CurrentSpeed, 0.0f, DeltaTime, Tuning.BrakeDeceleration);
		return;
	}

	// スロットルを踏んでいる間は基本巡航速度を目標とする。
	// これを超える速度は踏みつけ加速でのみ得られ、自然減速で基本速度まで戻る。
	const bool bThrottleHeld = MoveInput.Y > 0.0f;
	const float TargetSpeed = bThrottleHeld ? Tuning.BaseSpeed : 0.0f;

	if (CurrentSpeed < TargetSpeed)
	{
		CurrentSpeed = FMath::FInterpConstantTo(CurrentSpeed, TargetSpeed, DeltaTime, Tuning.ThrottleAcceleration);
	}
	else if (CurrentSpeed > TargetSpeed)
	{
		// デバッグ操作で加速の維持が ON なら、スロットル中は BaseSpeed 超過分を減衰させない。
		// スロットルを離したとき（目標 0）は通常どおり減速する。ブレーキと衝突の減速は別の処理で行われる。
		const bool bHoldBoost = bDebugHoldBoost && bThrottleHeld;
		if (!bHoldBoost)
		{
			CurrentSpeed = FMath::FInterpConstantTo(CurrentSpeed, TargetSpeed, DeltaTime, Tuning.NaturalDeceleration);
		}
	}

	CurrentSpeed = FMath::Clamp(CurrentSpeed, 0.0f, Tuning.MaxSpeed);
}

void UImpactVehicleMovementComponent::UpdateVerticalSpeed(float DeltaTime)
{
	if (bGrounded)
	{
		// 接地中は地面へ軽く押し付け、微小な段差で浮き上がらないようにする。
		VerticalSpeed = GroundedStickSpeed;
		return;
	}

	VerticalSpeed += GetWorld()->GetGravityZ() * DeltaTime;
}

void UImpactVehicleMovementComponent::ApplyMovement(float DeltaTime)
{
	// UMovementComponent::Velocity は UpdateComponentVelocity() で Pawn へ反映される。
	const FVector HorizontalVelocity = MoveDirection * CurrentSpeed;
	Velocity = HorizontalVelocity + FVector(0.0f, 0.0f, VerticalSpeed);

	// 今フレームの接地状態は移動結果から判定し直す。
	bGrounded = false;
	ResolvedThisMove.Reset();

	// 1. 水平の移動。床は衝突相手にならず、壁・建物・機体への衝突を必ず判定に回せる。
	const FVector HorizontalDelta = HorizontalVelocity * DeltaTime;
	if (!HorizontalDelta.IsNearlyZero())
	{
		const FVector StartLocation = UpdatedComponent->GetComponentLocation();

		FHitResult Hit;
		SafeMoveUpdatedComponent(HorizontalDelta, UpdatedComponent->GetComponentQuat(), true, Hit);
		if (Hit.IsValidBlockingHit())
		{
			HandleMoveHit(Hit, DeltaTime);
		}
		else if (Hit.bBlockingHit)
		{
			// 掃引の開始時点で既にめり込んでいた衝突。エンジンは有効な衝突として扱わず、判定にも回らない。
			LogWallHit(*this, TEXT("move"), TEXT("NOT RESOLVED (start penetrating)"), Hit, CurrentSpeed);
		}

		// 速度があるのにほとんど進めなかったフレーム。何にも判定されずに押し止められていないかを見る。
		const float Moved = FVector::Dist2D(StartLocation, UpdatedComponent->GetComponentLocation());
		if (ShouldLogWallHits() && Moved < HorizontalDelta.Size2D() * StuckProgressRatio)
		{
			UE_LOG(LogImpactVehicle, Log, TEXT("[HitLog] %s move: barely moved %.1f / %.1f uu at speed %.0f (resolved this move: %d, last hit %s)"),
				*GetNameSafe(GetOwner()), Moved, HorizontalDelta.Size2D(), CurrentSpeed, ResolvedThisMove.Num(),
				*GetNameSafe(Hit.GetActor()));
		}
	}

	// 2. 垂直の移動（重力・地面への押し付け）。床なら接地する。
	// 落下中に壁・建物の側面へ触れた場合も衝突の判定に回す。真下へ落ちて建物の縁に触れたときなど、
	// 水平の移動では当たらなかった相手を取りこぼさないため。水平の移動で既に判定した相手は ResolvedThisMove で除かれる。
	// 水平の速さがほぼ無い落下では、判定しても破壊・自滅には至らず滑るだけになる。
	const FVector VerticalDelta(0.0f, 0.0f, VerticalSpeed * DeltaTime);
	if (!VerticalDelta.IsNearlyZero())
	{
		FHitResult Hit;
		SafeMoveUpdatedComponent(VerticalDelta, UpdatedComponent->GetComponentQuat(), true, Hit);
		if (Hit.IsValidBlockingHit())
		{
			if (Hit.ImpactNormal.Z >= GroundNormalThreshold)
			{
				Land();
			}
			else
			{
				LogWallHit(*this, TEXT("vertical"), TEXT("resolve"), Hit, CurrentSpeed);
				ResolveBlockingHit(Hit);
				SlideAlongSurface(VerticalDelta, 1.0f - Hit.Time, Hit.Normal, Hit, false);
			}
		}
	}

	UpdateComponentVelocity();
}

void UImpactVehicleMovementComponent::HandleMoveHit(FHitResult& Hit, float DeltaTime)
{
	// 踏み台は機体を Block しないため、ここには来ない。
	// 踏みつけの判定は TryStompTarget が重なりを起点に行う。
	if (Hit.ImpactNormal.Z >= GroundNormalThreshold)
	{
		// 上り斜面に当たった。地面として扱い、斜面に沿って登る。地面は衝突解決の対象にしない。
		LogWallHit(*this, TEXT("move"), TEXT("ground"), Hit, CurrentSpeed);
		Land();
	}
	else
	{
		// 壁・障害物・機体への衝突。破壊 / 弾き / 自滅 / 追突のいずれかへ振り分ける。
		LogWallHit(*this, TEXT("move"), TEXT("resolve"), Hit, CurrentSpeed);
		ResolveBlockingHit(Hit);
	}

	// 残りの移動を面に沿って滑らせる。途中で別の面に当たれば HandleImpact で同じ判定に回る。
	SlideAlongSurface(Hit.TraceEnd - Hit.TraceStart, 1.0f - Hit.Time, Hit.Normal, Hit, true);
}

void UImpactVehicleMovementComponent::HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta)
{
	Super::HandleImpact(Hit, TimeSlice, MoveDelta);

	if (!Hit.IsValidBlockingHit())
	{
		return;
	}

	if (Hit.ImpactNormal.Z >= GroundNormalThreshold)
	{
		Land();
		return;
	}

	LogWallHit(*this, TEXT("slide"), TEXT("resolve"), Hit, CurrentSpeed);

	// 解決の処理は Hit を書き換えないが、共通の関数が非 const を受けるため写しを渡す。
	FHitResult SlideHit = Hit;
	ResolveBlockingHit(SlideHit);
}

void UImpactVehicleMovementComponent::ResolveBlockingHit(FHitResult& Hit)
{
	const UVehicleTuningDataAsset* Tuning = GetTuning();
	const UImpactTuningDataAsset* Rules = GetImpactTuning();
	AActor* HitActor = Hit.GetActor();
	if (!Tuning || !Rules)
	{
		return;
	}

	// 同じ移動の中で同じ相手を二度解決しない（滑らせる途中で同じ壁の角に触れた場合など）。
	if (HitActor)
	{
		if (ResolvedThisMove.Contains(HitActor))
		{
			LogWallHit(*this, TEXT("resolve"), TEXT("skipped (already resolved this move)"), Hit, CurrentSpeed);
			return;
		}
		ResolvedThisMove.Add(HitActor);
	}

	// 相手が機体なら追突として解決する。
	// 壁として扱うと相手機体が「破壊不能の壁」になり、側面から触れるだけで自滅してしまう。
	if (AImpactVehiclePawn* OtherVehicle = Cast<AImpactVehiclePawn>(HitActor))
	{
		ResolveVehicleClash(*OtherVehicle, *Tuning, *Rules);
		return;
	}

	// 壁・障害物への衝突。破壊 / 弾き / 自滅のいずれかへ振り分ける。
	ResolveWallImpact(Hit, *Tuning, *Rules);
}

void UImpactVehicleMovementComponent::Land()
{
	bGrounded = true;
	VerticalSpeed = 0.0f;
}

void UImpactVehicleMovementComponent::DetectBumperContacts(
	const UVehicleTuningDataAsset& Tuning, const UImpactTuningDataAsset& Rules)
{
	// 移動のスイープで解決済みなら、同じ衝突をここで二重に拾わない。
	if (!UpdatedComponent || IsInClashCooldown())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector SelfLocation = UpdatedComponent->GetComponentLocation();
	const FVector SelfForward = UpdatedComponent->GetForwardVector();

	// 双方の実効半径が最大まで伸びた場合でも取りこぼさない範囲を調べる。
	const float MaxContactRadius = FMath::Max(Tuning.BubbleRadius, Tuning.BumperRadiusMax);

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VehicleBumperContact), false, PawnOwner);
	World->OverlapMultiByObjectType(
		Overlaps, SelfLocation, FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(MaxContactRadius * 2.0f), Params);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AImpactVehiclePawn* OtherVehicle = Cast<AImpactVehiclePawn>(Overlap.GetActor());
		if (!OtherVehicle || OtherVehicle == PawnOwner)
		{
			continue;
		}

		const UImpactVehicleMovementComponent* OtherMovement = OtherVehicle->GetVehicleMovement();
		const UVehicleTuningDataAsset* OtherTuning = OtherVehicle->GetTuning();
		if (!OtherMovement || !OtherTuning || OtherMovement->IsInClashCooldown())
		{
			continue;
		}

		const FVector ToOther = OtherVehicle->GetActorLocation() - SelfLocation;
		const FVector Direction = ToOther.GetSafeNormal2D();
		if (Direction.IsNearlyZero())
		{
			continue;
		}

		// 正面に相手がいる場合のみ、実効半径が速度に応じて広がる。
		const float SelfRadius = FImpactResolver::GetContactRadius(SelfForward, Direction, CurrentSpeed, Tuning);
		const float OtherRadius = FImpactResolver::GetContactRadius(
			OtherVehicle->GetActorForwardVector(), -Direction, OtherMovement->GetCurrentSpeed(), *OtherTuning);
		if (ToOther.Size2D() > SelfRadius + OtherRadius)
		{
			continue;
		}

		// 離れつつある相手は追突にしない。すれ違った直後に背後から解決されるのを防ぐ。
		const float ClosingSpeed =
			FVector::DotProduct(GetHorizontalVelocity() - OtherMovement->GetHorizontalVelocity(), Direction);
		if (ClosingSpeed <= 0.0f)
		{
			continue;
		}

		// 3体以上が同時に接触した場合は、1フレームに1組だけ解決する（仕様 7章の暫定判断）。
		if (ResolveVehicleClash(*OtherVehicle, Tuning, Rules))
		{
			return;
		}
	}
}

FVehicleClashBody UImpactVehicleMovementComponent::MakeClashBody(const UVehicleTuningDataAsset& Tuning) const
{
	FVehicleClashBody Body;
	Body.Velocity = GetHorizontalVelocity();
	Body.Forward = UpdatedComponent ? UpdatedComponent->GetForwardVector() : FVector::ForwardVector;
	Body.MassCoefficient = Tuning.MassCoefficient;
	Body.BumperAngle = Tuning.BumperAngle;
	return Body;
}

bool UImpactVehicleMovementComponent::ResolveVehicleClash(
	AImpactVehiclePawn& OtherVehicle, const UVehicleTuningDataAsset& Tuning, const UImpactTuningDataAsset& Rules)
{
	UImpactVehicleMovementComponent* OtherMovement = OtherVehicle.GetVehicleMovement();
	const UVehicleTuningDataAsset* OtherTuning = OtherVehicle.GetTuning();
	if (!OtherMovement || !OtherTuning || !UpdatedComponent)
	{
		return false;
	}

	// 片方が解決済みなら、クールダウン中はもう片方からの検出を解決しない。
	if (IsInClashCooldown() || OtherMovement->IsInClashCooldown())
	{
		return false;
	}

	const FVector SelfToOther =
		(OtherVehicle.GetActorLocation() - UpdatedComponent->GetComponentLocation()).GetSafeNormal2D();

	const FVehicleClashResult Result = FImpactResolver::ResolveVehicleClash(
		MakeClashBody(Tuning), OtherMovement->MakeClashBody(*OtherTuning), SelfToOther, Rules);

	UE_LOG(LogImpactVehicle, Log, TEXT("clash %s vs %s: %s (diff %.0f, %s v%.0f x%.2f / %s v%.0f x%.2f)"),
		*GetNameSafe(PawnOwner), *OtherVehicle.GetName(), LexToDisplayString(Result.Branch), Result.EnergyDifference,
		LexToDisplayString(Result.Self.Facing), Result.Self.RawSpeed, Result.Self.FacingPower,
		LexToDisplayString(Result.Other.Facing), Result.Other.RawSpeed, Result.Other.FacingPower);

	// 記録とクールダウンを先に済ませる。撃破の反映で相手が消滅処理に入る可能性があるため、
	// 相手への操作は結果の反映を最後にする。
	RecordClash(Result);
	OtherMovement->RecordClash(Result.Mirrored());
	StartClashCooldown();
	OtherMovement->StartClashCooldown();

	ApplyClashSide(Result.Self);
	OtherMovement->ApplyClashSide(Result.Other);
	return true;
}

void UImpactVehicleMovementComponent::ApplyClashSide(const FVehicleClashSide& Side)
{
	const UImpactTuningDataAsset* Rules = GetImpactTuning();
	if (!Rules)
	{
		return;
	}

	if (Side.bDefeated)
	{
		// 撃破の扱いは機体の種類で異なる（自機は軽いダウン、敵機は消滅）ため Pawn に委ねる。
		CurrentSpeed = 0.0f;
		if (AImpactVehiclePawn* VehiclePawn = Cast<AImpactVehiclePawn>(PawnOwner))
		{
			VehiclePawn->HandleDefeatedInClash();
		}
		return;
	}

	SetHorizontalVelocity(Side.ResultingVelocity);

	if (Side.StunDuration > 0.0f)
	{
		// 拮抗。スタンの間は離脱速度のまま流され、入力を受け付けない。
		StartKnockback(0.0f);
		StunRemaining = Side.StunDuration;
		DriveState = EVehicleDriveState::Stunned;
		return;
	}

	if (Side.bKnockedBack)
	{
		// 弾かれた。軌道は操作できない。
		StartKnockback(Rules->KnockbackRecoveryTime);
	}
}

void UImpactVehicleMovementComponent::RecordClash(const FVehicleClashResult& Result)
{
	LastClash = Result;
	bHasResolvedClash = true;
}

void UImpactVehicleMovementComponent::StartClashCooldown()
{
	const UImpactTuningDataAsset* Rules = GetImpactTuning();
	const UWorld* World = GetWorld();
	if (Rules && World)
	{
		ClashCooldownEndTime = World->GetTimeSeconds() + Rules->ClashCooldown;
	}
}

bool UImpactVehicleMovementComponent::IsInClashCooldown() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() < ClashCooldownEndTime;
}

void UImpactVehicleMovementComponent::ForceCrash()
{
	if (const UVehicleTuningDataAsset* Tuning = GetTuning())
	{
		ApplyCrash(*Tuning);
	}
}

void UImpactVehicleMovementComponent::ResolveWallImpact(
	FHitResult& Hit, const UVehicleTuningDataAsset& Tuning, const UImpactTuningDataAsset& Rules)
{
	// 衝突は面の法線と逆向きに到来する。面の判定は機首方向で行う。
	// 弾かれて後ろ向きに流されている最中の激突は背面扱いになり、「弾き飛ばされた先の壁で自滅」が成立する。
	const FVector ImpactDirection = (-Hit.ImpactNormal).GetSafeNormal2D();
	const FVector Facing = UpdatedComponent->GetForwardVector();

	FImpactResolveResult Result;
	Result.Facing = FImpactResolver::DetermineFacing(Facing, ImpactDirection, Rules);
	Result.ImpactSpeed = CurrentSpeed;
	// 面へ向かう速度。かすっただけの接触と、叩きつけられた接触を区別する。
	Result.SurfaceSpeed = FMath::Max(FVector::DotProduct(GetHorizontalVelocity(), ImpactDirection), 0.0f);

	// 衝突対象が自身への影響（ダメージ・破壊）を決める。対象ごとの規則（ゴールはプレイヤーのみ削れる、
	// ランクごとの破壊閾値など）は各対象の IImpactReceiver 実装にあり、ここでは結果に応じた減速と通知だけを行う。
	// 影響を受けなかった対象と、インターフェースを持たない対象は破壊不能の壁として以降の処理に回す。
	// 受け手は Actor 自身とは限らず、Actor に付けた部品（拠点・破壊可能オブジェクト）であることが多い。
	if (UObject* Receiver = ImpactReceiver::FindReceiver(Hit.GetActor()))
	{
		const AImpactVehiclePawn* VehiclePawn = Cast<AImpactVehiclePawn>(PawnOwner);

		FImpactReceiveContext Context;
		Context.ImpactSpeed = Result.ImpactSpeed;
		Context.Facing = Result.Facing;
		Context.Instigator = PawnOwner;
		Context.Tuning = VehiclePawn ? VehiclePawn->GetTuning() : nullptr;
		Context.ImpactVelocity = GetHorizontalVelocity();
		Context.ImpactDirection = ImpactDirection;
		Context.ImpactPoint = Hit.ImpactPoint;

		const FImpactReceiveResult Received = IImpactReceiver::Execute_ReceiveVehicleImpact(Receiver, Context);
		const EImpactReceiveReaction Reaction = FImpactResolver::DecideReceiveReaction(Received.Outcome);

		if (ShouldLogWallHits())
		{
			UE_LOG(LogImpactVehicle, Log, TEXT("[HitLog] %s receiver %s: outcome %s (impact speed %.0f, surface speed %.0f, facing %s)"),
				*GetNameSafe(GetOwner()), *GetNameSafe(Receiver), LexToDisplayString(Received.Outcome),
				Result.ImpactSpeed, Result.SurfaceSpeed, LexToDisplayString(Result.Facing));
		}
		if (Reaction != EImpactReceiveReaction::TreatAsWall)
		{
			// 削っても壊しても無傷では通れない。叩きつけた勢いは大きく削がれる（企画書 3-3 の減速ペナルティ）。
			CurrentSpeed *= Tuning.DestroyPenaltyRatio;

			switch (Reaction)
			{
			case EImpactReceiveReaction::BounceOffWeakened:
				// 建物は残っているため弾かれる。得点にはならない。
				ApplyWallBounce(Hit, Rules);
				Result.bBounced = true;
				OnGameplayEvent.Broadcast(EVehicleGameplayEvent::StructureDamaged, Received.Damage);
				break;

			case EImpactReceiveReaction::ScoreGoalHit:
				Result.GoalDamage = Received.Damage;
				OnGameplayEvent.Broadcast(EVehicleGameplayEvent::GoalHit, Received.Damage);
				break;

			case EImpactReceiveReaction::PassThroughDestroyed:
			default:
				Result.bDestroyedTarget = true;
				OnGameplayEvent.Broadcast(
					Received.Rank == EDestructionRank::Large ? EVehicleGameplayEvent::DestroyedLarge : EVehicleGameplayEvent::DestroyedSmall, 1);
				break;
			}

			Result.ResultingSpeed = CurrentSpeed;
			LastImpact = Result;
			bHasResolvedImpact = true;
			return;
		}
	}

	if (ShouldLogWallHits())
	{
		UE_LOG(LogImpactVehicle, Log, TEXT("[HitLog] %s wall %s: impact speed %.0f, surface speed %.0f (crash min %.0f), facing %s -> %s"),
			*GetNameSafe(GetOwner()), *GetNameSafe(Hit.GetActor()), Result.ImpactSpeed, Result.SurfaceSpeed, Tuning.CrashMinSpeed,
			LexToDisplayString(Result.Facing),
			Result.SurfaceSpeed < Tuning.CrashMinSpeed ? TEXT("slide")
				: (Result.Facing == EImpactFacing::Front ? TEXT("bounce") : TEXT("crash")));
	}

	// 破壊できなかった場合（仕様 2-7）。叩きつけた勢いが小さければ滑走のみで済ませる。
	if (Result.SurfaceSpeed >= Tuning.CrashMinSpeed)
	{
		if (Result.Facing == EImpactFacing::Front)
		{
			// 正面はバンパーで守られている面。弾かれて減速するだけで済む。
			ApplyWallBounce(Hit, Rules);
			Result.bBounced = true;
		}
		else
		{
			// 側面・背面から叩きつけられた。弾き飛ばされた先の壁で自滅する（企画書 3-3）。
			ApplyCrash(Tuning);
			Result.bCrashed = true;
			OnGameplayEvent.Broadcast(EVehicleGameplayEvent::Crash, 1);
		}
	}

	Result.ResultingSpeed = CurrentSpeed;
	LastImpact = Result;
	bHasResolvedImpact = true;
}

void UImpactVehicleMovementComponent::ApplyWallBounce(const FHitResult& Hit, const UImpactTuningDataAsset& Rules)
{
	// 面に垂直な成分だけを反発係数で反転させる。斜めに当たれば斜めに逸れ、真正面なら真後ろへ弾かれる。
	SetHorizontalVelocity(FImpactResolver::ComputeWallBounce(GetHorizontalVelocity(), Hit.ImpactNormal, Rules.Restitution));
	StartKnockback(Rules.KnockbackRecoveryTime);
}

void UImpactVehicleMovementComponent::ApplyCrash(const UVehicleTuningDataAsset& Tuning)
{
	// 企画書 4章の未決定事項に対し、プロトタイプでは即撃破ではなく軽いダウンを採用している。
	// 試行回数を稼ぐことを優先するため。
	StartKnockback(0.0f);
	CurrentSpeed = 0.0f;
	StunRemaining = Tuning.CrashStunDuration;
	DriveState = EVehicleDriveState::Stunned;
}

void UImpactVehicleMovementComponent::SetHorizontalVelocity(const FVector& NewVelocity)
{
	const FVector Horizontal(NewVelocity.X, NewVelocity.Y, 0.0f);
	CurrentSpeed = Horizontal.Size();

	// 止まった場合は方向を残し、発進時の慣性ブレンド（ApplyInertiaBlend）に任せる。
	if (CurrentSpeed >= MinMeaningfulSpeed)
	{
		MoveDirection = Horizontal / CurrentSpeed;
	}
}

void UImpactVehicleMovementComponent::StartKnockback(float Duration)
{
	// 弾かれた時点でブレーキターンは途切れる。保存していた勢いを持ち越すと、弾かれた直後に再加速できてしまう。
	StoredSpeed = 0.0f;
	BrakeTurnElapsed = 0.0f;
	KnockbackRemaining = Duration;

	if (Duration > 0.0f)
	{
		DriveState = EVehicleDriveState::KnockedBack;
	}
}

bool UImpactVehicleMovementComponent::TryStompTarget(AActor* Target)
{
	if (!IsValid(Target) || !UpdatedComponent || !Target->Implements<UStompable>())
	{
		return false;
	}

	const UVehicleTuningDataAsset* Tuning = GetTuning();
	if (!Tuning)
	{
		return false;
	}

	// 機首方向から見て前方から接触した場合のみ成立とする。
	// 側面・背面からの接触は踏みつけにならず、Phase 6 の通常衝突として扱う。
	const FVector Facing = UpdatedComponent->GetForwardVector().GetSafeNormal2D();
	const FVector ToTarget = (Target->GetActorLocation() - UpdatedComponent->GetComponentLocation()).GetSafeNormal2D();
	if (Facing.IsNearlyZero() || ToTarget.IsNearlyZero())
	{
		return false;
	}

	if (FVector::DotProduct(Facing, ToTarget) < Tuning->StompFrontDotThreshold)
	{
		return false;
	}

	if (!IStompable::Execute_ReceiveStomp(Target))
	{
		return false;
	}

	ApplyStompGain(*Tuning);

	++StompCount;
	return true;
}

void UImpactVehicleMovementComponent::ApplyStompGain(const UVehicleTuningDataAsset& Tuning)
{
	// 上限で頭打ちになる分を除いた、実際に得られた加速量を記録する。
	const float SpeedBefore = CurrentSpeed;
	CurrentSpeed = FMath::Min(CurrentSpeed + Tuning.StompSpeedGain, Tuning.MaxSpeed);
	LastStompGain = CurrentSpeed - SpeedBefore;

	// 跳ねを設定している場合のみ空中へ上がる。既定では接地したまま踏み潰して走る。
	if (Tuning.StompBounceImpulse > 0.0f)
	{
		VerticalSpeed = Tuning.StompBounceImpulse;
		bGrounded = false;
	}
}

void UImpactVehicleMovementComponent::DebugApplyStompBoost()
{
	// 行動不能中は実際の踏みつけも成立しないため、デバッグ操作でも加速させない。
	if (StunRemaining > 0.0f)
	{
		return;
	}

	if (const UVehicleTuningDataAsset* Tuning = GetTuning())
	{
		ApplyStompGain(*Tuning);
	}
}

void UImpactVehicleMovementComponent::ToggleDebugHoldBoost()
{
	bDebugHoldBoost = !bDebugHoldBoost;
}
