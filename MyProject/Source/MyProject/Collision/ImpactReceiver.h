// 踏みつけ加速メカゲーム — 機体の衝突を受ける対象のインターフェース

#pragma once

#include "CoreMinimal.h"
#include "Collision/ImpactTypes.h"
#include "UObject/Interface.h"
#include "ImpactReceiver.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UImpactReceiver : public UInterface
{
	GENERATED_BODY()
};

/**
 * 機体の衝突に反応する対象（ゴール・破壊可能オブジェクトなど）。
 *
 * 移動コンポーネントは衝突相手の具体的な型を知らず、このインターフェースを通して反応を尋ねる。
 * 衝突対象の種類を増やす際は、そのクラスが本インターフェースを実装すればよく、移動コンポーネントは変更しない。
 * 実装していないブロック判定のアクタ（床・外周壁など）は破壊不能の壁として扱われる。
 *
 * Actor 自身だけでなく、Actor に付けた部品（UGoalComponent・UDestructibleComponent など）も実装できる。
 * 受け手の探索は ImpactReceiver::FindReceiver に集約しており、Actor → 部品の順に調べる。
 *
 * Blueprint だけで実装した対象も扱えるよう、判定は必ず `Implements<UImpactReceiver>()`、
 * 呼び出しは `IImpactReceiver::Execute_ReceiveVehicleImpact` で行うこと
 * （`Cast<IImpactReceiver>` は Blueprint 実装に対して null を返す）。
 */
class MYPROJECT_API IImpactReceiver
{
	GENERATED_BODY()

public:
	/**
	 * 機体の衝突を受ける。ダメージや破壊が起きたかを返す。
	 * 影響を受けない場合は Outcome を Unaffected のまま返す。
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Impact")
	FImpactReceiveResult ReceiveVehicleImpact(const FImpactReceiveContext& Context);
};

namespace ImpactReceiver
{
	/**
	 * 機体の衝突を受ける対象を探す。Actor 自身が IImpactReceiver を実装していればそれを、
	 * そうでなければ IImpactReceiver を実装した最初の部品を返す。どちらも無ければ nullptr（破壊不能の壁）。
	 */
	MYPROJECT_API UObject* FindReceiver(AActor* Actor);
}
