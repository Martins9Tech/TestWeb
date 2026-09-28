using UnrealBuildTool;

public class ElVestuarioEditorTarget : TargetRules
{
	public ElVestuarioEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("ElVestuario");
	}
}
