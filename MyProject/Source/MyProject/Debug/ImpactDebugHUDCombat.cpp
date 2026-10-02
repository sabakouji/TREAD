// 踏みつけ加速メカゲーム — デバッグ HUD（機体同士の戦闘）
//
// AImpactDebugHUD のうち、敵機と機体同士の衝突に関する表示をこのファイルに分けて実装する。
// 敵機・AI への依存をここに閉じ込め、本体（ImpactDebugHUD.cpp）の描画処理を小さく保つ。

#include "Debug/ImpactDebugHUD.h"

#include "AI/EnemyVehicleAIController.h"
#include "AI/EnemyVehiclePawn.h"
#include "Collision/ImpactResolver.h"
#include "Collision/ImpactTypes.h"
#include "Core/ImpactGameMode.h"
#include "Engine/World.h"
#include "Tuning/ImpactTuningDataAsset.h"
#include "Tuning/VehicleTuningDataAsset.h"
#include "Vehicle/ImpactLockOnComponent.h"
#include "Vehicle/ImpactVehicleMovementComponent.h"
#include "Vehicle/ImpactVehiclePawn.h"

// 配色は AImpactDebugHUD のメンバ（Debug|Style）にあり、Blueprint 派生から調整できる。
namespace
{
	/**
	 * 衝突の片側を1行に要約する（仕様 5章のデバッグ表示）。
	 * 生速度 × 面補正 = 補正後エネルギー、バンパーの有無、立場 → 結果、の順に並べる。
	 */
	FString DescribeClashSide(const TCHAR* Label, const FVehicleClashSide& Side)
	{
		FString Outcome;
		if (Side.bDefeated)
		{
			Outcome = TEXT("DEFEATED");
		}
		else if (Side.StunDuration > 0.0f)
		{
			Outcome = FString::Printf(TEXT("stun %.2fs  v%.0f"), Side.StunDuration, Side.GetResultingSpeed());
		}
		else
		{
			Outcome = FString::Printf(TEXT("%sv%.0f"), Side.bKnockedBack ? TEXT("knocked  ") : TEXT(""), Side.GetResultingSpeed());
		}

		return FString::Printf(TEXT("  %s: %-5s v%.0f x%.2f = E%.0f  bumper %s  %s -> %s"),
			Label, LexToDisplayString(Side.Facing), Side.RawSpeed, Side.FacingPower, Side.Energy,
			Side.bBumperEngaged ? TEXT("Y") : TEXT("-"), LexToDisplayString(Side.Role), *Outcome);
	}
}

float AImpactDebugHUD::DrawCombatDebug(const AImpactVehiclePawn& Vehicle, float PosY)
{
	PosY = DrawDebugLine(TEXT("--- Combat ---"), PosY, CombatHeaderColor);

	PosY = DrawOverdriveDebug(Vehicle, PosY);
	PosY = DrawLockOnDebug(Vehicle, PosY);

	const UWorld* World = GetWorld();
	const AImpactGameMode* GameMode = World ? World->GetAuthGameMode<AImpactGameMode>() : nullptr;
	const AEnemyVehiclePawn* Enemy = GameMode ? GameMode->GetCurrentEnemy() : nullptr;

	if (GameMode)
	{
		PosY = DrawDebugLine(
			FString::Printf(TEXT("Enemies Beaten : %d"), GameMode->GetEnemiesDefeated()),
			PosY, ValueColor);
	}

	// 敵機の状態。行動状態が見えないと、AI が想定どおり動いているかを判断できない。
	if (Enemy)
	{
		const AEnemyVehicleAIController* AIController = Cast<AEnemyVehicleAIController>(Enemy->GetController());
		const float Distance = FVector::Dist2D(Vehicle.GetActorLocation(), Enemy->GetActorLocation());

		PosY = DrawDebugLine(
			FString::Printf(TEXT("Enemy          : %s  %.0f uu/s  dist %.0f"),
				AIController ? LexToDisplayString(AIController->GetAIState()) : TEXT("(no AI)"),
				Enemy->GetCurrentSpeed(), Distance),
			PosY, AIController ? ValueColor : WarnColor);
	}
	else
	{
		PosY = DrawDebugLine(TEXT("Enemy          : (none / respawning)"), PosY, WarnColor);
	}

	// 直近の機体同士の衝突の内訳。数値設計が「常に拮抗」「常に一方的」に偏っていないかは、
	// 双方のエネルギーと差分、確定した分岐が見えないと追えない。
	const UImpactVehicleMovementComponent* Movement = Vehicle.GetVehicleMovement();
	if (!Movement || !Movement->HasResolvedClash())
	{
		return DrawDebugLine(TEXT("Last Clash     : (none yet)"), PosY, ValueColor);
	}

	const UImpactTuningDataAsset* Rules = Vehicle.GetImpactTuning();
	const FVehicleClashResult& Clash = Movement->GetLastClash();
	PosY = DrawDebugLine(
		FString::Printf(TEXT("Last Clash     : %s  diff %.0f  (clash <= %.0f, overdrive >= %.0f)"),
			LexToDisplayString(Clash.Branch), Clash.EnergyDifference,
			Rules ? Rules->ClashThreshold : 0.0f, Rules ? Rules->OverdriveSpeed : 0.0f),
		PosY, Clash.Self.bDefeated ? WarnColor : ValueColor);
	PosY = DrawDebugLine(DescribeClashSide(TEXT("Self "), Clash.Self), PosY, ValueColor);
	return DrawDebugLine(DescribeClashSide(TEXT("Other"), Clash.Other), PosY, ValueColor);
}

