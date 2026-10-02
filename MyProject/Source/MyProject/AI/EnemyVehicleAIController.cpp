// 踏みつけ加速メカゲーム — 敵機の AI コントローラ

#include "AI/EnemyVehicleAIController.h"

#include "AI/EnemyVehiclePawn.h"
#include "Collision/ImpactResolver.h"
#include "Core/ImpactGameMode.h"
#include "Core/ImpactGameState.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Field/DestructibleComponent.h"
#include "Field/FieldSubsystem.h"
#include "Field/GoalComponent.h"
#include "Field/TeamComponent.h"
#include "GameFramework/PlayerController.h"
#include "Stomp/StompTargetActor.h"
#include "Tuning/AITuningDataAsset.h"
#include "Tuning/VehicleTuningDataAsset.h"
#include "Vehicle/ImpactVehicleMovementComponent.h"

DEFINE_LOG_CATEGORY(LogImpactAI);

namespace
{
	/** 障害物探索で地面とみなすヒット法線の上向き成分。移動コンポーネントの接地判定と揃える。 */
	constexpr float GroundNormalThreshold = 0.7f;

	/** 踏み台を探す範囲を制限しないときに渡す距離（uu）。 */
	constexpr float UnlimitedLeashRadius = TNumericLimits<float>::Max();
}

AEnemyVehicleAIController::AEnemyVehicleAIController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AEnemyVehicleAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	AIState = EEnemyAIState::Accelerating;
	TargetActor.Reset();
	ThinkCooldown = 0.0f;
	AvoidRemaining = 0.0f;

	const AEnemyVehiclePawn* Enemy = Cast<AEnemyVehiclePawn>(InPawn);
	if (!Enemy || !Enemy->GetAITuning())
	{
		UE_LOG(LogImpactAI, Error, TEXT("%s possessed %s without AI tuning; the enemy will not move"),
			*GetName(), *GetNameSafe(InPawn));
		return;
	}

	DefendedGoal = FindGoal();
	UE_LOG(LogImpactAI, Log, TEXT("%s possessed %s (%s)"), *GetName(), *Enemy->GetName(),
		DefendedGoal.IsValid() ? *FString::Printf(TEXT("defending %s"), *GetNameSafe(DefendedGoal->GetOwner())) : TEXT("no goal: offense"));
}

void AEnemyVehicleAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const AEnemyVehiclePawn* Enemy = Cast<AEnemyVehiclePawn>(GetPawn());
	if (!Enemy || Enemy->IsDefeated())
	{
		return;
	}

	const UAITuningDataAsset* AITuning = Enemy->GetAITuning();
	if (!AITuning)
	{
		return;
	}

	// カウントダウン中と試合終了後は、自機と同じく動かない。
	const AImpactGameState* MatchState = GetWorld()->GetGameState<AImpactGameState>();
	if (MatchState && !MatchState->IsControlAllowed())
	{
		UImpactVehicleMovementComponent* Movement = Enemy->GetVehicleMovement();
		Movement->SetMoveInput(FVector2D::ZeroVector);
		Movement->SetBrakeInput(false);
		return;
	}

	AvoidRemaining = FMath::Max(AvoidRemaining - DeltaSeconds, 0.0f);

	// 判断は反応遅延の間隔でのみ行う。毎フレーム最適解を選ぶと人間には対処できない。
	ThinkCooldown -= DeltaSeconds;
	if (ThinkCooldown <= 0.0f)
	{
		ThinkCooldown = AITuning->ReactionDelay;
		Think(*AITuning);
	}

	Drive(*AITuning);
}

void AEnemyVehicleAIController::Think(const UAITuningDataAsset& AITuning)
{
	// 回避は完遂させる。途中で目標へ振り向くと再び障害物へ突っ込むため。
	if (AvoidRemaining > 0.0f)
	{
		return;
	}

	FVector NewAvoidDirection;
	if (FindBlockingObstacle(AITuning, NewAvoidDirection))
	{
		AIState = EEnemyAIState::Avoiding;
		AvoidDirection = NewAvoidDirection;
		AvoidRemaining = AITuning.AvoidDuration;
		TargetActor.Reset();
		return;
	}

	const AEnemyVehiclePawn* Enemy = Cast<AEnemyVehiclePawn>(GetPawn());
	const float Speed = Enemy->GetCurrentSpeed();

	// 攻撃の開始と終了で閾値をずらし、境界付近で状態が振動しないようにする。
	const bool bWasAttacking = AIState == EEnemyAIState::Attacking;
	const float Threshold = bWasAttacking
		? AITuning.AttackSpeedThreshold - AITuning.AttackHysteresis
		: AITuning.AttackSpeedThreshold;
	const bool bFastEnough = Speed >= Threshold;

	// 守る拠点が未登録だった（拠点より先に操作を始めた）場合に備え、見つかるまで探し直す。
	if (!DefendedGoal.IsValid())
	{
		DefendedGoal = FindGoal();
	}

	// ゴールが破壊されたら守るものがないので攻めに転じる。
	const UGoalComponent* Goal = DefendedGoal.Get();
	if (Goal && !Goal->IsGoalDestroyed())
	{
		ThinkDefense(AITuning, *Goal, bFastEnough);
	}
	else
	{
		ThinkOffense(bFastEnough);
	}
}

