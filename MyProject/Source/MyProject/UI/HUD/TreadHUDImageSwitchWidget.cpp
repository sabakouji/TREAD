// 踏みつけ加速メカゲーム — 汎用 HUD 部品: 値による画像の切り替え

#include "UI/HUD/TreadHUDImageSwitchWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "UI/HUD/TreadHUDMath.h"
#include "UI/ImpactWidgetHelpers.h"

namespace
{
	/** 状態の数の想定上限。これを超えてもヒープに確保するだけで動作は変わらない。 */
	constexpr int32 TypicalStateCount = 8;
}

bool UTreadHUDImageSwitchWidget::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();

	if (WidgetTree && !StateImage)
	{
		if (WidgetTree->RootWidget)
		{
			UE_LOG(LogImpactUI, Warning, TEXT("%s: the designer layout has no StateImage; it is replaced by the C++ layout"), *GetName());
		}
		StateImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("StateImage"));
		WidgetTree->RootWidget = StateImage;
	}
	if (StateImage && ShownState == NotShownYet)
	{
		StateImage->SetVisibility(ESlateVisibility::Collapsed);
	}
	return bSuperInitialized;
}

int32 UTreadHUDImageSwitchWidget::SelectState(float Raw) const
{
	TArray<float, TInlineAllocator<TypicalStateCount>> Thresholds;
	Thresholds.Reserve(States.Num());
	for (const FTreadHUDImageState& State : States)
	{
		Thresholds.Add(State.Threshold);
	}
	return TreadHUDMath::SelectThresholdIndex(Raw, Thresholds);
}

void UTreadHUDImageSwitchWidget::ApplyValue(float Raw, float InDisplayNormalized)
{
	ShowState(SelectState(Raw), false);
}

void UTreadHUDImageSwitchWidget::ApplyPreview(float PreviewNormalized)
{
	ShowState(SelectState(PreviewRaw), true);
}

void UTreadHUDImageSwitchWidget::ShowState(int32 StateIndex, bool bForce)
{
	if (!StateImage || (StateIndex == ShownState && !bForce))
	{
		return;
	}
	ShownState = StateIndex;

	const FTreadHUDImageState* State = States.IsValidIndex(StateIndex) ? &States[StateIndex] : nullptr;
	if (!State || !State->Texture)
	{
		StateImage->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	StateImage->SetBrushFromTexture(State->Texture, true);
	StateImage->SetColorAndOpacity(State->Tint);
	StateImage->SetVisibility(ESlateVisibility::HitTestInvisible);
}
