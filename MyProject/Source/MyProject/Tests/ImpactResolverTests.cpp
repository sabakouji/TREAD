// 踏みつけ加速メカゲーム — 衝突解決の自動テスト
//
// 仕様（Plan/踏みつけ加速メカゲーム_追突・ロックオン仕様.md）の判定例と、弾き・拮抗・接触半径・壁反射の性質を確かめる。
// 調整値はアセットではなく C++ の既定値（= 仕様の暫定値）で検証する。
//
// 実行:
//   UnrealEditor-Cmd.exe <uproject> -ExecCmds="Automation RunTests MyProject.Impact;Quit" -unattended -nullrhi -nosplash

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Collision/ImpactResolver.h"
#include "Tuning/ImpactTuningDataAsset.h"
#include "Tuning/VehicleTuningDataAsset.h"

namespace ImpactResolverTests
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	/** 速度・エネルギーの比較の許容誤差（uu/s）。 */
	constexpr float SpeedTolerance = 1.0f;

	/** 角度の比較の許容誤差（deg）。 */
	constexpr float AngleTolerance = 0.5f;

	/** 自機は原点にいて、相手は +X 方向にいる配置を基本とする。 */
	const FVector SelfToOther = FVector::ForwardVector;

	FVehicleClashBody MakeBody(const FVector& Forward, const FVector& Velocity, const UVehicleTuningDataAsset& Vehicle)
	{
		FVehicleClashBody Body;
		Body.Forward = Forward;
		Body.Velocity = Velocity;
		Body.MassCoefficient = Vehicle.MassCoefficient;
		Body.BumperAngle = Vehicle.BumperAngle;
		return Body;
	}

	/** 自機が +X を向き +X へ SelfSpeed で進む機体を作る。 */
	FVehicleClashBody MakeForwardBody(float SelfSpeed, const UVehicleTuningDataAsset& Vehicle)
	{
		return MakeBody(FVector::ForwardVector, FVector::ForwardVector * SelfSpeed, Vehicle);
	}

	float AngleDegrees(const FVector& A, const FVector& B)
	{
		const float Dot = FMath::Clamp(FVector::DotProduct(A.GetSafeNormal2D(), B.GetSafeNormal2D()), -1.0f, 1.0f);
		return FMath::RadiansToDegrees(FMath::Acos(Dot));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImpactClashBranchTest, "MyProject.Impact.ClashBranch", ImpactResolverTests::TestFlags)

