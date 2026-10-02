// 踏みつけ加速メカゲーム — 分割しない小物の Actor

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BreakablePropActor.generated.h"

class UBoxComponent;
class UBreakablePropComponent;
class UStaticMeshComponent;

/**
 * 街灯などの、分割しない小物。当たり判定・見た目・小物の振る舞い（UBreakablePropComponent）を組み合わせるだけの Actor。
 * 機体が触れると壊れる。機体を止めず、減速も得点もない（FIELD-02）。
 */
UCLASS()
class MYPROJECT_API ABreakablePropActor : public AActor
{
	GENERATED_BODY()

public:
	ABreakablePropActor();

	UBoxComponent* GetCollisionBox() const { return CollisionBox; }
	UBreakablePropComponent* GetBreakable() const { return Breakable; }

protected:
	/**
	 * 当たり判定。ルートコンポーネント。
	 * 機体（Pawn）とは重なるだけで止めない。種類は WorldDynamic とし、NPC の障害物回避や踏み台の停止判定
	 * （いずれも WorldStatic だけを調べる）の対象にしない。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> CollisionBox;

	/** 見た目。メッシュは Blueprint 側で割り当てる。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** 小物の振る舞い。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBreakablePropComponent> Breakable;
};
