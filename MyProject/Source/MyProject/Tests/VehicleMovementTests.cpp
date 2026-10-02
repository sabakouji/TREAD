// 踏みつけ加速メカゲーム — 機体の移動と衝突の拾い方の自動テスト
//
// テスト用の World に床・破壊可能オブジェクト・小物・機体を置き、移動コンポーネントを実際に進めて、
// 衝突の拾い方（判定に回るか・止められずに通り抜けるか）の動作を確かめる。
//
// 注意: GroundedImpact は「接地中に建物へ突っ込んだ衝突が判定に回る」ことの動作確認であり、回帰テストではない。
// FIX-08 で直した PIE の症状（接触しても破壊されないことがある）は、修正前のコードでもこのテストでは再現しなかった。
// 症状が消えたことはユーザーの PIE で確認した（2026-09-28）が、原因はログで確定していない。
// 再発したときは `Impact.LogWallHits 1` で衝突のログを取り、再現する条件でテストを作り直すこと（CLAUDE.md §7）。
// 調整値はアセットではなく C++ の既定値で検証する。
//
// 実行:
//   UnrealEditor-Cmd.exe <uproject> -ExecCmds="Automation RunTests MyProject.Impact;Quit" -unattended -nullrhi -nosplash

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/BoxComponent.h"
#include "Field/BreakablePropComponent.h"
#include "Field/DestructibleComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Tests/ImpactTestWorld.h"
#include "Tuning/ImpactTuningDataAsset.h"
#include "Tuning/VehicleTuningDataAsset.h"
#include "Vehicle/ImpactVehicleMovementComponent.h"
#include "Vehicle/ImpactVehiclePawn.h"
#include "World/BreakablePropActor.h"
#include "World/DestructibleObstacleActor.h"

namespace VehicleMovementTests
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	/** 1フレームの長さ（秒）。60fps 相当。 */
	constexpr float FrameSeconds = 1.0f / 60.0f;

	/** 建物に届くまでに進める最大フレーム数。速さ 4000 uu/s なら 1 秒で 4000 uu 進む。 */
	constexpr int32 MaxFrames = 60;

	/** 床の半分の大きさ（uu）。上面を Z = 0 にする。 */
	const FVector FloorExtent(5000.0f, 5000.0f, 50.0f);

	/** 機体の前方、建物の中心までの距離（uu）。 */
	constexpr float ObstacleDistance = 600.0f;

	/** 閾値に達するまで踏みつけ1回分の加速を重ねる回数の上限。 */
	constexpr int32 MaxBoosts = 20;

	/** 保護された UPROPERTY（オブジェクト参照）をテストから設定する。 */
	void SetObjectProperty(UObject* Object, const TCHAR* Name, UObject* Value)
	{
		FObjectProperty* Property = FindFProperty<FObjectProperty>(Object->GetClass(), Name);
		check(Property);
		Property->SetObjectPropertyValue_InContainer(Object, Value);
	}

	/** 上面が Z = 0 の、全チャンネルを止める床を置く。 */
	void SpawnFloor(UWorld& World)
	{
		AActor* Floor = World.SpawnActor<AActor>();
		UBoxComponent* Box = NewObject<UBoxComponent>(Floor);
		Box->SetBoxExtent(FloorExtent);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Box->SetCollisionObjectType(ECC_WorldStatic);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		Floor->SetRootComponent(Box);
		Box->RegisterComponent();
		Box->SetWorldLocation(FVector(0.0f, 0.0f, -FloorExtent.Z));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleGroundedImpactTest, "MyProject.Impact.Movement.GroundedImpact", VehicleMovementTests::TestFlags)

bool FVehicleGroundedImpactTest::RunTest(const FString& Parameters)
{
	using namespace VehicleMovementTests;

	ImpactTests::FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld.World;
	SpawnFloor(World);

	UVehicleTuningDataAsset* Tuning = NewObject<UVehicleTuningDataAsset>();
	UImpactTuningDataAsset* Rules = NewObject<UImpactTuningDataAsset>();

	// 動作確認: 接地中に閾値を超える速さで建物へ突っ込むと、衝突が判定に回って建物が壊れる。
	// 機体は床に接した高さに、+X を向けて置く。BeginPlay は呼ばない（見た目・カメラの設定は不要なため）。
	AImpactVehiclePawn* Vehicle = World.SpawnActor<AImpactVehiclePawn>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("vehicle spawned"), Vehicle))
	{
		return false;
	}
	SetObjectProperty(Vehicle, TEXT("Tuning"), Tuning);
	SetObjectProperty(Vehicle, TEXT("ImpactTuning"), Rules);
	const float VehicleHalfHeight = Vehicle->GetCollisionBox()->GetScaledBoxExtent().Z;
	Vehicle->SetActorLocation(FVector(0.0f, 0.0f, VehicleHalfHeight));

	// 建物（既定の Small ランク・一撃破壊）も床に接して置く。
	ADestructibleObstacleActor* Obstacle = World.SpawnActor<ADestructibleObstacleActor>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("obstacle spawned"), Obstacle))
	{
		return false;
	}
	const float ObstacleHalfHeight = Obstacle->GetCollisionBox()->GetScaledBoxExtent().Z;
	Obstacle->SetActorLocation(FVector(ObstacleDistance, 0.0f, ObstacleHalfHeight));

	// 閾値を超えるまで加速し、スロットルを入れたまま速さを保つ。
	UImpactVehicleMovementComponent* Movement = Vehicle->GetVehicleMovement();
	Movement->SetMoveInput(FVector2D(0.0f, 1.0f));
	for (int32 Boost = 0; Boost < MaxBoosts && Movement->GetCurrentSpeed() < Tuning->SmallDestructionSpeed * 1.5f; ++Boost)
	{
		Movement->DebugApplyStompBoost();
	}
	Movement->ToggleDebugHoldBoost();
	TestTrue(TEXT("fast enough to break the obstacle"), Movement->GetCurrentSpeed() >= Tuning->SmallDestructionSpeed);

	// まず数フレーム進めて接地させてから、建物まで走らせる。
	UDestructibleComponent* Destructible = Obstacle->GetDestructible();
	bool bWasGrounded = false;
	for (int32 Frame = 0; Frame < MaxFrames && !Destructible->IsBroken(); ++Frame)
	{
		Movement->TickComponent(FrameSeconds, LEVELTICK_All, nullptr);
		bWasGrounded |= Movement->IsGrounded();
	}

	TestTrue(TEXT("the vehicle was on the ground"), bWasGrounded);
	TestTrue(TEXT("a grounded charge breaks the obstacle"), Destructible->IsBroken());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehiclePassesPropTest, "MyProject.Impact.Movement.PassesProp", VehicleMovementTests::TestFlags)

