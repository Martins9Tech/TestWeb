#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/VestuarioTypes.h"
#include "Interaction/Interactable.h"
#include "FusePickup.generated.h"

class UPointLightComponent;
class USphereComponent;
class UStaticMeshComponent;

/** Fusible que se puede coger. Tiene un brillo muy tenue para poder encontrarlo con la linterna. */
UCLASS()
class ELVESTUARIO_API AFusePickup : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AFusePickup();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual void Interact(AVestuarioCharacter* Player, UPrimitiveComponent* HitComponent) override;
	virtual FText GetInteractText(const AVestuarioCharacter* Player, UPrimitiveComponent* HitComponent) const override;

	/** El chat (!pista) hace que brille durante unos segundos. */
	void Highlight(float Duration);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fusible")
	EFuseColor FuseColor = EFuseColor::Red;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Componentes")
	TObjectPtr<USphereComponent> InteractSphere;

	UPROPERTY(VisibleAnywhere, Category = "Componentes")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Componentes")
	TObjectPtr<UPointLightComponent> Glint;

	UPROPERTY(EditAnywhere, Category = "Fusible")
	float GlintLumens = 6.f;

private:
	void ApplyAppearance();
	float HighlightUntil = 0.f;
};
