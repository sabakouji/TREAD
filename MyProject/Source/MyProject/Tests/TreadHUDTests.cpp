// 踏みつけ加速メカゲーム — 汎用 HUD 部品の自動テスト
//
// 値の置き場（正規化・変化時のみの通知）と、表示の計算（追従・セグメント・閾値）を確かめる。
// ウィジェットの描画には依存させず、計算は TreadHUDMath を直接呼ぶ。
//
// 実行:
//   UnrealEditor-Cmd.exe <uproject> -ExecCmds="Automation RunTests MyProject.Impact;Quit" -unattended -nullrhi -nosplash

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Curves/CurveFloat.h"
#include "UI/HUD/TreadHUDDataComponent.h"
#include "UI/HUD/TreadHUDImageSwitchWidget.h"
#include "UI/HUD/TreadHUDLibrary.h"
#include "UI/HUD/TreadHUDMath.h"
#include "UI/HUD/TreadHUDMeterWidget.h"
#include "UI/HUD/TreadHUDTags.h"
#include "UI/HUD/TreadHUDTextWidget.h"

namespace TreadHUDTests
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	/** 比率の比較の許容誤差。 */
	constexpr float Tolerance = 0.0001f;

	/** 1.0 → 0.0 の遷移を検証する際の、表示値の追従速度と 1 フレームの時間。 */
	constexpr float InterpSpeed = 10.0f;
	constexpr float FrameTime = 1.0f / 60.0f;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTreadHUDDataTest, "MyProject.Impact.HUD.Data", TreadHUDTests::TestFlags)

