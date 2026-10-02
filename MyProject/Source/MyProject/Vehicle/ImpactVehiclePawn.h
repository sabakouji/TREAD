// 踏みつけ加速メカゲーム — プレイヤー機体 Pawn

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ImpactVehiclePawn.generated.h"

class UBoxComponent;
class UCameraComponent;
class UImpactLockOnComponent;
class UImpactTuningDataAsset;
class UImpactVehicleMovementComponent;
class UInputAction;
class UMaterialInstanceDynamic;
class USpringArmComponent;
class UStaticMeshComponent;
class UTeamComponent;
class UTreadHUDDataComponent;
class UVehicleTuningDataAsset;
struct FInputActionValue;

/** 機体の設定不備・追突の解決に関するログカテゴリ。 */
DECLARE_LOG_CATEGORY_EXTERN(LogImpactVehicle, Log, All);

/**
 * 機体の基底 Pawn。
 *
 * ルートは当たり判定専用の UBoxComponent とし、メッシュはその子の見た目専用とする。
 * 壁・床との当たり判定は本体のモデル（BodyMesh）の境界を基準にし、BeginPlay でモデルに合わせる。
 * 機体同士の接触に使うバブルの半径（DA_VehicleTuning の BubbleRadius）は論理的な判定だけに使い、壁や床を押し出さない。
 * UE のスポーン・移動処理は、干渉時の退避位置探索でルートがコライダであることを前提とするため、
 * StaticMesh をルートにすると退避に失敗する。
 *
 * 3D モデルが未完成のため見た目は UE 標準の基本形状を Blueprint 側で割り当てる運用とし、
 * C++ 側ではメッシュアセットを参照しない。
 */
UCLASS()
class MYPROJECT_API AImpactVehiclePawn : public APawn
{
	GENERATED_BODY()

public:
	AImpactVehiclePawn();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** 現在の速さ（uu/s）。 */
	UFUNCTION(BlueprintPure, Category = "Vehicle")
	float GetCurrentSpeed() const;

	/** 挙動調整値。速度・旋回・カメラの全パラメータをここから取得する。 */
	UFUNCTION(BlueprintPure, Category = "Vehicle")
	UVehicleTuningDataAsset* GetTuning() const { return Tuning; }

	/** 追突・面判定・ロックオンの規則。 */
	UFUNCTION(BlueprintPure, Category = "Vehicle")
	UImpactTuningDataAsset* GetImpactTuning() const { return ImpactTuning; }

	/**
	 * 機体同士の衝突で撃破された際の処理。
	 * 自機は企画書 4章の暫定判断どおり軽いダウン（行動不能）とし、敵機は派生クラスで消滅させる。
	 * 敗者の扱いは今後変更の可能性が高いため、Blueprint 派生からも上書きできるようにする。
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Vehicle")
	void HandleDefeatedInClash();
	virtual void HandleDefeatedInClash_Implementation();

	/**
	 * 操作を受け付けるかを切り替える。カウントダウン中と試合終了後に GameMode が止める。
	 * 止めた時点で入力を離したことにし、押しっぱなしの入力が残らないようにする。視点操作は止めない。
	 */
	void SetControlsLocked(bool bLocked);

	/** 操作が止められているか。 */
	UFUNCTION(BlueprintPure, Category = "Vehicle")
	bool AreControlsLocked() const { return bControlsLocked; }

	UImpactVehicleMovementComponent* GetVehicleMovement() const { return VehicleMovement; }
	UImpactLockOnComponent* GetLockOn() const { return LockOn; }

