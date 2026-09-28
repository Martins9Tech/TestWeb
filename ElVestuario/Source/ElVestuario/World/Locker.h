#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/Interactable.h"
#include "Locker.generated.h"

class UStaticMeshComponent;

/**
 * Taquilla en la que el jugador puede esconderse. Desde dentro se ve a traves de las lamas.
 * Si el enemigo te esta persiguiendo y te oye entrar, sabe donde estas y abre la puerta.
 */
UCLASS()
class ELVESTUARIO_API ALocker : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ALocker();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual void Interact(AVestuarioCharacter* Player, UPrimitiveComponent* HitComponent) override;
	virtual FText GetInteractText(const AVestuarioCharacter* Player, UPrimitiveComponent* HitComponent) const override;

	FVector GetHideLocation() const;
	FRotator GetHideRotation() const;
	FVector GetExitLocation() const;

	/** El enemigo arranca la puerta. */
	void ForceOpen();

protected:
	UPROPERTY(VisibleAnywhere, Category = "Componentes")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Componentes")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, Category = "Componentes")
	TObjectPtr<USceneComponent> DoorPivot;

	UPROPERTY(VisibleAnywhere, Category = "Componentes")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	/** Ruido al entrar (multiplica el oido del enemigo). */
	UPROPERTY(EditAnywhere, Category = "Taquilla")
	float EnterLoudness = 0.4f;

	UPROPERTY(EditAnywhere, Category = "Taquilla")
	float ExitLoudness = 0.3f;

private:
	void ApplyMeshes();
	void OpenBriefly(float CloseAfter);
	void CloseDoor();

	float DoorAlpha = 0.f;
	float DoorTarget = 0.f;
	bool bForcedOpen = false;
	FTimerHandle CloseTimer;
};
