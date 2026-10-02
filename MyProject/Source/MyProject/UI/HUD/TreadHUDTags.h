// 踏みつけ加速メカゲーム — HUD の値の行先（GameplayTag）

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 * HUD に公開する値の行先。表示部品は詳細パネルでこのタグを選んで値に紐付ける。
 *
 * 新しい HUD 要素を足すときは、ここ（またはプロジェクト設定の GameplayTag）にタグを追加し、
 * 値の出どころで UTreadHUDDataComponent::SetValue を 1 行呼ぶ。表示部品のクラスは増やさない。
 */
namespace TreadHUDTags
{
	/** 機体の速さ（uu/s）。範囲は 0〜MaxSpeed。 */
	MYPROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Speed);

	/** ブースト。ゲーム側に対応する値が無いため、現在は書き込み元が無い（GUI-01 Q2）。 */
	MYPROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Boost);

	/** ロックオンの状態。値は TreadHUDLockOn の定数（未捕捉 / 捕捉中 / アシスト作動中）。 */
	MYPROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(LockOn);

	/** 行動不能の残り時間（秒）。範囲は 0〜行動不能時間の最大値。 */
	MYPROJECT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stun);
}
