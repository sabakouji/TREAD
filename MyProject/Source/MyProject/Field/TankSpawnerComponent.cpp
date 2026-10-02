// 踏みつけ加速メカゲーム — 踏み台（戦車）の供給

#include "Field/TankSpawnerComponent.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Field/LaneActor.h"
#include "GameFramework/Actor.h"
#include "Score/ImpactScoreTypes.h"
#include "Stomp/StompTargetActor.h"
#include "TimerManager.h"

UTankSpawnerComponent::UTankSpawnerComponent()
{
	// 供給はタイマーで行うため、毎フレームの処理は持たない。
	PrimaryComponentTick.bCanEverTick = false;
}

void UTankSpawnerComponent::BeginPlay()
{
	Super::BeginPlay();

	WarnIfWaveSpacingTooShort();

	// 開始直後に1ウェーブ供給し、待ち時間なく検証を始められるようにする。以降は供給間隔ごとに繰り返す。
	HandleSpawnTimer();
	GetWorld()->GetTimerManager().SetTimer(SpawnTimer, this, &UTankSpawnerComponent::HandleSpawnTimer, SpawnInterval, true);
}

void UTankSpawnerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpawnTimer);
	}

	Super::EndPlay(EndPlayReason);
}

void UTankSpawnerComponent::WarnIfWaveSpacingTooShort() const
{
	const AStompTargetActor* Defaults = StompTargetClass ? StompTargetClass->GetDefaultObject<AStompTargetActor>() : nullptr;
	const UBoxComponent* Box = Defaults ? Defaults->GetCollisionBox() : nullptr;
	if (!Box || TanksPerWave <= 1)
	{
		return;
	}

	const float BodyLength = Box->GetScaledBoxExtent().X * 2.0f;
	if (WaveSpacing < BodyLength)
	{
		UE_LOG(LogImpactMatch, Warning,
			TEXT("%s: WaveSpacing %.0f is shorter than the tank body length %.0f; tanks in the same wave will overlap"),
			*GetNameSafe(GetOwner()), WaveSpacing, BodyLength);
	}
}

void UTankSpawnerComponent::HandleSpawnTimer()
{
	// 破壊済み・寿命切れ・ルート終端に着いた踏み台を一覧から除く。
	AliveTargets.RemoveAll([](const TObjectPtr<AStompTargetActor>& Target)
	{
		return !IsValid(Target);
	});

	bool bHasLanes = false;
	ALaneActor* Lane = PickNextLane(bHasLanes);
	if (bHasLanes && !Lane)
	{
		// どのレーンも上限に達している。
		return;
	}

	const int32 Room = MaxAliveTargets - CountAliveOn(Lane);
	if (Room <= 0)
	{
		return;
	}

	// 出現位置が空いているかは、ウェーブを出す前にまとめて判定する。判定の対象は既に走っている踏み台だけで、
	// 同じウェーブの踏み台同士は判定し合わない（WaveSpacing が SpawnClearance より狭くても2体目以降を見送らない）。
	// 塞がれて車列が始点まで伸びていれば、重ならないよう出現を見送る。直進の踏み台は止まらないため調べない。
	const int32 Count = FMath::Min(TanksPerWave, Room);
	TArray<int32, TInlineAllocator<8>> ClearSlots;
	for (int32 WaveIndex = 0; WaveIndex < Count; ++WaveIndex)
	{
		if (!Lane || !IsLaneOccupiedAt(Lane, WaveSpacing * WaveIndex))
		{
			ClearSlots.Add(WaveIndex);
		}
	}

	for (const int32 WaveIndex : ClearSlots)
	{
		SpawnStompTarget(Lane, WaveIndex);
	}
}

ALaneActor* UTankSpawnerComponent::PickNextLane(bool& bOutHasLanes)
{
	// 削除されたレーンと、上限に達したレーンは飛ばす。全て無効ならレーン無し（直進）として扱う。
	bOutHasLanes = false;
	for (int32 Attempt = 0; Attempt < Lanes.Num(); ++Attempt)
	{
		const int32 Index = NextLaneIndex % Lanes.Num();
		NextLaneIndex = (Index + 1) % Lanes.Num();

		ALaneActor* Lane = Lanes[Index].Get();
		if (!IsValid(Lane))
		{
			continue;
		}

		bOutHasLanes = true;
		if (CountAliveOn(Lane) < MaxAliveTargets)
		{
			return Lane;
		}
	}

	return nullptr;
}

bool UTankSpawnerComponent::IsLaneOccupiedAt(const ALaneActor* Lane, float Distance) const
{
	return AliveTargets.ContainsByPredicate([this, Lane, Distance](const TObjectPtr<AStompTargetActor>& Target)
	{
		return IsValid(Target) && Target->GetLane() == Lane
			&& FMath::Abs(Target->GetLaneDistance() - Distance) <= SpawnClearance;
	});
}

int32 UTankSpawnerComponent::CountAliveOn(const ALaneActor* Lane) const
{
	int32 Count = 0;
	for (const TObjectPtr<AStompTargetActor>& Target : AliveTargets)
	{
		if (IsValid(Target) && Target->GetLane() == Lane)
		{
			++Count;
		}
	}

	return Count;
}

void UTankSpawnerComponent::SpawnStompTarget(ALaneActor* Lane, int32 WaveIndex)
{
	const AActor* Owner = GetOwner();
	if (!StompTargetClass || !Owner)
	{
		return;
	}

	// 同じウェーブの踏み台は、先頭から WaveSpacing ずつ間隔を空けた列にする。
	const float Offset = WaveSpacing * WaveIndex;

	FVector SpawnLocation;
	FVector Direction;
	if (Lane)
	{
		SpawnLocation = Lane->GetGroundLocationAtDistance(Offset);
		Direction = Lane->GetDirectionAtDistance(Offset);
	}
	else
	{
		Direction = Owner->GetActorForwardVector().GetSafeNormal2D();
		SpawnLocation = Owner->GetActorLocation() + Direction * (SpawnForwardOffset + Offset);
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AStompTargetActor* Target = GetWorld()->SpawnActor<AStompTargetActor>(
		StompTargetClass, SpawnLocation, Direction.Rotation(), SpawnParams);
	if (!Target)
	{
		return;
	}

	if (Lane)
	{
		Target->SetLane(Lane, Offset);
	}
	else
	{
		Target->SetMarchDirection(Direction);
	}

	AliveTargets.Add(Target);
}
