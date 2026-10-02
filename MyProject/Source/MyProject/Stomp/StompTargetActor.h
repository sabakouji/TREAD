// 踏みつけ加速メカゲーム — 踏み台（戦車）アクタ

#pragma once

#include "CoreMinimal.h"
#include "Collision/Stompable.h"
#include "GameFramework/Actor.h"
#include "StompTargetActor.generated.h"

class ALaneActor;
class UBoxComponent;
class UStaticMeshComponent;

/**
 * 機体が踏みつけて加速するための踏み台。企画書における「行進してくる戦車」に相当する。
 *
 * 拠点から一定間隔で供給され、レーン（スプライン）に沿って、レーンが無ければ直進で行進する（MOBA のミニオン相当）。
 * 上面を踏まれた場合のみ踏みつけとして成立し、側面への接触は通常の衝突として扱う。
 *
 * レーンに沿う間は、前方が壁や建物（WorldStatic）で塞がれていればその場で止まって待ち、建物が壊されて道が開けば再開する。
 * 前の踏み台が止まっていれば、車間（FollowSpacing）を空けて後ろで待つ。
 * レーンに沿わない直進の踏み台は、従来どおり壁をすり抜けて進み、寿命で消える。
 */
UCLASS()
class MYPROJECT_API AStompTargetActor : public AActor, public IStompable
{
	GENERATED_BODY()

public:
	AStompTargetActor();

	virtual void Tick(float DeltaSeconds) override;

	/** 行進する方向を設定する。水平成分のみ使用する。レーンに沿わず直進する。 */
	void SetMarchDirection(const FVector& InDirection);

	/**
	 * レーンに沿って行進させる。始点から StartDistance（uu）の位置に置き直す。
	 * レーンに沿う間は寿命を持たず、レーンの終端に着いた時点で消える。
	 */
	void SetLane(ALaneActor* InLane, float StartDistance);

	/** 前方が塞がれて止まっているか。 */
	UFUNCTION(BlueprintPure, Category = "Stomp Target")
	bool IsMarchBlocked() const { return bMarchBlocked; }

	/** 沿って進んでいるレーン。直進中なら nullptr。 */
	ALaneActor* GetLane() const { return Lane.Get(); }

	/** レーンの始点からの距離（uu）。直進中は意味を持たない。 */
	float GetLaneDistance() const { return LaneDistance; }

	/**
	 * 踏みつけを受ける（IStompable）。耐久値を減らし、0 になった時点で破壊する。
	 * @return 踏みつけが成立し、加速を与えてよい場合に true。
	 */
	virtual bool ReceiveStomp_Implementation() override;

	/** 残り耐久値。 */
	UFUNCTION(BlueprintPure, Category = "Stomp Target")
	int32 GetDurability() const { return Durability; }

	UBoxComponent* GetCollisionBox() const { return CollisionBox; }
	UStaticMeshComponent* GetBodyMesh() const { return BodyMesh; }

protected:
	/** 寿命（LifeSpanSeconds）をエンジンの寿命機構に設定する。 */
	virtual void BeginPlay() override;

	/** 当たり判定。ルートコンポーネント。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> CollisionBox;

	/** 見た目。メッシュは Blueprint 側で割り当てる。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** 耐久値。この回数だけ踏みつけに耐える。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stomp Target", meta = (ClampMin = "1"))
	int32 Durability = 1;

	/** 行進速度（uu/s）。移動目標として狙いやすい速さにする。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stomp Target", meta = (ClampMin = "0.0"))
	float MarchSpeed = 300.0f;

	/** 直進時の生存時間の上限（秒）。到達点を越えて無限に進み続けないようにする。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stomp Target", meta = (ClampMin = "1.0"))
	float LifeSpanSeconds = 30.0f;

	/** レーンに沿う間、前方の塞がりを調べる距離（uu）。車体の前端からこの距離に壁があれば止まる。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stomp Target", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BlockProbeDistance = 100.0f;

	/**
	 * レーンに沿う間、前の踏み台との車間（uu）。車体の前端からこの距離に踏み台があれば止まる。
	 * 同じ速さで進むウェーブの列（UTankSpawnerComponent::WaveSpacing）では止まらない値にする。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stomp Target", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float FollowSpacing = 100.0f;

private:
	/** 前方が壁や建物で塞がれているか。床や緩い斜面は塞がりとみなさない。 */
	bool ProbeBlocked(const FVector& Direction) const;

	/** 車間の範囲に前の踏み台がいるか。 */
	bool ProbeTankAhead(const FVector& Direction) const;

	/** レーンに沿って1フレーム分進む。終端に着いたら消える。 */
	void AdvanceAlongLane(float DeltaSeconds);

	/** 底面がレーンに乗る位置と、その地点の進行方向に置く。 */
	void PlaceOnLane();

	/** 行進方向（水平・正規化済み）。レーンに沿う間は現在地点の進行方向。 */
	FVector MarchDirection = FVector::ForwardVector;

	/** 沿って進むレーン。無ければ直進する。 */
	TWeakObjectPtr<ALaneActor> Lane;

	/** レーンの始点からの距離（uu）。 */
	float LaneDistance = 0.0f;

	/** 前方が塞がれて止まっているか。 */
	bool bMarchBlocked = false;
};
