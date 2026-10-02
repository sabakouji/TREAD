// 踏みつけ加速メカゲーム — プレイヤーコントローラ

#include "Core/ImpactPlayerController.h"

#include "Core/ImpactGameMode.h"
#include "Debug/ImpactDebugHUD.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Feedback/ImpactFeedbackComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Score/ImpactScoreSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "UI/HUD/TreadHUDElementWidget.h"
#include "UI/ImpactHUDWidget.h"
#include "UI/ImpactResultWidget.h"
#include "UI/ImpactWidgetHelpers.h"

DEFINE_LOG_CATEGORY(LogImpactInput);

AImpactPlayerController::AImpactPlayerController()
{
	bShowMouseCursor = false;
	HUDWidgetClass = UImpactHUDWidget::StaticClass();
	ResultWidgetClass = UImpactResultWidget::StaticClass();

	Feedback = CreateDefaultSubobject<UImpactFeedbackComponent>(TEXT("Feedback"));
}

void AImpactPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		// 入力モードはレベル遷移後も残る GameViewportClient 側に保持される。
		// タイトルやリザルトの UI 専用モード（入力を無視する設定）を持ち越すと機体を操作できないため、
		// 起動時に必ずゲーム用へ戻す（CLAUDE.md 5-7）。
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = false;

		CreateMatchWidgets();
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		UE_LOG(LogImpactInput, Error, TEXT("GetLocalPlayer() returned null on %s"), *GetName());
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);
	if (!Subsystem)
	{
		UE_LOG(LogImpactInput, Error, TEXT("UEnhancedInputLocalPlayerSubsystem is unavailable on %s"), *GetName());
		return;
	}

	if (VehicleMappingContext)
	{
		ApplyMappingContext(*Subsystem, VehicleMappingContext, VehicleMappingPriority);
	}
	else
	{
		UE_LOG(LogImpactInput, Error, TEXT("VehicleMappingContext is not assigned on %s"), *GetName());
	}

#if !UE_BUILD_SHIPPING
	// デバッグ操作は本番の操作系と別の Context に分けており、Shipping では適用しない。
	if (DebugMappingContext)
	{
		ApplyMappingContext(*Subsystem, DebugMappingContext, DebugMappingPriority);
	}
	else
	{
		UE_LOG(LogImpactInput, Warning, TEXT("DebugMappingContext is not assigned on %s; debug keys are unavailable"), *GetName());
	}
#endif
}

void AImpactPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

#if !UE_BUILD_SHIPPING
	// デバッグ HUD は機体ではなく画面の操作なので、Pawn ではなくコントローラで受ける。
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (EnhancedInput && ToggleDebugHUDAction)
	{
		EnhancedInput->BindAction(ToggleDebugHUDAction, ETriggerEvent::Started, this, &AImpactPlayerController::OnToggleDebugHUD);
	}
#endif
}

void AImpactPlayerController::ApplyMappingContext(
	UEnhancedInputLocalPlayerSubsystem& Subsystem, const UInputMappingContext* Context, int32 Priority)
{
	const UEnhancedPlayerInput* EnhancedInput = Cast<UEnhancedPlayerInput>(PlayerInput);
	if (!EnhancedInput)
	{
		UE_LOG(LogImpactInput, Error, TEXT("PlayerInput is not a UEnhancedPlayerInput (actual: %s)"),
			PlayerInput ? *PlayerInput->GetClass()->GetName() : TEXT("null"));
		return;
	}

	const int32 MappingsBefore = EnhancedInput->GetEnhancedActionMappingsView().Num();

	// 既定では制御マッピングの再構築が次の入力処理まで遅延されるため、
	// 適用結果をこの場で検証できない。即時再構築を指定して結果を確定させる。
	FModifyContextOptions Options;
	Options.bForceImmediately = true;
	Subsystem.AddMappingContext(Context, Priority, Options);

	// 追加しただけでは実際に適用されたことにならない。Input Mode のクエリが一致しない Context や、
	// 非推奨の Mappings にしか中身がない Context は、エラーを出さずにマッピング0件として扱われる。
	const bool bApplied = Subsystem.HasMappingContext(Context);
	const int32 MappingsAfter = EnhancedInput->GetEnhancedActionMappingsView().Num();
	const int32 AddedMappings = MappingsAfter - MappingsBefore;

	UE_LOG(LogImpactInput, Log, TEXT("%s: applied=%s priority=%d added mappings=%d (total active %d), InputMode=%s"),
		*Context->GetName(), bApplied ? TEXT("true") : TEXT("false"), Priority, AddedMappings, MappingsAfter,
		*Subsystem.GetInputMode().ToString());

	if (!bApplied || AddedMappings <= 0)
	{
		UE_LOG(LogImpactInput, Error,
			TEXT("%s contributed no active mappings. Check DefaultKeyMappings and the input mode query (CLAUDE.md 5-1, 5-3)."),
			*Context->GetName());
	}
}

