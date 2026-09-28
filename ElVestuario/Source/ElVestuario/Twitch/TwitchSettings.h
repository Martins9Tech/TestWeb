#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TwitchSettings.generated.h"

/**
 * Ajustes de Twitch. Se editan en Project Settings > Game > "Twitch (El Vestuario)"
 * o directamente en Config/DefaultGame.ini.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Twitch (El Vestuario)"))
class ELVESTUARIO_API UTwitchSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Canal de Twitch a escuchar, sin '#'. Vacio = el juego funciona sin Twitch. */
	UPROPERTY(Config, EditAnywhere, Category = "Twitch")
	FString Channel;

	/** Conectar automaticamente al arrancar si hay canal configurado. */
	UPROPERTY(Config, EditAnywhere, Category = "Twitch")
	bool bAutoConnect = true;

	/** Segundos entre dos !ruido (para todo el chat). */
	UPROPERTY(Config, EditAnywhere, Category = "Comandos", meta = (ClampMin = "0"))
	float NoiseCooldown = 60.f;

	/** Segundos entre dos !pista (para todo el chat). */
	UPROPERTY(Config, EditAnywhere, Category = "Comandos", meta = (ClampMin = "0"))
	float HintCooldown = 90.f;

	/** Segundos que un mismo espectador tiene que esperar entre comandos. */
	UPROPERTY(Config, EditAnywhere, Category = "Comandos", meta = (ClampMin = "0"))
	float PerUserCooldown = 20.f;

	/** Mostrar en pantalla el nombre de quien lanzo el comando. Desactivado por seguridad. */
	UPROPERTY(Config, EditAnywhere, Category = "Comandos")
	bool bShowUserNames = false;
};
