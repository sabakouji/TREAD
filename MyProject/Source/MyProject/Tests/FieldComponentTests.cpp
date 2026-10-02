// 踏みつけ加速メカゲーム — フィールド用コンポーネントの自動テスト
//
// 陣営の敵味方判定と、破壊可能オブジェクトの判定規則（方向・段階・必要速度）を確かめる。
// 拠点の敵対判定・受け手の探索・登録先の検索は、テスト用の World に Actor を生成して確かめる。
// 調整値はアセットではなく C++ の既定値で検証する。
//
// 実行:
//   UnrealEditor-Cmd.exe <uproject> -ExecCmds="Automation RunTests MyProject.Impact;Quit" -unattended -nullrhi -nosplash

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Collision/ImpactReceiver.h"
#include "Collision/ImpactResolver.h"
#include "Engine/World.h"
#include "Field/DestructibleComponent.h"
#include "Field/FieldMarkerActor.h"
#include "Field/FieldSubsystem.h"
#include "Field/GoalComponent.h"
#include "Field/TeamComponent.h"
#include "GameFramework/Pawn.h"
#include "Tests/ImpactTestWorld.h"
#include "Tuning/VehicleTuningDataAsset.h"
#include "World/DestructibleObstacleActor.h"
#include "World/GoalActor.h"

namespace FieldComponentTests
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	/** 閾値のわずかに下を表す速度差（uu/s）。 */
	constexpr float BelowThreshold = 1.0f;

	/** 正面限定の判定に用いる角度（deg）。 */
	constexpr float FrontAngle = 60.0f;

	/** 段階破壊の検証に用いる耐久値。 */
	constexpr int32 StagedMaxHP = 3;

	/** マーカーの配置間隔（uu）。 */
	constexpr float MarkerOffset = 1000.0f;

	/** 部品の保護されたプロパティをテストから設定する。 */
	void SetProtectedInt(UObject* Object, const TCHAR* Name, int32 Value)
	{
		FIntProperty* Property = FindFProperty<FIntProperty>(Object->GetClass(), Name);
		check(Property);
		Property->SetPropertyValue_InContainer(Object, Value);
	}

	/** テスト用の World（共通）に、陣営を持つ Pawn の生成を加えたもの。部品は BeginPlay で UFieldSubsystem に登録される。 */
	struct FScopedTestWorld : ImpactTests::FScopedTestWorld
	{
		/** 陣営を持つ Pawn を生成する。衝突した機体の代わりに用いる。 */
		APawn* SpawnTeamPawn(int32 TeamId)
		{
			APawn* Pawn = World->SpawnActor<APawn>();
			UTeamComponent* Team = NewObject<UTeamComponent>(Pawn);
			Team->SetTeamId(TeamId);
			Team->RegisterComponent();
			return Pawn;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFieldTeamTest, "MyProject.Impact.Field.Team", FieldComponentTests::TestFlags)

bool FFieldTeamTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("different teams are hostile"), UTeamComponent::AreTeamsHostile(ImpactTeam::Player, ImpactTeam::Opponent));
	TestFalse(TEXT("same team is not hostile"), UTeamComponent::AreTeamsHostile(ImpactTeam::Opponent, ImpactTeam::Opponent));
	TestFalse(TEXT("neutral is not hostile to a team"), UTeamComponent::AreTeamsHostile(ImpactTeam::Neutral, ImpactTeam::Player));
	TestFalse(TEXT("neutral is not hostile to neutral"), UTeamComponent::AreTeamsHostile(ImpactTeam::Neutral, ImpactTeam::Neutral));
	TestEqual(TEXT("null actor is neutral"), UTeamComponent::GetTeamIdOf(nullptr), ImpactTeam::Neutral);
	TestFalse(TEXT("null actors are not hostile"), UTeamComponent::AreHostile(nullptr, nullptr));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFieldDestructibleRulesTest, "MyProject.Impact.Field.DestructibleRules", FieldComponentTests::TestFlags)