void AEnemyVehicleAIController::ThinkOffense(bool bFastEnough)
{
	if (bFastEnough)
	{
		AIState = EEnemyAIState::Attacking;
		TargetActor = FindPlayerVehicle();
		return;
	}

	const APawn* ControlledPawn = GetPawn();
	AIState = EEnemyAIState::Accelerating;
	TargetActor = FindNearestStompTarget(ControlledPawn->GetActorLocation(), UnlimitedLeashRadius);
}

void AEnemyVehicleAIController::ThinkDefense(const UAITuningDataAsset& AITuning, const UGoalComponent& Goal, bool bFastEnough)
{
	const FTransform Guard = Goal.GetGuardTransform();
	GuardPoint = Guard.GetLocation();
	GuardFacing = Guard.GetRotation().Vector().GetSafeNormal2D();
	const FVector GoalLocation = Goal.GetOwner()->GetActorLocation();

	AActor* Player = FindPlayerVehicle();
	const bool bPlayerThreat = Player
		&& FVector::Dist2D(Player->GetActorLocation(), GoalLocation) <= AITuning.InterceptRadius;

	// ゴールへ近づくプレイヤーを、速度が十分なら迎え撃つ。
	if (bPlayerThreat && bFastEnough)
	{
		AIState = EEnemyAIState::Attacking;
		TargetActor = Player;
		return;
	}

	// 速度が足りなければ、守備位置の周囲の踏み台で加速する。遠くの踏み台は追わずにゴールを空けない。
	if (!bFastEnough)
	{
		if (AActor* StompTarget = FindNearestStompTarget(GuardPoint, AITuning.LeashRadius))
		{
			AIState = EEnemyAIState::Accelerating;
			TargetActor = StompTarget;
			return;
		}
	}

	// 加速の手段がなくても、脅威が迫っていれば進路を塞ぎに行く。
	if (bPlayerThreat)
	{
		AIState = EEnemyAIState::Attacking;
		TargetActor = Player;
		return;
	}

	AIState = EEnemyAIState::Guarding;
	TargetActor.Reset();
}

void AEnemyVehicleAIController::Drive(const UAITuningDataAsset& AITuning)
{
	AEnemyVehiclePawn* Enemy = Cast<AEnemyVehiclePawn>(GetPawn());
	UImpactVehicleMovementComponent* Movement = Enemy->GetVehicleMovement();

	FVector DesiredDirection = Enemy->GetActorForwardVector();
	float Throttle = 1.0f;
	bool bBrake = false;

	if (AIState == EEnemyAIState::Avoiding)
	{
		DesiredDirection = AvoidDirection;

		// 回避はブレーキターンで行う。勢いを保存したまま向きを変えられる。
		bBrake = AvoidRemaining > 0.0f;
	}
	else if (AIState == EEnemyAIState::Guarding)
	{
		const FVector ToGuardPoint = GuardPoint - Enemy->GetActorLocation();
		if (ToGuardPoint.Size2D() <= AITuning.GuardArrivalRadius)
		{
			// 守備位置に着いたら惰性で止まりつつ、迎え撃つ方向（ゴールの正面）を向く。
			DesiredDirection = GuardFacing;
			Throttle = 0.0f;
		}
		else
		{
			DesiredDirection = ToGuardPoint;
		}
	}
	else if (TargetActor.IsValid())
	{
		DesiredDirection = TargetActor->GetActorLocation() - Enemy->GetActorLocation();
	}

	const FVector Forward = Enemy->GetActorForwardVector().GetSafeNormal2D();
	const FVector Desired = DesiredDirection.GetSafeNormal2D();

	// 目標との左右の角度差（deg）。正なら右。移動コンポーネントは正の旋回入力で右へ曲がる。
	float AngleDegrees = 0.0f;
	if (!Forward.IsNearlyZero() && !Desired.IsNearlyZero())
	{
		const float Dot = FMath::Clamp(FVector::DotProduct(Forward, Desired), -1.0f, 1.0f);
		const float Sign = FMath::Sign(FVector::CrossProduct(Forward, Desired).Z);
		AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(Dot)) * (Sign != 0.0f ? Sign : 1.0f);
	}

	const float Turn = FMath::Clamp(AngleDegrees / AITuning.SteerFullLockAngle, -1.0f, 1.0f);

	// 自機と同じ経路で入力を与える。挙動は移動コンポーネントがすべて決める。
	Movement->SetMoveInput(FVector2D(Turn, Throttle));
	Movement->SetBrakeInput(bBrake);
}