void AImpactPlayerController::CreateMatchWidgets()
{
	if (AImpactGameState* State = GetWorld()->GetGameState<AImpactGameState>())
	{
		State->OnMatchStateChanged.AddUniqueDynamic(this, &AImpactPlayerController::HandleMatchStateChanged);
	}
	else
	{
		UE_LOG(LogImpactUI, Warning, TEXT("GameState is not an AImpactGameState; the result screen will not appear"));
	}

	CreateBattleHUD();

	if (!HUDWidgetClass)
	{
		UE_LOG(LogImpactUI, Warning, TEXT("HUDWidgetClass is not set on %s; the play HUD is hidden"), *GetName());
		return;
	}

	HUDWidget = CreateWidget<UImpactHUDWidget>(this, HUDWidgetClass);
	if (!HUDWidget)
	{
		UE_LOG(LogImpactUI, Error, TEXT("could not create %s"), *HUDWidgetClass->GetName());
		return;
	}

	HUDWidget->AddToViewport();
	UE_LOG(LogImpactUI, Log, TEXT("play HUD shown: %s"), *HUDWidget->GetClass()->GetName());
}

void AImpactPlayerController::CreateBattleHUD()
{
	if (!BattleHUDWidgetClass)
	{
		UE_LOG(LogImpactUI, Warning, TEXT("BattleHUDWidgetClass is not set on %s; the battle HUD is hidden"), *GetName());
		return;
	}

	BattleHUDWidget = CreateWidget<UUserWidget>(this, BattleHUDWidgetClass);
	if (!BattleHUDWidget)
	{
		UE_LOG(LogImpactUI, Error, TEXT("could not create %s"), *BattleHUDWidgetClass->GetName());
		return;
	}
	BattleHUDWidget->AddToViewport();

	// 部品の配置はデザイナーの手作業のため、置かれた部品と紐付け先をログで確かめられるようにする。
	TArray<FString> Elements;
	if (BattleHUDWidget->WidgetTree)
	{
		BattleHUDWidget->WidgetTree->ForEachWidget([&Elements](UWidget* Widget)
		{
			if (const UTreadHUDElementWidget* Element = Cast<UTreadHUDElementWidget>(Widget))
			{
				Elements.Add(FString::Printf(TEXT("%s(%s)"), *Element->GetClass()->GetName(), *Element->BindTag.ToString()));
			}
		});
	}
	UE_LOG(LogImpactUI, Log, TEXT("battle HUD shown: %s with %d HUD elements [%s]"),
		*BattleHUDWidget->GetClass()->GetName(), Elements.Num(), *FString::Join(Elements, TEXT(", ")));
}

void AImpactPlayerController::HandleMatchStateChanged(EImpactMatchState NewState)
{
	if (NewState == EImpactMatchState::Finished)
	{
		ShowResult();
	}
}

void AImpactPlayerController::ShowResult()
{
	if (!ResultWidgetClass)
	{
		UE_LOG(LogImpactUI, Error, TEXT("ResultWidgetClass is not set on %s"), *GetName());
		return;
	}

	const UWorld* World = GetWorld();
	// 結果と配点は GameState から読む。GameMode はサーバにしか存在しないため画面の表示には使わない。
	const AImpactGameState* State = World->GetGameState<AImpactGameState>();
	const UGameInstance* GameInstance = GetGameInstance();
	const UImpactScoreSubsystem* Score = GameInstance ? GameInstance->GetSubsystem<UImpactScoreSubsystem>() : nullptr;
	if (!State || !Score)
	{
		UE_LOG(LogImpactUI, Error, TEXT("cannot show the result: match state or score is unavailable"));
		return;
	}

	ResultWidget = CreateWidget<UImpactResultWidget>(this, ResultWidgetClass);
	if (!ResultWidget)
	{
		UE_LOG(LogImpactUI, Error, TEXT("could not create %s"), *ResultWidgetClass->GetName());
		return;
	}

	ResultWidget->ShowResult(State->GetMatchResult(), Score->GetTally(), State->GetScoreTuning(), State->GetElapsedTime());
	ResultWidget->AddToViewport(ResultZOrder);

	// リザルトの内訳と重複するため、プレイ中の画面は隠す。
	if (HUDWidget)
	{
		HUDWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (BattleHUDWidget)
	{
		BattleHUDWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
}

void AImpactPlayerController::RequestRetry()
{
	if (AImpactGameMode* GameMode = GetWorld()->GetAuthGameMode<AImpactGameMode>())
	{
		GameMode->RestartMatch();
	}
}

void AImpactPlayerController::RequestReturnToTitle()
{
	if (AImpactGameMode* GameMode = GetWorld()->GetAuthGameMode<AImpactGameMode>())
	{
		GameMode->ReturnToTitle();
	}
}

void AImpactPlayerController::ImpactShowDebug()
{
	AImpactDebugHUD* DebugHUD = GetHUD<AImpactDebugHUD>();
	if (!DebugHUD)
	{
		UE_LOG(LogImpactUI, Warning, TEXT("the HUD of %s is not an AImpactDebugHUD"), *GetName());
		return;
	}

	DebugHUD->SetShowVehicleDebug(!DebugHUD->IsShowingVehicleDebug());
	UE_LOG(LogImpactUI, Log, TEXT("vehicle debug HUD: %s"), DebugHUD->IsShowingVehicleDebug() ? TEXT("shown") : TEXT("hidden"));
}

void AImpactPlayerController::OnToggleDebugHUD(const FInputActionValue& Value)
{
	ImpactShowDebug();
}
