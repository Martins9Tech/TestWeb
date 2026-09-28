#include "World/ClueNote.h"

#include "Core/VestuarioGameMode.h"
#include "Core/VestuarioTypes.h"

#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "Vestuario"

AClueNote::AClueNote()
{
	PrimaryActorTick.bCanEverTick = true;

	ReadText = LOCTEXT("NoteRead",
		"Nota: \"Los fusibles NO van al azar. El ROJO nunca va primero. El VERDE siempre sigue al ROJO. No hagas ruido: te oye.\"");

	InteractBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractBox"));
	InteractBox->InitBoxExtent(FVector(3.f, 17.f, 22.f));
	InteractBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	SetRootComponent(InteractBox);

	PaperMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Paper"));
	PaperMesh->SetupAttachment(InteractBox);
	PaperMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Texto sin tildes: la fuente por defecto del TextRender no las garantiza
	Text = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Text"));
	Text->SetupAttachment(InteractBox);
	Text->SetRelativeLocation(FVector(0.6f, 0.f, 4.f));
	Text->SetText(FText::FromString(TEXT("LOS FUSIBLES NO VAN AL AZAR<br><br>EL ROJO NUNCA VA PRIMERO<br>EL VERDE SIEMPRE SIGUE AL ROJO<br><br>NO HAGAS RUIDO. TE OYE.")));
	Text->SetWorldSize(1.6f);
	Text->SetHorizontalAlignment(EHTA_Center);
	Text->SetVerticalAlignment(EVRTA_TextCenter);
	Text->SetTextRenderColor(FColor(40, 25, 20));
	Text->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	HintLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("HintLight"));
	HintLight->SetupAttachment(InteractBox);
	HintLight->SetRelativeLocation(FVector(45.f, 0.f, 0.f));
	HintLight->IntensityUnits = ELightUnits::Lumens;
	HintLight->Intensity = 0.f;
	HintLight->AttenuationRadius = 250.f;
	HintLight->SetLightColor(FLinearColor(1.f, 0.85f, 0.5f));
}

void AClueNote::ApplyMeshes()
{
	if (!PaperMesh->GetStaticMesh())
	{
		PaperMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *Vestuario::MeshPath(TEXT("SM_Note")), nullptr, LOAD_NoWarn));
	}
}

void AClueNote::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyMeshes();
}

void AClueNote::BeginPlay()
{
	Super::BeginPlay();
	ApplyMeshes();
}

void AClueNote::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < FlashUntil)
	{
		HintLight->SetIntensity(120.f + 80.f * FMath::Sin(Now * 6.f));
	}
	else if (HintLight->Intensity > 0.f)
	{
		HintLight->SetIntensity(0.f);
	}
}

void AClueNote::Flash(float Duration)
{
	FlashUntil = GetWorld()->GetTimeSeconds() + Duration;
}

void AClueNote::Interact(AVestuarioCharacter* Player, UPrimitiveComponent* HitComponent)
{
	if (AVestuarioGameMode* GM = GetWorld()->GetAuthGameMode<AVestuarioGameMode>())
	{
		GM->ShowMessage(ReadText, 9.f);
	}
}

FText AClueNote::GetInteractText(const AVestuarioCharacter* Player, UPrimitiveComponent* HitComponent) const
{
	return LOCTEXT("ReadNote", "[E] Leer la nota");
}

#undef LOCTEXT_NAMESPACE
