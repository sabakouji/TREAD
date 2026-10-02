// 踏みつけ加速メカゲーム — フィールド要素の登録先

#include "Field/FieldSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Field/DestructibleComponent.h"
#include "Field/GoalComponent.h"
#include "Field/TeamComponent.h"

UFieldSubsystem* UFieldSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	return World ? World->GetSubsystem<UFieldSubsystem>() : nullptr;
}

void UFieldSubsystem::RegisterGoal(UGoalComponent* Goal)
{
	if (Goal && !Goals.Contains(Goal))
	{
		Goals.Add(Goal);
		OnGoalRegistered.Broadcast(Goal);
	}
}

void UFieldSubsystem::UnregisterGoal(UGoalComponent* Goal)
{
	Goals.Remove(Goal);
}

UGoalComponent* UFieldSubsystem::FindGoal(int32 TeamId) const
{
	for (const TWeakObjectPtr<UGoalComponent>& Goal : Goals)
	{
		if (Goal.IsValid() && !Goal->IsGoalDestroyed() && UTeamComponent::GetTeamIdOf(Goal->GetOwner()) == TeamId)
		{
			return Goal.Get();
		}
	}

	return nullptr;
}

UGoalComponent* UFieldSubsystem::FindHostileGoal(int32 TeamId) const
{
	for (const TWeakObjectPtr<UGoalComponent>& Goal : Goals)
	{
		if (Goal.IsValid() && !Goal->IsGoalDestroyed()
			&& UTeamComponent::AreTeamsHostile(TeamId, UTeamComponent::GetTeamIdOf(Goal->GetOwner())))
		{
			return Goal.Get();
		}
	}

	return nullptr;
}

void UFieldSubsystem::RegisterFieldPoint(UFieldPointComponent* Point)
{
	if (Point)
	{
		FieldPoints.AddUnique(Point);
	}
}

void UFieldSubsystem::UnregisterFieldPoint(UFieldPointComponent* Point)
{
	FieldPoints.Remove(Point);
}

UFieldPointComponent* UFieldSubsystem::FindNearestFieldPoint(EFieldPointRole Role, int32 TeamId, const FVector& Location) const
{
	UFieldPointComponent* Nearest = nullptr;
	float NearestDistanceSq = TNumericLimits<float>::Max();

	for (const TWeakObjectPtr<UFieldPointComponent>& Point : FieldPoints)
	{
		if (!Point.IsValid() || Point->GetRole() != Role)
		{
			continue;
		}

		const int32 PointTeam = UTeamComponent::GetTeamIdOf(Point->GetOwner());
		if (PointTeam != ImpactTeam::Neutral && PointTeam != TeamId)
		{
			continue;
		}

		const float DistanceSq = FVector::DistSquared(Point->GetComponentLocation(), Location);
		if (DistanceSq < NearestDistanceSq)
		{
			Nearest = Point.Get();
			NearestDistanceSq = DistanceSq;
		}
	}

	return Nearest;
}

void UFieldSubsystem::NotifyObjectBroken(UDestructibleComponent* Destructible)
{
	OnObjectBroken.Broadcast(Destructible);
}
