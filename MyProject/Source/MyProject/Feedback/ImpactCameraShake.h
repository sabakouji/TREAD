// 踏みつけ加速メカゲーム — 衝突の手応えのカメラシェイク

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "ImpactCameraShake.generated.h"

/**
 * 破壊・命中の瞬間の短く鋭い揺れ。Perlin ノイズのパターンを C++ で組み込み、シェイク用の資産を作らずに使えるようにする。
 *
 * 揺れの形（長さ・振幅・周波数）はこのクラスが決め、出来事ごとの強さは DA_FeedbackTuning の ShakeScale で掛ける。
 * 形を変えたい場合は Blueprint で派生させ、DA_FeedbackTuning の ShakeClass に割り当てる。
 */
UCLASS()
class MYPROJECT_API UImpactCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	UImpactCameraShake(const FObjectInitializer& ObjectInitializer);
};
