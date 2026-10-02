// 踏みつけ加速メカゲーム — 踏み台の行進ルート

#include "Field/LaneActor.h"

#include "Components/SplineComponent.h"

ALaneActor::ALaneActor()
{
	// ルートは形状を提供するだけで、進むのは踏み台の側。
	PrimaryActorTick.bCanEverTick = false;

	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	SetRootComponent(Spline);
}

float ALaneActor::GetLength() const
{
	return Spline->GetSplineLength();
}

FVector ALaneActor::GetGroundLocationAtDistance(float Distance) const
{
	return Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
}

FVector ALaneActor::GetDirectionAtDistance(float Distance) const
{
	return Spline->GetDirectionAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World).GetSafeNormal2D();
}
