#include "Enemy/EnemyAIController.h"

#include "ElVestuario.h"
#include "Core/VestuarioGameMode.h"
#include "Core/VestuarioTypes.h"
#include "Enemy/EnemyCharacter.h"
#include "Player/VestuarioCharacter.h"
#include "World/Locker.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISenseConfig_Hearing.h"

AEnemyAIController::AEnemyAIController()
{
	PrimaryActorTick.bCanEverTick = true;

	Perception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));
	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	HearingConfig->HearingRange = HearingRange;
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
	HearingConfig->SetMaxAge(1.f);
	Perception->ConfigureSense(*HearingConfig);
	Perception->SetDominantSense(HearingConfig->GetSenseImplementation());
	SetPerceptionComponent(*Perception);
}

void AEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	Perception->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &AEnemyAIController::OnTargetPerceived);

	PatrolLocations.Reset();
	if (AEnemyCharacter* Enemy = GetEnemy())
	{
		for (AActor* Point : Enemy->PatrolPoints)
		{
			if (Point)
			{
				PatrolLocations.Add(Point->GetActorLocation());
			}
		}
	}
	if (PatrolLocations.Num() == 0)
	{
		TArray<AActor*> Tagged;
		UGameplayStatics::GetAllActorsWithTag(this, TEXT("Patrol"), Tagged);
		for (AActor* Point : Tagged)
		{
			PatrolLocations.Add(Point->GetActorLocation());
		}
	}
	if (PatrolLocations.Num() == 0)
	{
		UE_LOG(LogVestuario, Warning, TEXT("El enemigo no tiene puntos de patrulla: se quedara quieto hasta oir algo."));
	}

	SetState(EEnemyState::Patrol);
	WaitTimer = 1.5f; // da tiempo a que se genere el navmesh
}

AVestuarioCharacter* AEnemyAIController::GetPlayer() const
{
	return Cast<AVestuarioCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
}

AEnemyCharacter* AEnemyAIController::GetEnemy() const
{
	return Cast<AEnemyCharacter>(GetPawn());
}

FString AEnemyAIController::GetStateName() const
{
	switch (State)
	{
	case EEnemyState::Patrol:		return TEXT("Patrulla");
	case EEnemyState::Investigate:	return TEXT("Investiga");
	case EEnemyState::Chase:		return TEXT("PERSIGUE");
	case EEnemyState::Kill:			return TEXT("Mata");
	}
	return TEXT("?");
}

void AEnemyAIController::SetState(EEnemyState NewState)
{
	const EEnemyState Old = State;
	State = NewState;

	AEnemyCharacter* Enemy = GetEnemy();
	if (!Enemy)
	{
		return;
	}
	switch (NewState)
	{
	case EEnemyState::Patrol:
		Enemy->SetMoveSpeed(Enemy->PatrolSpeed);
		bKnowsHidingSpot = false;
		break;
	case EEnemyState::Investigate:
		Enemy->SetMoveSpeed(Enemy->InvestigateSpeed);
		bKnowsHidingSpot = false;
		break;
	case EEnemyState::Chase:
		Enemy->SetMoveSpeed(Enemy->ChaseSpeed);
		if (Old != EEnemyState::Chase)
		{
			Enemy->OnChaseStarted();
		}
		break;
	case EEnemyState::Kill:
		Enemy->SetMoveSpeed(0.f);
		break;
	}
}

void AEnemyAIController::OnTargetPerceived(AActor* Actor, FAIStimulus Stimulus)
{
	if (State == EEnemyState::Kill || !Stimulus.WasSuccessfullySensed())
	{
		return;
	}
	if (Stimulus.Type != UAISense::GetSenseID<UAISense_Hearing>())
	{
		return;
	}
	APawn* Me = GetPawn();
	if (!Me)
	{
		return;
	}

	const FVector NoiseLocation = Stimulus.StimulusLocation;
	const float Distance = FVector::Dist2D(Me->GetActorLocation(), NoiseLocation);
	const float Loudness = Stimulus.Strength;

	// Si te oye meterte en una taquilla mientras te persigue, sabe donde estas
	if (Stimulus.Tag == Vestuario::LockerNoiseTag() && State == EEnemyState::Chase && Distance < 500.f)
	{
		bKnowsHidingSpot = true;
	}

	const bool bAlarm = Stimulus.Tag == Vestuario::AlarmNoiseTag();
	const bool bChase = bAlarm
		|| State == EEnemyState::Chase
		|| Distance < CloseNoiseDistance
		|| (Loudness >= 1.f && Distance < LoudNoiseDistance);

	SetState(bChase ? EEnemyState::Chase : EEnemyState::Investigate);
	TargetLocation = NoiseLocation;
	MoveToTarget(TargetLocation);
}

