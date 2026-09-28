#include "Player/VestuarioCharacter.h"

#include "ElVestuario.h"
#include "Core/VestuarioGameMode.h"
#include "Enemy/EnemyAIController.h"
#include "Enemy/EnemyCharacter.h"
#include "Interaction/Interactable.h"
#include "Twitch/TwitchChatSubsystem.h"
#include "World/Locker.h"

#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Perception/AISense_Hearing.h"

#define LOCTEXT_NAMESPACE "Vestuario"

AVestuarioCharacter::AVestuarioCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);
	BaseEyeHeight = CameraBaseZ;
	CrouchedEyeHeight = 32.f;
	bUseControllerRotationYaw = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.f, 0.f, CameraBaseZ));
	Camera->bUsePawnControlRotation = true;
	Camera->SetFieldOfView(90.f);

	Flashlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Flashlight"));
	Flashlight->SetupAttachment(Camera);
	Flashlight->SetRelativeLocation(FVector(10.f, 14.f, -14.f));
	Flashlight->IntensityUnits = ELightUnits::Lumens;
	Flashlight->Intensity = 900.f;
	Flashlight->AttenuationRadius = 2200.f;
	Flashlight->InnerConeAngle = 12.f;
	Flashlight->OuterConeAngle = 30.f;
	Flashlight->SetLightColor(FLinearColor(1.f, 0.93f, 0.82f));
	Flashlight->SetCastShadows(true);

	HeartbeatAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("HeartbeatAudio"));
	HeartbeatAudio->SetupAttachment(GetCapsuleComponent());
	HeartbeatAudio->bAutoActivate = false;
	HeartbeatAudio->bIsUISound = true;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = WalkSpeed;
	Move->MaxWalkSpeedCrouched = CrouchSpeed;
	Move->NavAgentProps.bCanCrouch = true;
	Move->bCanWalkOffLedgesWhenCrouching = true;
	Move->BrakingDecelerationWalking = 1500.f;
	Move->SetCrouchedHalfHeight(55.f);
}

void AVestuarioCharacter::BeginPlay()
{
	Super::BeginPlay();

	FlashlightBaseIntensity = Flashlight->Intensity;
	if (AVestuarioGameMode* GM = GetVestuarioGameMode())
	{
		HeartbeatAudio->SetSound(GM->GetSound(EVestuarioSound::Heartbeat));
	}
}

AVestuarioGameMode* AVestuarioCharacter::GetVestuarioGameMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<AVestuarioGameMode>() : nullptr;
}

// ---------------------------------------------------------------------------
// Input (Enhanced Input creado por codigo)
// ---------------------------------------------------------------------------

void AVestuarioCharacter::EnsureInputObjects()
{
	if (MappingContext)
	{
		return;
	}

	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Vestuario"));

	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(this, Name);
		Action->ValueType = Type;
		return Action;
	};

	MoveAction = MakeAction(TEXT("IA_Move"), EInputActionValueType::Axis2D);
	LookAction = MakeAction(TEXT("IA_Look"), EInputActionValueType::Axis2D);
	SprintAction = MakeAction(TEXT("IA_Sprint"), EInputActionValueType::Boolean);
	CrouchAction = MakeAction(TEXT("IA_Crouch"), EInputActionValueType::Boolean);
	InteractAction = MakeAction(TEXT("IA_Interact"), EInputActionValueType::Boolean);
	FlashlightAction = MakeAction(TEXT("IA_Flashlight"), EInputActionValueType::Boolean);
	CycleFuseAction = MakeAction(TEXT("IA_CycleFuse"), EInputActionValueType::Boolean);
	QuitAction = MakeAction(TEXT("IA_Quit"), EInputActionValueType::Boolean);
	DebugNoiseAction = MakeAction(TEXT("IA_DebugNoise"), EInputActionValueType::Boolean);
	DebugHintAction = MakeAction(TEXT("IA_DebugHint"), EInputActionValueType::Boolean);
	DebugAIAction = MakeAction(TEXT("IA_DebugAI"), EInputActionValueType::Boolean);

	// WASD -> eje 2D (X = derecha, Y = adelante)
	{
		FEnhancedActionKeyMapping& W = MappingContext->MapKey(MoveAction, EKeys::W);
		UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(this);
		Swizzle->Order = EInputAxisSwizzle::YXZ;
		W.Modifiers.Add(Swizzle);
	}
	{
		FEnhancedActionKeyMapping& S = MappingContext->MapKey(MoveAction, EKeys::S);
		UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(this);
		Swizzle->Order = EInputAxisSwizzle::YXZ;
		S.Modifiers.Add(Swizzle);
		S.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	}
	{
		FEnhancedActionKeyMapping& A = MappingContext->MapKey(MoveAction, EKeys::A);
		A.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	}
	MappingContext->MapKey(MoveAction, EKeys::D);
	MappingContext->MapKey(MoveAction, EKeys::Gamepad_Left2D);

	MappingContext->MapKey(LookAction, EKeys::Mouse2D);
	MappingContext->MapKey(SprintAction, EKeys::LeftShift);
	MappingContext->MapKey(CrouchAction, EKeys::LeftControl);
	MappingContext->MapKey(CrouchAction, EKeys::C);
	MappingContext->MapKey(InteractAction, EKeys::E);
	MappingContext->MapKey(InteractAction, EKeys::LeftMouseButton);
	MappingContext->MapKey(FlashlightAction, EKeys::F);
	MappingContext->MapKey(CycleFuseAction, EKeys::Q);
	MappingContext->MapKey(CycleFuseAction, EKeys::MouseScrollUp);
	MappingContext->MapKey(QuitAction, EKeys::Escape);
	MappingContext->MapKey(DebugNoiseAction, EKeys::One);
	MappingContext->MapKey(DebugHintAction, EKeys::Two);
	MappingContext->MapKey(DebugAIAction, EKeys::Three);
}

void AVestuarioCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	EnsureInputObjects();
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				Subsystem->ClearAllMappings();
				Subsystem->AddMappingContext(MappingContext, 0);
			}
		}
	}
}

void AVestuarioCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	EnsureInputObjects();

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		UE_LOG(LogVestuario, Error, TEXT("Enhanced Input no esta activo. Revisa Config/DefaultInput.ini"));
		return;
	}

	Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AVestuarioCharacter::Move);
	Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &AVestuarioCharacter::Look);
	Input->BindAction(SprintAction, ETriggerEvent::Started, this, &AVestuarioCharacter::StartSprint);
	Input->BindAction(SprintAction, ETriggerEvent::Completed, this, &AVestuarioCharacter::StopSprint);
	Input->BindAction(CrouchAction, ETriggerEvent::Started, this, &AVestuarioCharacter::ToggleCrouch);
	Input->BindAction(InteractAction, ETriggerEvent::Started, this, &AVestuarioCharacter::Interact);
	Input->BindAction(FlashlightAction, ETriggerEvent::Started, this, &AVestuarioCharacter::ToggleFlashlight);
	Input->BindAction(CycleFuseAction, ETriggerEvent::Started, this, &AVestuarioCharacter::CycleFuse);
	Input->BindAction(QuitAction, ETriggerEvent::Started, this, &AVestuarioCharacter::QuitGame);
	Input->BindAction(DebugNoiseAction, ETriggerEvent::Started, this, &AVestuarioCharacter::DebugNoise);
	Input->BindAction(DebugHintAction, ETriggerEvent::Started, this, &AVestuarioCharacter::DebugHint);
	Input->BindAction(DebugAIAction, ETriggerEvent::Started, this, &AVestuarioCharacter::DebugAI);
}

void AVestuarioCharacter::Move(const FInputActionValue& Value)
{
	if (bDead || CurrentLocker)
	{
		return;
	}
	const FVector2D Axis = Value.Get<FVector2D>();
	AddMovementInput(GetActorForwardVector(), Axis.Y);
	AddMovementInput(GetActorRightVector(), Axis.X);
}

void AVestuarioCharacter::Look(const FInputActionValue& Value)
{
	if (bDead)
	{
		return;
	}
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X * MouseSensitivity);
	AddControllerPitchInput((bInvertMouseY ? -Axis.Y : Axis.Y) * MouseSensitivity);
}

void AVestuarioCharacter::StartSprint()
{
	bWantsSprint = true;
	if (bIsCrouched)
	{
		UnCrouch();
	}
}

void AVestuarioCharacter::StopSprint()
{
	bWantsSprint = false;
}

void AVestuarioCharacter::ToggleCrouch()
{
	if (CurrentLocker || bDead)
	{
		return;
	}
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		bWantsSprint = false;
		Crouch();
	}
}

void AVestuarioCharacter::Interact()
{
	if (bDead)
	{
		return;
	}
	if (IInteractable* Target = Cast<IInteractable>(FocusedActor.Get()))
	{
		Target->Interact(this, FocusedComponent.Get());
	}
}

void AVestuarioCharacter::ToggleFlashlight()
{
	bFlashlightOn = !bFlashlightOn;
	Flashlight->SetVisibility(bFlashlightOn);
	if (AVestuarioGameMode* GM = GetVestuarioGameMode())
	{
		GM->PlaySound2D(EVestuarioSound::FuseClick, 0.25f, 1.6f);
	}
}

void AVestuarioCharacter::CycleFuse()
{
	if (HeldFuses.Num() > 1)
	{
		SelectedFuse = (SelectedFuse + 1) % HeldFuses.Num();
	}
}

