// 踏みつけ加速メカゲーム — 機体の移動コンポーネント

#pragma once

#include "CoreMinimal.h"
#include "Collision/ImpactTypes.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Vehicle/ImpactVehicleTypes.h"
#include "ImpactVehicleMovementComponent.generated.h"

class AActor;
class AImpactVehiclePawn;
class UImpactTuningDataAsset;
class UVehicleTuningDataAsset;

/**
 * 機体の走行・慣性・旋回を担う移動コンポーネント。
 *
 * UCharacterMovementComponent は本企画の慣性モデル（速度が上がるほど旋回できなくなる）と
 * 噛み合わないため使用せず、UPawnMovementComponent から独自に実装する。
 *
 * 1フレームの処理順は以下に固定する:
 *   入力取得 → 目標方向の決定 → 慣性ブレンド → 旋回角の制限適用
 *     → 速度の更新（加速 / 自然減速） → 移動適用（SafeMoveUpdatedComponent）
 *     → 衝突の処理
 * 行動不能中・弾かれ中は入力を読まず、速度方向を固定したまま減速と移動適用・衝突の処理のみ行う。
 *
 * この順序は将来のサーバ権威型ネットワーク実装で「入力 → シミュレーション → 結果適用」を
 * 分離できるようにするための前提であり、崩さないこと。
 */
UCLASS()
class MYPROJECT_API UImpactVehicleMovementComponent : public UPawnMovementComponent
{
	GENERATED_BODY()

public:
	UImpactVehicleMovementComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual float GetMaxSpeed() const override;

	/** 移動入力を設定する。X = 旋回（左右）、Y = スロットル（前進）。 */
	void SetMoveInput(const FVector2D& InMoveInput);

	/** ブレーキ入力を設定する。 */
	void SetBrakeInput(bool bInBraking);

	/**
	 * ロックオンのステアリングアシストによる角速度（deg/s、正で右）を設定する。
	 * 旋回入力と同じく「入力」として扱い、通常の旋回角速度に加算する（仕様 3-3）。
	 * 毎フレーム設定すること。ロックオンが外れた場合は 0 を渡す。
	 */
	void SetAssistYawRate(float DegreesPerSecond);

	/** 現在の走行状態。 */
	UFUNCTION(BlueprintPure, Category = "Vehicle Movement")
	EVehicleDriveState GetDriveState() const { return DriveState; }

	/** ブレーキターン中に保存している速度（uu/s）。 */
	UFUNCTION(BlueprintPure, Category = "Vehicle Movement")
	float GetStoredSpeed() const { return StoredSpeed; }

	/**
	 * 現在の旋回半径（uu）。旋回入力が無い、または速度が無い場合は 0 を返す。
	 * 半径 = 速さ / 角速度（rad/s）で求まる。
	 */
	UFUNCTION(BlueprintPure, Category = "Vehicle Movement")
	float GetTurnRadius() const;

	/** 現在の速さ（uu/s）。 */
	UFUNCTION(BlueprintPure, Category = "Vehicle Movement")
	float GetCurrentSpeed() const { return CurrentSpeed; }

	/** 現在の水平速度（uu/s）。実際に進んでいる方向 × 速さ。 */
	UFUNCTION(BlueprintPure, Category = "Vehicle Movement")
	FVector GetHorizontalVelocity() const { return MoveDirection * CurrentSpeed; }

	/** 現在の慣性ブレンド重み（0..1）。1 に近いほど入力より慣性が優先される。 */
	UFUNCTION(BlueprintPure, Category = "Vehicle Movement")
	float GetInertiaWeight() const { return InertiaWeight; }

	/** 現在の旋回可能角速度（deg/s）。 */
	UFUNCTION(BlueprintPure, Category = "Vehicle Movement")
	float GetTurnRate() const { return TurnRate; }

	/** 実際に進んでいる方向。旋回中は機首方向と一致しない。 */
	UFUNCTION(BlueprintPure, Category = "Vehicle Movement")
	FVector GetMoveDirection() const { return MoveDirection; }

	/** 接地しているか。 */
	UFUNCTION(BlueprintPure, Category = "Vehicle Movement")
	bool IsGrounded() const { return bGrounded; }

	/** 機首方向と実進行方向のなす角（deg）。旋回中にどれだけ横を向いているかを表す。 */
	UFUNCTION(BlueprintPure, Category = "Vehicle Movement")
	float GetDriftAngleDegrees() const;

