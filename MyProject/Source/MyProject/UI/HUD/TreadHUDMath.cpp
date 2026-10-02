// 踏みつけ加速メカゲーム — 汎用 HUD 部品の計算

#include "UI/HUD/TreadHUDMath.h"

namespace TreadHUDMath
{
	float Normalize(float Raw, float Min, float Max)
	{
		if (Min >= Max)
		{
			return 0.0f;
		}
		return FMath::Clamp((Raw - Min) / (Max - Min), 0.0f, 1.0f);
	}

	float StepToward(float Current, float Target, float DeltaTime, float InterpSpeed)
	{
		if (InterpSpeed <= 0.0f)
		{
			return Target;
		}
		return FMath::FInterpTo(Current, Target, DeltaTime, InterpSpeed);
	}

	int32 CountEnteredSegments(float Normalized, TConstArrayView<float> SegmentEndRatios)
	{
		int32 Count = 0;
		for (int32 Index = 0; Index < SegmentEndRatios.Num(); ++Index)
		{
			const float SegmentStart = Index == 0 ? 0.0f : SegmentEndRatios[Index - 1];
			if (Normalized <= SegmentStart)
			{
				break;
			}
			++Count;
		}
		return Count;
	}

	float QuantizeToSegments(float Normalized, TConstArrayView<float> SegmentEndRatios)
	{
		if (SegmentEndRatios.IsEmpty())
		{
			return Normalized;
		}

		const int32 Count = CountEnteredSegments(Normalized, SegmentEndRatios);
		return Count == 0 ? 0.0f : SegmentEndRatios[Count - 1];
	}

	void CollectSegmentCrossings(int32 PreviousCount, int32 CurrentCount, TArray<FSegmentCrossing>& OutCrossings)
	{
		OutCrossings.Reset();
		for (int32 Index = PreviousCount; Index < CurrentCount; ++Index)
		{
			OutCrossings.Add({ Index, true });
		}
		for (int32 Index = PreviousCount - 1; Index >= CurrentCount; --Index)
		{
			OutCrossings.Add({ Index, false });
		}
	}

	int32 SelectThresholdIndex(float Raw, TConstArrayView<float> Thresholds)
	{
		int32 Selected = INDEX_NONE;
		for (int32 Index = 0; Index < Thresholds.Num(); ++Index)
		{
			if (Raw >= Thresholds[Index] && (Selected == INDEX_NONE || Thresholds[Index] > Thresholds[Selected]))
			{
				Selected = Index;
			}
		}
		return Selected;
	}
}