void AVestuarioCharacter::QuitGame()
{
	UKismetSystemLibrary::QuitGame(this, Cast<APlayerController>(GetController()), EQuitPreference::Quit, false);
}

void AVestuarioCharacter::DebugNoise()
{
	SimularChat(TEXT("!ruido"));
}

void AVestuarioCharacter::DebugHint()
{
	SimularChat(TEXT("!pista"));
}

void AVestuarioCharacter::DebugAI()
{
	if (AVestuarioGameMode* GM = GetVestuarioGameMode())
	{
		GM->ToggleDebugAI();
	}
}

void AVestuarioCharacter::SimularChat(const FString& Comando)
{
	if (AVestuarioGameMode* GM = GetVestuarioGameMode())
	{
		GM->HandleDebugCommand(Comando);
	}
}

void AVestuarioCharacter::TwitchConectar(const FString& Canal)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTwitchChatSubsystem* Twitch = GI->GetSubsystem<UTwitchChatSubsystem>())
		{
			Twitch->Connect(Canal);
		}
	}
}

// ---------------------------------------------------------------------------
// Fusibles
// ---------------------------------------------------------------------------

void AVestuarioCharacter::AddFuse(EFuseColor Color)
{
	HeldFuses.Add(Color);
	SelectedFuse = HeldFuses.Num() - 1;
}

bool AVestuarioCharacter::GetSelectedFuse(EFuseColor& OutColor) const
{
	if (!HeldFuses.IsValidIndex(SelectedFuse))
	{
		return false;
	}
	OutColor = HeldFuses[SelectedFuse];
	return true;
}

void AVestuarioCharacter::ConsumeSelectedFuse()
{
	if (HeldFuses.IsValidIndex(SelectedFuse))
	{
		HeldFuses.RemoveAt(SelectedFuse);
		SelectedFuse = HeldFuses.Num() > 0 ? FMath::Min(SelectedFuse, HeldFuses.Num() - 1) : 0;
	}
}

// ---------------------------------------------------------------------------
// Taquillas
// ---------------------------------------------------------------------------

void AVestuarioCharacter::EnterLocker(ALocker* Locker)
{
	if (!Locker || CurrentLocker || bDead)
	{
		return;
	}
	if (bIsCrouched)
	{
		UnCrouch();
	}
	bWantsSprint = false;
	CurrentLocker = Locker;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->StopMovementImmediately();
	Move->DisableMovement();
	SetActorEnableCollision(false);
	SetActorLocation(Locker->GetHideLocation());
	if (AController* C = GetController())
	{
		C->SetControlRotation(Locker->GetHideRotation());
	}
}

void AVestuarioCharacter::ExitLocker()
{
	if (!CurrentLocker)
	{
		return;
	}
	SetActorLocation(CurrentLocker->GetExitLocation());
	SetActorEnableCollision(true);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	CurrentLocker = nullptr;
}

// ---------------------------------------------------------------------------
// Muerte
// ---------------------------------------------------------------------------

void AVestuarioCharacter::Die(AActor* InKiller)
{
	if (bDead)
	{
		return;
	}
	bDead = true;
	Killer = InKiller;
	FocusText = FText::GetEmpty();

	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		DisableInput(PC);
	}
	HeartbeatAudio->Stop();

	if (AVestuarioGameMode* GM = GetVestuarioGameMode())
	{
		GM->PlaySound2D(EVestuarioSound::Jumpscare, 1.f);
		GM->OnPlayerDied();
	}
}

// ---------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------

void AVestuarioCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bDead)
	{
		UpdateDeathCamera(DeltaSeconds);
		return;
	}

	UpdateSpeed();
	UpdateFootsteps(DeltaSeconds);
	UpdateFocus();
	UpdateFear(DeltaSeconds);
	UpdateCamera(DeltaSeconds);
}

bool AVestuarioCharacter::IsSprinting() const
{
	return bWantsSprint && !bIsCrouched && GetVelocity().Size2D() > WalkSpeed * 1.05f;
}

void AVestuarioCharacter::UpdateSpeed()
{
	GetCharacterMovement()->MaxWalkSpeed = (bWantsSprint && !bIsCrouched) ? SprintSpeed : WalkSpeed;
}