	/** これまでに成立した踏みつけの回数。 */
	UFUNCTION(BlueprintPure, Category = "Vehicle Movement")
	int32 GetStompCount() const { return StompCount; }

	/** 直近の踏みつけで得た速度（uu/s）。 */
	UFUNCTION(BlueprintPure, Category = "Vehicle Movement")
	float GetLastStompGain() const { return LastStompGain; }

	/** 直近の壁・障害物への衝突の内訳。数値設計の破綻を追跡するために保持する。 */
	UFUNCTION(BlueprintPure, Category = "Vehicle Movement")
	const FImpactResolveResult& GetLastImpact() const { return LastImpact; }

	/** 一度でも壁・障害物への衝突を解決したか。 */
	UFUNCTION(BlueprintPure, Category = "Vehicle Movement")
	bool HasResolvedImpact() const { return bHasResolvedImpact; }

	/** 行動不能の残り時間（秒）。0 なら操作可能。 */
	UFUNCTION(BlueprintPure, Category = "Vehicle Movement")
	float GetStunRemaining() const { return StunRemaining; }

	/** 弾かれて操作できない残り時間（秒）。0 なら操作可能。 */
	UFUNCTION(BlueprintPure, Category = "Vehicle Movement")
	float GetKnockbackRemaining() const { return KnockbackRemaining; }

protected:
	/** 所有 Pawn から機体の調整値アセットを取得する。未設定なら nullptr。 */
	const UVehicleTuningDataAsset* GetTuning() const;

	/** 所有 Pawn から追突・ロックオンの規則アセットを取得する。未設定なら nullptr。 */
	const UImpactTuningDataAsset* GetImpactTuning() const;

	/** 入力から走行状態を決定し、状態遷移に伴う速度の保存・復帰を行う。 */
	void UpdateDriveState(float DeltaTime, const UVehicleTuningDataAsset& Tuning);

	/** 機首方向を旋回入力に応じて回す。速度が高いほど旋回角速度が下がる。 */
	void ApplyTurn(float DeltaTime, const UVehicleTuningDataAsset& Tuning);

	/** 機首方向と現在の進行方向を速度依存の重みでブレンドする。 */
	void ApplyInertiaBlend(const UVehicleTuningDataAsset& Tuning);

	/** スロットル入力と自然減速から速さを更新する。 */
	void UpdateSpeed(float DeltaTime, const UVehicleTuningDataAsset& Tuning);

	/**
	 * 行動不能中・弾かれ中の更新。入力を読まず、速度方向を固定したまま減速する。
	 * 弾かれた軌道は操作できない（CLAUDE.md §7 の暫定判断）。
	 */
	void UpdateUncontrolled(float DeltaTime, const UImpactTuningDataAsset& Rules);

	/** 重力による落下速度を更新する。 */
	void UpdateVerticalSpeed(float DeltaTime);

	/**
	 * 算出した速度でコンポーネントを移動させる。水平の移動と垂直の移動（重力・地面への押し付け）を別々に掃引する。
	 *
	 * 1回の掃引にまとめると、接地中は最初の衝突が床（距離ほぼ 0）になることがあり、壁・建物への衝突が
	 * 床に沿って滑らせる途中のものとして扱われて、衝突の判定から漏れていた（FIX-08）。
	 * 水平の掃引では床が衝突相手にならないため、壁・建物・機体への衝突を必ず判定に回せる。
	 */
	void ApplyMovement(float DeltaTime);

	/**
	 * 水平の移動中に衝突した際の処理。相手が機体なら追突、それ以外は壁・障害物として解決し、面に沿って滑らせる。
	 * Hit は SlideAlongSurface が滑走後の結果で更新するため非 const で受ける。
	 */
	virtual void HandleMoveHit(FHitResult& Hit, float DeltaTime);

	/**
	 * 面に沿って滑らせる途中（SlideAlongSurface）で別の面に当たったときにエンジンから呼ばれる。
	 * 壁に沿って滑りながら建物や機体に当たった場合も、最初の衝突と同じ判定に回す（FIX-08）。
	 */
	virtual void HandleImpact(const FHitResult& Hit, float TimeSlice = 0.0f, const FVector& MoveDelta = FVector::ZeroVector) override;

