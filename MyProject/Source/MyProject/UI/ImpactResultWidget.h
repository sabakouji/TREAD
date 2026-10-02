// 踏みつけ加速メカゲーム — リザルト画面

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/ImpactGameState.h"
#include "Score/ImpactScoreTypes.h"
#include "ImpactResultWidget.generated.h"

class UButton;
class UScoreTuningDataAsset;
class UTextBlock;
class UVerticalBox;

/**
 * リザルト画面。結果・得点の内訳・最終得点・経過時間・最高速度を表示し、
 * 「リトライ」「タイトルへ戻る」を受け付ける。
 *
 * 配置は C++ で組み立てる（ImpactWidgetHelpers.h 参照）。Blueprint 派生（WBP_Result）では
 * 色・文字サイズ・寸法を調整する。デザイナーで配置を置いても C++ の配置に置き換わる。
 */
UCLASS()
class MYPROJECT_API UImpactResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 結果を表示する。
	 * @param ScoreTuning  配点。未設定の場合は内訳の回数だけを表示し、点数は表示しない。
	 */
	void ShowResult(EImpactMatchResult Result, const FImpactScoreTally& Tally,
		const UScoreTuningDataAsset* ScoreTuning, float ElapsedSeconds);

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(EditDefaultsOnly, Category = "Style|Font")
	int32 TitleFontSize = 72;

	UPROPERTY(EditDefaultsOnly, Category = "Style|Font")
	int32 RowFontSize = 24;

	UPROPERTY(EditDefaultsOnly, Category = "Style|Font")
	int32 FinalFontSize = 44;

	UPROPERTY(EditDefaultsOnly, Category = "Style|Font")
	int32 InfoFontSize = 22;

	UPROPERTY(EditDefaultsOnly, Category = "Style|Font")
	int32 ButtonFontSize = 28;

	/** ゲーム画面の上に重ねる背景の色。 */
	UPROPERTY(EditDefaultsOnly, Category = "Style|Color")
	FLinearColor BackdropColor = FLinearColor(0.01f, 0.01f, 0.02f, 0.82f);

	UPROPERTY(EditDefaultsOnly, Category = "Style|Color")
	FLinearColor TextColor = FLinearColor::White;

	/** 内訳の補足（回数 × 配点）の色。 */
	UPROPERTY(EditDefaultsOnly, Category = "Style|Color")
	FLinearColor SubTextColor = FLinearColor(0.7f, 0.7f, 0.75f, 1.0f);

	/** 時間切れの見出しの色。 */
	UPROPERTY(EditDefaultsOnly, Category = "Style|Color")
	FLinearColor TimeUpColor = FLinearColor(1.0f, 0.85f, 0.3f, 1.0f);

	/** 減点の色。 */
	UPROPERTY(EditDefaultsOnly, Category = "Style|Color")
	FLinearColor PenaltyColor = FLinearColor(1.0f, 0.45f, 0.4f, 1.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Style|Color")
	FLinearColor ButtonColor = FLinearColor(0.12f, 0.16f, 0.28f, 1.0f);

	/** 内訳の表の幅（px）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Style|Layout", meta = (ClampMin = "1.0"))
	float PanelWidth = 640.0f;

	/** 内訳の「回数 × 配点」の列の幅（px）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Style|Layout", meta = (ClampMin = "1.0"))
	float DetailColumnWidth = 200.0f;

	/** 内訳の点数の列の幅（px）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Style|Layout", meta = (ClampMin = "1.0"))
	float PointsColumnWidth = 140.0f;

	/** 見出し・内訳・最終得点・ボタンの各まとまりの間隔（px）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Style|Layout", meta = (ClampMin = "0.0"))
	float SectionSpacing = 24.0f;

	/** ボタン同士の間隔（px）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Style|Layout", meta = (ClampMin = "0.0"))
	float ButtonSpacing = 24.0f;

private:
	void BuildLayout();

	/** 内訳の 1 行を追加する。bShowPoints が false なら点数の列を空欄にする。 */
	void AddRow(const FText& Label, const FText& Detail, int32 Points, bool bShowPoints);

	/** 結果に応じた見出しの色。 */
	FLinearColor GetResultColor(EImpactMatchResult Result) const;

	UFUNCTION()
	void HandleRetryClicked();

	UFUNCTION()
	void HandleTitleClicked();

	/** 押された後はボタンを無効にし、遷移中の二重操作を防ぐ。 */
	void DisableButtons();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ResultText;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> RowsBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FinalText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InfoText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> RetryButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> TitleButton;
};
