// 踏みつけ加速メカゲーム — プレイヤーコントローラ

#pragma once

#include "CoreMinimal.h"
#include "Core/ImpactGameState.h"
#include "GameFramework/PlayerController.h"
#include "ImpactPlayerController.generated.h"

class UImpactFeedbackComponent;
class UImpactHUDWidget;
class UImpactResultWidget;
class UInputAction;
class UInputMappingContext;
class UUserWidget;
struct FInputActionValue;

/**
 * 入力初期化のログカテゴリ。
 *
 * Enhanced Input は初期化に失敗しても例外を出さず「入力が効かない」という形でしか現れないため、
 * 適用結果を毎回ログに残し、失敗を切り分け可能にしておく。
 */
DECLARE_LOG_CATEGORY_EXTERN(LogImpactInput, Log, All);

/**
 * Enhanced Input の Mapping Context の適用と、プレイ中の画面（HUD・リザルト）の表示を担当するプレイヤーコントローラ。
 *
 * 入力の解釈そのものは Pawn 側（移動コンポーネント）が担当し、
 * 本クラスは「どの Mapping Context を有効にするか」と「どの画面を出すか」のみを管理する。
 */
UCLASS()
class MYPROJECT_API AImpactPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AImpactPlayerController();

	/** リザルト画面から、同じマップでの再戦を求める。 */
	void RequestRetry();

	/** リザルト画面から、タイトルへ戻ることを求める。 */
	void RequestReturnToTitle();

	/** デバッグ HUD の表示を切り替える。コンソールから `ImpactShowDebug` で呼べる。 */
	UFUNCTION(Exec)
	void ImpactShowDebug();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	/** 機体操作用の Mapping Context。Blueprint 側で IMC_Vehicle を割り当てる。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> VehicleMappingContext;

	/** Mapping Context の優先度。値が大きいほど優先される。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	int32 VehicleMappingPriority = 0;

	/**
	 * デバッグ操作用の Mapping Context。Blueprint 側で IMC_Debug を割り当てる。
	 * 本番の操作系と混ぜず別の Context に分け、Shipping ビルドでは適用しない。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Debug")
	TObjectPtr<UInputMappingContext> DebugMappingContext;

	/** デバッグ用 Mapping Context の優先度。機体操作より優先する。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Debug")
	int32 DebugMappingPriority = 1;

	/** デバッグ HUD の表示切り替え（F1）。Blueprint 側で IA_DebugToggleHUD を割り当てる。Shipping では無効。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Debug")
	TObjectPtr<UInputAction> ToggleDebugHUDAction;

	/** プレイ中の画面。Blueprint 側で WBP_HUD を割り当てる。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UImpactHUDWidget> HUDWidgetClass;

	/**
	 * 対戦 HUD（汎用 HUD 部品の配置先）。Blueprint 側で WBP_BattleHUD を割り当てる。
	 * 部品の配置は WBP のデザイナーで行い、ここでは生成と表示だけを行う（GUI-01）。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UUserWidget> BattleHUDWidgetClass;

	/** リザルト画面。Blueprint 側で WBP_Result を割り当てる。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UImpactResultWidget> ResultWidgetClass;

	/** リザルト画面の重なり順。プレイ中の画面より手前に出す。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	int32 ResultZOrder = 10;

private:
	/**
	 * Mapping Context を追加し、実際に適用されたかを検証してログに残す。
	 * Enhanced Input は適用に失敗しても何も報告しないため（CLAUDE.md 5-3）。
	 */
	void ApplyMappingContext(class UEnhancedInputLocalPlayerSubsystem& Subsystem, const UInputMappingContext* Context, int32 Priority);

	/** プレイ中の画面を出し、試合の進行段階の通知を受け取れるようにする。 */
	void CreateMatchWidgets();

	/** 対戦 HUD を出し、配置されている汎用 HUD 部品の数と紐付け先をログに残す（配置の確認用）。 */
	void CreateBattleHUD();

	/** 試合が終わったらリザルト画面を出す。dynamic デリゲートの受け手のため UFUNCTION が必須。 */
	UFUNCTION()
	void HandleMatchStateChanged(EImpactMatchState NewState);

	/** リザルト画面を出し、マウスで操作できるようにする。 */
	void ShowResult();

	void OnToggleDebugHUD(const FInputActionValue& Value);

	UPROPERTY(Transient)
	TObjectPtr<UImpactHUDWidget> HUDWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> BattleHUDWidget;

	/** 自機の破壊・命中の手応え（ヒットストップ・カメラシェイク）。調整値は Blueprint 側で割り当てる。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UImpactFeedbackComponent> Feedback;

	UPROPERTY(Transient)
	TObjectPtr<UImpactResultWidget> ResultWidget;
};
