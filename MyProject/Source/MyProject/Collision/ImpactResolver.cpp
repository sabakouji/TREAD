// 踏みつけ加速メカゲーム — 衝突解決

#include "Collision/ImpactResolver.h"

#include "Tuning/ImpactTuningDataAsset.h"
#include "Tuning/VehicleTuningDataAsset.h"

namespace
{
	/** 質量係数の下限。運動量交換での 0 除算を避ける。 */
	constexpr float MinMassCoefficient = KINDA_SMALL_NUMBER;

	/** 水平面上の2方向のなす角（deg）を返す。どちらかが退化していれば 0（正面扱い）を返す。 */
	float AngleBetween2D(const FVector& A, const FVector& B)
	{
		const FVector NormalizedA = A.GetSafeNormal2D();
		const FVector NormalizedB = B.GetSafeNormal2D();
		if (NormalizedA.IsNearlyZero() || NormalizedB.IsNearlyZero())
		{
			return 0.0f;
		}

		const float Dot = FMath::Clamp(FVector::DotProduct(NormalizedA, NormalizedB), -1.0f, 1.0f);
		return FMath::RadiansToDegrees(FMath::Acos(Dot));
	}

	/** 鉛直成分を捨てる。追突は水平面上で解く。 */
	FVector Flatten(const FVector& Vector)
	{
		return FVector(Vector.X, Vector.Y, 0.0f);
	}
}

EImpactFacing FImpactResolver::DetermineFacing(
	const FVector& Forward,
	const FVector& ImpactDirection,
	const UImpactTuningDataAsset& Rules)
{
	// 機首方向と、接触が到来した方向のなす角で分割する。
	const float AngleDegrees = AngleBetween2D(Forward, ImpactDirection);

	if (AngleDegrees < Rules.FrontAngleThreshold)
	{
		return EImpactFacing::Front;
	}

	if (AngleDegrees < Rules.SideAngleThreshold)
	{
		return EImpactFacing::Side;
	}

	return EImpactFacing::Back;
}

float FImpactResolver::GetFacingPower(EImpactFacing Facing, const UImpactTuningDataAsset& Rules)
{
	switch (Facing)
	{
	case EImpactFacing::Side:
		return Rules.FacingPowerSide;
	case EImpactFacing::Back:
		return Rules.FacingPowerBack;
	case EImpactFacing::Front:
	default:
		return Rules.FacingPowerFront;
	}
}

float FImpactResolver::GetRequiredSpeed(EDestructionRank Rank, const UVehicleTuningDataAsset& Tuning)
{
	switch (Rank)
	{
	case EDestructionRank::Large:
		return Tuning.LargeDestructionSpeed;
	case EDestructionRank::Small:
	default:
		return Tuning.SmallDestructionSpeed;
	}
}

bool FImpactResolver::GetDestroyableRank(
	float Speed, const UVehicleTuningDataAsset& Tuning, EDestructionRank& OutRank)
{
	if (Speed >= Tuning.LargeDestructionSpeed)
	{
		OutRank = EDestructionRank::Large;
		return true;
	}

	if (Speed >= Tuning.SmallDestructionSpeed)
	{
		OutRank = EDestructionRank::Small;
		return true;
	}

	return false;
}

bool FImpactResolver::IsBumperEngaged(const FVector& Forward, const FVector& Direction, float BumperAngle)
{
	return AngleBetween2D(Forward, Direction) < BumperAngle;
}

float FImpactResolver::GetContactRadius(
	const FVector& Forward,
	const FVector& Direction,
	float Speed,
	const UVehicleTuningDataAsset& Tuning)
{
	if (!IsBumperEngaged(Forward, Direction, Tuning.BumperAngle))
	{
		// 側面・背面は判定を削らない。側面を取る戦術の価値を保つため（仕様 1-2）。
		return Tuning.BubbleRadius;
	}

	// 速いほど当たる。ただし速いほど曲がれない（仕様 1-3）。
	const float MaxRadius = FMath::Max(Tuning.BumperRadiusMax, Tuning.BubbleRadius);
	return FMath::Lerp(Tuning.BubbleRadius, MaxRadius, Tuning.GetSpeedRatio(Speed));
}

namespace
{
	/** 片側の内訳を組み立てる。解決後の速度は衝突前の速度で初期化しておく。 */
	FVehicleClashSide MakeClashSide(
		const FVehicleClashBody& Body, const FVector& TowardOther, const UImpactTuningDataAsset& Rules)
	{
		FVehicleClashSide Side;
		Side.Facing = FImpactResolver::DetermineFacing(Body.Forward, TowardOther, Rules);
		Side.RawSpeed = Flatten(Body.Velocity).Size();
		Side.FacingPower = FImpactResolver::GetFacingPower(Side.Facing, Rules);
		Side.Energy = Side.RawSpeed * Body.MassCoefficient * Side.FacingPower;
		Side.bBumperEngaged = FImpactResolver::IsBumperEngaged(Body.Forward, TowardOther, Body.BumperAngle);
		Side.ResultingVelocity = Flatten(Body.Velocity);
		return Side;
	}

	/**
	 * 拮抗スタンを片側へ適用する（仕様 2-6）。
	 * 離れる方向は真後ろではなく横へずらす。両者を同じ回転方向へずらすため互いに反対側の横へ抜け、
	 * スタン明けに正面同士の膠着が続かない。
	 */
	void ApplyClashStun(FVehicleClashSide& Side, const FVector& AwayFromOther, const UImpactTuningDataAsset& Rules)
	{
		const FVector Direction = AwayFromOther.RotateAngleAxis(Rules.ClashKnockbackSideAngle, FVector::UpVector);
		Side.Role = EVehicleClashRole::Even;
		Side.StunDuration = Rules.ClashStunDuration;
		Side.ResultingVelocity = Direction.GetSafeNormal2D() * Rules.ClashKnockbackSpeed;
	}

