// 踏みつけ加速メカゲーム — 汎用 HUD 部品の共通基底

#include "UI/HUD/TreadHUDElementWidget.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UI/HUD/TreadHUDDataComponent.h"
#include "UI/HUD/TreadHUDMath.h"
#include "UI/ImpactWidgetHelpers.h"

namespace
{
	/** 表示を書き換えるとみなす正規化値の変化量。これ未満の変化では ApplyValue を呼ばない。 */
	constexpr float NormalizedChangeTolerance = 0.0005f;

	/** 表示を書き換えるとみなす元の値の変化量。 */
	constexpr float RawChangeTolerance = 0.01f;
}

void UTreadHUDElementWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (IsDesignTime())
	{
		ApplyPreview(PreviewFill);
	}
}

void UTreadHUDElementWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (APlayerController* Player = GetOwningPlayer())
	{
		Player->OnPossessedPawnChanged.AddUniqueDynamic(this, &UTreadHUDElementWidget::HandlePossessedPawnChanged);
	}
	RefreshDataSource();
}

void UTreadHUDElementWidget::NativeDestruct()
{
	if (APlayerController* Player = GetOwningPlayer())
	{
		Player->OnPossessedPawnChanged.RemoveDynamic(this, &UTreadHUDElementWidget::HandlePossessedPawnChanged);
	}
	UnbindDataSource();

	Super::NativeDestruct();
}

void UTreadHUDElementWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const UTreadHUDDataComponent* Data = DataSource.Get();
	if (!Data)
	{
		return;
	}

	if (UpdateMode == ETreadHUDUpdateMode::Tick)
	{
		float Raw = 0.0f;
		float Normalized = 0.0f;
		if (Data->GetValue(BindTag, Raw, Normalized))
		{
			HandleValueChanged(BindTag, Raw, Normalized);
		}
	}

	StepDisplay(InDeltaTime);
}

void UTreadHUDElementWidget::ApplyValue(float Raw, float InDisplayNormalized)
{
	// 基底は表示を持たない。表示は派生クラスが作る。
}

void UTreadHUDElementWidget::ApplyPreview(float PreviewNormalized)
{
	ApplyValue(PreviewNormalized, PreviewNormalized);
}

void UTreadHUDElementWidget::RefreshDataSource()
{
	BindToPawn(GetOwningPlayerPawn());
}

void UTreadHUDElementWidget::HandleValueChanged(FGameplayTag Tag, float Raw, float Normalized)
{
	if (Tag != BindTag)
	{
		return;
	}

	TargetRaw = Raw;
	TargetNormalized = Normalized;
	if (!bHasTarget)
	{
		DisplayRaw = Raw;
		DisplayNormalized = Normalized;
		bHasTarget = true;
	}
}

void UTreadHUDElementWidget::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	BindToPawn(NewPawn);
}

void UTreadHUDElementWidget::BindToPawn(APawn* Pawn)
{
	UnbindDataSource();

	UTreadHUDDataComponent* Data = Pawn ? Pawn->FindComponentByClass<UTreadHUDDataComponent>() : nullptr;
	if (!Data)
	{
		// 試合開始前やリスポーン待ちでは Pawn が無い。差し替えの通知で取り直す。
		UE_LOG(LogImpactUI, Verbose, TEXT("%s: no HUD data on %s"), *GetName(), *GetNameSafe(Pawn));
		return;
	}

	if (!BindTag.IsValid())
	{
		UE_LOG(LogImpactUI, Warning, TEXT("%s: BindTag is not set; nothing will be shown"), *GetName());
	}

	DataSource = Data;
	bHasTarget = false;
	if (UpdateMode == ETreadHUDUpdateMode::Event)
	{
		Data->OnValueChanged.AddUniqueDynamic(this, &UTreadHUDElementWidget::HandleValueChanged);
	}

	// 通知方式でも、購読前に書き込まれていた値は今読んでおく。
	float Raw = 0.0f;
	float Normalized = 0.0f;
	if (Data->GetValue(BindTag, Raw, Normalized))
	{
		HandleValueChanged(BindTag, Raw, Normalized);
	}
}

void UTreadHUDElementWidget::UnbindDataSource()
{
	if (UTreadHUDDataComponent* Data = DataSource.Get())
	{
		Data->OnValueChanged.RemoveDynamic(this, &UTreadHUDElementWidget::HandleValueChanged);
	}
	DataSource.Reset();
}

void UTreadHUDElementWidget::StepDisplay(float DeltaTime)
{
	if (!bHasTarget)
	{
		return;
	}

	DisplayRaw = TreadHUDMath::StepToward(DisplayRaw, TargetRaw, DeltaTime, InterpSpeed);
	DisplayNormalized = TreadHUDMath::StepToward(DisplayNormalized, TargetNormalized, DeltaTime, InterpSpeed);

	if (bHasApplied
		&& FMath::IsNearlyEqual(DisplayNormalized, AppliedNormalized, NormalizedChangeTolerance)
		&& FMath::IsNearlyEqual(DisplayRaw, AppliedRaw, RawChangeTolerance))
	{
		return;
	}

	bHasApplied = true;
	AppliedRaw = DisplayRaw;
	AppliedNormalized = DisplayNormalized;
	ApplyValue(DisplayRaw, DisplayNormalized);
}
