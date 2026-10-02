// 踏みつけ加速メカゲーム — 汎用 HUD 部品: 半円メーター

#include "UI/HUD/TreadHUDMeterWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/HUD/TreadHUDMath.h"
#include "UI/ImpactWidgetHelpers.h"

namespace
{
	/**
	 * 弧マスクのマテリアル（M_HUD_ArcFill）のパラメータ名。
	 * Scripts/Setup_GUI01Assets.py の PARAM_* と一致させること（スクリプトが作成時に検証する）。
	 */
	namespace ArcParameter
	{
		const FName Texture(TEXT("FillTexture"));
		const FName Fill(TEXT("Fill"));
		const FName PivotUV(TEXT("PivotUV"));
		const FName StartAngle(TEXT("StartAngle"));
		const FName SweepAngle(TEXT("SweepAngle"));
		const FName Aspect(TEXT("Aspect"));
	}

	/** 塗りの量を書き換えるとみなす変化量。 */
	constexpr float FillChangeTolerance = 0.0001f;
}

bool UTreadHUDMeterWidget::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();
	EnsureLayers();
	RefreshLayers();
	return bSuperInitialized;
}

float UTreadHUDMeterWidget::MapToDisplay(float Normalized) const
{
	const float Clamped = FMath::Clamp(Normalized, 0.0f, 1.0f);
	if (!bUseCurve)
	{
		return Clamped;
	}

	const FRichCurve* Curve = ResponseCurve.GetRichCurveConst();
	if (!Curve || Curve->GetNumKeys() == 0)
	{
		return Clamped;
	}
	return FMath::Clamp(Curve->Eval(Clamped), 0.0f, 1.0f);
}

float UTreadHUDMeterWidget::DisplayToFill(float Display) const
{
	return FillMode == ETreadHUDFillMode::Stepped
		? TreadHUDMath::QuantizeToSegments(Display, SegmentEndRatios)
		: Display;
}

void UTreadHUDMeterWidget::RefreshLayers()
{
	ApplyTexture(BackgroundImage, BackgroundTexture, BackgroundLayerOffset);
	ApplyFillLayer();
	ApplyTexture(OverlayImage, OverlayTexture, OverlayLayerOffset);
	ApplyTexture(FrameImage, FrameTexture, FrameLayerOffset);
	UpdateOverlayVisibility();
}

void UTreadHUDMeterWidget::ApplyValue(float Raw, float InDisplayNormalized)
{
	const float Display = MapToDisplay(InDisplayNormalized);
	UpdateSegments(Display, true);
	SetFill(DisplayToFill(Display));
}

void UTreadHUDMeterWidget::ApplyPreview(float PreviewNormalized)
{
	// デザイナーで値を変えるたびに呼ばれるため、画像の差し替えもここで反映する。
	RefreshLayers();

	const float Display = MapToDisplay(PreviewNormalized);
	UpdateSegments(Display, false);
	SetFill(DisplayToFill(Display));
}

void UTreadHUDMeterWidget::EnsureLayers()
{
	if (!WidgetTree)
	{
		UE_LOG(LogImpactUI, Error, TEXT("%s has no widget tree; the meter is not built"), *GetName());
		return;
	}

	// デザイナーで部品を置いている場合はそれを使う。
	if (!LayerRoot)
	{
		if (WidgetTree->RootWidget)
		{
			UE_LOG(LogImpactUI, Warning, TEXT("%s: the designer layout has no LayerRoot overlay; it is replaced by the C++ layout"), *GetName());
		}
		LayerRoot = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("LayerRoot"));
		WidgetTree->RootWidget = LayerRoot;
	}

	// 奥から順に重ねる。
	if (!BackgroundImage)
	{
		BackgroundImage = AddLayerImage(TEXT("BackgroundImage"));
	}
	if (!FillImage)
	{
		FillImage = AddLayerImage(TEXT("FillImage"));
	}
	if (!OverlayImage)
	{
		OverlayImage = AddLayerImage(TEXT("OverlayImage"));
	}
	if (!FrameImage)
	{
		FrameImage = AddLayerImage(TEXT("FrameImage"));
	}
}

UImage* UTreadHUDMeterWidget::AddLayerImage(const TCHAR* Name)
{
	UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
	UOverlaySlot* LayerSlot = LayerRoot->AddChildToOverlay(Image);
	LayerSlot->SetHorizontalAlignment(HAlign_Left);
	LayerSlot->SetVerticalAlignment(VAlign_Top);
	return Image;
}