AActor* AEnemyVehicleAIController::FindNearestStompTarget(const FVector& LeashCenter, float LeashRadius) const
{
	const APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return nullptr;
	}

	const FVector Origin = ControlledPawn->GetActorLocation();
	const float LeashRadiusSquared = FMath::Square(LeashRadius);
	AActor* Nearest = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();

	for (TActorIterator<AStompTargetActor> It(GetWorld()); It; ++It)
	{
		AStompTargetActor* Candidate = *It;
		if (!IsValid(Candidate))
		{
			continue;
		}

		const FVector CandidateLocation = Candidate->GetActorLocation();
		if (FVector::DistSquared2D(LeashCenter, CandidateLocation) > LeashRadiusSquared)
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared2D(Origin, CandidateLocation);
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			Nearest = Candidate;
		}
	}

	return Nearest;
}

AActor* AEnemyVehicleAIController::FindPlayerVehicle() const
{
	// 攻撃対象の決定は GameMode に集約している。AI コントローラは権威側にしか存在しないため GameMode を参照してよい。
	const UWorld* World = GetWorld();
	const AImpactGameMode* GameMode = World ? World->GetAuthGameMode<AImpactGameMode>() : nullptr;
	return GameMode ? GameMode->GetPlayerVehicle() : nullptr;
}

UGoalComponent* AEnemyVehicleAIController::FindGoal() const
{
	// 守るのは自分と同じ陣営の拠点。陣営を持たない（中立の）機体には守る拠点が無い。
	const int32 TeamId = UTeamComponent::GetTeamIdOf(GetPawn());
	if (TeamId == ImpactTeam::Neutral)
	{
		return nullptr;
	}

	const UFieldSubsystem* Field = UFieldSubsystem::Get(this);
	return Field ? Field->FindGoal(TeamId) : nullptr;
}

bool AEnemyVehicleAIController::FindBlockingObstacle(const UAITuningDataAsset& AITuning, FVector& OutAvoidDirection) const
{
	const AEnemyVehiclePawn* Enemy = Cast<AEnemyVehiclePawn>(GetPawn());
	const UVehicleTuningDataAsset* Tuning = Enemy ? Enemy->GetTuning() : nullptr;
	if (!Tuning)
	{
		return false;
	}

	// 機首ではなく実際の進行方向を探る。高速時は慣性で機首と進行方向がずれるため。
	const FVector TravelDirection = Enemy->GetVehicleMovement()->GetMoveDirection().GetSafeNormal2D();
	if (TravelDirection.IsNearlyZero())
	{
		return false;
	}

	const FVector Start = Enemy->GetActorLocation();
	const FVector End = Start + TravelDirection * AITuning.ObstacleProbeDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemyObstacleProbe), false, Enemy);

	// 壁・障害物・ゴールは WorldStatic。機体（Pawn）と踏み台は回避対象にしない。
	// ゴールもここにかかるため、守備側の NPC は自陣ゴールへ突っ込まずに避ける。
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return false;
	}

	if (Hit.ImpactNormal.Z >= GroundNormalThreshold)
	{
		return false;
	}

	// 今の速度で壊せる障害物なら避けずに突っ込む。速度を積む意味は NPC にとっても同じ。
	const AActor* HitActor = Hit.GetActor();
	if (const UDestructibleComponent* Obstacle = HitActor ? HitActor->FindComponentByClass<UDestructibleComponent>() : nullptr)
	{
		if (Enemy->GetCurrentSpeed() >= FImpactResolver::GetRequiredSpeed(Obstacle->GetRank(), *Tuning))
		{
			return false;
		}
	}

	// 面に沿う2方向のうち、今の進行方向に近い側へ逃げる。大きく向きを変えずに済む。
	const FVector Normal = Hit.ImpactNormal.GetSafeNormal2D();
	const FVector AlongA = FVector::CrossProduct(FVector::UpVector, Normal).GetSafeNormal2D();
	const FVector AlongB = -AlongA;
	OutAvoidDirection = FVector::DotProduct(AlongA, TravelDirection) >= FVector::DotProduct(AlongB, TravelDirection)
		? AlongA
		: AlongB;

	return !OutAvoidDirection.IsNearlyZero();
}
