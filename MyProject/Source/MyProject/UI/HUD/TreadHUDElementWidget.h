// 踏みつけ加速メカゲーム — 汎用 HUD 部品の共通基底

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "UI/HUD/TreadHUDTypes.h"
#include "TreadHUDElementWidget.generated.h"

class APawn;
class UTreadHUDDataComponent;

/**
 * 汎用 HUD 部品の共通基底。「どの値（BindTag）を、どの方式で、どの速さで追従して表示するか」を受け持つ。
 *
 * 所有プレイヤーの Pawn に付いた UTreadHUDDataComponent から値を取り、表示値を追従させて
 * 表示値が変わったときだけ ApplyValue を呼ぶ。Pawn が差し替わったら値の置き場を取り直す。
 * 派生クラスは ApplyValue（実行時）と ApplyPreview（デザイナー上の表示）を実装するだけでよい。
 */
UCLASS(Abstract)
class MYPROJECT_API UTreadHUDElementWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 紐付け先（値の行先）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TREAD|HUD")
	FGameplayTag BindTag;

	/** 値の受け取り方。毎フレーム読むか、変化の通知だけで受け取るか。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TREAD|HUD")
	ETreadHUDUpdateMode UpdateMode = ETreadHUDUpdateMode::Tick;

	/** 表示値の追従速度。0 で即時に反映する。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TREAD|HUD", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float InterpSpeed = 0.0f;

	/** デザイナー上で表示する値（0〜1）。実行時には使わない。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TREAD|HUD", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PreviewFill = 0.5f;

	/** 値の置き場を所有プレイヤーの Pawn から取り直す。 */
	UFUNCTION(BlueprintCallable, Category = "TREAD|HUD")
	void RefreshDataSource();

	/** 値の置き場が見つかっているか。 */
	UFUNCTION(BlueprintPure, Category = "TREAD|HUD")
	bool HasDataSource() const { return DataSource.IsValid(); }

	/** 今表示している正規化値（追従後）。 */
	UFUNCTION(BlueprintPure, Category = "TREAD|HUD")
	float GetDisplayNormalized() const { return DisplayNormalized; }

	/** 今表示している元の値（追従後）。 */
	UFUNCTION(BlueprintPure, Category = "TREAD|HUD")
	float GetDisplayRaw() const { return DisplayRaw; }

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 表示を値に合わせる。表示値が変わったときだけ呼ばれる。 */
	virtual void ApplyValue(float Raw, float DisplayNormalized);

	/** デザイナー上の表示を作る。既定では PreviewFill を値として ApplyValue に渡す。 */
	virtual void ApplyPreview(float PreviewNormalized);

private:
	/** 値の置き場の通知の受け手。dynamic デリゲートの受け手のため UFUNCTION が必須。 */
	UFUNCTION()
	void HandleValueChanged(FGameplayTag Tag, float Raw, float Normalized);

	/** 所有プレイヤーの Pawn の差し替え（リスポーンなど）の受け手。UFUNCTION が必須。 */
	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	/** Pawn の値の置き場に付け替える。null なら外すだけ。 */
	void BindToPawn(APawn* Pawn);

	/** 今の値の置き場から外れる。 */
	void UnbindDataSource();

	/** 表示値を目標値へ進め、変わっていれば ApplyValue を呼ぶ。 */
	void StepDisplay(float DeltaTime);

	TWeakObjectPtr<UTreadHUDDataComponent> DataSource;

	float TargetRaw = 0.0f;
	float TargetNormalized = 0.0f;
	float DisplayRaw = 0.0f;
	float DisplayNormalized = 0.0f;

	/** 最初の値を受け取ったか。最初の値は追従させず、0 から伸びて見えないようにする。 */
	bool bHasTarget = false;

	/** 一度でも ApplyValue を呼んだか。初回は必ず反映する。 */
	bool bHasApplied = false;
	float AppliedRaw = 0.0f;
	float AppliedNormalized = 0.0f;
};
