// 踏みつけ加速メカゲーム — 衝突の手応えのカメラシェイク

#include "Feedback/ImpactCameraShake.h"

#include "Shakes/PerlinNoiseCameraShakePattern.h"

namespace
{
	/** 揺れの長さ（秒）。ヒットストップ明けまで残る程度に短くする。 */
	constexpr float ShakeDuration = 0.3f;

	/** 立ち上がりは即座に、収まりは緩やかにする（秒）。 */
	constexpr float ShakeBlendInTime = 0.0f;
	constexpr float ShakeBlendOutTime = 0.15f;

	/** 位置の揺れ（uu）と周波数（Hz）。突進方向の前後より、上下左右の揺れで衝撃を表す。 */
	constexpr float LocationAmplitude = 12.0f;
	constexpr float LocationFrequency = 25.0f;

	/** 回転の揺れ（deg）と周波数（Hz）。 */
	constexpr float RotationAmplitude = 1.5f;
	constexpr float RotationFrequency = 20.0f;
}

UImpactCameraShake::UImpactCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 同じ出来事が続いたときは揺れを重ねず、最新の揺れで上書きする。
	bSingleInstance = true;

	UPerlinNoiseCameraShakePattern* Pattern = CreateDefaultSubobject<UPerlinNoiseCameraShakePattern>(TEXT("Pattern"));
	Pattern->Duration = ShakeDuration;
	Pattern->BlendInTime = ShakeBlendInTime;
	Pattern->BlendOutTime = ShakeBlendOutTime;

	Pattern->Y.Amplitude = LocationAmplitude;
	Pattern->Y.Frequency = LocationFrequency;
	Pattern->Z.Amplitude = LocationAmplitude;
	Pattern->Z.Frequency = LocationFrequency;

	Pattern->Pitch.Amplitude = RotationAmplitude;
	Pattern->Pitch.Frequency = RotationFrequency;
	Pattern->Roll.Amplitude = RotationAmplitude;
	Pattern->Roll.Frequency = RotationFrequency;

	SetRootShakePattern(Pattern);
}