void UTreadHUDMeterWidget::ApplyTexture(UImage* Image, UTexture2D* Texture, const FVector2D& Offset)
{
	if (!Image)
	{
		return;
	}

	if (!Texture)
	{
		Image->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	Image->SetBrushFromTexture(Texture, true);
	Image->SetRenderTranslation(Offset);
	Image->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UTreadHUDMeterWidget::ApplyFillLayer()
{
	if (!FillImage)
	{
		return;
	}

	if (!FillTexture)
	{
		FillImage->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	if (!FillMaterial)
	{
		// 切り抜けないので塗り画像全体がそのまま見える。設定漏れが分かるよう一度だけ知らせる。
		if (!bWarnedMissingMaterial && !IsDesignTime())
		{
			UE_LOG(LogImpactUI, Warning, TEXT("%s: FillMaterial is not set; the fill is shown without the arc mask"), *GetName());
			bWarnedMissingMaterial = true;
		}
		FillMaterialInstance = nullptr;
		ApplyTexture(FillImage, FillTexture, FillLayerOffset);
		return;
	}

	if (!FillMaterialInstance || FillMaterialInstance->Parent != FillMaterial)
	{
		FillMaterialInstance = UMaterialInstanceDynamic::Create(FillMaterial, this);
	}

	const FVector2D TextureSize(FillTexture->GetSizeX(), FillTexture->GetSizeY());
	FillMaterialInstance->SetTextureParameterValue(ArcParameter::Texture, FillTexture);
	FillMaterialInstance->SetVectorParameterValue(ArcParameter::PivotUV, FLinearColor(PivotUV.X, PivotUV.Y, 0.0f, 0.0f));
	FillMaterialInstance->SetScalarParameterValue(ArcParameter::StartAngle, StartAngle);
	FillMaterialInstance->SetScalarParameterValue(ArcParameter::SweepAngle, SweepAngle);
	FillMaterialInstance->SetScalarParameterValue(ArcParameter::Aspect, TextureSize.Y > 0.0f ? TextureSize.X / TextureSize.Y : 1.0f);

	FSlateBrush Brush;
	Brush.SetResourceObject(FillMaterialInstance);
	Brush.ImageSize = TextureSize;
	FillImage->SetBrush(Brush);
	FillImage->SetRenderTranslation(FillLayerOffset);
	FillImage->SetVisibility(ESlateVisibility::HitTestInvisible);

	// 新しいインスタンスにも今の塗りの量を入れ直す。
	const float CurrentFill = FMath::Max(AppliedFill, 0.0f);
	AppliedFill = -1.0f;
	SetFill(CurrentFill);
}

void UTreadHUDMeterWidget::SetFill(float Fill)
{
	if (!FillMaterialInstance || FMath::IsNearlyEqual(Fill, AppliedFill, FillChangeTolerance))
	{
		return;
	}

	AppliedFill = Fill;
	FillMaterialInstance->SetScalarParameterValue(ArcParameter::Fill, Fill);
}

void UTreadHUDMeterWidget::UpdateSegments(float Display, bool bNotify)
{
	const int32 Count = TreadHUDMath::CountEnteredSegments(Display, SegmentEndRatios);
	const int32 Previous = EnteredSegments;
	if (Count == Previous)
	{
		return;
	}

	EnteredSegments = Count;
	UpdateOverlayVisibility();

	// 最初の値は「跨いだ」のではなく初期状態なので通知しない。
	if (!bNotify || Previous == INDEX_NONE)
	{
		return;
	}

	TArray<TreadHUDMath::FSegmentCrossing> Crossings;
	TreadHUDMath::CollectSegmentCrossings(Previous, Count, Crossings);
	for (const TreadHUDMath::FSegmentCrossing& Crossing : Crossings)
	{
		OnThresholdCrossed(Crossing.SegmentIndex, Crossing.bRising);
		OnThresholdCrossedEvent.Broadcast(Crossing.SegmentIndex, Crossing.bRising);
	}
}

void UTreadHUDMeterWidget::UpdateOverlayVisibility()
{
	if (!OverlayImage || !OverlayTexture)
	{
		return;
	}

	const bool bShow = OverlayThresholdSegment != INDEX_NONE && EnteredSegments > OverlayThresholdSegment;
	OverlayImage->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
