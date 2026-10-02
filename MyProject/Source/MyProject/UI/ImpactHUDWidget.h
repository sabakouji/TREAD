// 踏みつけ加速メカゲーム — プレイ中の画面表示

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ImpactHUDWidget.generated.h"

class AImpactGameState;
class AImpactVehiclePawn;
class UImage;
class UOverlay;
class UProgressBar;
class USizeBox;
class UTextBlock;
class UWidget;

/**
 * プレイ中の画面表示（残り時間・得点・ゴール耐久値・自滅回数・速度ゲージ・カウントダウン）。
 *
 * 配置は C++ で組み立てる（ImpactWidgetHelpers.h 参照）。Blueprint 派生（WBP_HUD）では
 * 色・文字サイズ・寸法を調整する。デザイナーで配置を置いても C++ の配置に置き換わる。
 *
 * 表示する値は GameState・得点の集計・自機から毎フレーム読むが、表示の書き換え（SetText など）は
 * 値が変わったときだけ行う。毎フレームの書き換えは文字の再レイアウトを招くため。
 * デバッグ HUD（左上）と重ならないよう、上中央・右上・中央・下中央に配置する。
 */
UCLASS()
class MYPROJECT_API UImpactHUDWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(EditDefaultsOnly, Category = "Style|Font")
	int32 TimeFontSize = 40;

	UPROPERTY(EditDefaultsOnly, Category = "Style|Font")
	int32 ScoreFontSize = 28;

	UPROPERTY(EditDefaultsOnly, Category = "Style|Font")
	int32 InfoFontSize = 22;

	UPROPERTY(EditDefaultsOnly, Category = "Style|Font")
	int32 CountdownFontSize = 120;

	UPROPERTY(EditDefaultsOnly, Category = "Style|Font")
	int32 SpeedFontSize = 22;

	UPROPERTY(EditDefaultsOnly, Category = "Style|Font")
	int32 MarkerLabelFontSize = 14;

	UPROPERTY(EditDefaultsOnly, Category = "Style|Color")
	FLinearColor TextColor = FLinearColor::White;

	/** 残り時間が少ないときなど、注意を促す表示の色。 */
	UPROPERTY(EditDefaultsOnly, Category = "Style|Color")
	FLinearColor WarnColor = FLinearColor(1.0f, 0.45f, 0.3f, 1.0f);

	/** 何も壊せない速度のゲージの色。 */
	UPROPERTY(EditDefaultsOnly, Category = "Style|Color")
	FLinearColor GaugeNoneColor = FLinearColor(0.8f, 0.8f, 0.8f, 1.0f);

	/** 小物を壊せる速度のゲージの色。 */
	UPROPERTY(EditDefaultsOnly, Category = "Style|Color")
	FLinearColor GaugeSmallColor = FLinearColor(1.0f, 0.85f, 0.2f, 1.0f);

	/** 壁・建物（とゴール）を壊せる速度のゲージの色。 */
	UPROPERTY(EditDefaultsOnly, Category = "Style|Color")
	FLinearColor GaugeLargeColor = FLinearColor(1.0f, 0.4f, 0.15f, 1.0f);

	/** 超加速（相手の機体を破壊できる状態）のゲージの色。 */
	UPROPERTY(EditDefaultsOnly, Category = "Style|Color")
	FLinearColor GaugeOverdriveColor = FLinearColor(1.0f, 0.85f, 0.4f, 1.0f);

	/** 狙われていることを示す印の色。 */
	UPROPERTY(EditDefaultsOnly, Category = "Style|Color")
	FLinearColor LockWarningColor = FLinearColor(1.0f, 0.2f, 0.2f, 1.0f);

	/** 自分が捕捉している相手に付けるレティクルの色（誘導補助が効いていないとき）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Style|Color")
	FLinearColor LockOnReticleIdleColor = FLinearColor::White;

	/** 誘導補助が効いているときのレティクルの色。 */
	UPROPERTY(EditDefaultsOnly, Category = "Style|Color")
	FLinearColor LockOnReticleActiveColor = FLinearColor(1.0f, 0.85f, 0.1f, 1.0f);

	/** 破壊閾値の目印の色。 */
	UPROPERTY(EditDefaultsOnly, Category = "Style|Color")
	FLinearColor MarkerColor = FLinearColor::White;

	/**
	 * 下中央の速度ゲージ（横棒・数値・閾値の目印）を出すか。
	 * 速度を対戦 HUD（WBP_BattleHUD）の半円メーターで見せる場合は false にする（GUI-01）。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Style|Layout")
	bool bShowSpeedGauge = true;

	/** 速度ゲージの幅（px）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Style|Layout", meta = (ClampMin = "1.0"))
	float GaugeWidth = 640.0f;

	/** 速度ゲージの高さ（px）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Style|Layout", meta = (ClampMin = "1.0"))
	float GaugeHeight = 22.0f;

	/** 破壊閾値の目印の幅（px）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Style|Layout", meta = (ClampMin = "1.0"))
	float MarkerWidth = 3.0f;

	/** 狙われている方向を示す印の、画面中央からの距離（px）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Style|Layout", meta = (ClampMin = "0.0"))
	float LockWarningRadius = 190.0f;

	/** 狙われている方向を示す印の大きさ（px）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Style|Layout")
	FVector2D LockWarningSize = FVector2D(56.0f, 8.0f);

	/** 捕捉している相手に付けるレティクルの一辺（px）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Style|Layout", meta = (ClampMin = "1.0"))
	float LockOnReticleSize = 72.0f;

	/** レティクルの四隅のカギの長さ（px）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Style|Layout", meta = (ClampMin = "1.0"))
	float LockOnReticleCornerLength = 18.0f;

	/** レティクルの線の太さ（px）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Style|Layout", meta = (ClampMin = "1.0"))
	float LockOnReticleThickness = 3.0f;

	/** 画面端からの余白（px）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Style|Layout")
	FVector2D ScreenMargin = FVector2D(32.0f, 24.0f);

	/** カウントダウン表示の、画面中央からの縦のずれ（px）。負の値で上へ。 */
	UPROPERTY(EditDefaultsOnly, Category = "Style|Layout")
	float CountdownOffsetY = -120.0f;

	/** 開始直後に「GO!」を表示し続ける時間（秒）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Style|Timing", meta = (ClampMin = "0.0"))
	float GoSignalDuration = 1.0f;

	/** 残り時間がこの秒数以下になったら警告色にする。 */
	UPROPERTY(EditDefaultsOnly, Category = "Style|Timing", meta = (ClampMin = "0.0"))
	float LowTimeWarningSeconds = 30.0f;

