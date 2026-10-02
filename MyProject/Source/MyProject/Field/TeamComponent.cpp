// 踏みつけ加速メカゲーム — 陣営

#include "Field/TeamComponent.h"

#include "GameFramework/Actor.h"

UTeamComponent::UTeamComponent()
{
	// 陣営は参照されるだけの値であり、毎フレームの処理を持たない。
	PrimaryComponentTick.bCanEverTick = false;
}

void UTeamComponent::SetTeamId(int32 NewTeamId)
{
	TeamId = FMath::Clamp(NewTeamId, ImpactTeam::Neutral, ImpactTeam::Opponent);
}

int32 UTeamComponent::GetTeamIdOf(const AActor* Actor)
{
	const UTeamComponent* Team = Actor ? Actor->FindComponentByClass<UTeamComponent>() : nullptr;
	return Team ? Team->GetTeamId() : ImpactTeam::Neutral;
}

bool UTeamComponent::AreHostile(const AActor* A, const AActor* B)
{
	return AreTeamsHostile(GetTeamIdOf(A), GetTeamIdOf(B));
}

bool UTeamComponent::AreTeamsHostile(int32 TeamA, int32 TeamB)
{
	return TeamA != ImpactTeam::Neutral && TeamB != ImpactTeam::Neutral && TeamA != TeamB;
}
