// 踏みつけ加速メカゲーム — 汎用 HUD 部品: 数値の表示

#include "UI/HUD/TreadHUDTextWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "UI/ImpactWidgetHelpers.h"

bool UTreadHUDTextWidget::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();

	if (WidgetTree && !ValueText)
	{
		if (WidgetTree->RootWidget)
		{
			UE_LOG(LogImpactUI, Warning, TEXT("%s: the designer layout has no ValueText; it is replaced by the C++ layout"), *GetName());
		}
		ValueText = ImpactWidget::MakeText(*WidgetTree, FontSize, TextColor, ETextJustify::Center);
		WidgetTree->RootWidget = ValueText;
		bBuiltText = true;
	}
	return bSuperInitialized;
}

FText UTreadHUDTextWidget::FormatValue(float Raw) const
{
	FNumberFormattingOptions Options;
	Options.MinimumFractionalDigits = FractionalDigits;
	Options.MaximumFractionalDigits = FractionalDigits;
	return FText::Format(Format, FText::AsNumber(RoundForDisplay(Raw), &Options));
}

double UTreadHUDTextWidget::RoundForDisplay(float Raw) const
{
	const double Scale = FMath::Pow(10.0, static_cast<double>(FractionalDigits));
	return FMath::RoundToDouble(static_cast<double>(Raw) * Multiplier * Scale) / Scale;
}

void UTreadHUDTextWidget::ApplyValue(float Raw, float InDisplayNormalized)
{
	if (!ValueText)
	{
		return;
	}

	const double Value = RoundForDisplay(Raw);
	if (bHasShown && Value == ShownValue)
	{
		return;
	}

	bHasShown = true;
	ShownValue = Value;
	ValueText->SetText(FormatValue(Raw));
}

void UTreadHUDTextWidget::ApplyPreview(float PreviewNormalized)
{
	if (!ValueText)
	{
		return;
	}

	// デザイナーで文字の大きさ・色を変えたら見えるようにする。
	if (bBuiltText)
	{
		FSlateFontInfo Font = ValueText->GetFont();
		Font.Size = FontSize;
		ValueText->SetFont(Font);
		ValueText->SetColorAndOpacity(FSlateColor(TextColor));
	}
	ValueText->SetText(FormatValue(PreviewRaw));
}
