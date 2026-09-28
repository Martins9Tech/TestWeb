#include "World/FusePanel.h"

#include "Core/VestuarioGameMode.h"
#include "Player/VestuarioCharacter.h"

#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Perception/AISense_Hearing.h"

#define LOCTEXT_NAMESPACE "Vestuario"

namespace FusePanelConst
{
	constexpr int32 NumSlots = 3;
	const TCHAR* SlotRoman[NumSlots] = { TEXT("I"), TEXT("II"), TEXT("III") };

	// Ranura I a la izquierda mirando el cuadro de frente (el cuadro mira a +X)
	FVector SlotLocation(int32 Index)
	{
		return FVector(15.f, (1 - Index) * 18.f, 2.f);
	}
}

AFusePanel::AFusePanel()
{
	PrimaryActorTick.bCanEverTick = true;
	CorrectOrder = { EFuseColor::Blue, EFuseColor::Red, EFuseColor::Green };

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	PanelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Panel"));
	PanelMesh->SetupAttachment(Root);

	for (int32 i = 0; i < FusePanelConst::NumSlots; ++i)
	{
		UBoxComponent* Box = CreateDefaultSubobject<UBoxComponent>(*FString::Printf(TEXT("Slot%d"), i));
		Box->SetupAttachment(Root);
		Box->SetRelativeLocation(FusePanelConst::SlotLocation(i));
		Box->InitBoxExtent(FVector(5.f, 6.f, 10.f));
		Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Box->SetCollisionResponseToAllChannels(ECR_Ignore);
		Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		SlotBoxes.Add(Box);

		UStaticMeshComponent* Fuse = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("SlotFuse%d"), i));
		Fuse->SetupAttachment(Root);
		Fuse->SetRelativeLocation(FusePanelConst::SlotLocation(i) + FVector(2.f, 0.f, 0.f));
		Fuse->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Fuse->SetVisibility(false);
		SlotFuses.Add(Fuse);

		UTextRenderComponent* Label = CreateDefaultSubobject<UTextRenderComponent>(*FString::Printf(TEXT("SlotLabel%d"), i));
		Label->SetupAttachment(Root);
		Label->SetRelativeLocation(FVector(14.f, (1 - i) * 18.f, 17.f));
		Label->SetText(FText::FromString(FusePanelConst::SlotRoman[i]));
		Label->SetWorldSize(5.f);
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetVerticalAlignment(EVRTA_TextCenter);
		Label->SetTextRenderColor(FColor(20, 20, 20));
		Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SlotLabels.Add(Label);
	}

	SparkLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("SparkLight"));
	SparkLight->SetupAttachment(Root);
	SparkLight->SetRelativeLocation(FVector(30.f, 0.f, 0.f));
	SparkLight->IntensityUnits = ELightUnits::Lumens;
	SparkLight->Intensity = 0.f;
	SparkLight->AttenuationRadius = 900.f;
	SparkLight->SetLightColor(FLinearColor(0.6f, 0.75f, 1.f));

	StatusLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StatusLight"));
	StatusLight->SetupAttachment(Root);
	StatusLight->SetRelativeLocation(FVector(18.f, 0.f, 32.f));
	StatusLight->IntensityUnits = ELightUnits::Lumens;
	StatusLight->Intensity = 4.f;
	StatusLight->AttenuationRadius = 70.f;
	StatusLight->SetCastShadows(false);
	StatusLight->SetLightColor(FLinearColor(1.f, 0.05f, 0.02f));
}

void AFusePanel::ApplyMeshes()
{
	if (!PanelMesh->GetStaticMesh())
	{
		PanelMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *Vestuario::MeshPath(TEXT("SM_FusePanel")), nullptr, LOAD_NoWarn));
	}
}

void AFusePanel::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyMeshes();
}

void AFusePanel::BeginPlay()
{
	Super::BeginPlay();
	ApplyMeshes();
	SlotFilled.Init(false, FusePanelConst::NumSlots);
	if (CorrectOrder.Num() != FusePanelConst::NumSlots)
	{
		CorrectOrder = { EFuseColor::Blue, EFuseColor::Red, EFuseColor::Green };
	}
}

