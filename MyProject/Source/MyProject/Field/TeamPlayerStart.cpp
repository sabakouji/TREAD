// 踏みつけ加速メカゲーム — 陣営付きの開始地点

#include "Field/TeamPlayerStart.h"

#include "Field/TeamComponent.h"

ATeamPlayerStart::ATeamPlayerStart(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Team = CreateDefaultSubobject<UTeamComponent>(TEXT("Team"));
	Team->SetTeamId(ImpactTeam::Player);
}
