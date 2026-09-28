#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EnemyCharacter.generated.h"

class UAudioComponent;
class UStaticMeshComponent;

/**
 * "El Banista": figura alta y ciega que caza por el sonido.
 * Usa un mesh estatico con animacion procedural (balanceo, inclinacion y tics de cabeza),
 * asi el prototipo funciona sin esqueleto ni animaciones. Mas adelante se sustituye por un
 * Skeletal Mesh con animaciones reales (Mixamo, mocap...).
 */
UCLASS()
class ELVESTUARIO_API AEnemyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AEnemyCharacter();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void SetMoveSpeed(float Speed);
	void OnChaseStarted();
	void OnKill();

	/** Puntos de patrulla (se rellenan desde el script de nivel). */
	UPROPERTY(EditInstanceOnly, Category = "IA")
	TArray<TObjectPtr<AActor>> PatrolPoints;

	UPROPERTY(EditDefaultsOnly, Category = "IA")
	float PatrolSpeed = 120.f;

	UPROPERTY(EditDefaultsOnly, Category = "IA")
	float InvestigateSpeed = 175.f;

	/** Un pelin mas lento que el jugador corriendo: puedes huir, pero corriendo haces ruido. */
	UPROPERTY(EditDefaultsOnly, Category = "IA")
	float ChaseSpeed = 385.f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Componentes")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, Category = "Componentes")
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	UPROPERTY(VisibleAnywhere, Category = "Componentes")
	TObjectPtr<UAudioComponent> BreathAudio;

private:
	void ApplyMeshes();
	void UpdateProceduralAnimation(float DeltaSeconds);
	void UpdateFootsteps(float DeltaSeconds);

	float AnimTime = 0.f;
	float StepDistance = 0.f;
	float NextTwitchTime = 0.f;
	float TwitchEndTime = 0.f;
	FRotator TwitchRotation = FRotator::ZeroRotator;
	FRotator CurrentHeadRotation = FRotator::ZeroRotator;
	bool bKilling = false;
};