void AFusePanel::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < SparkUntil)
	{
		SparkLight->SetIntensity(FMath::FRand() < 0.5f ? 6000.f : 800.f);
	}
	else if (SparkLight->Intensity > 0.f)
	{
		SparkLight->SetIntensity(0.f);
	}
}

int32 AFusePanel::SlotFromComponent(const UPrimitiveComponent* Component) const
{
	for (int32 i = 0; i < SlotBoxes.Num(); ++i)
	{
		if (SlotBoxes[i].Get() == Component)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

void AFusePanel::Interact(AVestuarioCharacter* Player, UPrimitiveComponent* HitComponent)
{
	const int32 Slot = SlotFromComponent(HitComponent);
	AVestuarioGameMode* GM = GetWorld()->GetAuthGameMode<AVestuarioGameMode>();
	EFuseColor Fuse;
	if (!Player || bSolved || Slot == INDEX_NONE || SlotFilled[Slot] || !Player->GetSelectedFuse(Fuse))
	{
		return;
	}

	if (Fuse != CorrectOrder[Slot])
	{
		Spark(Player);
		return;
	}

	Player->ConsumeSelectedFuse();
	SlotFilled[Slot] = true;
	SlotFuses[Slot]->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *Vestuario::FuseMeshPath(Fuse), nullptr, LOAD_NoWarn));
	SlotFuses[Slot]->SetVisibility(true);
	if (GM)
	{
		GM->PlaySound3D(EVestuarioSound::FuseClick, GetActorLocation(), 1.f);
	}

	for (bool bFilled : SlotFilled)
	{
		if (!bFilled)
		{
			if (GM)
			{
				GM->ShowMessage(LOCTEXT("FuseOk", "El fusible encaja con un clic."), 2.5f);
			}
			return;
		}
	}

	bSolved = true;
	StatusLight->SetLightColor(FLinearColor(0.1f, 1.f, 0.2f));
	StatusLight->SetIntensity(15.f);
	if (GM)
	{
		GM->OnPowerRestored();
	}
}

void AFusePanel::Spark(AVestuarioCharacter* Player)
{
	SparkUntil = GetWorld()->GetTimeSeconds() + 0.45f;
	if (AVestuarioGameMode* GM = GetWorld()->GetAuthGameMode<AVestuarioGameMode>())
	{
		GM->PlaySound3D(EVestuarioSound::Spark, GetActorLocation(), 1.5f);
		GM->ShowMessage(LOCTEXT("Spark", "\u00A1CHISPAZO! Esa no era su ranura... y ha sonado muy fuerte."), 4.f);
	}
	UAISense_Hearing::ReportNoiseEvent(this, GetActorLocation() + GetActorForwardVector() * 60.f, SparkLoudness, Player);
}

FText AFusePanel::GetInteractText(const AVestuarioCharacter* Player, UPrimitiveComponent* HitComponent) const
{
	if (bSolved)
	{
		return LOCTEXT("PanelSolved", "El cuadro vuelve a tener corriente.");
	}
	const int32 Slot = SlotFromComponent(HitComponent);
	if (Slot == INDEX_NONE)
	{
		return LOCTEXT("PanelBody", "Cuadro de fusibles. Tres ranuras vac\u00EDas: I, II, III.");
	}
	const FText SlotName = FText::FromString(FusePanelConst::SlotRoman[Slot]);
	if (SlotFilled.IsValidIndex(Slot) && SlotFilled[Slot])
	{
		return FText::Format(LOCTEXT("SlotFull", "Ranura {0}: ya tiene su fusible."), SlotName);
	}
	EFuseColor Fuse;
	if (!Player || !Player->GetSelectedFuse(Fuse))
	{
		return FText::Format(LOCTEXT("SlotEmpty", "Ranura {0}: vac\u00EDa. Necesitas un fusible."), SlotName);
	}
	return FText::Format(LOCTEXT("SlotPlace", "[E] Colocar fusible {0} en la ranura {1}"), Vestuario::FuseName(Fuse), SlotName);
}

#undef LOCTEXT_NAMESPACE