private:
	void BuildLayout();

	/** 破壊閾値の目印を 1 本作り、ゲージに重ねる。 */
	UImage* AddGaugeMarker(UOverlay& Gauge);

	/** 目印の数値を 1 つ作り、ゲージの下の段に置く。 */
	UTextBlock* AddMarkerLabel(UOverlay& LabelRow);

	void UpdateMatchInfo(const AImpactGameState* State);
	void UpdateScore(const AImpactGameState* State);
	void UpdateSpeedGauge(const AImpactVehiclePawn* Vehicle);

	/**
	 * 狙われている方向を画面中央の周りに示す（仕様 3-2 / 5章）。
	 * 攻撃入力がないパッシブ方式では、狙われていることが読めないと回避の判断ができない。
	 */
	void UpdateLockWarning(const AImpactVehiclePawn* Vehicle);

	/**
	 * 自分が捕捉している相手の画面位置にレティクルを出す（仕様 3章の確認用）。
	 * 誘導補助が効いている間は色を変え、捕捉とアシストの状態を画面から読めるようにする。
	 * 相手が背後や画面外にいる場合は出さない。
	 */
	void UpdateLockOnReticle(const AImpactVehiclePawn* Vehicle);

	/** レティクルの四隅のカギを構成する線を 1 本作り、枠に重ねる。 */
	void AddReticleBar(UOverlay& Frame, EHorizontalAlignment Horizontal, EVerticalAlignment Vertical, const FVector2D& Size);

	/** 目印とその数値を、ゲージ上の Ratio（0〜1）の位置へ動かす。 */
	void PlaceMarker(UWidget& Marker, UTextBlock& Label, float Ratio) const;

	/**
	 * 前回表示した値。値が変わったときだけ表示を書き換えるために保持する。
	 * 初回は必ず書き換わるよう「未表示」（int32 の最小値）で初期化する。意味の詳細は ImpactHUDWidget.cpp の定数を参照。
	 */
	struct FShownValues
	{
		int32 ClockSeconds = TNumericLimits<int32>::Min();
		int32 TimeWarning = TNumericLimits<int32>::Min();
		int32 CrashCount = TNumericLimits<int32>::Min();
		int32 Score = TNumericLimits<int32>::Min();
		int32 GoalDurability = TNumericLimits<int32>::Min();
		int32 GoalMaxDurability = TNumericLimits<int32>::Min();
		int32 Countdown = TNumericLimits<int32>::Min();
		int32 Speed = TNumericLimits<int32>::Min();
		int32 FillRank = TNumericLimits<int32>::Min();
		int32 SmallThreshold = TNumericLimits<int32>::Min();
		int32 LargeThreshold = TNumericLimits<int32>::Min();
		int32 OverdriveThreshold = TNumericLimits<int32>::Min();
		int32 Overdrive = TNumericLimits<int32>::Min();
		int32 LockWarning = TNumericLimits<int32>::Min();
		int32 LockOnReticle = TNumericLimits<int32>::Min();
	};

	FShownValues Shown;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TimeText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ScoreText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GoalText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CrashText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CountdownText;

	/** 速度ゲージ一式（数値・横棒・目印）をまとめた枠。bShowSpeedGauge が false のとき隠す。 */
	UPROPERTY(Transient)
	TObjectPtr<UWidget> SpeedGauge;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SpeedText;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> SpeedBar;

	UPROPERTY(Transient)
	TObjectPtr<UImage> SmallMarker;

	UPROPERTY(Transient)
	TObjectPtr<UImage> LargeMarker;

	UPROPERTY(Transient)
	TObjectPtr<UImage> OverdriveMarker;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SmallMarkerLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LargeMarkerLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> OverdriveMarkerLabel;

	/** 超加速に達していることを示す表示。 */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> OverdriveText;

	/** 狙われている方向を示す印。画面中央の周りを回る。 */
	UPROPERTY(Transient)
	TObjectPtr<UImage> LockWarningMarker;

	/** 狙われていることを示す文字。 */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LockWarningText;

	/** 捕捉している相手に付けるレティクル。位置は毎フレーム相手の画面位置へ動かす。 */
	UPROPERTY(Transient)
	TObjectPtr<USizeBox> LockOnReticle;

	/** レティクルを構成する線。色の切り替えに使う。 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> ReticleBars;
};
