// 踏みつけ加速メカゲーム — 陣営

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TeamComponent.generated.h"

/**
 * 陣営の番号。4対4 の2陣営と、どちらにも属さない中立を表す。
 * 番号の意味（どちらがプレイヤー側か）はここ1箇所で決める。
 */
namespace ImpactTeam
{
	/** どちらの陣営にも属さない。誰とも敵対しない。 */
	inline constexpr int32 Neutral = -1;

	/** ソロモードでプレイヤーが属する陣営。 */
	inline constexpr int32 Player = 0;

	/** ソロモードで NPC とゴールが属する陣営。 */
	inline constexpr int32 Opponent = 1;
}

/**
 * フィールド要素の陣営。拠点・スポナー・マーカー・プレイヤー・NPC のすべてに付ける土台の部品。
 *
 * 敵味方の判定（NPC の標的選び・ロックオン対象の絞り込み・ゴールへのダメージ可否）は
 * 相手の具象型を知らずに、この部品の有無と番号だけで行う。
 * 部品が付いていない Actor は中立として扱う。
 */
UCLASS(ClassGroup = (Impact), meta = (BlueprintSpawnableComponent))
class MYPROJECT_API UTeamComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTeamComponent();

	/** 陣営の番号。 */
	UFUNCTION(BlueprintPure, Category = "Team")
	int32 GetTeamId() const { return TeamId; }

	/** 陣営を変える。出現時に GameMode が割り当てる場合に用いる。 */
	UFUNCTION(BlueprintCallable, Category = "Team")
	void SetTeamId(int32 NewTeamId);

	/** Actor の陣営。陣営の部品を持たない Actor（床・壁など）と nullptr は中立を返す。 */
	UFUNCTION(BlueprintPure, Category = "Team")
	static int32 GetTeamIdOf(const AActor* Actor);

	/** 2つの Actor が敵対しているか。どちらかが中立なら敵対しない。 */
	UFUNCTION(BlueprintPure, Category = "Team")
	static bool AreHostile(const AActor* A, const AActor* B);

	/** 2つの陣営が敵対しているか。どちらかが中立なら敵対しない。判定規則の本体。 */
	static bool AreTeamsHostile(int32 TeamA, int32 TeamB);

protected:
	/** 陣営の番号（0 / 1、中立 -1）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Team", meta = (ClampMin = "-1", ClampMax = "1", UIMin = "-1", UIMax = "1"))
	int32 TeamId = ImpactTeam::Neutral;
};
