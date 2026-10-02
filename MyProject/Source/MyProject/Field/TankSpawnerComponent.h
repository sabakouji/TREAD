// 踏みつけ加速メカゲーム — 踏み台（戦車）の供給

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TankSpawnerComponent.generated.h"

class ALaneActor;
class AStompTargetActor;

/**
 * 踏み台（戦車）を一定間隔で供給する振る舞い。拠点など任意の Actor に付ける。
 *
 * 企画書の「拠点から一定間隔で戦車が供給され、敵拠点に向かって行進する」に相当する。
 * 供給間隔ごとに1ウェーブ（TanksPerWave 体）を出す。レーンが指定されていればウェーブごとに順番に割り振り、
 * スプラインに沿って行進させる。レーンが無ければ所有 Actor の前方へ直進させる。
 * 同時存在の上限はレーンごとに数える。塞がれて踏み台が溜まったレーンは飛ばし、他のレーンへの供給は続ける。
 */
UCLASS(ClassGroup = (Impact), meta = (BlueprintSpawnableComponent))
class MYPROJECT_API UTankSpawnerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTankSpawnerComponent();

	/** 存命の踏み台の数。 */
	UFUNCTION(BlueprintPure, Category = "Tank Spawner")
	int32 GetAliveCount() const { return AliveTargets.Num(); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 生成する踏み台のクラス。Blueprint 側で BP_StompTarget を割り当てる。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank Spawner")
	TSubclassOf<AStompTargetActor> StompTargetClass;

	/** 供給間隔（秒）。踏み台を探して走る時間が生まれる長さにする。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank Spawner", meta = (ClampMin = "0.1"))
	float SpawnInterval = 4.0f;

	/** 1ウェーブで出す数。同時存在の上限に達した分は出さない。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank Spawner", meta = (ClampMin = "1"))
	int32 TanksPerWave = 1;

	/**
	 * 同じウェーブ内の踏み台同士の間隔（中心間、uu）。列を作って行進させる。
	 * 車体の長さより短いと同じウェーブの踏み台が重なって出現し、重なったまま進むため、
	 * 下限は既定の車体の長さ（AStompTargetActor の当たり判定の半長 120 × 2 = 240）とする。
	 * 踏み台の Blueprint で車体を大きくした場合に備え、BeginPlay で実際の車体の長さと比べて警告する。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank Spawner", meta = (ClampMin = "240.0", UIMin = "240.0"))
	float WaveSpacing = 400.0f;

	/** 同時に存在できる踏み台の上限（レーンごと。レーンが無ければ全体）。増えすぎて的が濃くなりすぎるのを防ぐ。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank Spawner", meta = (ClampMin = "1"))
	int32 MaxAliveTargets = 6;

	/**
	 * 流すレーン。複数あればウェーブごとに順番に使う。
	 * 空なら、所有 Actor の前方 SpawnForwardOffset の位置から前方へ直進させる。
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Tank Spawner")
	TArray<TObjectPtr<ALaneActor>> Lanes;

	/**
	 * レーンの出現位置の前後にこの距離（uu）以内で踏み台がいれば、その踏み台は出さずに見送る。
	 * 塞がれたレーンで車列が始点まで伸びたとき、新しい踏み台が最後尾に重なって出現するのを防ぐ。
	 * 車体の長さ（240）に車間（AStompTargetActor::FollowSpacing の既定 100）と余裕を足した値にする。
	 *
	 * 判定の対象はウェーブを出す前から走っている踏み台だけで、同じウェーブの踏み台同士は判定し合わない。
	 * そのため WaveSpacing をこの値より小さくしても、同じウェーブの2体目以降が見送られることはない。
	 * 同じウェーブの踏み台同士が重ならないかは、この値ではなく WaveSpacing（車体の長さ以上）で決まる。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank Spawner", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SpawnClearance = 350.0f;

	/** レーンが無い場合の、生成位置の所有 Actor からの前方オフセット（uu）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tank Spawner", meta = (ClampMin = "0.0"))
	float SpawnForwardOffset = 200.0f;

private:
	/**
	 * 供給間隔ごとにタイマーから呼ばれる。消えた踏み台を一覧から除き、上限未満なら1ウェーブ生成する。
	 * 毎フレームの Tick で残り時間を数える代わりにタイマーを使う。タイマーも一時停止中は進まない。
	 */
	void HandleSpawnTimer();

	/**
	 * 次に使うレーン。上限に達したレーンは飛ばす。
	 * @param bOutHasLanes  有効なレーンが1本でもあるか。false なら直進で供給する。
	 * @return 供給できるレーン。全て上限に達していれば nullptr。
	 */
	ALaneActor* PickNextLane(bool& bOutHasLanes);

	/** WaveSpacing が供給する踏み台の実際の車体の長さより短ければ警告する。 */
	void WarnIfWaveSpacingTooShort() const;

	/** 指定レーン（nullptr なら直進）を進む存命の踏み台の数。 */
	int32 CountAliveOn(const ALaneActor* Lane) const;

	/** レーンの指定距離の前後 SpawnClearance 以内に踏み台がいるか。 */
	bool IsLaneOccupiedAt(const ALaneActor* Lane, float Distance) const;

	/** 踏み台を1体生成する。WaveIndex はウェーブ内の順番で、列の間隔に用いる。出現位置が空いているかは呼び出し側で判定する。 */
	void SpawnStompTarget(ALaneActor* Lane, int32 WaveIndex);

	FTimerHandle SpawnTimer;

	/** 次のウェーブで使うレーンの番号。 */
	int32 NextLaneIndex = 0;

	/** 生成済みで存命の踏み台。上限判定に用いる。 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AStompTargetActor>> AliveTargets;
};
