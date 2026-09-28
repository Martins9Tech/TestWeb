#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/VestuarioTypes.h"
#include "Interaction/Interactable.h"
#include "FusePanel.generated.h"

class UBoxComponent;
class UPointLightComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * EL PUZLE: cuadro electrico con 3 ranuras (I, II, III).
 * Hay que colocar los fusibles en el orden correcto. La nota de la pared da las pistas:
 *   "EL ROJO NUNCA VA PRIMERO" + "EL VERDE SIEMPRE SIGUE AL ROJO"  ->  AZUL, ROJO, VERDE
 * Un fusible en la ranura equivocada provoca un chispazo MUY ruidoso (el enemigo lo oye).
 */
UCLASS()
class ELVESTUARIO_API AFusePanel : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AFusePanel();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual void Interact(AVestuarioCharacter* Player, UPrimitiveComponent* HitComponent) override;
	virtual FText GetInteractText(const AVestuarioCharacter* Player, UPrimitiveComponent* HitComponent) const override;

	bool IsSolved() const { return bSolved; }

	/** Solucion del puzle, ranura I -> III. */
	UPROPERTY(EditAnywhere, Category = "Puzle")
	TArray<EFuseColor> CorrectOrder;

	/** Ruido del chispazo (multiplica el oido del enemigo; 1.5 = se oye en toda la sala). */
	UPROPERTY(EditAnywhere, Category = "Puzle")
	float SparkLoudness = 1.5f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Componentes")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Componentes")
	TObjectPtr<UStaticMeshComponent> PanelMesh;

	UPROPERTY(VisibleAnywhere, Category = "Componentes")
	TArray<TObjectPtr<UBoxComponent>> SlotBoxes;

	UPROPERTY(VisibleAnywhere, Category = "Componentes")
	TArray<TObjectPtr<UStaticMeshComponent>> SlotFuses;

	UPROPERTY(VisibleAnywhere, Category = "Componentes")
	TArray<TObjectPtr<UTextRenderComponent>> SlotLabels;

	UPROPERTY(VisibleAnywhere, Category = "Componentes")
	TObjectPtr<UPointLightComponent> SparkLight;

	UPROPERTY(VisibleAnywhere, Category = "Componentes")
	TObjectPtr<UPointLightComponent> StatusLight;

private:
	int32 SlotFromComponent(const UPrimitiveComponent* Component) const;
	void Spark(AVestuarioCharacter* Player);
	void ApplyMeshes();

	TArray<bool> SlotFilled;
	float SparkUntil = 0.f;
	bool bSolved = false;
};
