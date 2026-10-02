// 踏みつけ加速メカゲーム — 機体挙動の調整値アセット

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VehicleTuningDataAsset.generated.h"

/**
 * 機体の挙動調整値を集約する DataAsset。
 *
 * 調整値をコードへ直書きせず本アセットに集約することで、
 * PIE 実行中でも数値を変更して手触りを評価できるようにする。
 */
UCLASS(BlueprintType)
class MYPROJECT_API UVehicleTuningDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	//~ 速度

	/** 初期巡航速度（uu/s）。無入力時に収束する速度。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Speed", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "5000.0"))
	float BaseSpeed = 800.0f;

	/** 速度上限（uu/s）。踏みつけを重ねた到達点。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Speed", meta = (ClampMin = "1.0", UIMin = "1.0", UIMax = "20000.0"))
	float MaxSpeed = 5000.0f;

	/** スロットル入力時の加速（uu/s^2）。停止から BaseSpeed へ到達するまでの立ち上がりを決める。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Speed", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ThrottleAcceleration = 1500.0f;

	/**
	 * 自然減速（uu/s^2）。
	 * スロットルを踏んでいる間は BaseSpeed へ、離している間は 0 へ向けて減衰する。
	 * 踏みつけで得た BaseSpeed 超過分がこの値で失われるため、踏みつけの連鎖を促す速さに調整する。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Speed", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float NaturalDeceleration = 300.0f;

	//~ 旋回・慣性

	/** 停止時の旋回角速度（deg/s）。低速時は自在に旋回できる。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turn", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "720.0"))
	float MaxTurnRateAtRest = 180.0f;

	/** 最高速時の旋回角速度（deg/s）。実質直進しかできない値にする。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turn", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "720.0"))
	float MinTurnRateAtTopSpeed = 20.0f;

	/** 慣性ブレンドの指数。大きいほど高速域で急激に慣性が勝つ。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turn", meta = (ClampMin = "0.1", UIMin = "0.1", UIMax = "5.0"))
	float InertiaExponent = 1.5f;

	//~ 踏みつけ加速

	/** 踏みつけ1回で得る速度（uu/s）。BaseSpeed から MaxSpeed までの到達回数を決める。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stomp", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float StompSpeedGain = 600.0f;

	/**
	 * 踏みつけ時に得る上方向の速度（uu/s）。
	 * 0 で跳ね上がらない。前方接触方式では空中に上がる必要がないため既定は 0 とし、
	 * 手触りを見て跳ねを足したい場合にのみ値を入れる。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stomp", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float StompBounceImpulse = 0.0f;

	/**
	 * 踏みつけ成立と判定する、機首方向と踏み台方向の内積のしきい値（-1..1）。
	 * 0.5 で前方60度以内からの接触を踏みつけとみなす。
	 * 側面・背面からの接触は踏みつけにならず、Phase 6 の通常衝突として扱う。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stomp", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float StompFrontDotThreshold = 0.5f;

	//~ ブレーキ・ブレーキターン

	/** ブレーキ入力時の減速（uu/s^2）。明確に「止まる」と感じる値にする。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Brake", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BrakeDeceleration = 2500.0f;

	/**
	 * ブレーキターン中の旋回角速度の上限（deg/s）。
	 * この上限が最小旋回半径（半径 = 速さ / 角速度）を生み、瞬間反転を防ぐ。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Brake", meta = (ClampMin = "1.0", UIMin = "1.0", UIMax = "720.0"))
	float MaxBrakeTurnAngularSpeed = 140.0f;

	/** ブレーキターン終了時に復帰する速度の割合。1 未満にしてノーリスクにしない。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Brake", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BrakeTurnSpeedRetention = 0.75f;

	/** 速度を保存し続けられる時間の上限（秒）。超過後は保存速度も減衰する。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Brake", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BrakeTurnMaxHoldTime = 2.0f;

	/** ブレーキターン中の機体のロール角（deg）。側面を晒していることを視覚的に示す。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Brake", meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float BrakeTurnRollAngle = 25.0f;

	/** 機体のロール角の追従速度（deg/s）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Brake", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float BodyRollInterpSpeed = 180.0f;

	//~ 当たり判定（バブル）

	/**
	 * 機体同士の接触に使う、全周共通の接触半径（uu）。
	 * 側面・背面の判定を削らないことで、側面を取る戦術の価値を保つ（仕様 1-2）。
	 *
	 * 機体同士の論理的な接触判定にのみ使い、壁・床との当たり判定には使わない（壁や床を押し出さない）。
	 * 壁・床との当たり判定は本体のモデルの大きさを基準にする（AImpactVehiclePawn::ApplyCollisionFromModel）。
	 * 現在は本体のプレースホルダ（Cube 100 uu 立方）に合わせており、本番モデルの導入時に合わせ直す。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bubble", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float BubbleRadius = 50.0f;

	/** 反発力（バンパー）を発生させる前方範囲（deg）。相手がこの角度以内にいるときのみ正面で押し返せる。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bubble", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float BumperAngle = 60.0f;

	/**
	 * 正面接触時の実効半径の上限（uu）。速度比（GetSpeedRatio）に比例して BubbleRadius から拡大する。
	 * 「速いほど当たる／ただし曲がれない」のトレードオフを作る（仕様 1-3）。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bubble", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float BumperRadiusMax = 700.0f;

	//~ 壁・障害物への衝突

	/** 小物（柵など）の破壊に必要な速度（uu/s）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Impact", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SmallDestructionSpeed = 1500.0f;

	/** 壁・建物の破壊に必要な速度（uu/s）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Impact", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float LargeDestructionSpeed = 3200.0f;

	/** 破壊成功時に残る速度の割合。企画書 3-3 の減速ペナルティに相当する。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Impact", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DestroyPenaltyRatio = 0.7f;

	/** 破壊できず激突した際の行動不能時間（秒）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Impact", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CrashStunDuration = 1.5f;

	/**
	 * 破壊できない対象への衝突を「弾き」「自滅」として扱う最低速度（uu/s）。
	 * これ未満での接触は滑走のみで処理し、低速走行中に壁へ触れただけで
	 * 弾かれたり行動不能になったりするのを防ぐ。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Impact", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CrashMinSpeed = 1500.0f;

	//~ 機体同士の衝突

	/**
	 * 機体の質量係数。衝突エネルギー = 生速度 × 質量係数 × 面補正（DA_ImpactTuning の FacingPower）。
	 * 弾きの運動量交換にも用いる。企画書 3-5 のとおり機体は1種類のため、現状は全機体で同じ値になる。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clash", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float MassCoefficient = 1.0f;

	//~ カメラ

	/** 停止時のカメラ距離（uu）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CameraArmLengthAtRest = 700.0f;

	/** 最高速時のカメラ距離（uu）。速度感を出すため引く。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CameraArmLengthAtTopSpeed = 1100.0f;

	/** 停止時の視野角（deg）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "5.0", UIMin = "5.0", UIMax = "170.0"))
	float CameraFovAtRest = 90.0f;

	/** 最高速時の視野角（deg）。広げることで速度感を増幅する。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "5.0", UIMin = "5.0", UIMax = "170.0"))
	float CameraFovAtTopSpeed = 115.0f;

	/** 視点操作の感度（入力1単位あたりの回転角、deg）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float LookSensitivity = 1.0f;

	/** 視点の俯角下限（deg）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "-89.0", ClampMax = "0.0"))
	float CameraPitchMin = -60.0f;

	/** 視点の仰角上限（deg）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float CameraPitchMax = 20.0f;

	/** カメラが機体を追う速さ。大きいほど機体にぴったり付き、小さいほど遅れて追う。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CameraLagSpeed = 8.0f;

	/** カメラアームの基準俯角（deg）。視点操作はこの角度を中心に CameraPitchMin〜Max の範囲で行う。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "-89.0", ClampMax = "0.0"))
	float CameraBasePitch = -15.0f;

	//~ 導出値

	/**
	 * 速度比を返す。BaseSpeed を 0、MaxSpeed を 1 とした 0..1 の値。
	 * BaseSpeed 以下では 0 を返す。
	 */
	UFUNCTION(BlueprintPure, Category = "Vehicle Tuning")
	float GetSpeedRatio(float Speed) const;

	/** 速度に応じた旋回角速度（deg/s）を返す。 */
	UFUNCTION(BlueprintPure, Category = "Vehicle Tuning")
	float GetTurnRateForSpeed(float Speed) const;

	/**
	 * 速度に応じた慣性ブレンド重み（0..1）を返す。
	 * 1 に近いほど入力方向より速度方向が優先される。
	 */
	UFUNCTION(BlueprintPure, Category = "Vehicle Tuning")
	float GetInertiaWeightForSpeed(float Speed) const;

	/** 速度に応じたカメラ距離（uu）を返す。 */
	UFUNCTION(BlueprintPure, Category = "Vehicle Tuning")
	float GetCameraArmLengthForSpeed(float Speed) const;

	/** 速度に応じた視野角（deg）を返す。 */
	UFUNCTION(BlueprintPure, Category = "Vehicle Tuning")
	float GetCameraFovForSpeed(float Speed) const;
};
