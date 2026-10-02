// 踏みつけ加速メカゲーム — 衝突判定の型定義

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectPtr.h"
#include "ImpactTypes.generated.h"

class APawn;
class UVehicleTuningDataAsset;

/**
 * 接触を受けた面。
 *
 * 勝敗のエネルギーに掛ける面補正（FacingPower）と、壁へ叩きつけられた際の自滅判定に用いる。
 * 側面と背面で補正値を分けるため、2分割ではなく3分割を採用している。
 */
UENUM(BlueprintType)
enum class EImpactFacing : uint8
{
	/** 正面（既定では前方60度以内）。最も強い。 */
	Front,

	/** 側面（60〜135度）。ブレーキターン中に晒される向き。 */
	Side,

	/** 背面（135度以上）。最も弱い。 */
	Back
};

/**
 * オブジェクトの破壊難度ランク。
 *
 * 企画書 4章の未決定事項「破壊閾値は単一段階か多段階か」に対し、
 * プロトタイプでは2段階を暫定採用している。
 */
UENUM(BlueprintType)
enum class EDestructionRank : uint8
{
	/** 小物・柵。低い速度で壊せる。 */
	Small,

	/** 壁・建物。相応に速度を積まないと壊せない。 */
	Large
};

/**
 * 壁・障害物への衝突の解決結果。
 *
 * 判定の内訳をそのまま保持し、デバッグ表示で追跡できるようにする。
 * 数値設計の破綻（常に破壊できる / 常に自滅する）は内訳が見えないと切り分けられない。
 */
USTRUCT(BlueprintType)
struct FImpactResolveResult
{
	GENERATED_BODY()

	/** 衝突を受けた面（機首方向から見た向き）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Impact")
	EImpactFacing Facing = EImpactFacing::Front;

	/** 衝突時の生速度（uu/s）。破壊・ダメージの判定はこの値で行う（仕様 2-1）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Impact")
	float ImpactSpeed = 0.0f;

	/**
	 * 面へ向かう速度成分（uu/s）。弾き・自滅はこの値が CrashMinSpeed 以上のときのみ起きる。
	 * 壁沿いを走ってかすっただけの接触を、叩きつけられた激突と区別するため。
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Impact")
	float SurfaceSpeed = 0.0f;

	/** 相手を破壊できたか。 */
	UPROPERTY(BlueprintReadOnly, Category = "Impact")
	bool bDestroyedTarget = false;

	/** 破壊できない対象へ正面から当たり、弾き返されたか。 */
	UPROPERTY(BlueprintReadOnly, Category = "Impact")
	bool bBounced = false;

	/** 破壊できない対象へ側面・背面から叩きつけられ、自滅したか。 */
	UPROPERTY(BlueprintReadOnly, Category = "Impact")
	bool bCrashed = false;

	/** 解決後の速度（uu/s）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Impact")
	float ResultingSpeed = 0.0f;

	/** ゴールに与えたダメージ。ゴール以外への衝突、またはダメージが入らなかった場合は 0。 */
	UPROPERTY(BlueprintReadOnly, Category = "Impact")
	int32 GoalDamage = 0;
};

/** 衝突方向を表示用の文字列に変換する。 */
inline const TCHAR* LexToDisplayString(EImpactFacing Facing)
{
	switch (Facing)
	{
	case EImpactFacing::Front:
		return TEXT("Front");
	case EImpactFacing::Side:
		return TEXT("Side");
	case EImpactFacing::Back:
		return TEXT("Back");
	default:
		return TEXT("Unknown");
	}
}

/** 破壊ランクを表示用の文字列に変換する。 */
inline const TCHAR* LexToDisplayString(EDestructionRank Rank)
{
	switch (Rank)
	{
	case EDestructionRank::Small:
		return TEXT("Small");
	case EDestructionRank::Large:
		return TEXT("Large");
	default:
		return TEXT("Unknown");
	}
}

/**
 * 機体の衝突を受けた対象（IImpactReceiver）の反応。
 *
 * 移動コンポーネントは結果に応じて機体側の減速と通知だけを行い、
 * 対象ごとの規則（ダメージ量・破壊の可否）は対象側の実装が持つ。
 */
UENUM(BlueprintType)
enum class EImpactReceiveOutcome : uint8
{
	/** 影響を受けなかった。機体側では破壊不能の壁として扱う（正面なら弾かれ、側面・背面なら自滅する）。 */
	Unaffected,

	/** 拠点（ゴール）の耐久値を削ったが残っている。機体側ではゴールへの命中として通知し、得点になる。 */
	Damaged,

	/** 破壊された。機体側では破壊ランクに応じた破壊として通知する。 */
	Destroyed,

