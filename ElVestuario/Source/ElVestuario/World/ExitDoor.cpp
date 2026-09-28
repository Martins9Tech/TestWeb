#include "World/ExitDoor.h"

#include "Core/VestuarioGameMode.h"
#include "Core/VestuarioTypes.h"
#include "Player/VestuarioCharacter.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Perception/AISense_Hearing.h"

#define LOCTEXT_NAMESPACE "Vestuario"

namespace ExitDoorConst
{
	constexpr float DoorHalfWidth = 60.f;
}

AExitDoor::AExitDoor()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	DoorPivot = CreateDefaultSubobject<USceneComponent>(TEXT("DoorPivot"));
	DoorPivot->SetupAttachment(Root);
	DoorPivot->SetRelativeLocation(FVector(0.f, ExitDoorConst::DoorHalfWidth, 0.f));

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Door"));
	DoorMesh->SetupAttachment(DoorPivot);
	DoorMesh->SetRelativeLocation(FVector(0.f, -ExitDoorConst::DoorHalfWidth, 0.f));

	EscapeTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("EscapeTrigger"));
	EscapeTrigger->SetupAttachment(Root);
	EscapeTrigger->SetRelativeLocation(FVector(-260.f, 0.f, 110.f));
	EscapeTrigger->InitBoxExtent(FVector(80.f, 100.f, 110.f));
	EscapeTrigger->SetCollisionProfileName(TEXT("Trigger"));
}

void AExitDoor::ApplyMeshes()
{
	if (!DoorMesh->GetStaticMesh())
	{
		DoorMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *Vestuario::MeshPath(TEXT("SM_ExitDoor")), nullptr, LOAD_NoWarn));
	}
}

void AExitDoor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyMeshes();
}

void AExitDoor::BeginPlay()
{
	Super::BeginPlay();
	ApplyMeshes();
	EscapeTrigger->OnComponentBeginOverlap.AddDynamic(this, &AExitDoor::OnEscapeOverlap);
}

void AExitDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bOpen && OpenAlpha < 1.f)
	{
		OpenAlpha = FMath::Min(1.f, OpenAlpha + DeltaSeconds / OpenDuration);
		const float Eased = FMath::InterpEaseInOut(0.f, 1.f, OpenAlpha, 2.f);
		DoorPivot->SetRelativeRotation(FRotator(0.f, OpenYaw * Eased, 0.f));
	}
}

void AExitDoor::Open()
{
	if (bOpen)
	{
		return;
	}
	bOpen = true;
	if (AVestuarioGameMode* GM = GetWorld()->GetAuthGameMode<AVestuarioGameMode>())
	{
		GM->PlaySound3D(EVestuarioSound::DoorOpen, GetActorLocation() + FVector(0.f, 0.f, 110.f), 1.2f);
	}
}

void AExitDoor::Interact(AVestuarioCharacter* Player, UPrimitiveComponent* HitComponent)
{
	if (bOpen)
	{
		return;
	}
	AVestuarioGameMode* GM = GetWorld()->GetAuthGameMode<AVestuarioGameMode>();
	if (GM)
	{
		GM->PlaySound3D(EVestuarioSound::Knock, GetActorLocation() + FVector(0.f, 0.f, 100.f), 0.5f, 1.3f);
		GM->ShowMessage(LOCTEXT("DoorLocked", "La puerta no se mueve. El cierre el\u00E9ctrico no tiene corriente."), 3.5f);
	}
	// Forcejear con la puerta hace algo de ruido
	UAISense_Hearing::ReportNoiseEvent(this, GetActorLocation() + GetActorForwardVector() * 60.f, 0.35f, Player);
}

FText AExitDoor::GetInteractText(const AVestuarioCharacter* Player, UPrimitiveComponent* HitComponent) const
{
	return bOpen ? FText::GetEmpty() : LOCTEXT("DoorTry", "[E] Intentar abrir la puerta");
}

void AExitDoor::OnEscapeOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bOpen && Cast<AVestuarioCharacter>(OtherActor))
	{
		if (AVestuarioGameMode* GM = GetWorld()->GetAuthGameMode<AVestuarioGameMode>())
		{
			GM->OnPlayerEscaped();
		}
	}
}

#undef LOCTEXT_NAMESPACE
