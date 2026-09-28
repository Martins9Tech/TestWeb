#pragma once

#include "CoreMinimal.h"
#include "VestuarioTypes.generated.h"

/** Colores de los fusibles del puzle. */
UENUM(BlueprintType)
enum class EFuseColor : uint8
{
	Red		UMETA(DisplayName = "Rojo"),
	Blue	UMETA(DisplayName = "Azul"),
	Green	UMETA(DisplayName = "Verde")
};

/** Sonidos del juego. Cada uno se carga de /Game/Vestuario/Audio/S_<Nombre>. */
UENUM(BlueprintType)
enum class EVestuarioSound : uint8
{
	Footstep,
	EnemyStep,
	EnemyBreath,
	EnemyScream,
	Knock,
	Spark,
	FuseClick,
	Pickup,
	LockerCreak,
	DoorOpen,
	Heartbeat,
	Drip,
	Jumpscare,
	PowerOn,
	Ambience,
	Whisper,
	MAX UMETA(Hidden)
};

namespace Vestuario
{
	/** Nombre del fusible para mostrar en pantalla ("ROJO", "AZUL"...). */
	FText FuseName(EFuseColor Color);

	/** Color de luz asociado a cada fusible. */
	FLinearColor FuseLinearColor(EFuseColor Color);

	/** Ruta del mesh del fusible, p. ej. /Game/Vestuario/Meshes/SM_Fuse_Red.SM_Fuse_Red */
	FString FuseMeshPath(EFuseColor Color);

	/** Nombre del asset de sonido, p. ej. "S_Footstep". */
	FString SoundAssetName(EVestuarioSound Sound);

	/** Ruta completa de un mesh a partir de su nombre ("SM_Locker"). */
	FString MeshPath(const TCHAR* MeshName);

	/** Etiquetas de ruido que el enemigo interpreta de forma especial. */
	FName LockerNoiseTag();
	FName ChatNoiseTag();
	FName AlarmNoiseTag();
}
