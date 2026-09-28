#include "Enemy/EnemyCharacter.h"

#include "Core/VestuarioGameMode.h"
#include "Core/VestuarioTypes.h"
#include "Enemy/EnemyAIController.h"

#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace EnemyConst
{
	constexpr float CapsuleHalfHeight = 115.f;
	// Debe coincidir con ENEMY_HEAD_ATTACH en Tools/Art/gen_meshes.py
	const FVector HeadAttachOffset(30.f, 0.f, 195.f);
}

AEnemyCharacter::AEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(38.f, EnemyConst::CapsuleHalfHeight);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(GetCapsuleComponent());
	BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, -EnemyConst::CapsuleHalfHeight));
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
	HeadMesh->SetupAttachment(BodyMesh);
	HeadMesh->SetRelativeLocation(EnemyConst::HeadAttachOffset);
	HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	BreathAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("BreathAudio"));
	BreathAudio->SetupAttachment(HeadMesh);
	BreathAudio->bAutoActivate = false;
	BreathAudio->bOverrideAttenuation = true;
	BreathAudio->AttenuationOverrides.bAttenuate = true;
	BreathAudio->AttenuationOverrides.bSpatialize = true;
	BreathAudio->AttenuationOverrides.AttenuationShape = EAttenuationShape::Sphere;
	BreathAudio->AttenuationOverrides.AttenuationShapeExtents = FVector(120.f, 0.f, 0.f);
	BreathAudio->AttenuationOverrides.FalloffDistance = 1100.f;

	AIControllerClass = AEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationYaw = false;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 220.f, 0.f);
	Move->MaxWalkSpeed = PatrolSpeed;
	Move->bUseRVOAvoidance = false;
}

void AEnemyCharacter::ApplyMeshes()
{
	if (!BodyMesh->GetStaticMesh())
	{
		BodyMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *Vestuario::MeshPath(TEXT("SM_Enemy")), nullptr, LOAD_NoWarn));
	}
	if (!HeadMesh->GetStaticMesh())
	{
		HeadMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *Vestuario::MeshPath(TEXT("SM_EnemyHead")), nullptr, LOAD_NoWarn));
	}
}

void AEnemyCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyMeshes();
}

void AEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplyMeshes();
	NextTwitchTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(1.f, 3.f);

	if (AVestuarioGameMode* GM = GetWorld()->GetAuthGameMode<AVestuarioGameMode>())
	{
		BreathAudio->SetSound(GM->GetSound(EVestuarioSound::EnemyBreath));
		BreathAudio->Play(FMath::FRandRange(0.f, 2.f));
	}
}

void AEnemyCharacter::SetMoveSpeed(float Speed)
{
	GetCharacterMovement()->MaxWalkSpeed = Speed;
}

void AEnemyCharacter::OnChaseStarted()
{
	if (AVestuarioGameMode* GM = GetWorld()->GetAuthGameMode<AVestuarioGameMode>())
	{
		GM->PlaySound3D(EVestuarioSound::EnemyScream, HeadMesh->GetComponentLocation(), 1.3f, FMath::FRandRange(0.9f, 1.05f));
	}
	TwitchRotation = FRotator(-25.f, 0.f, 20.f);
	TwitchEndTime = GetWorld()->GetTimeSeconds() + 0.6f;
}

void AEnemyCharacter::OnKill()
{
	bKilling = true;
	BreathAudio->Stop();
}

void AEnemyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateProceduralAnimation(DeltaSeconds);
	UpdateFootsteps(DeltaSeconds);
}

void AEnemyCharacter::UpdateProceduralAnimation(float DeltaSeconds)
{
	AnimTime += DeltaSeconds;
	const float Now = GetWorld()->GetTimeSeconds();
	const float SpeedRatio = FMath::Clamp(GetVelocity().Size2D() / ChaseSpeed, 0.f, 1.f);

	// Cuerpo: inclinado hacia delante, balanceo lateral y rebote al andar
	const float StepFreq = 1.4f + SpeedRatio * 6.f;
	const float Lean = 6.f + SpeedRatio * 14.f;
	const float Sway = FMath::Sin(AnimTime * StepFreq) * (2.5f + SpeedRatio * 4.f);
	const float Bob = FMath::Abs(FMath::Sin(AnimTime * StepFreq)) * (1.5f + SpeedRatio * 6.f);
	BodyMesh->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -EnemyConst::CapsuleHalfHeight + Bob), FRotator(-Lean, 0.f, Sway));

	// Cabeza: tics bruscos cada pocos segundos (escuchando)
	if (bKilling)
	{
		TwitchRotation = FRotator(FMath::FRandRange(-15.f, 15.f), FMath::FRandRange(-20.f, 20.f), FMath::FRandRange(-40.f, 40.f));
		TwitchEndTime = Now + 0.05f;
	}
	else if (Now > NextTwitchTime)
	{
		TwitchRotation = FRotator(FMath::FRandRange(-20.f, 25.f), FMath::FRandRange(-45.f, 45.f), FMath::FRandRange(-40.f, 40.f));
		TwitchEndTime = Now + FMath::FRandRange(0.3f, 1.2f);
		NextTwitchTime = Now + FMath::FRandRange(1.5f, 4.5f);
	}

	const FRotator Idle(FMath::Sin(AnimTime * 0.7f) * 4.f, FMath::Sin(AnimTime * 0.45f) * 8.f, 0.f);
	const FRotator Target = Now < TwitchEndTime ? TwitchRotation : Idle;
	const float InterpSpeed = Now < TwitchEndTime ? 22.f : 3.f;
	CurrentHeadRotation = FMath::RInterpTo(CurrentHeadRotation, Target, DeltaSeconds, InterpSpeed);
	HeadMesh->SetRelativeRotation(CurrentHeadRotation);
}

void AEnemyCharacter::UpdateFootsteps(float DeltaSeconds)
{
	const float Speed = GetVelocity().Size2D();
	if (Speed < 20.f || !GetCharacterMovement()->IsMovingOnGround())
	{
		return;
	}
	StepDistance += Speed * DeltaSeconds;
	if (StepDistance < 115.f)
	{
		return;
	}
	StepDistance = 0.f;
	if (AVestuarioGameMode* GM = GetWorld()->GetAuthGameMode<AVestuarioGameMode>())
	{
		const float SpeedRatio = FMath::Clamp(Speed / ChaseSpeed, 0.f, 1.f);
		const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, EnemyConst::CapsuleHalfHeight);
		GM->PlaySound3D(EVestuarioSound::EnemyStep, Feet, 0.6f + 0.7f * SpeedRatio, FMath::FRandRange(0.85f, 1.05f));
	}
}