bool FImpactClashBranchTest::RunTest(const FString& Parameters)
{
	using namespace ImpactResolverTests;

	const UImpactTuningDataAsset* Rules = NewObject<UImpactTuningDataAsset>();
	const UVehicleTuningDataAsset* Vehicle = NewObject<UVehicleTuningDataAsset>();

	struct FCase
	{
		const TCHAR* Name;
		float SelfSpeed;
		FVector OtherForward;
		FVector OtherVelocity;
		EVehicleClashBranch ExpectedBranch;
		float ExpectedDifference;
	};

	// 仕様 2-3 の判定例（OverdriveSpeed 3600 / ClashThreshold 800 / Side 0.35 / Back 0.10）。
	// 相手の面は「自機のいる方向（-X）」と相手の機首のなす角で決まる。
	const FCase Cases[] =
	{
		{ TEXT("4200 front vs 1000 front"), 4200.0f, -FVector::ForwardVector, -FVector::ForwardVector * 1000.0f, EVehicleClashBranch::Destroy,   3200.0f },
		{ TEXT("2400 front vs 600 front"),  2400.0f, -FVector::ForwardVector, -FVector::ForwardVector * 600.0f,  EVehicleClashBranch::Deflect,   1800.0f },
		{ TEXT("4000 front vs 3800 front"), 4000.0f, -FVector::ForwardVector, -FVector::ForwardVector * 3800.0f, EVehicleClashBranch::ClashStun, 200.0f },
		{ TEXT("4000 front vs 3800 side"),  4000.0f, FVector::RightVector,    FVector::RightVector * 3800.0f,    EVehicleClashBranch::Destroy,   2670.0f },
		{ TEXT("2000 front vs 3800 back"),  2000.0f, FVector::ForwardVector,  FVector::ForwardVector * 3800.0f,  EVehicleClashBranch::Deflect,   1620.0f },
	};

	for (const FCase& Case : Cases)
	{
		const FVehicleClashBody Self = MakeForwardBody(Case.SelfSpeed, *Vehicle);
		const FVehicleClashBody Other = MakeBody(Case.OtherForward, Case.OtherVelocity, *Vehicle);

		const FVehicleClashResult Result = FImpactResolver::ResolveVehicleClash(Self, Other, SelfToOther, *Rules);

		TestEqual(FString::Printf(TEXT("%s: branch"), Case.Name), Result.Branch, Case.ExpectedBranch);
		TestNearlyEqual(FString::Printf(TEXT("%s: energy difference"), Case.Name),
			Result.EnergyDifference, Case.ExpectedDifference, SpeedTolerance);

		if (Case.ExpectedBranch == EVehicleClashBranch::Destroy)
		{
			TestTrue(FString::Printf(TEXT("%s: other is defeated"), Case.Name), Result.Other.bDefeated);
			TestFalse(FString::Printf(TEXT("%s: self survives"), Case.Name), Result.Self.bDefeated);
			TestEqual(FString::Printf(TEXT("%s: self wins"), Case.Name), Result.Self.Role, EVehicleClashRole::Winner);
		}
		else
		{
			TestFalse(FString::Printf(TEXT("%s: nobody is defeated"), Case.Name), Result.Self.bDefeated || Result.Other.bDefeated);
		}

		// どちらの機体が解決しても同じ結論になること（双方の移動が同じ衝突を拾うため）。
		const FVehicleClashResult Swapped = FImpactResolver::ResolveVehicleClash(Other, Self, -SelfToOther, *Rules);
		TestEqual(FString::Printf(TEXT("%s: symmetric branch"), Case.Name), Swapped.Branch, Result.Branch);
		TestNearlyEqual(FString::Printf(TEXT("%s: symmetric difference"), Case.Name),
			Swapped.EnergyDifference, -Result.EnergyDifference, SpeedTolerance);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImpactDeflectionTest, "MyProject.Impact.Deflection", ImpactResolverTests::TestFlags)

bool FImpactDeflectionTest::RunTest(const FString& Parameters)
{
	using namespace ImpactResolverTests;

	const UImpactTuningDataAsset* Rules = NewObject<UImpactTuningDataAsset>();
	const UVehicleTuningDataAsset* Vehicle = NewObject<UVehicleTuningDataAsset>();

	// 正面同士: 運動量が保存され、受けた側は真後ろ（+X）へ弾かれる。
	{
		const FVehicleClashBody Self = MakeForwardBody(2400.0f, *Vehicle);
		const FVehicleClashBody Other = MakeBody(-FVector::ForwardVector, -FVector::ForwardVector * 600.0f, *Vehicle);
		const FVehicleClashResult Result = FImpactResolver::ResolveVehicleClash(Self, Other, SelfToOther, *Rules);

		TestEqual(TEXT("head-on: branch"), Result.Branch, EVehicleClashBranch::Deflect);
		TestNearlyEqual(TEXT("head-on: momentum is conserved"),
			static_cast<float>(Result.Self.ResultingVelocity.X + Result.Other.ResultingVelocity.X), 2400.0f - 600.0f, SpeedTolerance);
		TestTrue(TEXT("head-on: other is pushed straight back"), Result.Other.ResultingVelocity.X > 0.0f);
		TestNearlyEqual(TEXT("head-on: no sideways component"), static_cast<float>(Result.Other.ResultingVelocity.Y), 0.0f, SpeedTolerance);
		TestTrue(TEXT("head-on: both are knocked back"), Result.Self.bKnockedBack && Result.Other.bKnockedBack);
	}

	// 側面で受けた側は押し返せない。当てた側は正面同士より勢いを保ち、受けた側だけが飛ばされる。
	{
		const FVehicleClashBody Self = MakeForwardBody(2000.0f, *Vehicle);
		const FVehicleClashBody FrontVictim = MakeBody(-FVector::ForwardVector, FVector::ZeroVector, *Vehicle);
		const FVehicleClashBody SideVictim = MakeBody(FVector::RightVector, FVector::ZeroVector, *Vehicle);

		const FVehicleClashResult FrontResult = FImpactResolver::ResolveVehicleClash(Self, FrontVictim, SelfToOther, *Rules);
		const FVehicleClashResult SideResult = FImpactResolver::ResolveVehicleClash(Self, SideVictim, SelfToOther, *Rules);

		TestEqual(TEXT("side victim: branch"), SideResult.Branch, EVehicleClashBranch::Deflect);
		TestFalse(TEXT("side victim: victim has no bumper"), SideResult.Other.bBumperEngaged);
		TestTrue(TEXT("side victim: hitter keeps more speed than head-on"),
			SideResult.Self.ResultingVelocity.X > FrontResult.Self.ResultingVelocity.X + SpeedTolerance);
		TestTrue(TEXT("side victim: victim flies ahead of the hitter"),
			SideResult.Other.ResultingVelocity.X > SideResult.Self.ResultingVelocity.X);
	}

	// かすり: 速度変化が KnockbackMinDeltaSpeed 未満の側は弾かれ状態にならず、接線方向の速度は保たれる。
	{
		const FVector Offset = FVector::ForwardVector.RotateAngleAxis(55.0f, FVector::UpVector);
		const FVehicleClashBody Self = MakeForwardBody(1000.0f, *Vehicle);
		// 相手は自機を側面で受ける向き（押し返せない）で止まっている。
		const FVehicleClashBody Other = MakeBody((-Offset).RotateAngleAxis(90.0f, FVector::UpVector), FVector::ZeroVector, *Vehicle);
		const FVehicleClashResult Result = FImpactResolver::ResolveVehicleClash(Self, Other, Offset, *Rules);

		TestEqual(TEXT("glancing: branch"), Result.Branch, EVehicleClashBranch::Deflect);
		TestFalse(TEXT("glancing: hitter keeps control"), Result.Self.bKnockedBack);
		TestTrue(TEXT("glancing: victim is knocked back"), Result.Other.bKnockedBack);

		const FVector Tangent = FVector::CrossProduct(FVector::UpVector, Offset);
		TestNearlyEqual(TEXT("glancing: tangential speed is kept"),
			static_cast<float>(FVector::DotProduct(Result.Self.ResultingVelocity, Tangent)),
			static_cast<float>(FVector::DotProduct(Self.Velocity, Tangent)), SpeedTolerance);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImpactClashStunTest, "MyProject.Impact.ClashStun", ImpactResolverTests::TestFlags)

bool FImpactClashStunTest::RunTest(const FString& Parameters)
{
	using namespace ImpactResolverTests;

	const UImpactTuningDataAsset* Rules = NewObject<UImpactTuningDataAsset>();
	const UVehicleTuningDataAsset* Vehicle = NewObject<UVehicleTuningDataAsset>();

	const FVehicleClashBody Self = MakeForwardBody(4000.0f, *Vehicle);
	const FVehicleClashBody Other = MakeBody(-FVector::ForwardVector, -FVector::ForwardVector * 3800.0f, *Vehicle);
	const FVehicleClashResult Result = FImpactResolver::ResolveVehicleClash(Self, Other, SelfToOther, *Rules);

	TestEqual(TEXT("branch"), Result.Branch, EVehicleClashBranch::ClashStun);
	TestNearlyEqual(TEXT("self stun"), Result.Self.StunDuration, Rules->ClashStunDuration, KINDA_SMALL_NUMBER);
	TestNearlyEqual(TEXT("other stun"), Result.Other.StunDuration, Rules->ClashStunDuration, KINDA_SMALL_NUMBER);
	TestNearlyEqual(TEXT("self knockback speed"), Result.Self.GetResultingSpeed(), Rules->ClashKnockbackSpeed, SpeedTolerance);
	TestNearlyEqual(TEXT("other knockback speed"), Result.Other.GetResultingSpeed(), Rules->ClashKnockbackSpeed, SpeedTolerance);

	// 互いに離れ、真後ろから横へずれる。横のずれは反対側になり、スタン明けに正面同士が続かない。
	TestTrue(TEXT("self moves away"), Result.Self.ResultingVelocity.X < 0.0f);
	TestTrue(TEXT("other moves away"), Result.Other.ResultingVelocity.X > 0.0f);
	TestNearlyEqual(TEXT("self side angle"),
		AngleDegrees(Result.Self.ResultingVelocity, -SelfToOther), Rules->ClashKnockbackSideAngle, AngleTolerance);
	TestTrue(TEXT("opposite sideways offsets"),
		Result.Self.ResultingVelocity.Y * Result.Other.ResultingVelocity.Y < 0.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImpactContactRadiusTest, "MyProject.Impact.ContactRadius", ImpactResolverTests::TestFlags)

bool FImpactContactRadiusTest::RunTest(const FString& Parameters)
{
	using namespace ImpactResolverTests;

	const UVehicleTuningDataAsset* Vehicle = NewObject<UVehicleTuningDataAsset>();
	const FVector Forward = FVector::ForwardVector;
	const float MidSpeed = (Vehicle->BaseSpeed + Vehicle->MaxSpeed) * 0.5f;
	const float MidRadius = (Vehicle->BubbleRadius + Vehicle->BumperRadiusMax) * 0.5f;

	TestNearlyEqual(TEXT("front at top speed"),
		FImpactResolver::GetContactRadius(Forward, Forward, Vehicle->MaxSpeed, *Vehicle), Vehicle->BumperRadiusMax, KINDA_SMALL_NUMBER);
	TestNearlyEqual(TEXT("front at mid speed"),
		FImpactResolver::GetContactRadius(Forward, Forward, MidSpeed, *Vehicle), MidRadius, SpeedTolerance);
	TestNearlyEqual(TEXT("front at base speed"),
		FImpactResolver::GetContactRadius(Forward, Forward, Vehicle->BaseSpeed, *Vehicle), Vehicle->BubbleRadius, KINDA_SMALL_NUMBER);
	TestNearlyEqual(TEXT("side at top speed"),
		FImpactResolver::GetContactRadius(Forward, FVector::RightVector, Vehicle->MaxSpeed, *Vehicle), Vehicle->BubbleRadius, KINDA_SMALL_NUMBER);
	TestNearlyEqual(TEXT("back at top speed"),
		FImpactResolver::GetContactRadius(Forward, -Forward, Vehicle->MaxSpeed, *Vehicle), Vehicle->BubbleRadius, KINDA_SMALL_NUMBER);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImpactWallBounceTest, "MyProject.Impact.WallBounce", ImpactResolverTests::TestFlags)

bool FImpactWallBounceTest::RunTest(const FString& Parameters)
{
	using namespace ImpactResolverTests;

	const UImpactTuningDataAsset* Rules = NewObject<UImpactTuningDataAsset>();
	const FVector WallNormal = -FVector::ForwardVector;
	const float Restitution = Rules->Restitution;

	TestTrue(TEXT("straight into the wall"),
		FImpactResolver::ComputeWallBounce(FVector(1000.0f, 0.0f, 0.0f), WallNormal, Restitution)
			.Equals(FVector(-1000.0f * Restitution, 0.0f, 0.0f), SpeedTolerance));
	TestTrue(TEXT("diagonal keeps tangential speed"),
		FImpactResolver::ComputeWallBounce(FVector(1000.0f, 500.0f, 0.0f), WallNormal, Restitution)
			.Equals(FVector(-1000.0f * Restitution, 500.0f, 0.0f), SpeedTolerance));
	TestTrue(TEXT("moving away is unchanged"),
		FImpactResolver::ComputeWallBounce(FVector(-1000.0f, 0.0f, 0.0f), WallNormal, Restitution)
			.Equals(FVector(-1000.0f, 0.0f, 0.0f), SpeedTolerance));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
