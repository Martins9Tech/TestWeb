#include "World/Locker.h"

#include "Core/VestuarioGameMode.h"
#include "Core/VestuarioTypes.h"
#include "Player/VestuarioCharacter.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Perception/AISense_Hearing.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "Vestuario"

namespace LockerConst
{
	// Medidas del mesh SM_Locker (cm): 50 de fondo, 55 de ancho, frente en +X
	const FVector HingeOffset(25.f, 27.5f, 0.f);
	constexpr float DoorHalfWidth = 27.5f;
	constexpr float OpenAngle = 105.f;
}

ALocker::ALocker()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	BodyMesh->SetupAttachment(Root);

	DoorPivot = CreateDefaultSubobject<USceneComponent>(TEXT("DoorPivot"));
	DoorPivot->SetupAttachment(Root);
	DoorPivot->SetRelativeLocation(LockerConst::HingeOffset);

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Door"));
	DoorMesh->SetupAttachment(DoorPivot);
	DoorMesh->SetRelativeLocation(FVector(0.f, -LockerConst::DoorHalfWidth, 0.f));
	DoorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ALocker::ApplyMeshes()
{
	if (!BodyMesh->GetStaticMesh())
	{
		BodyMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *Vestuario::MeshPath(TEXT("SM_Locker")), nullptr, LOAD_NoWarn));
	}
	if (!DoorMesh->GetStaticMesh())
	{
		DoorMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *Vestuario::MeshPath(TEXT("SM_LockerDoor")), nullptr, LOAD_NoWarn));
	}
}

void ALocker::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyMeshes();
}

void ALocker::BeginPlay()
{
	Super::BeginPlay();
	ApplyMeshes();
}

void ALocker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Speed = bForcedOpen ? 14.f : 7.f;
	DoorAlpha = FMath::FInterpTo(DoorAlpha, DoorTarget, DeltaSeconds, Speed);
	DoorPivot->SetRelativeRotation(FRotator(0.f, DoorAlpha * LockerConst::OpenAngle, 0.f));
}

FVector ALocker::GetHideLocation() const
{
	return GetActorTransform().TransformPosition(FVector(-2.f, 0.f, 90.f));
}

FRotator ALocker::GetHideRotation() const
{
	return GetActorRotation();
}

FVector ALocker::GetExitLocation() const
{
	return GetActorTransform().TransformPosition(FVector(80.f, 0.f, 92.f));
}

void ALocker::Interact(AVestuarioCharacter* Player, UPrimitiveComponent* HitComponent)
{
	if (!Player || bForcedOpen)
	{
		return;
	}
	AVestuarioGameMode* GM = GetWorld()->GetAuthGameMode<AVestuarioGameMode>();

	if (Player->GetCurrentLocker() == this)
	{
		OpenBriefly(0.9f);
		Player->ExitLocker();
		if (GM)
		{
			GM->PlaySound3D(EVestuarioSound::LockerCreak, GetActorLocation() + FVector(0, 0, 120.f), 0.6f, 1.1f);
		}
		UAISense_Hearing::ReportNoiseEvent(this, GetActorLocation(), ExitLoudness, Player);
	}
	else if (!Player->IsHiddenInLocker())
	{
		OpenBriefly(0.45f);
		Player->EnterLocker(this);
		if (GM)
		{
			GM->PlaySound3D(EVestuarioSound::LockerCreak, GetActorLocation() + FVector(0, 0, 120.f), 0.8f);
		}
		UAISense_Hearing::ReportNoiseEvent(this, GetActorLocation(), EnterLoudness, Player, 0.f, Vestuario::LockerNoiseTag());
	}
}

FText ALocker::GetInteractText(const AVestuarioCharacter* Player, UPrimitiveComponent* HitComponent) const
{
	if (bForcedOpen)
	{
		return FText::GetEmpty();
	}
	if (Player && Player->GetCurrentLocker() == this)
	{
		return LOCTEXT("LockerExit", "[E] Salir de la taquilla");
	}
	return LOCTEXT("LockerEnter", "[E] Esconderse en la taquilla");
}

void ALocker::OpenBriefly(float CloseAfter)
{
	DoorTarget = 1.f;
	GetWorldTimerManager().SetTimer(CloseTimer, this, &ALocker::CloseDoor, CloseAfter, false);
}

void ALocker::CloseDoor()
{
	if (!bForcedOpen)
	{
		DoorTarget = 0.f;
	}
}

void ALocker::ForceOpen()
{
	bForcedOpen = true;
	DoorTarget = 1.f;
	GetWorldTimerManager().ClearTimer(CloseTimer);
	if (AVestuarioGameMode* GM = GetWorld()->GetAuthGameMode<AVestuarioGameMode>())
	{
		GM->PlaySound3D(EVestuarioSound::LockerCreak, GetActorLocation() + FVector(0, 0, 120.f), 1.3f, 0.7f);
	}
}

#undef LOCTEXT_NAMESPACE