	UTeamComponent* GetTeam() const { return Team; }
	UTreadHUDDataComponent* GetHUDData() const { return HUDData; }
	UBoxComponent* GetCollisionBox() const { return CollisionBox; }
	UStaticMeshComponent* GetBodyMesh() const { return BodyMesh; }
	UStaticMeshComponent* GetBubbleMesh() const { return BubbleMesh; }
	USpringArmComponent* GetSpringArm() const { return SpringArm; }
	UCameraComponent* GetCamera() const { return Camera; }

protected:
	/** 挙動調整値アセット。Blueprint 側で DA_VehicleTuning を割り当てる。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle")
	TObjectPtr<UVehicleTuningDataAsset> Tuning;

	/**
	 * 追突・面判定・ロックオンの規則。Blueprint 側で DA_ImpactTuning を割り当てる。
	 * 試合の規則なので、全機体に同じアセットを割り当てること。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle")
	TObjectPtr<UImpactTuningDataAsset> ImpactTuning;

	/** 壁・床との当たり判定（本体のモデル基準）。ルートコンポーネントであり、移動処理はこれを動かす。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> CollisionBox;

	/** 中の機体の見た目。当たり判定は持たない。メッシュは Blueprint 側で割り当てる。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** バブルの見た目。半透明の球で、機体同士の接触半径（BubbleRadius）を見せる。当たり判定は持たない。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BubbleMesh;

	/** バブルの色。自機と敵機の識別に使う。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Style|Bubble")
	FLinearColor BubbleColor = FLinearColor(0.15f, 0.55f, 1.0f, 1.0f);

	/** バブルの不透明度。中の機体が見える程度にする。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Style|Bubble", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BubbleOpacity = 0.25f;

	/**
	 * 超加速（OverdriveSpeed 以上）のときのバブルの色。
	 * 「今この相手は壊しにくる状態か」が相手からも読めるようにする（仕様 5章）。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Style|Bubble")
	FLinearColor BubbleOverdriveColor = FLinearColor(1.0f, 0.45f, 0.05f, 1.0f);

	/** 通常時のバブルの発光の強さ。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Style|Bubble", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BubbleEmissive = 0.5f;

	/** 超加速時のバブルの発光の強さ。遠くからでも状態が分かる値にする。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Style|Bubble", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BubbleOverdriveEmissive = 6.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	/** 走行・慣性・旋回を担う移動コンポーネント。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UImpactVehicleMovementComponent> VehicleMovement;

	/** ロックオンとステアリングアシスト。移動コンポーネントより先に Tick する。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UImpactLockOnComponent> LockOn;

	/** 所属陣営。ロックオン対象の絞り込み・拠点へのダメージ可否・NPC の敵味方判定に用いる。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTeamComponent> Team;

	/**
	 * HUD に公開する値（速度・行動不能・ロックオン）の置き場。移動・ロックオンの各コンポーネントが書き込む。
	 * 表示部品は所有プレイヤーの Pawn からこれを探す。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTreadHUDDataComponent> HUDData;

	/** 移動入力。X = 旋回、Y = スロットル。Blueprint 側で IA_Move を割り当てる。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	/** 視点操作。Blueprint 側で IA_Look を割り当てる。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	/** ブレーキ。Blueprint 側で IA_Brake を割り当てる。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> BrakeAction;

	/** デバッグ: 踏みつけ1回分の加速を得る。Blueprint 側で IA_DebugBoost を割り当てる。Shipping では無効。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Debug")
	TObjectPtr<UInputAction> DebugBoostAction;

	/** デバッグ: 加速の維持を切り替える。Blueprint 側で IA_DebugToggleHold を割り当てる。Shipping では無効。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Debug")
	TObjectPtr<UInputAction> DebugToggleHoldAction;

	/** 踏み台との重なりを受けて踏みつけ判定へ渡す。 */
	UFUNCTION()
	void OnBodyBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	void OnMoveInput(const FInputActionValue& Value);
	void OnMoveReleased(const FInputActionValue& Value);
	void OnLookInput(const FInputActionValue& Value);
	void OnBrakeInput(const FInputActionValue& Value);
	void OnBrakeReleased(const FInputActionValue& Value);
	void OnDebugBoost(const FInputActionValue& Value);
	void OnDebugToggleHold(const FInputActionValue& Value);

	/** 壁・床との当たり判定の大きさを、本体のモデル（BodyMesh）の境界に合わせる。 */
	void ApplyCollisionFromModel();

	/** バブルの見た目の大きさを DA_VehicleTuning の BubbleRadius（機体同士の接触半径）に合わせる。 */
	void ApplyBubbleSize();

	/** バブルの材質（色・不透明度・発光）を適用する。 */
	void ApplyBubbleVisual();

	/** 超加速の状態に応じてバブルの色と発光を切り替える。状態が変わったときだけ書き換える。 */
	void UpdateOverdriveVisual();

	/** バブルの動的マテリアル。色の切り替えに使う。 */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BubbleMaterial;

private:
	/** カメラの追従の遅れと基準俯角を DA_VehicleTuning の値で設定する。 */
	void ApplyCameraTuning();

	/** 速度に応じてカメラ距離と視野角を更新する。 */
	void UpdateCameraForSpeed(float Speed);

	/** ブレーキターン中に機体を傾け、側面を晒していることを視覚的に示す。 */
	void UpdateBodyRoll(float DeltaSeconds);

	/** ロックオン中のカメラを、対象方向へ緩やかに寄せる（仕様 3-2）。 */
	void UpdateLockOnCamera(float DeltaSeconds);

	/** 視点操作とロックオンの寄せを合わせて、カメラアームの向きを決める。 */
	void UpdateCameraRotation();

	/** 視点操作によるカメラアームの回転量（Pitch, Yaw）。 */
	FRotator CameraArmOffset = FRotator::ZeroRotator;

	/** 現在の機体ロール角（deg）。 */
	float CurrentBodyRoll = 0.0f;

	/** 今フレームの旋回入力。ロールの向きを決めるために保持する。 */
	float LastTurnInput = 0.0f;

	/** ロックオンによるカメラの寄せ量（機体基準のヨー、deg）。 */
	float LockOnCameraYaw = 0.0f;

	/** 今バブルに適用している表示が超加速のものか。切り替えの取りこぼしを防ぐために持つ。 */
	bool bOverdriveVisualActive = false;

	/** 操作が止められているか。 */
	bool bControlsLocked = false;
};
