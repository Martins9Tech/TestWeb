#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TwitchChatSubsystem.generated.h"

class IWebSocket;

UENUM(BlueprintType)
enum class ETwitchStatus : uint8
{
	Disabled,
	Connecting,
	Connected,
	Reconnecting
};

/** (Usuario, Comando en minusculas, p. ej. "!ruido") */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnTwitchChatCommand, const FString& /*User*/, const FString& /*Command*/);

/**
 * Lee el chat de Twitch en modo anonimo (solo lectura) por WebSocket IRC.
 * No necesita login ni claves: basta con el nombre del canal.
 *
 * Vive en el GameInstance, asi que sigue conectado aunque se reinicie el nivel.
 * Tambien guarda los cooldowns de los comandos para que morir no los resetee.
 *
 * Mas adelante (bits, puntos de canal, encuestas) habra que migrar a EventSub + OAuth.
 */
UCLASS()
class ELVESTUARIO_API UTwitchChatSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Conecta (o reconecta) a un canal. Canal vacio = desconectar. */
	void Connect(const FString& InChannel);
	void Disconnect();

	ETwitchStatus GetStatus() const { return Status; }
	const FString& GetChannel() const { return Channel; }
	FText GetStatusText() const;

	/** Cooldowns en tiempo real (no se reinician al morir ni al recargar el nivel). */
	bool IsCooldownReady(FName Key, float CooldownSeconds) const;
	bool IsUserReady(const FString& User, float CooldownSeconds) const;
	void MarkUsed(FName Key, const FString& User);

	FOnTwitchChatCommand OnChatCommand;

private:
	void OpenSocket();
	void HandleConnected();
	void HandleConnectionError(const FString& Error);
	void HandleClosed(int32 StatusCode, const FString& Reason, bool bWasClean);
	void HandleMessage(const FString& Message);
	void ProcessLine(const FString& Line);
	void ScheduleReconnect();
	void Send(const FString& Line);

	TSharedPtr<IWebSocket> Socket;
	FString Channel;
	FString Pending;
	ETwitchStatus Status = ETwitchStatus::Disabled;
	int32 ReconnectAttempts = 0;
	bool bWantConnection = false;
	FTimerHandle ReconnectTimer;

	TMap<FName, double> CooldownTimes;
	TMap<FString, double> UserTimes;
};
