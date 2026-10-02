// 踏みつけ加速メカゲーム — 汎用 HUD 部品: 半円メーター

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "UI/HUD/TreadHUDElementWidget.h"
#include "TreadHUDMeterWidget.generated.h"

class UImage;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UOverlay;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHUDThresholdCrossed, int32, SegmentIndex, bool, bRising);

/**
 * 弧を描いて伸び縮みするメーター。画像・弧の角度・伸び方はすべて詳細パネルで設定する。
 *
 * 画像は奥から 未到達（Background）→ 到達（Fill）→ 演出（Overlay）→ 外枠（Frame）の順に重ねる。
 * 到達部分は FillMaterial（M_HUD_ArcFill）で FillTexture を角度で切り抜いて表す。色は画像に塗ってあるものを使う。
 * 画像の大きさや基準位置がそろっていない場合は、各レイヤーの LayerOffset（px）で合わせる。
 *
 * 内部の部品（LayerRoot・各 Image）は WBP のデザイナーで同名の部品を置けばそれを使い、
 * 置かれていなければ C++ で組み立てる（Python から WBP の中身を組めないため）。
 */
UCLASS()
class MYPROJECT_API UTreadHUDMeterWidget : public UTreadHUDElementWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	/** 外枠（最前面）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TREAD|HUD|Layers")
	TObjectPtr<UTexture2D> FrameTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TREAD|HUD|Layers")
	FVector2D FrameLayerOffset = FVector2D::ZeroVector;

	/** 未到達部分（最背面）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TREAD|HUD|Layers")
	TObjectPtr<UTexture2D> BackgroundTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TREAD|HUD|Layers")
	FVector2D BackgroundLayerOffset = FVector2D::ZeroVector;

	/** 到達部分。FillMaterial で角度に切り抜いて表示する。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TREAD|HUD|Layers")
	TObjectPtr<UTexture2D> FillTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TREAD|HUD|Layers")
	FVector2D FillLayerOffset = FVector2D::ZeroVector;

	/** 演出（超加速時の発光など）。任意。OverlayThresholdSegment のセグメントに入っている間だけ表示する。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TREAD|HUD|Layers")
	TObjectPtr<UTexture2D> OverlayTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TREAD|HUD|Layers")
	FVector2D OverlayLayerOffset = FVector2D::ZeroVector;

	/** 到達部分を切り抜くマテリアル（M_HUD_ArcFill）。パラメータ名は TreadHUDMeterWidget.cpp を参照。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TREAD|HUD|Arc")
	TObjectPtr<UMaterialInterface> FillMaterial;

	/** 弧の中心の UV（FillTexture 上の円の中心。画像の中心ではない。0〜1 の外でもよい）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TREAD|HUD|Arc")
	FVector2D PivotUV = FVector2D(0.5f, 0.5f);

	/** 0% の角度（度。右 = 0、反時計回りが正）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TREAD|HUD|Arc")
	float StartAngle = 180.0f;

	/** 100% までの回転量（度。負で時計回り）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TREAD|HUD|Arc")
	float SweepAngle = -180.0f;

	/** 伸び方。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TREAD|HUD|Fill")
	ETreadHUDFillMode FillMode = ETreadHUDFillMode::Continuous;

	/**
	 * 各セグメントの終端の比率（昇順、最後は 1）。Stepped の点灯単位と、OnThresholdCrossed の境界に使う。
	 * 画像のセグメントの区切りの角度に合わせる。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TREAD|HUD|Fill")
	TArray<float> SegmentEndRatios;

	/** 値 → 表示の割り当てカーブを使うか。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TREAD|HUD|Fill")
	bool bUseCurve = false;

	/** 値（0〜1）→ 表示（0〜1）の割り当て。低速域を広く見せる、閾値をセグメントの区切りに合わせる等に使う。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TREAD|HUD|Fill", meta = (EditCondition = "bUseCurve"))
	FRuntimeFloatCurve ResponseCurve;

	/** このセグメントに入っている間 OverlayTexture を表示する。-1 で表示しない。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TREAD|HUD|Fill", meta = (ClampMin = "-1"))
	int32 OverlayThresholdSegment = INDEX_NONE;

	/** セグメントの境界を跨いだときの通知（C++・他の Blueprint から購読する用）。 */
	UPROPERTY(BlueprintAssignable, Category = "TREAD|HUD")
	FOnHUDThresholdCrossed OnThresholdCrossedEvent;

	/** 値（0〜1）を表示上の位置（0〜1）に写す。カーブを適用した値で、段階化の前。 */
	UFUNCTION(BlueprintPure, Category = "TREAD|HUD")
	float MapToDisplay(float Normalized) const;

	/** 表示上の位置から塗りの量を求める。Stepped ならセグメント単位に丸める。 */
	UFUNCTION(BlueprintPure, Category = "TREAD|HUD")
	float DisplayToFill(float Display) const;

	/** 画像・マテリアルの設定を表示に反映し直す。実行中にプロパティを変えたときに呼ぶ。 */
	UFUNCTION(BlueprintCallable, Category = "TREAD|HUD")
	void RefreshLayers();

protected:
	/**
	 * セグメントの境界を跨いだときに呼ばれる。揺れ・発光などの演出を WBP で付ける。
	 * @param SegmentIndex  始端を跨いだセグメント（0 始まり）。超加速域が 4 つ目なら 3。
	 * @param bRising       入った（true）か、出た（false）か。
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "TREAD|HUD")
	void OnThresholdCrossed(int32 SegmentIndex, bool bRising);

	virtual void ApplyValue(float Raw, float InDisplayNormalized) override;
	virtual void ApplyPreview(float PreviewNormalized) override;

	UPROPERTY(BlueprintReadOnly, Category = "TREAD|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UOverlay> LayerRoot;

	UPROPERTY(BlueprintReadOnly, Category = "TREAD|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UImage> BackgroundImage;

	UPROPERTY(BlueprintReadOnly, Category = "TREAD|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UImage> FillImage;

	UPROPERTY(BlueprintReadOnly, Category = "TREAD|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UImage> OverlayImage;

	UPROPERTY(BlueprintReadOnly, Category = "TREAD|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UImage> FrameImage;

private:
	/** 内部の部品が WBP に置かれていなければ組み立てる。 */
	void EnsureLayers();

	/** 画像 1 枚を重ね合わせに加える。 */
	UImage* AddLayerImage(const TCHAR* Name);

	/** テクスチャを画像に設定する。未設定なら隠す。 */
	static void ApplyTexture(UImage* Image, UTexture2D* Texture, const FVector2D& Offset);

	/** 到達部分の画像に弧マスクのマテリアルを設定する。 */
	void ApplyFillLayer();

	/** 塗りの量を設定する。 */
	void SetFill(float Fill);

	/** 入っているセグメントの数を更新し、境界を跨いでいれば通知する。初回は通知しない。 */
	void UpdateSegments(float Display, bool bNotify);

	/** 演出の画像の表示を、入っているセグメントに合わせる。 */
	void UpdateOverlayVisibility();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FillMaterialInstance;

	/** 入っているセグメントの数。INDEX_NONE はまだ値を受け取っていない状態。 */
	int32 EnteredSegments = INDEX_NONE;

	/** 最後に設定した塗りの量。変わったときだけマテリアルに書き込む。 */
	float AppliedFill = -1.0f;

	/** 弧マスクのマテリアルが無いことを警告したか（毎回出さない）。 */
	bool bWarnedMissingMaterial = false;
};
