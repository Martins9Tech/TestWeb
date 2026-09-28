#include "World/FusePickup.h"

#include "Core/VestuarioGameMode.h"
#include "Player/VestuarioCharacter.h"

#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "Vestuario"

AFusePickup::AFusePickup()
{
	PrimaryActorTick.bCanEverTick = true;

	InteractSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractSphere"));
	InteractSphere->InitSphereRadius(16.f);
	InteractSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	SetRootComponent(InteractSphere);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(InteractSphere);
	Mesh->SetRelativeRotation(FRotator(90.f, 0.f, 0.f)); // tumbado
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Glint = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glint"));
	Glint->SetupAttachment(InteractSphere);
	Glint->SetRelativeLocation(FVector(0.f, 0.f, 8.f));
	Glint->IntensityUnits = ELightUnits::Lumens;
	Glint->Intensity = GlintLumens;
	Glint->AttenuationRadius = 90.f;
	Glint->SetCastShadows(false);
}

void AFusePickup::ApplyAppearance()
{
	Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *Vestuario::FuseMeshPath(FuseColor), nullptr, LOAD_NoWarn));
	Glint->SetLightColor(Vestuario::FuseLinearColor(FuseColor));
	Glint->SetIntensity(GlintLumens);
}

void AFusePickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyAppearance();
}

void AFusePickup::BeginPlay()
{
	Super::BeginPlay();
	ApplyAppearance();
}

void AFusePickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < HighlightUntil)
	{
		const float Pulse = 0.5f + 0.5f * FMath::Sin(Now * 7.f);
		Glint->SetIntensity(GlintLumens + 80.f * Pulse);
		Glint->SetAttenuationRadius(220.f);
	}
	else if (HighlightUntil > 0.f)
	{
		HighlightUntil = 0.f;
		Glint->SetIntensity(GlintLumens);
		Glint->SetAttenuationRadius(90.f);
	}
}

void AFusePickup::Highlight(float Duration)
{
	HighlightUntil = GetWorld()->GetTimeSeconds() + Duration;
}

void AFusePickup::Interact(AVestuarioCharacter* Player, UPrimitiveComponent* HitComponent)
{
	if (!Player)
	{
		return;
	}
	Player->AddFuse(FuseColor);
	if (AVestuarioGameMode* GM = GetWorld()->GetAuthGameMode<AVestuarioGameMode>())
	{
		GM->PlaySound3D(EVestuarioSound::Pickup, GetActorLocation(), 0.8f);
		GM->ShowMessage(FText::Format(LOCTEXT("GotFuse", "Has cogido el fusible {0}."), Vestuario::FuseName(FuseColor)), 3.f);
	}
	Destroy();
}

FText AFusePickup::GetInteractText(const AVestuarioCharacter* Player, UPrimitiveComponent* HitComponent) const
{
	return FText::Format(LOCTEXT("TakeFuse", "[E] Coger fusible {0}"), Vestuario::FuseName(FuseColor));
}

#undef LOCTEXT_NAMESPACE
