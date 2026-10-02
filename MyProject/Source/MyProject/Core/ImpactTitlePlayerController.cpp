// 踏みつけ加速メカゲーム — タイトル画面のプレイヤーコントローラ

#include "Core/ImpactTitlePlayerController.h"

#include "Core/ImpactTitleGameMode.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UI/ImpactTitleWidget.h"
#include "UI/ImpactWidgetHelpers.h"

AImpactTitlePlayerController::AImpactTitlePlayerController()
{
	TitleWidgetClass = UImpactTitleWidget::StaticClass();
	bShowMouseCursor = true;
}

void AImpactTitlePlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	if (!TitleWidgetClass)
	{
		UE_LOG(LogImpactUI, Error, TEXT("TitleWidgetClass is not set on %s"), *GetName());
		return;
	}

	TitleWidget = CreateWidget<UImpactTitleWidget>(this, TitleWidgetClass);
	if (!TitleWidget)
	{
		UE_LOG(LogImpactUI, Error, TEXT("could not create %s"), *TitleWidgetClass->GetName());
		return;
	}

	TitleWidget->AddToViewport();
	SetInputMode(FInputModeUIOnly());
	UE_LOG(LogImpactUI, Log, TEXT("title screen shown: %s"), *TitleWidget->GetClass()->GetName());
}

void AImpactTitlePlayerController::RequestStartGame()
{
	if (AImpactTitleGameMode* GameMode = GetWorld()->GetAuthGameMode<AImpactTitleGameMode>())
	{
		GameMode->StartSoloGame();
	}
}

void AImpactTitlePlayerController::RequestQuit()
{
	UE_LOG(LogImpactUI, Log, TEXT("quit requested from the title screen"));
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}