	/**
	 * 段階破壊の建物が損傷したが残っている。機体側では減速だけを行い、得点にはならない。
	 * 得点になるゴールへの命中（Damaged）と区別するために分けている。
	 */
	Weakened
};

/**
 * 衝突を受けた対象の反応（EImpactReceiveOutcome）に対する、機体側の扱い。
 * 対応は FImpactResolver::DecideReceiveReaction の1箇所で決め、移動コンポーネントはその結果で分岐する。
 */
UENUM(BlueprintType)
enum class EImpactReceiveReaction : uint8
{
	/** 影響なし。破壊不能の壁として扱う（正面なら弾かれ、側面・背面なら自滅する）。 */
	TreatAsWall,

	/** 建物を損傷させたが残っている。減速して弾かれる。どの面で当たっても自滅しない。得点なし。 */
	BounceOffWeakened,

	/** 拠点の耐久値を削った。減速してゴールへの命中として通知する。 */
	ScoreGoalHit,

	/** 破壊した。減速して通り抜け、破壊ランクに応じた破壊として通知する。 */
	PassThroughDestroyed
};

/** 衝突を受けた対象へ渡す、衝突の内訳。 */
USTRUCT(BlueprintType)
struct FImpactReceiveContext
{
	GENERATED_BODY()

	/**
	 * 衝突時の生速度（uu/s）。破壊・ダメージの判定はこの値で行う。
	 * 「超加速しているか」は機体自身の状態であり、どの面で当たったかとは無関係なため面補正を掛けない（仕様 2-1）。
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Impact")
	float ImpactSpeed = 0.0f;

	/** 機体から見た衝突の向き。 */
	UPROPERTY(BlueprintReadOnly, Category = "Impact")
	EImpactFacing Facing = EImpactFacing::Front;

	/** 衝突した機体。 */
	UPROPERTY(BlueprintReadOnly, Category = "Impact")
	TObjectPtr<APawn> Instigator = nullptr;

	/** 衝突した機体の調整値（破壊閾値など）。受け手は読み取りのみ行う。 */
	UPROPERTY(BlueprintReadOnly, Category = "Impact")
	TObjectPtr<UVehicleTuningDataAsset> Tuning = nullptr;

	/** 衝突時の機体の速度ベクトル（uu/s、水平）。破壊演出を突進方向へ飛ばすために用いる。 */
	UPROPERTY(BlueprintReadOnly, Category = "Impact")
	FVector ImpactVelocity = FVector::ZeroVector;

	/** 機体から当たった面へ向かう向き（水平・正規化済み）。受け手の面（正面か否か）の判定に用いる。 */
	UPROPERTY(BlueprintReadOnly, Category = "Impact")
	FVector ImpactDirection = FVector::ForwardVector;

	/** 衝突点（ワールド座標）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Impact")
	FVector ImpactPoint = FVector::ZeroVector;
};

/** 衝突を受けた対象が返す反応。 */
USTRUCT(BlueprintType)
struct FImpactReceiveResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Impact")
	EImpactReceiveOutcome Outcome = EImpactReceiveOutcome::Unaffected;

	/** 与えられたダメージ。Outcome が Damaged のときのみ意味を持つ。 */
	UPROPERTY(BlueprintReadWrite, Category = "Impact")
	int32 Damage = 0;

	/** 破壊された対象のランク。Outcome が Destroyed のときのみ意味を持つ。 */
	UPROPERTY(BlueprintReadWrite, Category = "Impact")
	EDestructionRank Rank = EDestructionRank::Small;
};

/**
 * 機体同士の衝突の分岐（仕様 2-2）。
 * 勝敗（面補正後のエネルギー差）を先に確定させ、その後に勝者の生速度で破壊可否を判定する。
 */
UENUM(BlueprintType)
enum class EVehicleClashBranch : uint8
{
	/** 勝者が超加速していた。敗者を一方的に破壊する。 */
	Destroy,

	/** 勝敗は付いたが、勝者が超加速していない。両者生存し、反発のみ起きる。 */
	Deflect,

	/** エネルギー差が拮抗の幅に収まった。両者スタンし、相互に弾かれる。 */
	ClashStun
};

/** 機体同士の衝突における一方の立場。 */
UENUM(BlueprintType)
enum class EVehicleClashRole : uint8
{
	/** エネルギーで押し勝った。 */
	Winner,

	/** エネルギーで押し負けた。 */
	Loser,

	/** 拮抗した。 */
	Even
};

/** 機体同士の衝突の解決に渡す、一方の機体の状態。 */
USTRUCT(BlueprintType)
struct FVehicleClashBody
{
	GENERATED_BODY()

	/** 衝突時の速度（uu/s）。水平成分のみを用いる。 */
	UPROPERTY(BlueprintReadWrite, Category = "Clash")
	FVector Velocity = FVector::ZeroVector;

