#include "Core/VestuarioTypes.h"

namespace Vestuario
{
	FText FuseName(EFuseColor Color)
	{
		switch (Color)
		{
		case EFuseColor::Red:	return NSLOCTEXT("Vestuario", "FuseRed", "ROJO");
		case EFuseColor::Blue:	return NSLOCTEXT("Vestuario", "FuseBlue", "AZUL");
		case EFuseColor::Green:	return NSLOCTEXT("Vestuario", "FuseGreen", "VERDE");
		}
		return FText::GetEmpty();
	}

	FLinearColor FuseLinearColor(EFuseColor Color)
	{
		switch (Color)
		{
		case EFuseColor::Red:	return FLinearColor(1.0f, 0.15f, 0.1f);
		case EFuseColor::Blue:	return FLinearColor(0.2f, 0.4f, 1.0f);
		case EFuseColor::Green:	return FLinearColor(0.2f, 1.0f, 0.3f);
		}
		return FLinearColor::White;
	}

	FString FuseMeshPath(EFuseColor Color)
	{
		switch (Color)
		{
		case EFuseColor::Red:	return MeshPath(TEXT("SM_Fuse_Red"));
		case EFuseColor::Blue:	return MeshPath(TEXT("SM_Fuse_Blue"));
		case EFuseColor::Green:	return MeshPath(TEXT("SM_Fuse_Green"));
		}
		return FString();
	}

	FString SoundAssetName(EVestuarioSound Sound)
	{
		const UEnum* Enum = StaticEnum<EVestuarioSound>();
		return FString(TEXT("S_")) + Enum->GetNameStringByValue(static_cast<int64>(Sound));
	}

	FString MeshPath(const TCHAR* MeshName)
	{
		return FString::Printf(TEXT("/Game/Vestuario/Meshes/%s.%s"), MeshName, MeshName);
	}

	FName LockerNoiseTag()
	{
		static const FName Tag(TEXT("Locker"));
		return Tag;
	}

	FName ChatNoiseTag()
	{
		static const FName Tag(TEXT("Chat"));
		return Tag;
	}

	FName AlarmNoiseTag()
	{
		static const FName Tag(TEXT("Alarm"));
		return Tag;
	}
}
