// 踏みつけ加速メカゲーム — 敵機 Pawn

#pragma once

#include "CoreMinimal.h"
#include "Vehicle/ImpactVehiclePawn.h"
#include "EnemyVehiclePawn.generated.h"

class UAITuningDataAsset;

/**
 * NPC の機体。
 *
 * 企画書 3-5 のとおり機体性能は自機と完全に同一とし、同じ移動コンポーネントと
 * DA_VehicleTuning を用いる。自機との差は AI の判断（UAITuningDataAsset）のみ。
 * 見た目はプレースホルダの Cube を色で区別する。
 */
UCLASS()
class MYPROJECT_API AEnemyVehiclePawn : public AImpactVehiclePawn
{
	GENERATED_BODY()

public:
	AEnemyVehiclePawn();

	virtual void BeginPlay() override;

	/** 撃破された。移動処理の最中に呼ばれるため、その場では消滅させず遅延させる。 */
	virtual void HandleDefeatedInClash_Implementation() override;

	/** AI の判断に関わる調整値。 */
	UFUNCTION(BlueprintPure, Category = "Enemy")
	UAITuningDataAsset* GetAITuning() const { return AITuning; }

	/** 撃破済みか。消滅までの間に AI が操作を続けたり、二重に撃破されたりしないようにする。 */
	bool IsDefeated() const { return bDefeated; }

protected:
	/** AI の判断に関わる調整値。Blueprint 側で DA_AITuning を割り当てる。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<UAITuningDataAsset> AITuning;

	/** 自機と見分けるための機体色。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy")
	FLinearColor BodyColor = FLinearColor(0.9f, 0.15f, 0.1f);

	/** 撃破から消滅までの時間（秒）。0 より大きくすること（0 は無期限を意味する）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float DefeatDespawnDelay = 0.1f;

private:
	/** 機体色を適用する。 */
	void ApplyBodyColor();

	bool bDefeated = false;
};
