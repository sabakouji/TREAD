// 踏みつけ加速メカゲーム — 汎用 HUD 部品の補助関数

#include "UI/HUD/TreadHUDLibrary.h"

#include "Curves/CurveFloat.h"

bool UTreadHUDLibrary::SetLinearCurveKeys(UCurveFloat* Curve, const TArray<FVector2D>& Keys)
{
	if (!Curve)
	{
		return false;
	}

	Curve->Modify();
	FRichCurve& RichCurve = Curve->FloatCurve;
	RichCurve.Reset();
	for (const FVector2D& Key : Keys)
	{
		const FKeyHandle Handle = RichCurve.AddKey(Key.X, Key.Y);
		RichCurve.SetKeyInterpMode(Handle, RCIM_Linear);
	}
	Curve->MarkPackageDirty();
	return true;
}
