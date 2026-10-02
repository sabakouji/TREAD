// 踏みつけ加速メカゲーム — 汎用 HUD 部品: 数値の表示

#pragma once

#include "CoreMinimal.h"
#include "UI/HUD/TreadHUDElementWidget.h"
#include "TreadHUDTextWidget.generated.h"

class UTextBlock;

/**
 * 値を書式付きの文字で表示する（例: 速度を「{0} km/h」で）。
 *
 * 表示する数値 = 元の値（Raw）× Multiplier を FractionalDigits 桁に丸めたもの。
 * 表示する数値が変わったときだけ文字を書き換える（毎フレームの書き換えは文字の再レイアウトを招くため）。
 * 文字の部品（ValueText）は WBP のデザイナーで同名の TextBlock を置けばそれを使い、無ければ C++ で作る。
 */
UCLASS()
class MYPROJECT_API UTreadHUDTextWidget : public UTreadHUDElementWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	/** 書式。{0} が数値に置き換わる。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TREAD|HUD")
	FText Format = INVTEXT("{0}");

	/** 元の値に掛ける倍率。uu/s → km/h なら 0.036。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TREAD|HUD")
	float Multiplier = 1.0f;

	/** 小数点以下の桁数。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TREAD|HUD", meta = (ClampMin = "0", ClampMax = "6"))
	int32 FractionalDigits = 0;

	/** 文字の大きさ。C++ で文字の部品を作る場合だけ使う。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TREAD|HUD", meta = (ClampMin = "1"))
	int32 FontSize = 28;

	/** 文字の色。C++ で文字の部品を作る場合だけ使う。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TREAD|HUD")
	FLinearColor TextColor = FLinearColor::White;

	/** デザイナー上で表示する元の値。実行時には使わない。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TREAD|HUD")
	float PreviewRaw = 0.0f;

	/** 元の値から表示する文字を作る。 */
	UFUNCTION(BlueprintPure, Category = "TREAD|HUD")
	FText FormatValue(float Raw) const;

protected:
	virtual void ApplyValue(float Raw, float InDisplayNormalized) override;
	virtual void ApplyPreview(float PreviewNormalized) override;

	UPROPERTY(BlueprintReadOnly, Category = "TREAD|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ValueText;

private:
	/** 元の値を、表示する桁に丸めた数値にする。 */
	double RoundForDisplay(float Raw) const;

	/** 文字の部品を C++ で作ったか。作った場合だけ FontSize・TextColor を反映する。 */
	bool bBuiltText = false;

	bool bHasShown = false;
	double ShownValue = 0.0;
};
