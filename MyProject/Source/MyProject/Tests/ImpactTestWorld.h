// 踏みつけ加速メカゲーム — 自動テスト用の World

#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"

namespace ImpactTests
{
	/**
	 * テスト用の World。生成した Actor ごと、スコープを抜けると破棄する。
	 * 部品の BeginPlay（登録・購読）や、ワールドの設定（時間の進み方）に依存する処理をエディタを開かずに確かめる。
	 */
	struct FScopedTestWorld
	{
		UWorld* World = nullptr;

		FScopedTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ImpactTestWorld"));
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}

		~FScopedTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		FScopedTestWorld(const FScopedTestWorld&) = delete;
		FScopedTestWorld& operator=(const FScopedTestWorld&) = delete;

		/** Actor を生成し、BeginPlay まで進める。 */
		template <typename TActor>
		TActor* SpawnAndBegin(const FVector& Location = FVector::ZeroVector)
		{
			TActor* Actor = World->SpawnActor<TActor>(Location, FRotator::ZeroRotator);
			if (Actor)
			{
				Actor->DispatchBeginPlay();
			}
			return Actor;
		}
	};
}

#endif // WITH_DEV_AUTOMATION_TESTS
