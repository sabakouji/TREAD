// 踏みつけ加速メカゲーム — フィールド要素の登録先

#pragma once

#include "CoreMinimal.h"
#include "Field/FieldPointComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "FieldSubsystem.generated.h"

class UDestructibleComponent;
class UGoalComponent;

/** 拠点が登録されたことの通知。GameMode が攻める拠点を決めるために購読する。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFieldGoalRegistered, UGoalComponent*, Goal);

/** 破壊可能オブジェクトが破壊されたことの通知。GameMode が GameState へ反映する。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFieldObjectBroken, UDestructibleComponent*, Destructible);

/**
 * フィールド要素（拠点・NPC 用マーカー）の登録先と、破壊の通知の集約点。
 *
 * 各要素は自身の BeginPlay で登録し、EndPlay で解除する。利用する側（GameMode・AI）は
 * マップ全体を走査せずにここから取得する。
 *
 * 登録先を GameMode にしないのは、各 Actor の BeginPlay と GameMode の初期化の順序が保証されないため。
 * WorldSubsystem はワールドの生成時に作られ、どの Actor の BeginPlay よりも先に存在する。
 * 利用する側は登録の通知（OnGoalRegistered）を購読するか、使う時点で検索し直し、初期化の順序に依存しないこと。
 *
 * 踏み台の供給拠点・レーン・破壊可能オブジェクトは登録しない。供給拠点はレーンを直接参照し、
 * 破壊は OnObjectBroken で集約しており、一覧を必要とする利用者がまだいないため（FIX-02）。
 */
UCLASS()
class MYPROJECT_API UFieldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** ワールドから取得する。ワールドが無ければ nullptr。 */
	static UFieldSubsystem* Get(const UObject* WorldContextObject);

	//~ 拠点

	/** 拠点を登録する。UGoalComponent が BeginPlay で呼ぶ。Blueprint だけで作った拠点の部品からも呼べる。 */
	UFUNCTION(BlueprintCallable, Category = "Field")
	void RegisterGoal(UGoalComponent* Goal);

	UFUNCTION(BlueprintCallable, Category = "Field")
	void UnregisterGoal(UGoalComponent* Goal);

	/** 拠点が登録されたことの通知。 */
	UPROPERTY(BlueprintAssignable, Category = "Field|Events")
	FOnFieldGoalRegistered OnGoalRegistered;

	/**
	 * 指定陣営の拠点のうち、破壊されていない最初のものを返す。無ければ nullptr。
	 * ソロモードでは各陣営の拠点は1つだけを想定する。
	 */
	UFUNCTION(BlueprintPure, Category = "Field")
	UGoalComponent* FindGoal(int32 TeamId) const;

	/** 指定陣営と敵対する陣営の拠点のうち、破壊されていない最初のもの。攻撃目標を探す際に用いる。 */
	UFUNCTION(BlueprintPure, Category = "Field")
	UGoalComponent* FindHostileGoal(int32 TeamId) const;

	/** 登録されている拠点の数（破壊済みを含む）。 */
	UFUNCTION(BlueprintPure, Category = "Field")
	int32 GetGoalCount() const { return Goals.Num(); }

	//~ NPC 用マーカー

	/** マーカーを登録する。UFieldPointComponent が BeginPlay で呼ぶ。 */
	UFUNCTION(BlueprintCallable, Category = "Field")
	void RegisterFieldPoint(UFieldPointComponent* Point);

	UFUNCTION(BlueprintCallable, Category = "Field")
	void UnregisterFieldPoint(UFieldPointComponent* Point);

	/**
	 * 役割と陣営が一致するマーカーのうち、指定位置に最も近いものを返す。無ければ nullptr。
	 * 中立のマーカーはどの陣営からも使える。
	 */
	UFUNCTION(BlueprintPure, Category = "Field")
	UFieldPointComponent* FindNearestFieldPoint(EFieldPointRole Role, int32 TeamId, const FVector& Location) const;

	//~ 破壊

	/** 破壊可能オブジェクトが破壊されたことを受け取り、購読者へ通知する。 */
	UFUNCTION(BlueprintCallable, Category = "Field")
	void NotifyObjectBroken(UDestructibleComponent* Destructible);

	/** 破壊可能オブジェクトが破壊されたことの通知。 */
	UPROPERTY(BlueprintAssignable, Category = "Field|Events")
	FOnFieldObjectBroken OnObjectBroken;

private:
	TArray<TWeakObjectPtr<UGoalComponent>> Goals;
	TArray<TWeakObjectPtr<UFieldPointComponent>> FieldPoints;
};
