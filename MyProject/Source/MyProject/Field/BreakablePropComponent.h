// 踏みつけ加速メカゲーム — 分割しない小物（街灯など）

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BreakablePropComponent.generated.h"

class UBreakablePropComponent;
class UNiagaraSystem;
class UPrimitiveComponent;

/** 小物が壊れたことの通知。Breaker は壊した機体。演出（音・破片）を Blueprint から繋げられるよう公開する。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPropBroken, UBreakablePropComponent*, Prop, AActor*, Breaker);

/**
 * 街灯などの、分割しない小物の振る舞い。機体が触れた瞬間に、速度に関係なく壊れる。
 *
 * 走行の邪魔にならない飾りとして扱い、ゲームの判定には関与しない（FIELD-02）。
 * - 所有 Actor の当たり判定は機体と「重なる」だけにし、機体を止めない。そのため移動の衝突判定を通らず、機体は減速しない
 * - 得点・出来事の通知（OnGameplayEvent）は出さない
 * - 建物（UDestructibleComponent）と違い、Chaos の破片は持たない。フィールド上の建物はすべて分割する方針のため、
 *   分割しないものはこの部品で作る小物に限る
 */
UCLASS(ClassGroup = (Impact), meta = (BlueprintSpawnableComponent))
class MYPROJECT_API UBreakablePropComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBreakablePropComponent();

	/** 壊れたか。 */
	UFUNCTION(BlueprintPure, Category = "Breakable Prop")
	bool IsBroken() const { return bBroken; }

	/** 壊す。既に壊れていれば何もしない。Breaker は壊した Actor（演出の向きに用いる。無ければ nullptr）。 */
	UFUNCTION(BlueprintCallable, Category = "Breakable Prop")
	void Break(AActor* Breaker);

	/** 壊れたことの通知。 */
	UPROPERTY(BlueprintAssignable, Category = "Breakable Prop|Events")
	FOnPropBroken OnBroken;

protected:
	virtual void BeginPlay() override;

	/** 壊れたときの演出（任意）。壊した機体の進行方向へ向けて出す。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breakable Prop")
	TObjectPtr<UNiagaraSystem> BreakFX;

private:
	/** 所有 Actor の当たり判定に機体が重なった。dynamic デリゲートの受け手。 */
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	bool bBroken = false;
};