bool FTreadHUDDataTest::RunTest(const FString& Parameters)
{
	UTreadHUDDataComponent* Data = NewObject<UTreadHUDDataComponent>();
	float Raw = -1.0f;
	float Normalized = -1.0f;

	// 未登録のタグは false を返し、出力は 0 になる。
	TestFalse(TEXT("unset tag is not found"), Data->GetValue(TreadHUDTags::Speed, Raw, Normalized));
	TestEqual(TEXT("unset raw is zero"), Raw, 0.0f);
	TestEqual(TEXT("unset normalized is zero"), Normalized, 0.0f);

	// 書き込むと正規化して保持する。
	TestTrue(TEXT("first write notifies"), Data->SetValue(TreadHUDTags::Speed, 2500.0f, 0.0f, 5000.0f));
	TestTrue(TEXT("written tag is found"), Data->GetValue(TreadHUDTags::Speed, Raw, Normalized));
	TestEqual(TEXT("raw is kept"), Raw, 2500.0f);
	TestEqual(TEXT("normalized to half"), Normalized, 0.5f, TreadHUDTests::Tolerance);

	// 同じ値の書き込みは通知しない（毎フレームの書き込みで通知が溢れない）。
	TestFalse(TEXT("same value does not notify"), Data->SetValue(TreadHUDTags::Speed, 2500.0f, 0.0f, 5000.0f));
	TestTrue(TEXT("changed value notifies"), Data->SetValue(TreadHUDTags::Speed, 2600.0f, 0.0f, 5000.0f));
	TestTrue(TEXT("changed range notifies"), Data->SetValue(TreadHUDTags::Speed, 2600.0f, 0.0f, 5200.0f));

	// 範囲外は切り詰め、Min >= Max は 0 除算せず 0 にする。
	Data->SetValue(TreadHUDTags::Speed, 9000.0f, 0.0f, 5000.0f);
	Data->GetValue(TreadHUDTags::Speed, Raw, Normalized);
	TestEqual(TEXT("over max is clamped"), Normalized, 1.0f);
	TestEqual(TEXT("raw over max is kept"), Raw, 9000.0f);

	Data->SetValue(TreadHUDTags::Stun, 1.0f, 0.0f, 0.0f);
	Data->GetValue(TreadHUDTags::Stun, Raw, Normalized);
	TestEqual(TEXT("empty range normalizes to zero"), Normalized, 0.0f);

	// タグごとに独立して保持する。
	Data->GetValue(TreadHUDTags::Speed, Raw, Normalized);
	TestEqual(TEXT("other tag is untouched"), Raw, 9000.0f);

	// 無効なタグは書き込まない。
	TestFalse(TEXT("invalid tag is ignored"), Data->SetValue(FGameplayTag(), 1.0f, 0.0f, 1.0f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTreadHUDMathTest, "MyProject.Impact.HUD.Math", TreadHUDTests::TestFlags)

bool FTreadHUDMathTest::RunTest(const FString& Parameters)
{
	using namespace TreadHUDTests;

	TestEqual(TEXT("normalize mid"), TreadHUDMath::Normalize(5.0f, 0.0f, 10.0f), 0.5f, Tolerance);
	TestEqual(TEXT("normalize with offset min"), TreadHUDMath::Normalize(15.0f, 10.0f, 20.0f), 0.5f, Tolerance);
	TestEqual(TEXT("normalize below min"), TreadHUDMath::Normalize(-5.0f, 0.0f, 10.0f), 0.0f);
	TestEqual(TEXT("normalize inverted range"), TreadHUDMath::Normalize(5.0f, 10.0f, 0.0f), 0.0f);

	// InterpSpeed 0 は即時に目標値になる。
	TestEqual(TEXT("zero interp snaps"), TreadHUDMath::StepToward(0.0f, 1.0f, FrameTime, 0.0f), 1.0f);

	// 追従は 1 フレームでは届かず、目標を越えずに近づき続ける。
	float Value = 1.0f;
	float Previous = Value;
	Value = TreadHUDMath::StepToward(Value, 0.0f, FrameTime, InterpSpeed);
	TestTrue(TEXT("interp moves toward target"), Value < Previous);
	TestTrue(TEXT("interp does not arrive in one frame"), Value > 0.0f);
	for (int32 Frame = 0; Frame < 600; ++Frame)
	{
		Previous = Value;
		Value = TreadHUDMath::StepToward(Value, 0.0f, FrameTime, InterpSpeed);
		if (Value > Previous || Value < 0.0f)
		{
			AddError(TEXT("interp overshot or reversed"));
			break;
		}
	}
	TestEqual(TEXT("interp converges"), Value, 0.0f, Tolerance);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTreadHUDMeterTest, "MyProject.Impact.HUD.Meter", TreadHUDTests::TestFlags)

bool FTreadHUDMeterTest::RunTest(const FString& Parameters)
{
	using namespace TreadHUDTests;

	// 赤・黄・緑・青の 4 セグメント（スピードメーターの画像の区切り）。
	const TArray<float> Ends = { 0.2f, 0.45f, 0.7f, 1.0f };

	// 入っているセグメントの数: 0 では何も入っておらず、始端を超えたら入る。
	TestEqual(TEXT("zero enters nothing"), TreadHUDMath::CountEnteredSegments(0.0f, Ends), 0);
	TestEqual(TEXT("just above zero enters the first"), TreadHUDMath::CountEnteredSegments(0.01f, Ends), 1);
	TestEqual(TEXT("on a boundary stays in the lower segment"), TreadHUDMath::CountEnteredSegments(0.45f, Ends), 2);
	TestEqual(TEXT("past the last start enters all"), TreadHUDMath::CountEnteredSegments(0.71f, Ends), 4);
	TestEqual(TEXT("no segments"), TreadHUDMath::CountEnteredSegments(0.5f, {}), 0);

	// Stepped: 入ったセグメントの終端まで点灯する。
	TestEqual(TEXT("stepped zero"), TreadHUDMath::QuantizeToSegments(0.0f, Ends), 0.0f);
	TestEqual(TEXT("stepped first"), TreadHUDMath::QuantizeToSegments(0.1f, Ends), 0.2f);
	TestEqual(TEXT("stepped third"), TreadHUDMath::QuantizeToSegments(0.5f, Ends), 0.7f);
	TestEqual(TEXT("stepped without segments is continuous"), TreadHUDMath::QuantizeToSegments(0.33f, {}), 0.33f);

	// 境界の通過: 上昇は小さい順、下降は大きい順に、跨いだ境界をすべて列挙する。
	TArray<TreadHUDMath::FSegmentCrossing> Crossings;
	TreadHUDMath::CollectSegmentCrossings(2, 4, Crossings);
	if (TestEqual(TEXT("two rising crossings"), Crossings.Num(), 2))
	{
		TestEqual(TEXT("rising first index"), Crossings[0].SegmentIndex, 2);
		TestEqual(TEXT("rising last index (blue)"), Crossings[1].SegmentIndex, 3);
		TestTrue(TEXT("rising flag"), Crossings[0].bRising && Crossings[1].bRising);
	}
	TreadHUDMath::CollectSegmentCrossings(4, 3, Crossings);
	if (TestEqual(TEXT("one falling crossing"), Crossings.Num(), 1))
	{
		TestEqual(TEXT("falling index (blue)"), Crossings[0].SegmentIndex, 3);
		TestFalse(TEXT("falling flag"), Crossings[0].bRising);
	}
	TreadHUDMath::CollectSegmentCrossings(3, 3, Crossings);
	TestEqual(TEXT("no crossing without change"), Crossings.Num(), 0);

	// メーター: 割り当てカーブと段階化。
	UTreadHUDMeterWidget* Meter = NewObject<UTreadHUDMeterWidget>();
	Meter->SegmentEndRatios = Ends;
	TestEqual(TEXT("continuous without curve"), Meter->DisplayToFill(Meter->MapToDisplay(0.33f)), 0.33f, Tolerance);
	TestEqual(TEXT("display is clamped"), Meter->MapToDisplay(1.5f), 1.0f);

	Meter->FillMode = ETreadHUDFillMode::Stepped;
	TestEqual(TEXT("stepped meter"), Meter->DisplayToFill(Meter->MapToDisplay(0.33f)), 0.45f, Tolerance);

	// 速度比 0.72（超加速）を青の始端 0.7 に合わせるカーブ。
	UCurveFloat* Curve = NewObject<UCurveFloat>();
	TestTrue(TEXT("curve keys set"), UTreadHUDLibrary::SetLinearCurveKeys(Curve, { FVector2D(1.0f, 1.0f), FVector2D(0.0f, 0.0f), FVector2D(0.72f, 0.7f) }));
	TestEqual(TEXT("curve key count"), Curve->FloatCurve.GetNumKeys(), 3);
	TestFalse(TEXT("null curve is rejected"), UTreadHUDLibrary::SetLinearCurveKeys(nullptr, {}));

	Meter->FillMode = ETreadHUDFillMode::Continuous;
	Meter->ResponseCurve.ExternalCurve = Curve;
	TestEqual(TEXT("curve ignored while disabled"), Meter->MapToDisplay(0.72f), 0.72f, Tolerance);
	Meter->bUseCurve = true;
	TestEqual(TEXT("curve maps overdrive to blue start"), Meter->MapToDisplay(0.72f), 0.7f, Tolerance);
	TestEqual(TEXT("curve is linear between keys"), Meter->MapToDisplay(0.36f), 0.35f, Tolerance);
	TestEqual(TEXT("overdrive enters blue only above the key"),
		TreadHUDMath::CountEnteredSegments(Meter->MapToDisplay(0.73f), Ends), 4);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTreadHUDImageSwitchTest, "MyProject.Impact.HUD.ImageSwitch", TreadHUDTests::TestFlags)

bool FTreadHUDImageSwitchTest::RunTest(const FString& Parameters)
{
	// 閾値の選択: Raw が閾値以上のうち最大のもの。並び順に依存しない。
	TestEqual(TEXT("none below all thresholds"), TreadHUDMath::SelectThresholdIndex(0.5f, { 1.0f, 2.0f }), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("exact threshold"), TreadHUDMath::SelectThresholdIndex(1.0f, { 1.0f, 2.0f }), 0);
	TestEqual(TEXT("highest reached"), TreadHUDMath::SelectThresholdIndex(2.0f, { 2.0f, 0.0f, 1.0f }), 0);
	TestEqual(TEXT("middle reached"), TreadHUDMath::SelectThresholdIndex(1.5f, { 2.0f, 0.0f, 1.0f }), 2);

	// ロックオン（0 / 1 / 2）を 2 状態の画像に割り当てた例。
	UTreadHUDImageSwitchWidget* Switch = NewObject<UTreadHUDImageSwitchWidget>();
	FTreadHUDImageState Locked;
	Locked.Threshold = TreadHUDLockOn::Locked;
	FTreadHUDImageState Assisting;
	Assisting.Threshold = TreadHUDLockOn::Assisting;
	Switch->States = { Assisting, Locked };
	TestEqual(TEXT("no lock shows nothing"), Switch->SelectState(TreadHUDLockOn::None), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("locked state"), Switch->SelectState(TreadHUDLockOn::Locked), 1);
	TestEqual(TEXT("assisting state"), Switch->SelectState(TreadHUDLockOn::Assisting), 0);

	// 数値の表示: 倍率と桁数。
	UTreadHUDTextWidget* Text = NewObject<UTreadHUDTextWidget>();
	Text->Format = INVTEXT("{0} km/h");
	Text->Multiplier = 0.036f;
	TestEqual(TEXT("speed in km/h"), Text->FormatValue(3600.0f).ToString(), FString(TEXT("130 km/h")));
	Text->FractionalDigits = 1;
	TestEqual(TEXT("one fractional digit"), Text->FormatValue(1000.0f).ToString(), FString(TEXT("36.0 km/h")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
