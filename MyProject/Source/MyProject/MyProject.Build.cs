// 踏みつけ加速メカゲーム — モジュール定義

using UnrealBuildTool;

public class MyProject : ModuleRules
{
	public MyProject(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Public/Private を分けず機能別フォルダ（Core / Vehicle / Tuning / Debug 等）で構成しているため、
		// モジュールルートを明示的にインクルードパスへ追加する。
		// これにより "Core/ImpactGameMode.h" 形式の相対インクルードが解決できる。
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			// UE 5.8 の Enhanced Input は Input Mode を GameplayTag で表現するため、
			// その内容を扱うには GameplayTags への明示的な依存が必要。
			"GameplayTags",
			// NPC の AAIController に必要。
			"AIModule",
			"UMG"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// 破壊可能オブジェクトの破壊演出（粉塵・火花）に必要。ヘッダには現れないため Private とする。
			"Niagara",
			// 破壊時の Chaos の破片（演出専用）。Geometry Collection と、初速・歪みを与える Field System。
			"Chaos",
			"FieldSystemEngine",
			"GeometryCollectionEngine",
			// 衝突の手応えのカメラシェイク（Perlin ノイズのパターン）。
			"EngineCameras",
			"Slate",
			"SlateCore"
		});
	}
}
