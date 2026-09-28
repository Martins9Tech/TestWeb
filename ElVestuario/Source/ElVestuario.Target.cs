using UnrealBuildTool;

public class ElVestuarioTarget : TargetRules
{
	public ElVestuarioTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("ElVestuario");
	}
}
