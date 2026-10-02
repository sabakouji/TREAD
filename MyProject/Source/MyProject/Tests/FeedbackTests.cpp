// 踏みつけ加速メカゲーム — 破壊演出の自動テスト
//
// 演出の調整値の選び方（出来事 → 演出）、ヒットストップの時間の換算、破片の初速の計算を確かめる。
// 調整値はアセットではなく C++ の既定値で検証する。
//
// 実行:
//   UnrealEditor-Cmd.exe <uproject> -ExecCmds="Automation RunTests MyProject.Impact;Quit" -unattended -nullrhi -nosplash

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Feedback/ImpactCameraShake.h"
#include "Feedback/ImpactFeedbackComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/WorldSettings.h"
#include "Tests/ImpactTestWorld.h"
#include "Tuning/FeedbackTuningDataAsset.h"

namespace FeedbackTests
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	/** 時間・速度の比較の許容誤差。 */
	constexpr float Tolerance = 0.001f;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFeedbackPresetTest, "MyProject.Impact.Feedback.Preset", FeedbackTests::TestFlags)

bool FFeedbackPresetTest::RunTest(const FString& Parameters)
{
	const UFeedbackTuningDataAsset* Tuning = NewObject<UFeedbackTuningDataAsset>();

	TestTrue(TEXT("small destruction has feedback"), Tuning->FindPreset(EVehicleGameplayEvent::DestroyedSmall) == &Tuning->SmallDestroyed);
	TestTrue(TEXT("large destruction has feedback"), Tuning->FindPreset(EVehicleGameplayEvent::DestroyedLarge) == &Tuning->LargeDestroyed);
	TestTrue(TEXT("structure damage has feedback"), Tuning->FindPreset(EVehicleGameplayEvent::StructureDamaged) == &Tuning->StructureDamaged);
	TestTrue(TEXT("goal hit has feedback"), Tuning->FindPreset(EVehicleGameplayEvent::GoalHit) == &Tuning->GoalHit);
	TestNull(TEXT("stomp has no feedback"), Tuning->FindPreset(EVehicleGameplayEvent::Stomp));
	TestNull(TEXT("crash has no feedback"), Tuning->FindPreset(EVehicleGameplayEvent::Crash));

	// 壁・建物の破壊が最も重い手応えになる（長く止まり、強く揺れる）。
	TestTrue(TEXT("large stops longer than small"), Tuning->LargeDestroyed.HitStopDuration > Tuning->SmallDestroyed.HitStopDuration);
	TestTrue(TEXT("large shakes harder than small"), Tuning->LargeDestroyed.ShakeScale > Tuning->SmallDestroyed.ShakeScale);

	TestTrue(TEXT("default shake is the impact shake"), Tuning->ShakeClass == UImpactCameraShake::StaticClass());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFeedbackHitStopTimerTest, "MyProject.Impact.Feedback.HitStopTimer", FeedbackTests::TestFlags)

