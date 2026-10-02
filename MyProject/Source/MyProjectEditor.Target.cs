// 踏みつけ加速メカゲーム — エディタ用ビルドターゲット

using UnrealBuildTool;
using System.Collections.Generic;

public class MyProjectEditorTarget : TargetRules
{
	public MyProjectEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		ExtraModuleNames.Add("MyProject");
	}
}
