#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/Interactable.h"
#include "ExitDoor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/** Puerta de salida con cierre electrico. Se abre cuando vuelve la luz. Cruzarla = victoria. */
UCLASS()
class ELVESTUARIO_API AExitDoor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AExitDoor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual void Interact(AVestuarioCharacter* Player, UPrimitiveComponent* HitComponent) override;
	virtual FText GetInteractText(const AVestuarioCharacter* Player, UPrimitiveComponent* HitComponent) const override;

	void Open();
	bool IsOpen() const { return bOpen; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Componentes")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Componentes")
	TObjectPtr<USceneComponent> DoorPivot;

	UPROPERTY(VisibleAnywhere, Category = "Componentes")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	/** Zona del pasillo que da la victoria al entrar. */
	UPROPERTY(VisibleAnywhere, Category = "Componentes")
	TObjectPtr<UBoxComponent> EscapeTrigger;

	UPROPERTY(EditAnywhere, Category = "Puerta")
	float OpenYaw = -95.f;

	UPROPERTY(EditAnywhere, Category = "Puerta")
	float OpenDuration = 2.4f;

private:
	UFUNCTION()
	void OnEscapeOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void ApplyMeshes();
	float OpenAlpha = 0.f;
	bool bOpen = false;
};
