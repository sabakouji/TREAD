// 踏みつけ加速メカゲーム — 汎用 HUD 部品の補助関数

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TreadHUDLibrary.generated.h"

class UCurveFloat;

/** 汎用 HUD 部品の補助関数。エディタ自動化（Python）から呼ぶ。 */
UCLASS()
class MYPROJECT_API UTreadHUDLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * カーブのキーを置き換える。キー間は直線で補間する。
	 * UE の Python からはカーブのキーを直接編集できないため、メーターの割り当てカーブの生成に使う
	 * （Scripts/Setup_GUI01Assets.py が DataAsset の閾値から計算したキーを渡す）。
	 * @param Keys  (入力, 出力) の組。入力の昇順でなくてもよい。
	 * @return 設定できたか（Curve が null なら false）。
	 */
	UFUNCTION(BlueprintCallable, Category = "TREAD|HUD")
	static bool SetLinearCurveKeys(UCurveFloat* Curve, const TArray<FVector2D>& Keys);
};