float AImpactDebugHUD::DrawOverdriveDebug(const AImpactVehiclePawn& Vehicle, float PosY)
{
	const UImpactTuningDataAsset* Rules = Vehicle.GetImpactTuning();
	const UVehicleTuningDataAsset* Tuning = Vehicle.GetTuning();
	const UImpactVehicleMovementComponent* Movement = Vehicle.GetVehicleMovement();
	if (!Rules || !Tuning || !Movement)
	{
		return DrawDebugLine(TEXT("Overdrive      : (no tuning)"), PosY, WarnColor);
	}

	// 超加速かどうかは破壊が起きるかどうかそのもの。相手の状態と併せて読めるよう先頭に置く。
	const float Speed = Movement->GetCurrentSpeed();
	const bool bOverdrive = Rules->IsOverdrive(Speed);
	PosY = DrawDebugLine(
		FString::Printf(TEXT("Overdrive      : %s  (%.0f / %.0f)"),
			bOverdrive ? TEXT("ON") : TEXT("off"), Speed, Rules->OverdriveSpeed),
		PosY, bOverdrive ? CombatHeaderColor : ValueColor);

	// 正面の実効半径は速度で変わる。当たりやすさの変化を数値で追えるようにする。
	const FVector Forward = Vehicle.GetActorForwardVector();
	const float FrontRadius = FImpactResolver::GetContactRadius(Forward, Forward, Speed, *Tuning);
	return DrawDebugLine(
		FString::Printf(TEXT("Contact Radius : front %.0f (max %.0f) / side %.0f"),
			FrontRadius, Tuning->BumperRadiusMax, Tuning->BubbleRadius),
		PosY, ValueColor);
}

float AImpactDebugHUD::DrawLockOnDebug(const AImpactVehiclePawn& Vehicle, float PosY)
{
	const UImpactLockOnComponent* LockOn = Vehicle.GetLockOn();
	const UImpactTuningDataAsset* Rules = Vehicle.GetImpactTuning();
	if (!LockOn || !Rules)
	{
		return DrawDebugLine(TEXT("Lock On        : (unavailable)"), PosY, WarnColor);
	}

	const AImpactVehiclePawn* Target = LockOn->GetTarget();
	if (Target)
	{
		PosY = DrawDebugLine(
			FString::Printf(TEXT("Lock On        : %s  dist %.0f  err %+.1f deg  ttc %.2f s"),
				*Target->GetName(),
				FVector::Dist2D(Vehicle.GetActorLocation(), Target->GetActorLocation()),
				LockOn->GetAimErrorDegrees(), LockOn->GetTimeToContact()),
			PosY, ValueColor);
	}
	else
	{
		PosY = DrawDebugLine(
			FString::Printf(TEXT("Lock On        : (none)  cone %.0f deg / range %.0f"),
				Rules->LockOnConeAngle, Rules->LockOnRange),
			PosY, ValueColor);
	}

	// アシストが効いているかは、当たった / 避けられたの判断に直結するため常に出す。
	const EImpactAssistState AssistState = LockOn->GetAssistState();
	PosY = DrawDebugLine(
		FString::Printf(TEXT("Assist         : %s  %+.1f deg/s  (cone %.0f deg, cutoff %.2f s)"),
			LexToDisplayString(AssistState), LockOn->GetAssistYawRate(),
			Rules->AssistConeAngle, Rules->AssistCutoffTime),
		PosY, AssistState == EImpactAssistState::Active ? CombatHeaderColor : ValueColor);

	// 被ロック。HUD の方向表示が出ているかを数値でも確かめられるようにする。
	int32 LockerCount = 0;
	for (const TWeakObjectPtr<AImpactVehiclePawn>& Locker : LockOn->GetLockedOnBy())
	{
		if (Locker.IsValid())
		{
			++LockerCount;
		}
	}

	return DrawDebugLine(
		FString::Printf(TEXT("Locked By      : %d"), LockerCount),
		PosY, LockerCount > 0 ? WarnColor : ValueColor);
}
