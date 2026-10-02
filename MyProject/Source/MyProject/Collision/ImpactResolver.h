// 踏みつけ加速メカゲーム — 衝突解決

#pragma once

#include "CoreMinimal.h"
#include "Collision/ImpactTypes.h"

class UImpactTuningDataAsset;
class UVehicleTuningDataAsset;

/**
 * 衝突の判定と解決を担う。
 *
 * 判定ロジックを Pawn や Actor へ散らさず本クラスへ集約する。
 * 自動テスト（Tests/ImpactResolverTests.cpp）と将来のサーバ権威型ネットワーク実装で
 * 同じ判定を再利用できるようにするため、状態を持たない静的関数の集合とする。
 *
 * 仕様: Plan/踏みつけ加速メカゲーム_追突・ロックオン仕様.md
 */
struct MYPROJECT_API FImpactResolver
{
	/**
	 * 接触を受けた面を判定する。
	 *
	 * @param Forward          判定する側の機首方向
	 * @param ImpactDirection  接触が到来した方向（判定する側から相手・壁へ向かう方向）
	 * @return 正面 / 側面 / 背面
	 */
	static EImpactFacing DetermineFacing(
		const FVector& Forward,
		const FVector& ImpactDirection,
		const UImpactTuningDataAsset& Rules);

	/** 接触を受けた面に応じたエネルギー補正（FacingPower）を返す。 */
	static float GetFacingPower(EImpactFacing Facing, const UImpactTuningDataAsset& Rules);

	/** 指定ランクのオブジェクトを破壊するのに必要な速度（uu/s）を返す。 */
	static float GetRequiredSpeed(EDestructionRank Rank, const UVehicleTuningDataAsset& Tuning);

	/**
	 * 現在の速度で破壊できる最上位のランクを返す。
	 * 何も破壊できない場合は false を返す。
	 */
	static bool GetDestroyableRank(float Speed, const UVehicleTuningDataAsset& Tuning, EDestructionRank& OutRank);

	/** 相手のいる方向が機首から BumperAngle 以内か（バンパーが働くか）。 */
	static bool IsBumperEngaged(const FVector& Forward, const FVector& Direction, float BumperAngle);

	/**
	 * 指定方向への接触半径（uu）を返す（仕様 1-1）。
	 * 方向がバンパーの範囲内なら速度比に比例して BubbleRadius から BumperRadiusMax へ拡大し、
	 * それ以外は全周共通の BubbleRadius を返す。
	 */
	static float GetContactRadius(
		const FVector& Forward,
		const FVector& Direction,
		float Speed,
		const UVehicleTuningDataAsset& Tuning);

	/**
	 * 機体同士の衝突を解決する（仕様 2-2）。
	 *
	 * 1. 各機体のエネルギー = 生速度 × 質量係数 × 自分が接触を受けた面の補正
	 * 2. 差が ClashThreshold 以内 → 拮抗スタン（両者スタン＋相互に離れる）
	 * 3. そうでなければ勝者を確定し、勝者の生速度が OverdriveSpeed 以上 → 破壊、未満 → 弾き
	 *
	 * 弾きは中心同士を結ぶ法線方向の運動量交換で解く。相手へ与える撃力は、自分のバンパーが
	 * 働いていれば反発係数のぶん大きくなる。側面・背面で受けた側は押し返せないため、
	 * 当てた側は勢いを保ち、受けた側だけが一方的に飛ばされる（仕様 2-5）。
	 *
	 * @param Self / Other   各機体の状態
	 * @param SelfToOther    Self の中心から Other の中心への方向（水平）
	 */
	static FVehicleClashResult ResolveVehicleClash(
		const FVehicleClashBody& Self,
		const FVehicleClashBody& Other,
		const FVector& SelfToOther,
		const UImpactTuningDataAsset& Rules);

	/**
	 * 破壊できない面へ正面から当たったときの反射速度を返す（仕様 2-7）。
	 * v' = v - (1 + 反発係数)(v・n)n。面から離れる向きに動いている場合は変えない。
	 *
	 * @param SurfaceNormal  面の法線（機体側を向く）
	 */
	static FVector ComputeWallBounce(const FVector& Velocity, const FVector& SurfaceNormal, float Restitution);

	/**
	 * 衝突を受けた対象の反応に対する、機体側の扱いを返す。
	 * 影響なし以外はいずれも減速ペナルティ（DestroyPenaltyRatio）を受ける。
	 * 損傷だけなら建物は残っているため弾かれる。弾かないと次のフレームも同じ建物に当たり、段階がまとめて進んでしまう。
	 */
	static EImpactReceiveReaction DecideReceiveReaction(EImpactReceiveOutcome Outcome);
};
