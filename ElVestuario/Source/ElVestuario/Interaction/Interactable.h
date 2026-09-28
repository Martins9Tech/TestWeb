#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interactable.generated.h"

class AVestuarioCharacter;
class UPrimitiveComponent;

UINTERFACE(MinimalAPI)
class UInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Cualquier cosa con la que el jugador puede interactuar pulsando [E].
 * El jugador lanza un rayo desde la camara; si golpea un actor con esta interfaz,
 * muestra GetInteractText() en pantalla y llama a Interact() al pulsar la tecla.
 */
class ELVESTUARIO_API IInteractable
{
	GENERATED_BODY()

public:
	/** HitComponent es el componente concreto que golpeo el rayo (util para las ranuras del cuadro). */
	virtual void Interact(AVestuarioCharacter* Player, UPrimitiveComponent* HitComponent) = 0;

	virtual FText GetInteractText(const AVestuarioCharacter* Player, UPrimitiveComponent* HitComponent) const = 0;
};
