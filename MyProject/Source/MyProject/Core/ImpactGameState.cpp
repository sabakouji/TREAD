// 踏みつけ加速メカゲーム — 試合の状態

#include "Core/ImpactGameState.h"

#include "Score/ImpactScoreTypes.h"

void AImpactGameState::ConfigureMatch(float InCountdownSeconds, float InMatchDuration)
{
	CountdownRemaining = FMath::Max(InCountdownSeconds, 0.0f);
	MatchDuration = FMath::Max(InMatchDuration, 0.0f);
	ElapsedTime = 0.0f;
	CrashCount = 0;
	MatchResult = EImpactMatchResult::None;
}

void AImpactGameState::SetMatchState(EImpactMatchState NewState)
{
	if (MatchState == NewState)
	{
		return;
	}

	MatchState = NewState;
	UE_LOG(LogImpactMatch, Log, TEXT("match state: %s"), LexToDisplayString(NewState));
	OnMatchStateChanged.Broadcast(NewState);
}

void AImpactGameState::AdvanceCountdown(float DeltaSeconds)
{
	CountdownRemaining = FMath::Max(CountdownRemaining - DeltaSeconds, 0.0f);
}

void AImpactGameState::AdvanceMatchTime(float DeltaSeconds)
{
	ElapsedTime += DeltaSeconds;
}

void AImpactGameState::SetGoalState(bool bInHasGoal, int32 InDurability, int32 InMaxDurability)
{
	bHasGoal = bInHasGoal;
	GoalDurability = bInHasGoal ? InDurability : 0;
	GoalMaxDurability = bInHasGoal ? InMaxDurability : 0;
}

void AImpactGameState::RecordFieldObjectBroken(FName OpenedRouteTag)
{
	++BrokenFieldObjectCount;

	if (!OpenedRouteTag.IsNone())
	{
		OpenedRoutes.Add(OpenedRouteTag);
		UE_LOG(LogImpactMatch, Log, TEXT("route opened: %s"), *OpenedRouteTag.ToString());
	}
}

float AImpactGameState::GetRemainingTime() const
{
	return HasTimeLimit() ? FMath::Max(MatchDuration - ElapsedTime, 0.0f) : 0.0f;
}
