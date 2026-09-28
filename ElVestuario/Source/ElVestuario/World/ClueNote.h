#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/Interactable.h"
#include "ClueNote.generated.h"

class UBoxComponent;
class UPointLightComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/** Nota pegada en la pared con la pista del puzle. Se puede leer de cerca o con [E]. */
UCLASS()
class ELVESTUARIO_API AClueNote : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AClueNote();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual void Interact(AVestuarioCharacter* Player, UPrimitiveComponent* HitComponent) override;
	virtual FText GetInteractText(const AVestuarioCharacter* Player, UPrimitiveComponent* HitComponent) const override;

	/** El chat (!pista) ilumina la nota. */
	void Flash(float Duration);

	/** Texto que aparece en pantalla al leerla. */
	UPROPERTY(EditAnywhere, Category = "Nota", meta = (MultiLine = true))
	FText ReadText;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Componentes")
	TObjectPtr<UBoxComponent> InteractBox;

	UPROPERTY(VisibleAnywhere, Category = "Componentes")
	TObjectPtr<UStaticMeshComponent> PaperMesh;

	UPROPERTY(VisibleAnywhere, Category = "Componentes")
	TObjectPtr<UTextRenderComponent> Text;

	UPROPERTY(VisibleAnywhere, Category = "Componentes")
	TObjectPtr<UPointLightComponent> HintLight;

private:
	void ApplyMeshes();
	float FlashUntil = 0.f;
};
