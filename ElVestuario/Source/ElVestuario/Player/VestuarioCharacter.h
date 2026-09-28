#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/VestuarioTypes.h"
#include "VestuarioCharacter.generated.h"

class AEnemyCharacter;
class AEnemyAIController;
class ALocker;
class UAudioComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class USpotLightComponent;
class UPrimitiveComponent;
struct FInputActionValue;

/**
 * Jugador en primera persona.
 *
 * Todo el ruido que hace (pasos, correr) se envia al sistema de percepcion de la IA con
 * UAISense_Hearing::ReportNoiseEvent. El enemigo es ciego: solo sabe donde estas por lo que oye.
 *
 * Los controles se crean por codigo (Enhanced Input) para no depender de assets.
 */
UCLASS()
class ELVESTUARIO_API AVestuarioCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AVestuarioCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;

	// --- Fusibles ---
	void AddFuse(EFuseColor Color);
	bool GetSelectedFuse(EFuseColor& OutColor) const;
	void ConsumeSelectedFuse();
	const TArray<EFuseColor>& GetHeldFuses() const { return HeldFuses; }
	int32 GetSelectedFuseIndex() const { return SelectedFuse; }

	// --- Taquillas ---
	void EnterLocker(ALocker* Locker);
	void ExitLocker();
	bool IsHiddenInLocker() const { return CurrentLocker != nullptr; }
	ALocker* GetCurrentLocker() const { return CurrentLocker; }

	// --- Muerte ---
	void Die(AActor* Killer);
	bool IsDead() const { return bDead; }

	// --- Para el HUD ---
	const FText& GetFocusText() const { return FocusText; }
	float GetFear() const { return Fear; }

	/** Consola: SimularChat !ruido  |  SimularChat !pista */
	UFUNCTION(Exec)
	void SimularChat(const FString& Comando);

	/** Consola: TwitchConectar nombredelcanal */
	UFUNCTION(Exec)
	void TwitchConectar(const FString& Canal);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Componentes")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Componentes")
	TObjectPtr<USpotLightComponent> Flashlight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Componentes")
	TObjectPtr<UAudioComponent> HeartbeatAudio;

	// --- Movimiento ---
	UPROPERTY(EditDefaultsOnly, Category = "Movimiento")
	float WalkSpeed = 190.f;

	UPROPERTY(EditDefaultsOnly, Category = "Movimiento")
	float SprintSpeed = 400.f;

	UPROPERTY(EditDefaultsOnly, Category = "Movimiento")
	float CrouchSpeed = 110.f;

	UPROPERTY(EditDefaultsOnly, Category = "Movimiento")
	float MouseSensitivity = 1.0f;

	/** Activalo si el raton va al reves arriba/abajo. */
	UPROPERTY(EditDefaultsOnly, Category = "Movimiento")
	bool bInvertMouseY = false;

	// --- Ruido (multiplica el radio de oido del enemigo, 3000 cm) ---
	UPROPERTY(EditDefaultsOnly, Category = "Ruido")
	float WalkLoudness = 0.2f;		// ~6 m

	UPROPERTY(EditDefaultsOnly, Category = "Ruido")
	float SprintLoudness = 1.0f;	// ~30 m

	UPROPERTY(EditDefaultsOnly, Category = "Ruido")
	float CrouchLoudness = 0.05f;	// ~1,5 m

	// --- Interaccion ---
	UPROPERTY(EditDefaultsOnly, Category = "Interaccion")
	float InteractDistance = 220.f;

private:
	void EnsureInputObjects();

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void StartSprint();
	void StopSprint();
	void ToggleCrouch();
	void Interact();
	void ToggleFlashlight();
	void CycleFuse();
	void QuitGame();
	void DebugNoise();
	void DebugHint();
	void DebugAI();

	void UpdateSpeed();
	void UpdateFootsteps(float DeltaSeconds);
	void UpdateFocus();
	void UpdateFear(float DeltaSeconds);
	void UpdateCamera(float DeltaSeconds);
	void UpdateDeathCamera(float DeltaSeconds);
	bool IsSprinting() const;
	class AVestuarioGameMode* GetVestuarioGameMode() const;

	// Objetos de input creados en tiempo de ejecucion
	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> MappingContext;
	UPROPERTY(Transient) TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LookAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> SprintAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> CrouchAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> InteractAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> FlashlightAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> CycleFuseAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> QuitAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> DebugNoiseAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> DebugHintAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> DebugAIAction;

	UPROPERTY(Transient)
	TObjectPtr<ALocker> CurrentLocker;

	TWeakObjectPtr<AActor> FocusedActor;
	TWeakObjectPtr<UPrimitiveComponent> FocusedComponent;
	TWeakObjectPtr<AActor> Killer;
	TWeakObjectPtr<AEnemyCharacter> CachedEnemy;

	TArray<EFuseColor> HeldFuses;
	int32 SelectedFuse = 0;

	FText FocusText;
	float Fear = 0.f;
	float StepDistance = 0.f;
	float BobTime = 0.f;
	float FlashlightBaseIntensity = 0.f;
	float CameraBaseZ = 64.f;
	bool bWantsSprint = false;
	bool bFlashlightOn = true;
	bool bDead = false;
};
