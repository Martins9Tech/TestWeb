#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/VestuarioTypes.h"
#include "VestuarioGameMode.generated.h"

class UAudioComponent;
class ULightComponent;
class USoundAttenuation;
class USoundBase;

struct FVestuarioMessage
{
	FText Text;
	float ExpireTime = 0.f;
	float Duration = 0.f;
};

/**
 * Reglas de la partida:
 *  - carga los sonidos y los reproduce (PlaySound3D / PlaySound2D)
 *  - parpadeo de las luces de emergencia y goteo ambiental
 *  - comandos del chat de Twitch (!ruido, !pista)
 *  - muerte, victoria y reinicio del nivel
 *  - cola de mensajes que pinta el HUD
 */
UCLASS()
class ELVESTUARIO_API AVestuarioGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AVestuarioGameMode();

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	// --- Sonido ---
	USoundBase* GetSound(EVestuarioSound Sound) const;
	void PlaySound3D(EVestuarioSound Sound, const FVector& Location, float Volume = 1.f, float Pitch = 1.f);
	void PlaySound2D(EVestuarioSound Sound, float Volume = 1.f, float Pitch = 1.f);
	USoundAttenuation* GetAttenuation() const { return Attenuation; }

	// --- Mensajes en pantalla ---
	void ShowMessage(const FText& Text, float Duration = 4.f);
	const TArray<FVestuarioMessage>& GetMessages() const { return Messages; }

	// --- Estado de la partida ---
	void OnPlayerDied();
	void OnPlayerEscaped();
	void OnPowerRestored();
	bool IsPlayerDead() const { return bPlayerDead; }
	bool HasEscaped() const { return bEscaped; }
	bool IsPowerOn() const { return bPowerOn; }
	bool IsGameOver() const { return bPlayerDead || bEscaped; }
	float GetElapsedTime() const;
	float GetEndTime() const { return EndTime; }

	// --- Twitch ---
	/** Comando real del chat (con cooldowns). */
	void HandleChatCommand(const FString& User, const FString& Command) { HandleChatCommandInternal(User, Command, false); }

	/** Comando simulado con las teclas 1 y 2 o la consola (sin cooldowns). */
	void HandleDebugCommand(const FString& Command) { HandleChatCommandInternal(FString(), Command, true); }

	// --- Depuracion ---
	void ToggleDebugAI() { bDebugAI = !bDebugAI; }
	bool IsDebugAI() const { return bDebugAI; }

protected:
	/** Volumen general del ambiente. */
	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	float AmbienceVolume = 0.55f;

	/** Segundos entre gotas de agua (min / max). */
	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	FVector2D DripInterval = FVector2D(3.f, 8.f);

private:
	void HandleChatCommandInternal(const FString& User, const FString& Command, bool bDebug);
	void LoadSounds();
	void CacheLights();
	void UpdateFlicker(float Now);
	void UpdateDrips(float Now);
	void DoChatNoise(const FString& User);
	void DoChatHint(const FString& User);
	void RestartMap();
	APawn* GetPlayerPawn() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundBase>> Sounds;

	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> Attenuation;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> AmbienceAudio;

	TArray<TWeakObjectPtr<ULightComponent>> FlickerLights;
	TArray<float> FlickerBaseIntensity;

	TArray<FVestuarioMessage> Messages;
	FDelegateHandle ChatHandle;
	FTimerHandle RestartTimer;

	float StartTime = 0.f;
	float EndTime = 0.f;
	float NextDripTime = 0.f;
	bool bPlayerDead = false;
	bool bEscaped = false;
	bool bPowerOn = false;
	bool bDebugAI = false;
};
