// 踏みつけ加速メカゲーム — タイトル画面のプレイヤーコントローラ

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ImpactTitlePlayerController.generated.h"

class UImpactTitleWidget;

/**
 * タイトル画面を表示し、マウスでの操作を受け付けるプレイヤーコントローラ。
 */
UCLASS()
class MYPROJECT_API AImpactTitlePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AImpactTitlePlayerController();

	/** ソロモードを開始する。 */
	void RequestStartGame();

	/** アプリケーションを終了する。 */
	void RequestQuit();

protected:
	virtual void BeginPlay() override;

	/** 表示するタイトル画面。Blueprint 側で WBP_Title を割り当てる。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UImpactTitleWidget> TitleWidgetClass;

private:
	UPROPERTY(Transient)
	TObjectPtr<UImpactTitleWidget> TitleWidget;
};
