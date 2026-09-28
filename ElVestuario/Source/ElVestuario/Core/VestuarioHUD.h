#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "VestuarioHUD.generated.h"

class UFont;

/**
 * HUD dibujado con Canvas (sin assets de UMG) para que el prototipo funcione solo con codigo.
 * Cuando querais un HUD bonito, se sustituye por un Widget Blueprint.
 */
UCLASS()
class ELVESTUARIO_API AVestuarioHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawCentered(const FString& Text, float Y, UFont* Font, float Scale, const FLinearColor& Color);
	void DrawRight(const FString& Text, float Y, UFont* Font, float Scale, const FLinearColor& Color);
};
