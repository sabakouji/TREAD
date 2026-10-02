// 踏みつけ加速メカゲーム — 破壊演出の調整アセット

#include "Tuning/FeedbackTuningDataAsset.h"

#include "Feedback/ImpactCameraShake.h"

UFeedbackTuningDataAsset::UFeedbackTuningDataAsset()
{
	ShakeClass = UImpactCameraShake::StaticClass();
}

const FImpactFeedbackPreset* UFeedbackTuningDataAsset::FindPreset(EVehicleGameplayEvent Event) const
{
	switch (Event)
	{
	case EVehicleGameplayEvent::DestroyedSmall:
		return &SmallDestroyed;
	case EVehicleGameplayEvent::DestroyedLarge:
		return &LargeDestroyed;
	case EVehicleGameplayEvent::StructureDamaged:
		return &StructureDamaged;
	case EVehicleGameplayEvent::GoalHit:
		return &GoalHit;
	case EVehicleGameplayEvent::Stomp:
	case EVehicleGameplayEvent::Crash:
	default:
		return nullptr;
	}
}

float UFeedbackTuningDataAsset::ToDilatedTimerDelay(float RealDuration, float TimeDilation)
{
	return FMath::Max(RealDuration, 0.0f) * FMath::Clamp(TimeDilation, KINDA_SMALL_NUMBER, 1.0f);
}

FVector UFeedbackTuningDataAsset::ComputeDebrisVelocity(const FVector& ImpactVelocity) const
{
	const FVector Horizontal(ImpactVelocity.X, ImpactVelocity.Y, 0.0f);
	return Horizontal * DebrisSpeedRatio;
}
