// 踏みつけ加速メカゲーム — NPC 用の地点

#include "Field/FieldPointComponent.h"

#include "Field/FieldSubsystem.h"

UFieldPointComponent::UFieldPointComponent()
{
	// 位置を提供するだけで、毎フレームの処理を持たない。
	PrimaryComponentTick.bCanEverTick = false;
}

void UFieldPointComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UFieldSubsystem* Field = UFieldSubsystem::Get(this))
	{
		Field->RegisterFieldPoint(this);
	}
}

void UFieldPointComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UFieldSubsystem* Field = UFieldSubsystem::Get(this))
	{
		Field->UnregisterFieldPoint(this);
	}

	Super::EndPlay(EndPlayReason);
}
