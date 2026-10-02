// 踏みつけ加速メカゲーム — 画面を組み立てる共通部品

#include "UI/ImpactWidgetHelpers.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

DEFINE_LOG_CATEGORY(LogImpactUI);

namespace
{
	/** 背景を問わず読めるよう、文字には影を付ける。 */
	const FVector2D TextShadowOffset(2.0f, 2.0f);
	const FLinearColor TextShadowColor(0.0f, 0.0f, 0.0f, 0.75f);

	/** ボタン内の文字の余白（px）。 */
	const FMargin ButtonContentPadding(40.0f, 12.0f);

	constexpr int32 SecondsPerMinute = 60;
}

namespace ImpactWidget
{
	UTextBlock* MakeText(UWidgetTree& Tree, int32 FontSize, const FLinearColor& Color, ETextJustify::Type Justification)
	{
		UTextBlock* Text = Tree.ConstructWidget<UTextBlock>();
		Text->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FontSize));
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetShadowOffset(TextShadowOffset);
		Text->SetShadowColorAndOpacity(TextShadowColor);
		Text->SetJustification(Justification);
		return Text;
	}

	UButton* MakeButton(UWidgetTree& Tree, const FText& Label, int32 FontSize,
		const FLinearColor& TextColor, const FLinearColor& BackgroundColor)
	{
		UButton* Button = Tree.ConstructWidget<UButton>();
		Button->SetBackgroundColor(BackgroundColor);

		UTextBlock* LabelText = MakeText(Tree, FontSize, TextColor, ETextJustify::Center);
		LabelText->SetText(Label);

		if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(Button->AddChild(LabelText)))
		{
			ContentSlot->SetPadding(ButtonContentPadding);
			ContentSlot->SetHorizontalAlignment(HAlign_Center);
			ContentSlot->SetVerticalAlignment(VAlign_Center);
		}

		return Button;
	}

	FText FormatClock(float Seconds, bool bRoundUp)
	{
		const float Clamped = FMath::Max(Seconds, 0.0f);
		const int32 TotalSeconds = bRoundUp ? FMath::CeilToInt(Clamped) : FMath::FloorToInt(Clamped);
		return FText::FromString(FString::Printf(TEXT("%d:%02d"), TotalSeconds / SecondsPerMinute, TotalSeconds % SecondsPerMinute));
	}
}
