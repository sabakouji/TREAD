// 踏みつけ加速メカゲーム — 汎用 HUD 部品の計算

#pragma once

#include "CoreMinimal.h"

/**
 * 汎用 HUD 部品の表示の計算。ウィジェットや UObject に依存させず、自動テストで直接確かめられるようにする。
 *
 * セグメントは SegmentEndRatios（各セグメントの終端の比率。昇順）で表す。
 * セグメント k の始端は k == 0 なら 0、それ以外は SegmentEndRatios[k - 1]。
 * 値が始端を超えたとき、そのセグメントに「入った」とみなす。
 */
namespace TreadHUDMath
{
	/** Raw を Min〜Max から 0〜1 に写す。Min >= Max のときは 0 を返す（0 除算をしない）。 */
	MYPROJECT_API float Normalize(float Raw, float Min, float Max);

	/** 表示値を目標値へ追従させる。InterpSpeed が 0 以下なら即座に目標値にする。 */
	MYPROJECT_API float StepToward(float Current, float Target, float DeltaTime, float InterpSpeed);

	/** 値が入っているセグメントの数（0〜セグメント数）。値が 0 以下なら 0。 */
	MYPROJECT_API int32 CountEnteredSegments(float Normalized, TConstArrayView<float> SegmentEndRatios);

	/**
	 * セグメント単位で点灯させたときの塗りの量。入っている最後のセグメントの終端まで点灯する。
	 * セグメントが無ければ値をそのまま返す。
	 */
	MYPROJECT_API float QuantizeToSegments(float Normalized, TConstArrayView<float> SegmentEndRatios);

	/** セグメントの境界の通過 1 件。 */
	struct FSegmentCrossing
	{
		/** 始端を通過したセグメント。 */
		int32 SegmentIndex = INDEX_NONE;

		/** 入った（true）か、出た（false）か。 */
		bool bRising = false;
	};

	/**
	 * 入っているセグメントの数の変化から、通過した境界を通過した順に列挙する。
	 * 一度に複数の境界を跨いだ場合はすべて列挙する（上昇は小さい順、下降は大きい順）。
	 */
	MYPROJECT_API void CollectSegmentCrossings(int32 PreviousCount, int32 CurrentCount, TArray<FSegmentCrossing>& OutCrossings);

	/** Raw が閾値以上になっている閾値のうち、最大のものの添字。該当が無ければ INDEX_NONE。 */
	MYPROJECT_API int32 SelectThresholdIndex(float Raw, TConstArrayView<float> Thresholds);
}