bool FFieldDestructibleRulesTest::RunTest(const FString& Parameters)
{
	using namespace FieldComponentTests;

	// 対象は +X を向いている。正面に当たる機体は -X 方向へ進んでくる。
	const FVector OwnerForward = FVector::ForwardVector;
	const FVector HitFront = -FVector::ForwardVector;
	const FVector HitBack = FVector::ForwardVector;
	const FVector HitSide = FVector::RightVector;
	const FVector HitFrontDiagonal = FVector(-1.0f, 0.5f, 0.0f).GetSafeNormal();

	TestTrue(TEXT("any direction when not front-only"), UDestructibleComponent::IsDirectionAccepted(false, FrontAngle, OwnerForward, HitBack));
	TestTrue(TEXT("front hit is accepted"), UDestructibleComponent::IsDirectionAccepted(true, FrontAngle, OwnerForward, HitFront));
	TestTrue(TEXT("diagonal front hit is accepted"), UDestructibleComponent::IsDirectionAccepted(true, FrontAngle, OwnerForward, HitFrontDiagonal));
	TestFalse(TEXT("side hit is rejected"), UDestructibleComponent::IsDirectionAccepted(true, FrontAngle, OwnerForward, HitSide));
	TestFalse(TEXT("back hit is rejected"), UDestructibleComponent::IsDirectionAccepted(true, FrontAngle, OwnerForward, HitBack));

	TestEqual(TEXT("one-hit object breaks"), UDestructibleComponent::ResolveHit(0, 1), EImpactReceiveOutcome::Destroyed);
	TestEqual(TEXT("first of three weakens"), UDestructibleComponent::ResolveHit(0, 3), EImpactReceiveOutcome::Weakened);
	TestEqual(TEXT("second of three weakens"), UDestructibleComponent::ResolveHit(1, 3), EImpactReceiveOutcome::Weakened);
	TestEqual(TEXT("third of three breaks"), UDestructibleComponent::ResolveHit(2, 3), EImpactReceiveOutcome::Destroyed);

	TestEqual(TEXT("intact has no damaged mesh"), UDestructibleComponent::GetDamagedMeshIndex(0, 2), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("no damaged meshes"), UDestructibleComponent::GetDamagedMeshIndex(1, 0), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("first damage uses first mesh"), UDestructibleComponent::GetDamagedMeshIndex(1, 2), 0);
	TestEqual(TEXT("extra damage keeps last mesh"), UDestructibleComponent::GetDamagedMeshIndex(5, 2), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFieldDestructibleImpactTest, "MyProject.Impact.Field.DestructibleImpact", FieldComponentTests::TestFlags)

bool FFieldDestructibleImpactTest::RunTest(const FString& Parameters)
{
	using namespace FieldComponentTests;

	UVehicleTuningDataAsset* Tuning = NewObject<UVehicleTuningDataAsset>();

	// 既定は Small ランク・一撃破壊・全方向。所有 Actor が無くても判定と状態は成立する。
	UDestructibleComponent* Destructible = NewObject<UDestructibleComponent>();

	FImpactReceiveContext Context;
	Context.Tuning = Tuning;
	Context.ImpactDirection = FVector::ForwardVector;

	Context.ImpactSpeed = Tuning->SmallDestructionSpeed - BelowThreshold;
	TestEqual(TEXT("below the threshold is unaffected"),
		Destructible->ReceiveVehicleImpact_Implementation(Context).Outcome, EImpactReceiveOutcome::Unaffected);
	TestFalse(TEXT("still intact"), Destructible->IsBroken());

	Context.ImpactSpeed = Tuning->SmallDestructionSpeed;
	const FImpactReceiveResult Broken = Destructible->ReceiveVehicleImpact_Implementation(Context);
	TestEqual(TEXT("at the threshold breaks"), Broken.Outcome, EImpactReceiveOutcome::Destroyed);
	TestEqual(TEXT("reports its rank"), Broken.Rank, EDestructionRank::Small);
	TestTrue(TEXT("now broken"), Destructible->IsBroken());

	TestEqual(TEXT("a broken object is unaffected"),
		Destructible->ReceiveVehicleImpact_Implementation(Context).Outcome, EImpactReceiveOutcome::Unaffected);

	Context.Tuning = nullptr;
	TestEqual(TEXT("missing tuning is unaffected"),
		NewObject<UDestructibleComponent>()->ReceiveVehicleImpact_Implementation(Context).Outcome, EImpactReceiveOutcome::Unaffected);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFieldDestructibleStagesTest, "MyProject.Impact.Field.DestructibleStages", FieldComponentTests::TestFlags)

bool FFieldDestructibleStagesTest::RunTest(const FString& Parameters)
{
	using namespace FieldComponentTests;

	UVehicleTuningDataAsset* Tuning = NewObject<UVehicleTuningDataAsset>();
	UDestructibleComponent* Destructible = NewObject<UDestructibleComponent>();
	SetProtectedInt(Destructible, TEXT("MaxHP"), StagedMaxHP);

	FImpactReceiveContext Context;
	Context.Tuning = Tuning;
	Context.ImpactSpeed = Tuning->SmallDestructionSpeed;

	// 閾値を超える衝突1回で1段階だけ進む。機体側は Weakened で弾かれるため、同じ突進で次の段階へは進まない。
	TestEqual(TEXT("first hit weakens"), Destructible->ReceiveVehicleImpact_Implementation(Context).Outcome, EImpactReceiveOutcome::Weakened);
	TestEqual(TEXT("two hits remain"), Destructible->GetRemainingHP(), StagedMaxHP - 1);
	TestFalse(TEXT("not broken after one hit"), Destructible->IsBroken());
	TestEqual(TEXT("second hit weakens"), Destructible->ReceiveVehicleImpact_Implementation(Context).Outcome, EImpactReceiveOutcome::Weakened);
	TestEqual(TEXT("third hit breaks"), Destructible->ReceiveVehicleImpact_Implementation(Context).Outcome, EImpactReceiveOutcome::Destroyed);
	TestTrue(TEXT("broken after three hits"), Destructible->IsBroken());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFieldReceiveReactionTest, "MyProject.Impact.Field.ReceiveReaction", FieldComponentTests::TestFlags)

bool FFieldReceiveReactionTest::RunTest(const FString& Parameters)
{
	// 損傷だけなら弾かれる。弾かれた後の速度（面から離れる向き）は MyProject.Impact.WallBounce で確かめている。
	TestEqual(TEXT("weakened building bounces the vehicle"),
		FImpactResolver::DecideReceiveReaction(EImpactReceiveOutcome::Weakened), EImpactReceiveReaction::BounceOffWeakened);
	TestEqual(TEXT("goal damage scores a goal hit"),
		FImpactResolver::DecideReceiveReaction(EImpactReceiveOutcome::Damaged), EImpactReceiveReaction::ScoreGoalHit);
	TestEqual(TEXT("destroyed object is passed through"),
		FImpactResolver::DecideReceiveReaction(EImpactReceiveOutcome::Destroyed), EImpactReceiveReaction::PassThroughDestroyed);
	TestEqual(TEXT("unaffected object is a wall"),
		FImpactResolver::DecideReceiveReaction(EImpactReceiveOutcome::Unaffected), EImpactReceiveReaction::TreatAsWall);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFieldReceiverLookupTest, "MyProject.Impact.Field.ReceiverLookup", FieldComponentTests::TestFlags)

bool FFieldReceiverLookupTest::RunTest(const FString& Parameters)
{
	using namespace FieldComponentTests;

	FScopedTestWorld TestWorld;

	AGoalActor* GoalActor = TestWorld.World->SpawnActor<AGoalActor>();
	ADestructibleObstacleActor* Obstacle = TestWorld.World->SpawnActor<ADestructibleObstacleActor>();
	AActor* Plain = TestWorld.World->SpawnActor<AActor>();

	TestTrue(TEXT("goal actor resolves to its goal component"),
		ImpactReceiver::FindReceiver(GoalActor) == GoalActor->GetGoalComponent());
	TestTrue(TEXT("obstacle resolves to its destructible component"),
		ImpactReceiver::FindReceiver(Obstacle) == Obstacle->GetDestructible());
	TestNull(TEXT("an actor without a receiver is a wall"), ImpactReceiver::FindReceiver(Plain));
	TestNull(TEXT("null actor has no receiver"), ImpactReceiver::FindReceiver(nullptr));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFieldGoalHostilityTest, "MyProject.Impact.Field.GoalHostility", FieldComponentTests::TestFlags)

bool FFieldGoalHostilityTest::RunTest(const FString& Parameters)
{
	using namespace FieldComponentTests;

	FScopedTestWorld TestWorld;
	UVehicleTuningDataAsset* Tuning = NewObject<UVehicleTuningDataAsset>();

	// AGoalActor はソロモードの NPC 側（ImpactTeam::Opponent）に属する。
	AGoalActor* GoalActor = TestWorld.SpawnAndBegin<AGoalActor>();
	UGoalComponent* Goal = GoalActor->GetGoalComponent();

	FImpactReceiveContext Context;
	Context.Tuning = Tuning;
	Context.ImpactSpeed = Tuning->LargeDestructionSpeed;

	Context.Instigator = TestWorld.SpawnTeamPawn(ImpactTeam::Opponent);
	TestEqual(TEXT("an ally cannot damage the goal"), Goal->ReceiveVehicleImpact_Implementation(Context).Outcome, EImpactReceiveOutcome::Unaffected);

	Context.Instigator = TestWorld.SpawnTeamPawn(ImpactTeam::Neutral);
	TestEqual(TEXT("a neutral vehicle cannot damage the goal"), Goal->ReceiveVehicleImpact_Implementation(Context).Outcome, EImpactReceiveOutcome::Unaffected);

	const int32 DurabilityBefore = Goal->GetDurability();
	Context.Instigator = TestWorld.SpawnTeamPawn(ImpactTeam::Player);
	const FImpactReceiveResult Hit = Goal->ReceiveVehicleImpact_Implementation(Context);
	TestEqual(TEXT("an enemy damages the goal"), Hit.Outcome, EImpactReceiveOutcome::Damaged);
	TestEqual(TEXT("durability drops by the damage"), Goal->GetDurability(), DurabilityBefore - Hit.Damage);

	Context.ImpactSpeed = Tuning->LargeDestructionSpeed - BelowThreshold;
	TestEqual(TEXT("an enemy below the threshold does nothing"), Goal->ReceiveVehicleImpact_Implementation(Context).Outcome, EImpactReceiveOutcome::Unaffected);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFieldSubsystemQueryTest, "MyProject.Impact.Field.SubsystemQuery", FieldComponentTests::TestFlags)

bool FFieldSubsystemQueryTest::RunTest(const FString& Parameters)
{
	using namespace FieldComponentTests;

	FScopedTestWorld TestWorld;
	const UFieldSubsystem* Field = TestWorld.World->GetSubsystem<UFieldSubsystem>();
	if (!TestNotNull(TEXT("field subsystem exists"), Field))
	{
		return false;
	}

	const AGoalActor* GoalActor = TestWorld.SpawnAndBegin<AGoalActor>();
	TestTrue(TEXT("finds the goal of its team"), Field->FindGoal(ImpactTeam::Opponent) == GoalActor->GetGoalComponent());
	TestNull(TEXT("no goal for the player team"), Field->FindGoal(ImpactTeam::Player));
	TestTrue(TEXT("the player attacks the opponent goal"), Field->FindHostileGoal(ImpactTeam::Player) == GoalActor->GetGoalComponent());
	TestNull(TEXT("the opponent has no goal to attack"), Field->FindHostileGoal(ImpactTeam::Opponent));

	// 中立のマーカーはどの陣営からも使える。近い方が選ばれる。
	const AFieldMarkerActor* Near = TestWorld.SpawnAndBegin<AFieldMarkerActor>(FVector(MarkerOffset, 0.0f, 0.0f));
	TestWorld.SpawnAndBegin<AFieldMarkerActor>(FVector(MarkerOffset * 3.0f, 0.0f, 0.0f));
	TestTrue(TEXT("nearest defense line marker"),
		Field->FindNearestFieldPoint(EFieldPointRole::DefenseLine, ImpactTeam::Opponent, FVector::ZeroVector) == Near->GetFieldPoint());
	TestNull(TEXT("no marker with another role"),
		Field->FindNearestFieldPoint(EFieldPointRole::AccelerationStart, ImpactTeam::Opponent, FVector::ZeroVector));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
