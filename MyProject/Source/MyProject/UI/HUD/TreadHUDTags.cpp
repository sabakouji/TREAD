// 踏みつけ加速メカゲーム — HUD の値の行先（GameplayTag）

#include "UI/HUD/TreadHUDTags.h"

namespace TreadHUDTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Speed, "HUD.Speed", "Vehicle speed (uu/s), 0 to MaxSpeed.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Boost, "HUD.Boost", "Boost amount. No game value writes it yet.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(LockOn, "HUD.LockOn", "Lock-on state: 0 none, 1 locked, 2 assisting.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stun, "HUD.Stun", "Remaining stun time (s).");
}