	/**
	 * 壁・障害物への衝突を解決する（仕様 2-7）。
	 *
	 * 対象が影響を受けた（破壊・ダメージ）なら減速して通知する。
	 * 破壊できなかった場合、面へ向かう速度が CrashMinSpeed 以上なら、正面は弾かれて減速、側面・背面は自滅する。
	 */
	void ResolveWallImpact(FHitResult& Hit, const UVehicleTuningDataAsset& Tuning, const UImpactTuningDataAsset& Rules);

	/** 破壊できない面へ正面から当たった際に、反発係数で弾き返す。 */
	void ApplyWallBounce(const FHitResult& Hit, const UImpactTuningDataAsset& Rules);

	/** 破壊できない対象へ叩きつけられた際の自滅処理（軽いダウン）。 */
	void ApplyCrash(const UVehicleTuningDataAsset& Tuning);

	/**
	 * 機体同士の衝突を解決する（仕様 2-2）。
	 *
	 * 双方の移動は同じフレーム内で互いにヒットするため、先に検出した側が
	 * 双方分をまとめて解決し、双方にクールダウンを設定して二重解決を防ぐ。
	 *
	 * @return 解決した場合 true。クールダウン中などで解決しなかった場合 false。
	 */
	bool ResolveVehicleClash(AImpactVehiclePawn& OtherVehicle, const UVehicleTuningDataAsset& Tuning, const UImpactTuningDataAsset& Rules);

	/**
	 * バンパーを含む機体同士の接触を検出する（仕様 1-1）。
	 *
	 * 移動のスイープは本体のモデル基準の当たり判定でしか当たらないため、バブルの半径（BubbleRadius）と
	 * 正面で速度に応じて広がる実効半径での接触はここで見る。機体（Pawn）だけを調べ、壁や床には作用しない。
	 * 接触の条件は「中心間の距離 ≦ 双方の実効半径の和」かつ「近づいている」。
	 * 解決は ResolveVehicleClash に一本化し、クールダウンで二重解決を防ぐ。
	 */
	void DetectBumperContacts(const UVehicleTuningDataAsset& Tuning, const UImpactTuningDataAsset& Rules);

public:
	/**
	 * 踏みつけの対象（IStompable を実装したアクタ）への接触を受けて、踏みつけが成立するか判定し、成立するなら加速を与える。
	 *
	 * 機首方向から見て前方から接触した場合のみ成立する。
	 * 側面・背面からの接触は踏みつけにならない。
	 *
	 * @param Target  重なった相手。IStompable を実装していなければ何もしない。
	 * @return 踏みつけが成立した場合に true。
	 */
	bool TryStompTarget(AActor* Target);

	/** 追突の解決に渡す、この機体の状態を組み立てる。 */
	FVehicleClashBody MakeClashBody(const UVehicleTuningDataAsset& Tuning) const;

	/** 機体同士の衝突結果のうち、この機体側の結果（破壊 / 弾き / 拮抗スタン）を反映する。 */
	void ApplyClashSide(const FVehicleClashSide& Side);

	/** 直近の機体同士の衝突の内訳を記録する。この機体から見た視点で渡すこと。 */
	void RecordClash(const FVehicleClashResult& Result);

	/** 機体同士の衝突のクールダウンを開始する。 */
	void StartClashCooldown();

	/** 機体同士の衝突のクールダウン中か。 */
	bool IsInClashCooldown() const;

	/** 直近の機体同士の衝突の内訳。この機体から見た視点。 */
	const FVehicleClashResult& GetLastClash() const { return LastClash; }

	/** 一度でも機体同士の衝突を解決したか。 */
	bool HasResolvedClash() const { return bHasResolvedClash; }

	/** 外部要因で行動不能にする。撃破時の軽いダウンに用いる。 */
	void ForceCrash();

	//~ デバッグ操作（Shipping ビルドでは入力がバインドされない）

	/**
	 * 踏み台を1回踏んだときと同じ加速を与える。
	 * 踏みつけの回数には数えない。スコア（Phase 8）に混ざらないようにするため。
	 */
	void DebugApplyStompBoost();

	/** 加速の維持を切り替える。ON の間、スロットル中に BaseSpeed を超えた分が自然減速しない。 */
	void ToggleDebugHoldBoost();

	/** 加速の維持が有効か。 */
	bool IsDebugHoldBoost() const { return bDebugHoldBoost; }