	/**
	 * 弾きを法線方向の運動量交換で解く（仕様 2-5）。
	 *
	 * 相手へ与える撃力 = (1 + 自分の反発) × 接近速度 × 換算質量。自分の反発はバンパーが働くときのみ Restitution、
	 * 働かなければ 0。正面から受けた側は真後ろへ弾かれ、かすった場合は接近速度が小さいため軽く逸れる。
	 */
	void ApplyDeflection(
		FVehicleClashResult& Result,
		const FVehicleClashBody& SelfBody,
		const FVehicleClashBody& OtherBody,
		const FVector& Normal,
		const UImpactTuningDataAsset& Rules)
	{
		const FVector SelfVelocity = Flatten(SelfBody.Velocity);
		const FVector OtherVelocity = Flatten(OtherBody.Velocity);

		// 離れつつある接触では押し合いが起きない。
		const float ClosingSpeed = FMath::Max(FVector::DotProduct(SelfVelocity - OtherVelocity, Normal), 0.0f);

		const float SelfMass = FMath::Max(SelfBody.MassCoefficient, MinMassCoefficient);
		const float OtherMass = FMath::Max(OtherBody.MassCoefficient, MinMassCoefficient);
		const float ReducedMass = SelfMass * OtherMass / (SelfMass + OtherMass);

		const float SelfRebound = Result.Self.bBumperEngaged ? Rules.Restitution : 0.0f;
		const float OtherRebound = Result.Other.bBumperEngaged ? Rules.Restitution : 0.0f;

		const float SelfDeltaSpeed = (1.0f + OtherRebound) * ClosingSpeed * ReducedMass / SelfMass;
		const float OtherDeltaSpeed = (1.0f + SelfRebound) * ClosingSpeed * ReducedMass / OtherMass;

		Result.Self.ResultingVelocity = SelfVelocity - Normal * SelfDeltaSpeed;
		Result.Other.ResultingVelocity = OtherVelocity + Normal * OtherDeltaSpeed;

		// かすっただけなら操作を奪わない。
		Result.Self.bKnockedBack = SelfDeltaSpeed >= Rules.KnockbackMinDeltaSpeed;
		Result.Other.bKnockedBack = OtherDeltaSpeed >= Rules.KnockbackMinDeltaSpeed;
	}
}

FVehicleClashResult FImpactResolver::ResolveVehicleClash(
	const FVehicleClashBody& Self,
	const FVehicleClashBody& Other,
	const FVector& SelfToOther,
	const UImpactTuningDataAsset& Rules)
{
	const FVector Normal = SelfToOther.GetSafeNormal2D();

	// 各機体にとって、接触は相手のいる方向から到来する。
	FVehicleClashResult Result;
	Result.Self = MakeClashSide(Self, Normal, Rules);
	Result.Other = MakeClashSide(Other, -Normal, Rules);
	Result.EnergyDifference = Result.Self.Energy - Result.Other.Energy;

	// 分岐の順序は仕様 2-2 を厳守する。勝敗を先に確定させ、その後に勝者の生速度で破壊可否を判定する。
	if (FMath::Abs(Result.EnergyDifference) <= Rules.ClashThreshold)
	{
		Result.Branch = EVehicleClashBranch::ClashStun;
		ApplyClashStun(Result.Self, -Normal, Rules);
		ApplyClashStun(Result.Other, Normal, Rules);
		return Result;
	}

	const bool bSelfWins = Result.EnergyDifference > 0.0f;
	FVehicleClashSide& Winner = bSelfWins ? Result.Self : Result.Other;
	FVehicleClashSide& Loser = bSelfWins ? Result.Other : Result.Self;
	Winner.Role = EVehicleClashRole::Winner;
	Loser.Role = EVehicleClashRole::Loser;

	// 破壊可否は面補正を掛けない生速度で見る。「超加速しているか」は勝者自身の状態の話であるため（仕様 2-1）。
	if (Rules.IsOverdrive(Winner.RawSpeed))
	{
		Result.Branch = EVehicleClashBranch::Destroy;
		Loser.bDefeated = true;
		Loser.ResultingVelocity = FVector::ZeroVector;
		Winner.ResultingVelocity *= Rules.ClashWinnerSpeedRatio;
		return Result;
	}

	Result.Branch = EVehicleClashBranch::Deflect;
	ApplyDeflection(Result, Self, Other, Normal, Rules);
	return Result;
}

FVector FImpactResolver::ComputeWallBounce(const FVector& Velocity, const FVector& SurfaceNormal, float Restitution)
{
	const FVector Normal = SurfaceNormal.GetSafeNormal2D();
	const FVector Horizontal = Flatten(Velocity);
	const float NormalSpeed = FVector::DotProduct(Horizontal, Normal);

	// 面から離れる向きに動いているなら反射しない。
	if (Normal.IsNearlyZero() || NormalSpeed >= 0.0f)
	{
		return Horizontal;
	}

	return Horizontal - Normal * ((1.0f + Restitution) * NormalSpeed);
}

EImpactReceiveReaction FImpactResolver::DecideReceiveReaction(EImpactReceiveOutcome Outcome)
{
	switch (Outcome)
	{
	case EImpactReceiveOutcome::Weakened:
		return EImpactReceiveReaction::BounceOffWeakened;
	case EImpactReceiveOutcome::Damaged:
		return EImpactReceiveReaction::ScoreGoalHit;
	case EImpactReceiveOutcome::Destroyed:
		return EImpactReceiveReaction::PassThroughDestroyed;
	case EImpactReceiveOutcome::Unaffected:
	default:
		return EImpactReceiveReaction::TreatAsWall;
	}
}
