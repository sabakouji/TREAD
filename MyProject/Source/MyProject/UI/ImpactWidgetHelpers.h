// 踏みつけ加速メカゲーム — 画面を組み立てる共通部品

#pragma once

#include "CoreMinimal.h"
#include "Framework/Text/TextLayout.h"

class UButton;
class UTextBlock;
class UWidgetTree;

/** 画面（HUD・リザルト・タイトル）の生成と操作に関するログカテゴリ。 */
DECLARE_LOG_CATEGORY_EXTERN(LogImpactUI, Log, All);

/**
 * C++ で画面を組み立てるための共通部品。
 *
 * UMG デザイナーでの配置はスクリプトから再現できず、手作業にすると状態を検証できないため、
 * 本プロジェクトの画面は C++ で組み立てる。文字の影や余白といった見た目の既定をここでそろえる。
 */
namespace ImpactWidget
{
	/** 太字・影付きの文字を作る。 */
	UTextBlock* MakeText(UWidgetTree& Tree, int32 FontSize, const FLinearColor& Color,
		ETextJustify::Type Justification = ETextJustify::Left);

	/** 文字ラベル付きのボタンを作る。 */
	UButton* MakeButton(UWidgetTree& Tree, const FText& Label, int32 FontSize,
		const FLinearColor& TextColor, const FLinearColor& BackgroundColor);

	/**
	 * 秒数を M:SS 形式にする。
	 * @param bRoundUp  端数を切り上げるか。残り時間は切り上げ（残り 0.4 秒を 0:00 と表示しない）、経過時間は切り捨てる。
	 */
	FText FormatClock(float Seconds, bool bRoundUp);
}
