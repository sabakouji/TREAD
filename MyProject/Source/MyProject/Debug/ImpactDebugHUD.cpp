// 踏みつけ加速メカゲーム — デバッグ HUD

#include "Debug/ImpactDebugHUD.h"

#include "Collision/ImpactResolver.h"
#include "Collision/ImpactTypes.h"
#include "Core/ImpactGameState.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Tuning/VehicleTuningDataAsset.h"
#include "Vehicle/ImpactVehicleMovementComponent.h"
#include "Vehicle/ImpactVehiclePawn.h"
#include "Vehicle/ImpactVehicleTypes.h"

AImpactDebugHUD::AImpactDebugHUD()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AImpactDebugHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	// 試合終了後はリザルト画面に任せ、ゲーム画面への上書き表示はしない。
	const AImpactGameState* MatchState = GetWorld() ? GetWorld()->GetGameState<AImpactGameState>() : nullptr;
	if (MatchState && MatchState->GetMatchState() == EImpactMatchState::Finished)
	{
		return;
	}

	// 加点・減点のポップアップはプレイ中のフィードバックなので、デバッグ表示の切り替えに関係なく出す。
	DrawScoreFeedback();

	if (!bShowVehicleDebug)
	{
		return;
	}

	float PosY = static_cast<float>(DebugDrawOrigin.Y);
	PosY = DrawDebugLine(TEXT("=== Impact Vehicle Debug ==="), PosY, HeaderColor);

	AImpactVehiclePawn* Vehicle = GetViewedVehicle();
	if (!Vehicle)
	{
		DrawDebugLine(TEXT("Vehicle: not possessed"), PosY, WarnColor);
		return;
	}

	const float Speed = Vehicle->GetCurrentSpeed();
	PosY = DrawDebugLine(FString::Printf(TEXT("Speed          : %8.1f uu/s"), Speed), PosY, ValueColor);

	const UVehicleTuningDataAsset* Tuning = Vehicle->GetTuning();
	if (!Tuning)
	{
		DrawDebugLine(TEXT("Tuning         : not assigned (DA_VehicleTuning)"), PosY, WarnColor);
		return;
	}

	const float SpeedRatio = Tuning->GetSpeedRatio(Speed);

	PosY = DrawDebugLine(
		FString::Printf(TEXT("Speed Ratio    : %8.3f  (Base %.0f / Max %.0f)"),
			SpeedRatio, Tuning->BaseSpeed, Tuning->MaxSpeed),
		PosY, ValueColor);

	const UImpactVehicleMovementComponent* Movement = Vehicle->GetVehicleMovement();
	const EVehicleDriveState DriveState = Movement->GetDriveState();
	const bool bBrakeTurning = DriveState == EVehicleDriveState::BrakeTurning;

	PosY = DrawDebugLine(
		FString::Printf(TEXT("Drive State    : %s"), LexToDisplayString(DriveState)),
		PosY, bBrakeTurning ? WarnColor : ValueColor);

	PosY = DrawDebugLine(
		FString::Printf(TEXT("Stored Speed   : %8.1f uu/s"), Movement->GetStoredSpeed()),
		PosY, ValueColor);

	const float TurnRadius = Movement->GetTurnRadius();
	PosY = DrawDebugLine(
		TurnRadius > 0.0f
			? FString::Printf(TEXT("Turn Radius    : %8.1f uu"), TurnRadius)
			: FString(TEXT("Turn Radius    :        - (not turning)")),
		PosY, ValueColor);

	PosY = DrawDebugLine(
		FString::Printf(TEXT("Turn Rate      : %8.1f deg/s"), Movement->GetTurnRate()),
		PosY, ValueColor);

	PosY = DrawDebugLine(
		FString::Printf(TEXT("Inertia Weight : %8.3f"), Movement->GetInertiaWeight()),
		PosY, ValueColor);

	// 機首方向と実進行方向のずれ。旋回中に側面を晒している量そのものを表す。
	PosY = DrawDebugLine(
		FString::Printf(TEXT("Drift Angle    : %8.1f deg"), Movement->GetDriftAngleDegrees()),
		PosY, ValueColor);

	const FVector Facing = Vehicle->GetActorForwardVector().GetSafeNormal2D();
	const FVector MoveDir = Movement->GetMoveDirection();

	PosY = DrawDebugLine(
		FString::Printf(TEXT("Facing / Move  : (%.2f, %.2f) / (%.2f, %.2f)"),
			Facing.X, Facing.Y, MoveDir.X, MoveDir.Y),
		PosY, ValueColor);

	PosY = DrawDebugLine(
		FString::Printf(TEXT("Grounded       : %s"), Movement->IsGrounded() ? TEXT("yes") : TEXT("no")),
		PosY, Movement->IsGrounded() ? ValueColor : WarnColor);

	PosY = DrawDebugLine(
		FString::Printf(TEXT("Stomps         : %5d   (last gain %.1f uu/s)"),
			Movement->GetStompCount(), Movement->GetLastStompGain()),
		PosY, ValueColor);

	// デバッグ操作の状態。維持 ON のままだと速度が落ちず評価を誤りやすいため、ON は警告色にする。
	const bool bHoldBoost = Movement->IsDebugHoldBoost();
	PosY = DrawDebugLine(
		FString::Printf(TEXT("Speed Hold     : %s   [Q] boost  [R] toggle hold"), bHoldBoost ? TEXT("ON") : TEXT("off")),
		PosY, bHoldBoost ? WarnColor : ValueColor);

	// 現在の速度で何を壊せるか。速度を積む意味を数値で示す。
	EDestructionRank DestroyableRank = EDestructionRank::Small;
	const bool bCanDestroy = FImpactResolver::GetDestroyableRank(Speed, *Tuning, DestroyableRank);

	PosY = DrawDebugLine(
		bCanDestroy
			? FString::Printf(TEXT("Can Destroy    : %s  (Small %.0f / Large %.0f)"),
				LexToDisplayString(DestroyableRank), Tuning->SmallDestructionSpeed, Tuning->LargeDestructionSpeed)
			: FString::Printf(TEXT("Can Destroy    : none  (Small %.0f / Large %.0f)"),
				Tuning->SmallDestructionSpeed, Tuning->LargeDestructionSpeed),
		PosY, bCanDestroy ? ValueColor : WarnColor);

	const float StunRemaining = Movement->GetStunRemaining();
	if (StunRemaining > 0.0f)
	{
		PosY = DrawDebugLine(
			FString::Printf(TEXT("STUNNED        : %5.2f s remaining"), StunRemaining),
			PosY, WarnColor);
	}

	const float KnockbackRemaining = Movement->GetKnockbackRemaining();
	if (KnockbackRemaining > 0.0f)
	{
		PosY = DrawDebugLine(
			FString::Printf(TEXT("KNOCKED BACK   : %5.2f s remaining"), KnockbackRemaining),
			PosY, WarnColor);
	}

	// ゴールの耐久値と得点。実装は ImpactDebugHUDScore.cpp。
	PosY = DrawScoreDebug(PosY);

	// 機体同士の戦闘（敵機の状態・直近の衝突の相殺内訳）。実装は ImpactDebugHUDCombat.cpp。
	PosY = DrawCombatDebug(*Vehicle, PosY);

	// 直近の衝突の内訳。数値設計の破綻はここを見ないと切り分けられない。
	if (!Movement->HasResolvedImpact())
	{
		DrawDebugLine(TEXT("Last Impact    : (none yet)"), PosY, ValueColor);
		return;
	}

	const FImpactResolveResult& Impact = Movement->GetLastImpact();

	FString Outcome = TEXT("blocked");
	if (Impact.GoalDamage > 0)
	{
		Outcome = FString::Printf(TEXT("GOAL HIT -%d"), Impact.GoalDamage);
	}
	else if (Impact.bDestroyedTarget)
	{
		Outcome = TEXT("DESTROYED");
	}
	else if (Impact.bCrashed)
	{
		Outcome = TEXT("CRASHED");
	}
	else if (Impact.bBounced)
	{
		Outcome = TEXT("bounced");
	}

	PosY = DrawDebugLine(
		FString::Printf(TEXT("Last Impact    : %s -> %s"),
			LexToDisplayString(Impact.Facing), *Outcome),
		PosY, Impact.bCrashed ? WarnColor : ValueColor);

	// 破壊判定は生速度、弾き・自滅の判定は面へ向かう速度で行う。
	DrawDebugLine(
		FString::Printf(TEXT("  speed %.0f (into surface %.0f / crash >= %.0f) -> result %.0f"),
			Impact.ImpactSpeed, Impact.SurfaceSpeed, Tuning->CrashMinSpeed, Impact.ResultingSpeed),
		PosY, ValueColor);
}

float AImpactDebugHUD::DrawDebugLine(const FString& Text, float PosY, const FLinearColor& Color)
{
	DrawText(Text, Color, static_cast<float>(DebugDrawOrigin.X), PosY, GEngine ? GEngine->GetMediumFont() : nullptr);
	return PosY + DebugLineHeight;
}

AImpactVehiclePawn* AImpactDebugHUD::GetViewedVehicle() const
{
	if (!PlayerOwner)
	{
		return nullptr;
	}

	return Cast<AImpactVehiclePawn>(PlayerOwner->GetPawn());
}
