// 踏みつけ加速メカゲーム — 機体の衝突を受ける対象のインターフェース

#include "Collision/ImpactReceiver.h"

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"

UObject* ImpactReceiver::FindReceiver(AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}

	if (Actor->Implements<UImpactReceiver>())
	{
		return Actor;
	}

	return Actor->FindComponentByInterface(UImpactReceiver::StaticClass());
}