void AVestuarioCharacter::UpdateFootsteps(float DeltaSeconds)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	const float Speed = GetVelocity().Size2D();
	if (CurrentLocker || !Move->IsMovingOnGround() || Speed < 20.f)
	{
		return;
	}

	const bool bSprint = IsSprinting();
	const float StepLength = bIsCrouched ? 55.f : (bSprint ? 95.f : 70.f);
	StepDistance += Speed * DeltaSeconds;
	if (StepDistance < StepLength)
	{
		return;
	}
	StepDistance = 0.f;

	const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const float Loudness = bIsCrouched ? CrouchLoudness : (bSprint ? SprintLoudness : WalkLoudness);
	const float Volume = bIsCrouched ? 0.2f : (bSprint ? 1.f : 0.5f);

	if (AVestuarioGameMode* GM = GetVestuarioGameMode())
	{
		GM->PlaySound3D(EVestuarioSound::Footstep, Feet, Volume, FMath::FRandRange(0.9f, 1.1f));
	}
	if (Loudness > 0.f)
	{
		UAISense_Hearing::ReportNoiseEvent(this, Feet, Loudness, this);
	}
}

void AVestuarioCharacter::UpdateFocus()
{
	FocusedActor = nullptr;
	FocusedComponent = nullptr;
	FocusText = FText::GetEmpty();

	if (CurrentLocker)
	{
		FocusedActor = CurrentLocker.Get();
		FocusText = CurrentLocker->GetInteractText(this, nullptr);
		return;
	}

	const FVector Start = Camera->GetComponentLocation();
	const FVector End = Start + Camera->GetForwardVector() * InteractDistance;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VestuarioInteract), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		if (IInteractable* Target = Cast<IInteractable>(Hit.GetActor()))
		{
			FocusedActor = Hit.GetActor();
			FocusedComponent = Hit.GetComponent();
			FocusText = Target->GetInteractText(this, Hit.GetComponent());
		}
	}
}

void AVestuarioCharacter::UpdateFear(float DeltaSeconds)
{
	if (!CachedEnemy.IsValid())
	{
		for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
		{
			CachedEnemy = *It;
			break;
		}
	}

	float Target = 0.f;
	if (AEnemyCharacter* Enemy = CachedEnemy.Get())
	{
		const float Dist = FVector::Dist(Enemy->GetActorLocation(), GetActorLocation());
		Target = FMath::Clamp(1.f - (Dist - 250.f) / 1300.f, 0.f, 1.f);
		const AEnemyAIController* AI = Cast<AEnemyAIController>(Enemy->GetController());
		if (!AI || AI->GetState() != EEnemyState::Chase)
		{
			Target *= 0.7f;
		}
	}
	Fear = FMath::FInterpTo(Fear, Target, DeltaSeconds, 2.5f);

	if (Fear > 0.05f)
	{
		if (!HeartbeatAudio->IsPlaying())
		{
			HeartbeatAudio->Play();
		}
		HeartbeatAudio->SetVolumeMultiplier(FMath::Clamp(Fear * 1.2f, 0.f, 1.f));
		HeartbeatAudio->SetPitchMultiplier(0.9f + Fear * 0.45f);
	}
	else if (HeartbeatAudio->IsPlaying())
	{
		HeartbeatAudio->Stop();
	}

	// La linterna falla cuando el enemigo esta muy cerca
	if (bFlashlightOn)
	{
		const bool bGlitch = Fear > 0.6f && FMath::FRand() < (Fear - 0.6f) * 0.5f;
		Flashlight->SetIntensity(bGlitch ? FlashlightBaseIntensity * 0.15f : FlashlightBaseIntensity);
	}
}

void AVestuarioCharacter::UpdateCamera(float DeltaSeconds)
{
	// Balanceo de la cabeza al andar
	const float Speed = GetVelocity().Size2D();
	float Amplitude = 0.f;
	if (!CurrentLocker && GetCharacterMovement()->IsMovingOnGround() && Speed > 20.f)
	{
		BobTime += DeltaSeconds * Speed / 70.f * PI;
		Amplitude = IsSprinting() ? 3.5f : (bIsCrouched ? 1.f : 1.8f);
	}
	const FVector Target(0.f, FMath::Sin(BobTime * 0.5f) * Amplitude * 0.6f, CameraBaseZ + FMath::Sin(BobTime) * Amplitude);
	Camera->SetRelativeLocation(FMath::VInterpTo(Camera->GetRelativeLocation(), Target, DeltaSeconds, 12.f));
}

void AVestuarioCharacter::UpdateDeathCamera(float DeltaSeconds)
{
	AController* C = GetController();
	AActor* K = Killer.Get();
	if (!C || !K)
	{
		return;
	}
	const FVector Face = K->GetActorLocation() + FVector(0.f, 0.f, 95.f);
	const FRotator LookAt = (Face - Camera->GetComponentLocation()).Rotation();
	C->SetControlRotation(FMath::RInterpTo(C->GetControlRotation(), LookAt, DeltaSeconds, 14.f));

	const FVector Shake(0.f, FMath::FRandRange(-2.f, 2.f), CameraBaseZ + FMath::FRandRange(-2.f, 2.f));
	Camera->SetRelativeLocation(Shake);
}

#undef LOCTEXT_NAMESPACE