bool FVehiclePassesPropTest::RunTest(const FString& Parameters)
{
	using namespace VehicleMovementTests;

	ImpactTests::FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld.World;
	SpawnFloor(World);

	UVehicleTuningDataAsset* Tuning = NewObject<UVehicleTuningDataAsset>();
	UImpactTuningDataAsset* Rules = NewObject<UImpactTuningDataAsset>();

	AImpactVehiclePawn* Vehicle = World.SpawnActor<AImpactVehiclePawn>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("vehicle spawned"), Vehicle))
	{
		return false;
	}
	SetObjectProperty(Vehicle, TEXT("Tuning"), Tuning);
	SetObjectProperty(Vehicle, TEXT("ImpactTuning"), Rules);
	Vehicle->SetActorLocation(FVector(0.0f, 0.0f, Vehicle->GetCollisionBox()->GetScaledBoxExtent().Z));

	ABreakablePropActor* Prop = World.SpawnActor<ABreakablePropActor>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("prop spawned"), Prop))
	{
		return false;
	}
	Prop->SetActorLocation(FVector(ObstacleDistance, 0.0f, Prop->GetCollisionBox()->GetScaledBoxExtent().Z));

	// 小物は機体の進路を止めない。どの破壊閾値にも届かない遅めの速さで、小物の位置を越えるまで走らせる。
	// 小物が壊れること自体は MyProject.Impact.Field.PropOverlap で確かめる（テスト用の World では重なりの通知が出ないため）。
	UImpactVehicleMovementComponent* Movement = Vehicle->GetVehicleMovement();
	Movement->SetMoveInput(FVector2D(0.0f, 1.0f));
	Movement->DebugApplyStompBoost();
	Movement->ToggleDebugHoldBoost();
	const float SpeedBefore = Movement->GetCurrentSpeed();
	TestTrue(TEXT("slower than any destruction threshold"), SpeedBefore < Tuning->SmallDestructionSpeed);

	const float PassedX = ObstacleDistance + Prop->GetCollisionBox()->GetScaledBoxExtent().X;
	for (int32 Frame = 0; Frame < MaxFrames && Vehicle->GetActorLocation().X <= PassedX; ++Frame)
	{
		Movement->TickComponent(FrameSeconds, LEVELTICK_All, nullptr);
	}

	TestTrue(TEXT("the vehicle passes the prop"), Vehicle->GetActorLocation().X > PassedX);
	TestFalse(TEXT("the prop never went through wall resolution (no score, no crash)"), Movement->HasResolvedImpact());
	TestTrue(TEXT("the vehicle keeps its speed"), Movement->GetCurrentSpeed() >= SpeedBefore * 0.99f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFieldPropOverlapTest, "MyProject.Impact.Field.PropOverlap", VehicleMovementTests::TestFlags)

bool FFieldPropOverlapTest::RunTest(const FString& Parameters)
{
	ImpactTests::FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld.World;

	// 小物は BeginPlay で重なりの購読を始める。
	ABreakablePropActor* Prop = TestWorld.SpawnAndBegin<ABreakablePropActor>();
	if (!TestNotNull(TEXT("prop spawned"), Prop))
	{
		return false;
	}
	UBoxComponent* Box = Prop->GetCollisionBox();
	UBreakablePropComponent* Breakable = Prop->GetBreakable();

	// 当たり判定は機体（Pawn）とだけ重なり、機体を止めない。
	TestEqual(TEXT("overlaps pawns"), Box->GetCollisionResponseToChannel(ECC_Pawn), ECR_Overlap);
	TestEqual(TEXT("does not block world static queries"), Box->GetCollisionResponseToChannel(ECC_WorldStatic), ECR_Ignore);
	TestTrue(TEXT("generates overlap events"), Box->GetGenerateOverlapEvents());

	// 重なりの通知を直接発生させる。機体（Pawn）以外では壊れない。
	AActor* NotAVehicle = World.SpawnActor<AActor>();
	Box->OnComponentBeginOverlap.Broadcast(Box, NotAVehicle, nullptr, 0, false, FHitResult());
	TestFalse(TEXT("a non-pawn does not break the prop"), Breakable->IsBroken());

	APawn* Vehicle = World.SpawnActor<APawn>();
	Box->OnComponentBeginOverlap.Broadcast(Box, Vehicle, nullptr, 0, false, FHitResult());
	TestTrue(TEXT("a vehicle breaks the prop"), Breakable->IsBroken());
	TestFalse(TEXT("collision is off after breaking"), Prop->GetActorEnableCollision());
	TestTrue(TEXT("hidden after breaking"), Prop->IsHidden());

	// 二度目は何もしない（通知も状態も変わらない）。
	Breakable->Break(Vehicle);
	TestTrue(TEXT("stays broken"), Breakable->IsBroken());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
