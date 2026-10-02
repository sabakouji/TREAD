// 踏みつけ加速メカゲーム — タイトル画面

#include "UI/ImpactTitleWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/ImpactTitlePlayerController.h"
#include "UI/ImpactWidgetHelpers.h"

#define LOCTEXT_NAMESPACE "ImpactTitle"

void UImpactTitleWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	BuildLayout();
}

void UImpactTitleWidget::BuildLayout()
{
	if (!WidgetTree)
	{
		UE_LOG(LogImpactUI, Error, TEXT("%s has no widget tree; the title screen is not built"), *GetName());
		return;
	}

	if (WidgetTree->RootWidget)
	{
		UE_LOG(LogImpactUI, Warning, TEXT("%s: the designer layout is replaced by the C++ layout"), *GetName());
	}

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("TitleRoot"));
	WidgetTree->RootWidget = Root;

	UImage* Background = WidgetTree->ConstructWidget<UImage>();
	Background->SetColorAndOpacity(BackgroundColor);
	UOverlaySlot* BackgroundSlot = Root->AddChildToOverlay(Background);
	BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
	BackgroundSlot->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
	UOverlaySlot* ContentSlot = Root->AddChildToOverlay(Content);
	ContentSlot->SetHorizontalAlignment(HAlign_Center);
	ContentSlot->SetVerticalAlignment(VAlign_Center);

	UTextBlock* TitleText = ImpactWidget::MakeText(*WidgetTree, TitleFontSize, TitleColor, ETextJustify::Center);
	TitleText->SetText(GameTitle);
	Content->AddChildToVerticalBox(TitleText)->SetHorizontalAlignment(HAlign_Center);

	UTextBlock* SubtitleText = ImpactWidget::MakeText(*WidgetTree, SubtitleFontSize, SubtitleColor, ETextJustify::Center);
	SubtitleText->SetText(Subtitle);
	Content->AddChildToVerticalBox(SubtitleText)->SetHorizontalAlignment(HAlign_Center);

	StartButton = ImpactWidget::MakeButton(*WidgetTree, LOCTEXT("Start", "START"), ButtonFontSize, TitleColor, ButtonColor);
	QuitButton = ImpactWidget::MakeButton(*WidgetTree, LOCTEXT("Quit", "QUIT"), ButtonFontSize, TitleColor, ButtonColor);

	const TPair<UButton*, float> ButtonsWithSpacing[] = {
		{ StartButton.Get(), SectionSpacing },
		{ QuitButton.Get(), ButtonSpacing },
	};
	for (const TPair<UButton*, float>& Entry : ButtonsWithSpacing)
	{
		USizeBox* ButtonBox = WidgetTree->ConstructWidget<USizeBox>();
		ButtonBox->SetWidthOverride(ButtonWidth);
		ButtonBox->AddChild(Entry.Key);

		UVerticalBoxSlot* ButtonSlot = Content->AddChildToVerticalBox(ButtonBox);
		ButtonSlot->SetHorizontalAlignment(HAlign_Center);
		ButtonSlot->SetPadding(FMargin(0.0f, Entry.Value, 0.0f, 0.0f));
	}

	StartButton->OnClicked.AddDynamic(this, &UImpactTitleWidget::HandleStartClicked);
	QuitButton->OnClicked.AddDynamic(this, &UImpactTitleWidget::HandleQuitClicked);

	UE_LOG(LogImpactUI, Log, TEXT("%s: title layout built"), *GetName());
}

void UImpactTitleWidget::HandleStartClicked()
{
	DisableButtons();

	if (AImpactTitlePlayerController* Controller = GetOwningPlayer<AImpactTitlePlayerController>())
	{
		Controller->RequestStartGame();
	}
}

void UImpactTitleWidget::HandleQuitClicked()
{
	DisableButtons();

	if (AImpactTitlePlayerController* Controller = GetOwningPlayer<AImpactTitlePlayerController>())
	{
		Controller->RequestQuit();
	}
}

void UImpactTitleWidget::DisableButtons()
{
	StartButton->SetIsEnabled(false);
	QuitButton->SetIsEnabled(false);
}

#undef LOCTEXT_NAMESPACE