	/**
	 * 走行中の出来事（踏みつけ・破壊・自滅・ゴールへのダメージ）の通知。
	 * 得点などのルールの判断は受け取った側（GameMode）が行う。Blueprint からも購読できる。
	 */
	UPROPERTY(BlueprintAssignable, Category = "Vehicle Movement|Events")
	FOnVehicleGameplayEvent OnGameplayEvent;

private:
	/** 踏みつけ1回分の加速を適用する。実際の踏みつけとデバッグ操作で共用する。 */
	void ApplyStompGain(const UVehicleTuningDataAsset& Tuning);

	/** 水平速度を「進行方向 × 速さ」に分けて設定する。速さが無視できるほど小さければ方向は変えない。 */
	void SetHorizontalVelocity(const FVector& NewVelocity);

	/**
	 * 弾かれ状態を開始する。ブレーキターンで保存していた勢いは失う。
	 * Duration が 0 なら弾かれ状態にはせず、勢いの破棄と残り時間の解除だけを行う（行動不能の開始時に用いる）。
	 */
	void StartKnockback(float Duration);

	/**
	 * 床以外への衝突を、相手に応じて追突（機体）か壁・障害物として解決する。
	 * 同じ移動の中で既に解決した相手は、二重に減速・弾き・破壊しないよう無視する。
	 */
	void ResolveBlockingHit(FHitResult& Hit);

	/** 接地したことを記録する。落下速度を打ち切る。 */
	void Land();

	/** 速さ（HUD.Speed）と行動不能の残り時間（HUD.Stun）を機体の HUD の値の置き場へ書き込む。 */
	void PublishHUDValues(const UVehicleTuningDataAsset& Tuning, const UImpactTuningDataAsset& Rules) const;

	/** 今回の移動で既に衝突を解決した相手。移動のたびに空にする。 */
	TArray<TWeakObjectPtr<AActor>, TInlineAllocator<4>> ResolvedThisMove;

	/** 加速の維持（デバッグ操作）。 */
	bool bDebugHoldBoost = false;

	/** 直近の機体同士の衝突の内訳。この機体から見た視点。 */
	FVehicleClashResult LastClash;

	/** 一度でも機体同士の衝突を解決したか。HUD で未発生と区別するために持つ。 */
	bool bHasResolvedClash = false;

	/**
	 * 機体同士の衝突のクールダウンが明ける時刻（ワールド時間、秒）。
	 * 残り時間を毎フレーム減算する方式にすると Tick 処理への依存が増えるため、時刻で持つ。
	 */
	double ClashCooldownEndTime = 0.0;

	/** 今フレームの移動入力。X = 旋回、Y = スロットル。 */
	FVector2D MoveInput = FVector2D::ZeroVector;

	/** 今フレームのブレーキ入力。 */
	bool bBrakeInput = false;

	/** 今フレームのアシスト角速度（deg/s）。ロックオンのコンポーネントが毎フレーム設定する。 */
	float AssistYawRate = 0.0f;

	/** 現在の走行状態。 */
	EVehicleDriveState DriveState = EVehicleDriveState::Cruising;

	/** ブレーキターン開始時に退避した速度（uu/s）。 */
	float StoredSpeed = 0.0f;

	/** ブレーキターンを継続している時間（秒）。 */
	float BrakeTurnElapsed = 0.0f;

	/** 水平方向の速さ（uu/s）。 */
	float CurrentSpeed = 0.0f;

	/** 鉛直方向の速度（uu/s）。負値が落下。 */
	float VerticalSpeed = 0.0f;

	/** 実際に進んでいる方向（水平・正規化済み）。 */
	FVector MoveDirection = FVector::ForwardVector;

	/** 今フレームの慣性ブレンド重み。デバッグ表示用に保持する。 */
	float InertiaWeight = 0.0f;

	/** 今フレームの旋回角速度（deg/s）。デバッグ表示用に保持する。 */
	float TurnRate = 0.0f;

	/** 接地判定。 */
	bool bGrounded = false;

	/** 成立した踏みつけの累計回数。 */
	int32 StompCount = 0;

	/** 直近の踏みつけで実際に得た速度（uu/s）。上限で頭打ちになった分は含まない。 */
	float LastStompGain = 0.0f;

	/** 直近の壁・障害物への衝突の内訳。 */
	FImpactResolveResult LastImpact;

	/** 一度でも壁・障害物への衝突を解決したか。HUD で未発生と区別するために持つ。 */
	bool bHasResolvedImpact = false;

	/** 行動不能の残り時間（秒）。 */
	float StunRemaining = 0.0f;

	/** 弾かれて操作できない残り時間（秒）。 */
	float KnockbackRemaining = 0.0f;
};
