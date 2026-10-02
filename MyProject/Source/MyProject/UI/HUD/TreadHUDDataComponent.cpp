// 踏みつけ加速メカゲーム — HUD に公開する値の置き場

#include "UI/HUD/TreadHUDDataComponent.h"

#include "UI/HUD/TreadHUDMath.h"

UTreadHUDDataComponent::UTreadHUDDataComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UTreadHUDDataComponent::SetValue(FGameplayTag Tag, float Raw, float Min, float Max)
{
	if (!Tag.IsValid())
	{
		return false;
	}

	FTreadHUDValue* Existing = Values.Find(Tag);
	if (Existing
		&& FMath::IsNearlyEqual(Existing->Raw, Raw)
		&& FMath::IsNearlyEqual(Existing->Min, Min)
		&& FMath::IsNearlyEqual(Existing->Max, Max))
	{
		return false;
	}

	FTreadHUDValue& Value = Existing ? *Existing : Values.Add(Tag);
	Value.Raw = Raw;
	Value.Min = Min;
	Value.Max = Max;
	Value.Normalized = TreadHUDMath::Normalize(Raw, Min, Max);

	OnValueChanged.Broadcast(Tag, Value.Raw, Value.Normalized);
	return true;
}

bool UTreadHUDDataComponent::GetValue(FGameplayTag Tag, float& OutRaw, float& OutNormalized) const
{
	if (const FTreadHUDValue* Value = Values.Find(Tag))
	{
		OutRaw = Value->Raw;
		OutNormalized = Value->Normalized;
		return true;
	}

	OutRaw = 0.0f;
	OutNormalized = 0.0f;
	return false;
}
