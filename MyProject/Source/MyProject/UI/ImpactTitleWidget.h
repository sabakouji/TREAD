// 踏みつけ加速メカゲーム — タイトル画面

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ImpactTitleWidget.generated.h"

class UButton;

/**
 * タイトル画面。「開始」「ゲーム終了」を受け付ける。
 *
 * 配置は C++ で組み立てる（ImpactWidgetHelpers.h 参照）。Blueprint 派生（WBP_Title）では
 * 文言・色・文字サイズを調整する。デザイナーで配置を置いても C++ の配置に置き換わる。
 * 画面全体を不透明な背景で覆うため、タイトル用マップには何も配置しなくてよい。
 */
UCLASS()
class MYPROJECT_API UImpactTitleWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(EditDefaultsOnly, Category = "Style|Text")
	FText GameTitle = NSLOCTEXT("ImpactTitle", "GameTitle", "IMPACT VEHICLE");

	UPROPERTY(EditDefaultsOnly, Category = "Style|Text")
	FText Subtitle = NSLOCTEXT("ImpactTitle", "Subtitle", "SOLO MODE PROTOTYPE");

	UPROPERTY(EditDefaultsOnly, Category = "Style|Font")
	int32 TitleFontSize = 80;

	UPROPERTY(EditDefaultsOnly, Category = "Style|Font")
	int32 SubtitleFontSize = 24;

	UPROPERTY(EditDefaultsOnly, Category = "Style|Font")
	int32 ButtonFontSize = 30;

	UPROPERTY(EditDefaultsOnly, Category = "Style|Color")
	FLinearColor BackgroundColor = FLinearColor(0.02f, 0.03f, 0.06f, 1.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Style|Color")
	FLinearColor TitleColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, Category = "Style|Color")
	FLinearColor SubtitleColor = FLinearColor(0.6f, 0.75f, 1.0f, 1.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Style|Color")
	FLinearColor ButtonColor = FLinearColor(0.12f, 0.16f, 0.28f, 1.0f);

	/** ボタンの幅（px）。全ボタンの幅をそろえる。 */
	UPROPERTY(EditDefaultsOnly, Category = "Style|Layout", meta = (ClampMin = "1.0"))
	float ButtonWidth = 320.0f;

	/** 見出しとボタンの間隔（px）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Style|Layout", meta = (ClampMin = "0.0"))
	float SectionSpacing = 64.0f;

	/** ボタン同士の間隔（px）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Style|Layout", meta = (ClampMin = "0.0"))
	float ButtonSpacing = 16.0f;

private:
	void BuildLayout();

	UFUNCTION()
	void HandleStartClicked();

	UFUNCTION()
	void HandleQuitClicked();

	/** 押された後はボタンを無効にし、遷移中の二重操作を防ぐ。 */
	void DisableButtons();

	UPROPERTY(Transient)
	TObjectPtr<UButton> StartButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> QuitButton;
};
