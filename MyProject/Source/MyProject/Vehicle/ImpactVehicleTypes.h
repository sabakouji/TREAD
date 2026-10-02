// 踏みつけ加速メカゲーム — 機体の走行状態定義

#pragma once

#include "CoreMinimal.h"
#include "ImpactVehicleTypes.generated.h"

/**
 * 機体の走行状態。
 *
 * ブレーキターンは「勢いを保存したまま弧を描いて方向転換する」操作であり、
 * 通常のブレーキとは速度の扱いが異なるため独立した状態として持つ。
 */
UENUM(BlueprintType)
enum class EVehicleDriveState : uint8
{
	/** 通常走行。速度に応じて旋回角速度が制限される。 */
	Cruising,

	/** ブレーキのみ。減速する。 */
	Braking,

	/** ブレーキターン。速度を保存しつつ、最小旋回半径を守って機首方向を変える。 */
	BrakeTurning,

	/** 行動不能。自滅・撃破の軽いダウン、または拮抗スタン中で、入力を受け付けない。 */
	Stunned,

	/** 弾かれ中。軌道は操作できず、入力を受け付けない（CLAUDE.md §7 の暫定判断）。 */
	KnockedBack
};

/** 走行状態を表示用の文字列に変換する。 */
inline const TCHAR* LexToDisplayString(EVehicleDriveState State)
{
	switch (State)
	{
	case EVehicleDriveState::Cruising:
		return TEXT("Cruising");
	case EVehicleDriveState::Braking:
		return TEXT("Braking");
	case EVehicleDriveState::BrakeTurning:
		return TEXT("BrakeTurning");
	case EVehicleDriveState::Stunned:
		return TEXT("Stunned");
	default:
		return TEXT("Unknown");
	}
}

/**
 * 機体の走行中に起きた、ゲーム進行上意味を持つ出来事。
 *
 * 移動コンポーネントは挙動の計算と出来事の通知に専念し、得点などのルールの判断は
 * 受け取った側（GameMode）が行う。将来サーバ権威で判定する際もルール側を差し替えるだけで済む。
 */
UENUM(BlueprintType)
enum class EVehicleGameplayEvent : uint8
{
	/** 踏み台を踏みつけた。 */
	Stomp,

	/** 小物を破壊した。 */
	DestroyedSmall,

	/** 壁・建物を破壊した。 */
	DestroyedLarge,

	/** 破壊できない対象へ側面・背面から叩きつけられ、自滅した。 */
	Crash,

	/** ゴールにダメージを与えた。 */
	GoalHit,

	/** 段階破壊の建物を損傷させて弾かれた。得点にはならず、演出（手応え）だけに用いる。 */
	StructureDamaged
};

/**
 * 走行中の出来事の通知。Amount は GoalHit では与えたダメージ、それ以外では 1。
 * 演出（エフェクト・サウンド）を Blueprint から繋げられるよう dynamic で宣言する。
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVehicleGameplayEvent, EVehicleGameplayEvent, Event, int32, Amount);
