// 踏みつけ加速メカゲーム — NPC 用マーカー

#include "Field/FieldMarkerActor.h"

#include "Components/ArrowComponent.h"
#include "Field/FieldPointComponent.h"
#include "Field/TeamComponent.h"

AFieldMarkerActor::AFieldMarkerActor()
{
	PrimaryActorTick.bCanEverTick = false;

	FieldPoint = CreateDefaultSubobject<UFieldPointComponent>(TEXT("FieldPoint"));
	SetRootComponent(FieldPoint);

	Team = CreateDefaultSubobject<UTeamComponent>(TEXT("Team"));

	DirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("DirectionArrow"));
	DirectionArrow->SetupAttachment(FieldPoint);
	DirectionArrow->SetArrowColor(FLinearColor(1.0f, 0.8f, 0.1f, 1.0f));
}