	/** 機首方向。接触を受けた面とバンパーの有無の判定に用いる。 */
	UPROPERTY(BlueprintReadWrite, Category = "Clash")
	FVector Forward = FVector::ForwardVector;

	/** 質量係数。呼び出し側が DA_VehicleTuning の MassCoefficient を入れる。 */
	UPROPERTY(BlueprintReadWrite, Category = "Clash")
	float MassCoefficient = 1.0f;

	/** バンパーが働く前方範囲（deg）。呼び出し側が DA_VehicleTuning の BumperAngle を入れる。0 ならバンパーなし。 */
	UPROPERTY(BlueprintReadWrite, Category = "Clash")
	float BumperAngle = 0.0f;
};

/** 機体同士の衝突における片側の内訳。 */
USTRUCT(BlueprintType)
struct FVehicleClashSide
{
	GENERATED_BODY()

	/** この機体が接触を受けた面。 */
	UPROPERTY(BlueprintReadOnly, Category = "Clash")
	EImpactFacing Facing = EImpactFacing::Front;

	/** 衝突時の生速度（uu/s）。破壊可否はこの値で判定する。 */
	UPROPERTY(BlueprintReadOnly, Category = "Clash")
	float RawSpeed = 0.0f;

	/** 接触を受けた面による補正（FacingPower）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Clash")
	float FacingPower = 1.0f;

	/** 勝敗の判定に入るエネルギー。生速度 × 質量係数 × 面補正。 */
	UPROPERTY(BlueprintReadOnly, Category = "Clash")
	float Energy = 0.0f;

	/** バンパー（前方の反発力）が相手に対して働いたか。 */
	UPROPERTY(BlueprintReadOnly, Category = "Clash")
	bool bBumperEngaged = false;

	/** 勝敗上の立場。 */
	UPROPERTY(BlueprintReadOnly, Category = "Clash")
	EVehicleClashRole Role = EVehicleClashRole::Even;

	/** 破壊されたか。 */
	UPROPERTY(BlueprintReadOnly, Category = "Clash")
	bool bDefeated = false;

	/** 弾かれて操作不能になるか（速度変化が KnockbackMinDeltaSpeed 以上）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Clash")
	bool bKnockedBack = false;

	/** 与えるスタン時間（秒）。拮抗のときのみ 0 より大きい。 */
	UPROPERTY(BlueprintReadOnly, Category = "Clash")
	float StunDuration = 0.0f;

	/** 解決後の速度（uu/s、水平）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Clash")
	FVector ResultingVelocity = FVector::ZeroVector;

	/** 解決後の速さ（uu/s）。 */
	float GetResultingSpeed() const { return ResultingVelocity.Size2D(); }
};

/**
 * 機体同士の衝突の解決結果。
 * Self と Other は解決を行った側から見た呼び分けであり、相手視点は Mirrored() で得る。
 */
USTRUCT(BlueprintType)
struct FVehicleClashResult
{
	GENERATED_BODY()

	/** 確定した分岐。 */
	UPROPERTY(BlueprintReadOnly, Category = "Clash")
	EVehicleClashBranch Branch = EVehicleClashBranch::Deflect;

	UPROPERTY(BlueprintReadOnly, Category = "Clash")
	FVehicleClashSide Self;

	UPROPERTY(BlueprintReadOnly, Category = "Clash")
	FVehicleClashSide Other;

	/** Self.Energy - Other.Energy。 */
	UPROPERTY(BlueprintReadOnly, Category = "Clash")
	float EnergyDifference = 0.0f;

	/** 相手視点に反転した結果を返す。 */
	FVehicleClashResult Mirrored() const
	{
		FVehicleClashResult Out;
		Out.Branch = Branch;
		Out.Self = Other;
		Out.Other = Self;
		Out.EnergyDifference = -EnergyDifference;
		return Out;
	}
};

/** 衝突の分岐を表示用の文字列に変換する。 */
inline const TCHAR* LexToDisplayString(EVehicleClashBranch Branch)
{
	switch (Branch)
	{
	case EVehicleClashBranch::Destroy:
		return TEXT("DESTROY");
	case EVehicleClashBranch::Deflect:
		return TEXT("DEFLECT");
	case EVehicleClashBranch::ClashStun:
		return TEXT("CLASH STUN");
	default:
		return TEXT("Unknown");
	}
}

/** 衝突の立場を表示用の文字列に変換する。 */
inline const TCHAR* LexToDisplayString(EVehicleClashRole Role)
{
	switch (Role)
	{
	case EVehicleClashRole::Winner:
		return TEXT("Winner");
	case EVehicleClashRole::Loser:
		return TEXT("Loser");
	case EVehicleClashRole::Even:
		return TEXT("Even");
	default:
		return TEXT("Unknown");
	}
}
