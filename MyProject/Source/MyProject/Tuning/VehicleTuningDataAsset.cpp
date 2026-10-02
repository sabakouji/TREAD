// 踏みつけ加速メカゲーム — 機体挙動の調整値アセット

#include "Tuning/VehicleTuningDataAsset.h"

float UVehicleTuningDataAsset::GetSpeedRatio(float Speed) const
{
	const float Range = MaxSpeed - BaseSpeed;
	if (Range <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	return FMath::Clamp((Speed - BaseSpeed) / Range, 0.0f, 1.0f);
}

float UVehicleTuningDataAsset::GetTurnRateForSpeed(float Speed) const
{
	return FMath::Lerp(MaxTurnRateAtRest, MinTurnRateAtTopSpeed, GetSpeedRatio(Speed));
}

float UVehicleTuningDataAsset::GetInertiaWeightForSpeed(float Speed) const
{
	return FMath::Clamp(FMath::Pow(GetSpeedRatio(Speed), InertiaExponent), 0.0f, 1.0f);
}

float UVehicleTuningDataAsset::GetCameraArmLengthForSpeed(float Speed) const
{
	return FMath::Lerp(CameraArmLengthAtRest, CameraArmLengthAtTopSpeed, GetSpeedRatio(Speed));
}

float UVehicleTuningDataAsset::GetCameraFovForSpeed(float Speed) const
{
	return FMath::Lerp(CameraFovAtRest, CameraFovAtTopSpeed, GetSpeedRatio(Speed));
}
