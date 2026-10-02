// 踏みつけ加速メカゲーム — 踏みつけの対象のインターフェース

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Stompable.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UStompable : public UInterface
{
	GENERATED_BODY()
};

/**
 * 踏みつけて加速の足場にできる対象（行進してくる踏み台など）。
 *
 * 機体は重なった相手の具体的な型を知らず、このインターフェースを通して踏みつけを要求する。
 * 前方からの接触かどうかの判定は機体側（移動コンポーネント）が行い、
 * 対象側は踏みつけを受け付けるか（耐久値が残っているか）だけを決める。
 *
 * 呼び出しは必ず `Actor->Implements<UStompable>()` と `IStompable::Execute_ReceiveStomp` で行うこと。
 */
class MYPROJECT_API IStompable
{
	GENERATED_BODY()

public:
	/**
	 * 踏みつけを受ける。
	 * @return 踏みつけが成立し、加速を与えてよい場合に true。
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Stomp")
	bool ReceiveStomp();
};