bool FFeedbackHitStopTimerTest::RunTest(const FString& Parameters)
{
	using namespace FeedbackTests;

	// 時間が 0.05 倍で進む間、実時間 0.1 秒はゲーム時間 0.005 秒にあたる。
	TestNearlyEqual(TEXT("dilated delay"), UFeedbackTuningDataAsset::ToDilatedTimerDelay(0.1f, 0.05f), 0.005f, Tolerance);
	TestNearlyEqual(TEXT("normal time keeps the duration"), UFeedbackTuningDataAsset::ToDilatedTimerDelay(0.1f, 1.0f), 0.1f, Tolerance);
	TestNearlyEqual(TEXT("negative duration is zero"), UFeedbackTuningDataAsset::ToDilatedTimerDelay(-1.0f, 0.5f), 0.0f, Tolerance);
	TestNearlyEqual(TEXT("dilation above one is clamped"), UFeedbackTuningDataAsset::ToDilatedTimerDelay(0.1f, 2.0f), 0.1f, Tolerance);
	TestTrue(TEXT("zero dilation still ends"), UFeedbackTuningDataAsset::ToDilatedTimerDelay(0.1f, 0.0f) > 0.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFeedbackDebrisVelocityTest, "MyProject.Impact.Feedback.DebrisVelocity", FeedbackTests::TestFlags)

bool FFeedbackDebrisVelocityTest::RunTest(const FString& Parameters)
{
	using namespace FeedbackTests;

	const UFeedbackTuningDataAsset* Tuning = NewObject<UFeedbackTuningDataAsset>();
	const float Ratio = Tuning->DebrisSpeedRatio;

	// 突進方向（水平）へ、機体の速さに比例して飛ばす。上下成分は使わない。
	TestTrue(TEXT("follows the charge direction"),
		Tuning->ComputeDebrisVelocity(FVector(3000.0f, 0.0f, 500.0f)).Equals(FVector(3000.0f * Ratio, 0.0f, 0.0f), Tolerance));
	TestTrue(TEXT("diagonal charge"),
		Tuning->ComputeDebrisVelocity(FVector(1000.0f, -1000.0f, 0.0f)).Equals(FVector(1000.0f * Ratio, -1000.0f * Ratio, 0.0f), Tolerance));
	TestTrue(TEXT("no velocity when stopped"), Tuning->ComputeDebrisVelocity(FVector::ZeroVector).IsNearlyZero());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFeedbackHitStopMergeTest, "MyProject.Impact.Feedback.HitStopMerge", FeedbackTests::TestFlags)

bool FFeedbackHitStopMergeTest::RunTest(const FString& Parameters)
{
	using namespace FeedbackTests;

	constexpr double Now = 10.0;

	// 実行中でなければ、新しい要求そのもの。
	const FHitStopState Idle;
	const FHitStopState Started = UImpactFeedbackComponent::MergeHitStop(Idle, Now, 0.09f, 0.05f);
	TestNearlyEqual(TEXT("starts with the requested dilation"), Started.TimeDilation, 0.05f, Tolerance);
	TestNearlyEqual(TEXT("ends after the requested duration"), static_cast<float>(Started.EndRealTime - Now), 0.09f, Tolerance);

	// 実行中に弱く短い要求が来ても、深さと長さは縮まない。
	const FHitStopState Weaker = UImpactFeedbackComponent::MergeHitStop(Started, Now + 0.01, 0.03f, 0.2f);
	TestNearlyEqual(TEXT("keeps the deeper dilation"), Weaker.TimeDilation, 0.05f, Tolerance);
	TestNearlyEqual(TEXT("keeps the later end"), static_cast<float>(Weaker.EndRealTime - Now), 0.09f, Tolerance);

	// 実行中に長い要求が来れば延びる。
	const FHitStopState Longer = UImpactFeedbackComponent::MergeHitStop(Started, Now + 0.05, 0.09f, 0.2f);
	TestNearlyEqual(TEXT("extends to the later end"), static_cast<float>(Longer.EndRealTime - Now), 0.14f, Tolerance);
	TestNearlyEqual(TEXT("still the deeper dilation"), Longer.TimeDilation, 0.05f, Tolerance);

	// 終わった後の要求は、前のものと合成しない。
	const FHitStopState AfterEnd = UImpactFeedbackComponent::MergeHitStop(Started, Now + 1.0, 0.03f, 0.2f);
	TestNearlyEqual(TEXT("fresh after the previous one ended"), AfterEnd.TimeDilation, 0.2f, Tolerance);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFeedbackHitStopWorldTest, "MyProject.Impact.Feedback.HitStopWorld", FeedbackTests::TestFlags)

bool FFeedbackHitStopWorldTest::RunTest(const FString& Parameters)
{
	using namespace FeedbackTests;

	ImpactTests::FScopedTestWorld TestWorld;
	const AWorldSettings* Settings = TestWorld.World->GetWorldSettings();
	if (!TestNotNull(TEXT("world settings exist"), Settings))
	{
		return false;
	}

	// PlayerController 以外に付けても、ヒットストップの開始・解除は働く（出来事の購読だけが行われない）。
	AActor* Owner = TestWorld.World->SpawnActor<AActor>();
	UImpactFeedbackComponent* Feedback = NewObject<UImpactFeedbackComponent>(Owner);
	Feedback->RegisterComponent();
	Owner->DispatchBeginPlay();

	Feedback->StartHitStop(0.09f, 0.05f);
	TestTrue(TEXT("hit stop is active"), Feedback->IsHitStopActive());
	TestNearlyEqual(TEXT("time slows down"), Settings->TimeDilation, 0.05f, Tolerance);

	Feedback->StartHitStop(0.03f, 0.2f);
	TestNearlyEqual(TEXT("a weaker request keeps the deeper slow-down"), Settings->TimeDilation, 0.05f, Tolerance);

	Feedback->EndHitStop();
	TestFalse(TEXT("hit stop ended"), Feedback->IsHitStopActive());
	TestNearlyEqual(TEXT("time is back to normal"), Settings->TimeDilation, 1.0f, Tolerance);

	Feedback->StartHitStop(0.09f, 1.0f);
	TestFalse(TEXT("a normal-speed request does nothing"), Feedback->IsHitStopActive());

	Feedback->StartHitStop(0.09f, 0.05f);
	Feedback->DestroyComponent();
	TestNearlyEqual(TEXT("destroying the component restores time"), Settings->TimeDilation, 1.0f, Tolerance);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
