// 踏みつけ加速メカゲーム — 汎用 HUD 部品: 値による画像の切り替え

#pragma once

#include "CoreMinimal.h"
#include "UI/HUD/TreadHUDElementWidget.h"
#include "TreadHUDImageSwitchWidget.generated.h"

class UImage;

/**
 * 値に応じて画像を切り替える（ロックオン・行動不能の表示など）。
 *
 * 元の値（Raw）が Threshold 以上の状態のうち、閾値が最大の状態の画像を出す。該当が無ければ何も出さない。
 * 表示する状態が変わったときだけ画像を書き換える。
 * 画像の部品（StateImage）は WBP のデザイナーで同名の Image を置けばそれを使い、無ければ C++ で作る。
 */
UCLASS()
class MYPROJECT_API UTreadHUDImageSwitchWidget : public UTreadHUDElementWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	/** 状態の一覧。並び順は問わない。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TREAD|HUD")
	TArray<FTreadHUDImageState> States;

	/** デザイナー上で表示する元の値。実行時には使わない。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TREAD|HUD")
	float PreviewRaw = 0.0f;

	/** 元の値から表示する状態の添字を求める。該当が無ければ INDEX_NONE。 */
	UFUNCTION(BlueprintPure, Category = "TREAD|HUD")
	int32 SelectState(float Raw) const;

protected:
	virtual void ApplyValue(float Raw, float InDisplayNormalized) override;
	virtual void ApplyPreview(float PreviewNormalized) override;

	UPROPERTY(BlueprintReadOnly, Category = "TREAD|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UImage> StateImage;

private:
	/** 状態の画像を表示する。bForce なら同じ状態でも書き直す（デザイナーでの画像の差し替え用）。 */
	void ShowState(int32 StateIndex, bool bForce);

	/** まだ一度も表示を書いていない状態。INDEX_NONE（該当なしで非表示）と区別し、初回は必ず書き換える。 */
	static constexpr int32 NotShownYet = INDEX_NONE - 1;

	/** 表示中の状態。INDEX_NONE は該当なしで何も出していない状態。 */
	int32 ShownState = NotShownYet;
};
