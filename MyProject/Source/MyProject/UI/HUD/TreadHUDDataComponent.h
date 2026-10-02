// 踏みつけ加速メカゲーム — HUD に公開する値の置き場

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "UI/HUD/TreadHUDTypes.h"
#include "TreadHUDDataComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnHUDValueChanged, FGameplayTag, Tag, float, Raw, float, Normalized);

/**
 * HUD に公開する値の置き場。Pawn に付ける。
 *
 * ゲームロジック側は値を GameplayTag の行先に書き込むだけで、GUI を知らない。
 * 表示部品（UTreadHUDElementWidget）はタグで値を読みに来るか、OnValueChanged で受け取る。
 * 自身は Tick しない。
 */
UCLASS(ClassGroup = (HUD), meta = (BlueprintSpawnableComponent))
class MYPROJECT_API UTreadHUDDataComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTreadHUDDataComponent();

	/**
	 * 値を書き込む。Min〜Max から正規化値を求めて保持する（Min >= Max なら正規化値は 0）。
	 * 値・範囲のいずれかが前回から変わったときだけ OnValueChanged を発火する。
	 * @return 値が変わり、通知したか。
	 */
	UFUNCTION(BlueprintCallable, Category = "TREAD|HUD")
	bool SetValue(FGameplayTag Tag, float Raw, float Min, float Max);

	/** 値を読む。まだ書き込まれていないタグなら false を返し、出力は 0 にする。 */
	UFUNCTION(BlueprintPure, Category = "TREAD|HUD")
	bool GetValue(FGameplayTag Tag, float& OutRaw, float& OutNormalized) const;

	/** 値が変わったときの通知。受け手の関数には UFUNCTION() が必須。 */
	UPROPERTY(BlueprintAssignable, Category = "TREAD|HUD")
	FOnHUDValueChanged OnValueChanged;

private:
	TMap<FGameplayTag, FTreadHUDValue> Values;
};