void AEnemyAIController::MoveToTarget(const FVector& Location)
{
	WaitTimer = 0.f;
	const EPathFollowingRequestResult::Type Result = MoveToLocation(Location, 40.f, true, true, true, false);
	if (Result == EPathFollowingRequestResult::Failed)
	{
		// Sin camino (p. ej. navmesh aun no generado): espera un poco y sigue
		WaitTimer = 1.f;
	}
}

void AEnemyAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	if (State == EEnemyState::Kill || Result.Code == EPathFollowingResult::Aborted)
	{
		return;
	}
	switch (State)
	{
	case EEnemyState::Patrol:		WaitTimer = FMath::FRandRange(1.f, 3.f); break;
	case EEnemyState::Investigate:	WaitTimer = InvestigateListenTime; break;
	case EEnemyState::Chase:		WaitTimer = ChaseListenTime; break;
	default: break;
	}
}

void AEnemyAIController::OnWaitFinished()
{
	switch (State)
	{
	case EEnemyState::Patrol:
		MoveToNextPatrolPoint();
		break;

	case EEnemyState::Investigate:
		SetState(EEnemyState::Patrol);
		MoveToNextPatrolPoint();
		break;

	case EEnemyState::Chase:
	{
		// Te ha perdido: busca por la zona antes de volver a patrullar
		SetState(EEnemyState::Investigate);
		FNavLocation Random;
		UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
		if (Nav && Nav->GetRandomReachablePointInRadius(TargetLocation, 450.f, Random))
		{
			MoveToTarget(Random.Location);
		}
		else
		{
			WaitTimer = InvestigateListenTime;
		}
		break;
	}
	default:
		break;
	}
}

void AEnemyAIController::MoveToNextPatrolPoint()
{
	if (PatrolLocations.Num() == 0)
	{
		WaitTimer = 3.f;
		return;
	}
	PatrolIndex = (PatrolIndex + 1) % PatrolLocations.Num();
	MoveToTarget(PatrolLocations[PatrolIndex]);
}

void AEnemyAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	APawn* Me = GetPawn();
	if (!Me || State == EEnemyState::Kill)
	{
		return;
	}

	AVestuarioGameMode* GM = GetWorld()->GetAuthGameMode<AVestuarioGameMode>();
	if (GM && GM->HasEscaped())
	{
		StopMovement();
		return;
	}

	if (AVestuarioCharacter* Player = GetPlayer())
	{
		if (!Player->IsDead())
		{
			const float Distance = FVector::Dist(Me->GetActorLocation(), Player->GetActorLocation());
			if (!Player->IsHiddenInLocker() && Distance < KillRadius)
			{
				KillPlayer(Player);
				return;
			}
			if (Player->IsHiddenInLocker() && bKnowsHidingSpot && State == EEnemyState::Chase && Distance < 200.f)
			{
				if (ALocker* Locker = Player->GetCurrentLocker())
				{
					Locker->ForceOpen();
				}
				KillPlayer(Player);
				return;
			}
		}
	}

	if (WaitTimer > 0.f)
	{
		WaitTimer -= DeltaSeconds;
		if (WaitTimer <= 0.f)
		{
			WaitTimer = 0.f;
			OnWaitFinished();
		}
	}
}

void AEnemyAIController::KillPlayer(AVestuarioCharacter* Player)
{
	SetState(EEnemyState::Kill);
	StopMovement();

	if (APawn* Me = GetPawn())
	{
		FVector ToPlayer = Player->GetActorLocation() - Me->GetActorLocation();
		ToPlayer.Z = 0.f;
		Me->SetActorRotation(ToPlayer.Rotation());
	}
	if (AEnemyCharacter* Enemy = GetEnemy())
	{
		Enemy->OnKill();
	}
	Player->Die(GetPawn());
}
