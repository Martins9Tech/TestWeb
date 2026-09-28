#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "EnemyAIController.generated.h"

class AEnemyCharacter;
class AVestuarioCharacter;
class UAIPerceptionComponent;
class UAISenseConfig_Hearing;

UENUM(BlueprintType)
enum class EEnemyState : uint8
{
	Patrol,			// camina entre sus puntos de patrulla
	Investigate,	// ha oido algo lejos: va despacio a mirar
	Chase,			// ha oido algo cerca o muy fuerte: corre hacia alli
	Kill			// te ha encontrado
};

/**
 * Cerebro del enemigo: una maquina de 4 estados en C++ (mas facil de leer que un Behavior Tree
 * para empezar). Solo tiene el sentido del OIDO (AI Perception > Hearing).
 */
UCLASS()
class ELVESTUARIO_API AEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	AEnemyAIController();

	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

	EEnemyState GetState() const { return State; }
	FString GetStateName() const;

protected:
	UPROPERTY(VisibleAnywhere, Category = "IA")
	TObjectPtr<UAIPerceptionComponent> Perception;

	UPROPERTY(VisibleAnywhere, Category = "IA")
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

	/** Radio de oido base (cm). La "loudness" de cada ruido lo multiplica. */
	UPROPERTY(EditDefaultsOnly, Category = "IA")
	float HearingRange = 3000.f;

	/** Si te toca (distancia entre centros), mueres. Es ciego, pero no sordo al tacto. */
	UPROPERTY(EditDefaultsOnly, Category = "IA")
	float KillRadius = 115.f;

	/** Ruidos mas cerca que esto -> persecucion. */
	UPROPERTY(EditDefaultsOnly, Category = "IA")
	float CloseNoiseDistance = 550.f;

	/** Ruidos fuertes (loudness >= 1) mas cerca que esto -> persecucion. */
	UPROPERTY(EditDefaultsOnly, Category = "IA")
	float LoudNoiseDistance = 1600.f;

	UPROPERTY(EditDefaultsOnly, Category = "IA")
	float InvestigateListenTime = 4.f;

	UPROPERTY(EditDefaultsOnly, Category = "IA")
	float ChaseListenTime = 2.5f;

private:
	UFUNCTION()
	void OnTargetPerceived(AActor* Actor, FAIStimulus Stimulus);

	void SetState(EEnemyState NewState);
	void MoveToTarget(const FVector& Location);
	void OnWaitFinished();
	void MoveToNextPatrolPoint();
	void KillPlayer(AVestuarioCharacter* Player);
	AVestuarioCharacter* GetPlayer() const;
	AEnemyCharacter* GetEnemy() const;

	TArray<FVector> PatrolLocations;
	FVector TargetLocation = FVector::ZeroVector;
	EEnemyState State = EEnemyState::Patrol;
	int32 PatrolIndex = 0;
	float WaitTimer = 0.f;
	bool bKnowsHidingSpot = false;
};
