#include "Core/VestuarioGameMode.h"

#include "ElVestuario.h"
#include "Core/VestuarioHUD.h"
#include "Player/VestuarioCharacter.h"
#include "Twitch/TwitchChatSubsystem.h"
#include "Twitch/TwitchSettings.h"
#include "World/ClueNote.h"
#include "World/ExitDoor.h"
#include "World/FusePickup.h"

#include "Components/AudioComponent.h"
#include "Components/LightComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Perception/AISense_Hearing.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "Vestuario"

AVestuarioGameMode::AVestuarioGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = AVestuarioCharacter::StaticClass();
	HUDClass = AVestuarioHUD::StaticClass();
}

void AVestuarioGameMode::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// Antes de cualquier BeginPlay: los actores del nivel empiezan antes que el GameMode
	LoadSounds();
}

void AVestuarioGameMode::BeginPlay()
{
	Super::BeginPlay();

	StartTime = GetWorld()->GetTimeSeconds();
	NextDripTime = StartTime + 2.f;

	CacheLights();

	if (USoundBase* Ambience = GetSound(EVestuarioSound::Ambience))
	{
		AmbienceAudio = UGameplayStatics::SpawnSound2D(this, Ambience, AmbienceVolume);
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTwitchChatSubsystem* Twitch = GI->GetSubsystem<UTwitchChatSubsystem>())
		{
			ChatHandle = Twitch->OnChatCommand.AddUObject(this, &AVestuarioGameMode::HandleChatCommand);
		}
	}
}

void AVestuarioGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTwitchChatSubsystem* Twitch = GI->GetSubsystem<UTwitchChatSubsystem>())
		{
			Twitch->OnChatCommand.Remove(ChatHandle);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AVestuarioGameMode::LoadSounds()
{
	const int32 Count = static_cast<int32>(EVestuarioSound::MAX);
	Sounds.SetNum(Count);
	for (int32 i = 0; i < Count; ++i)
	{
		const FString Name = Vestuario::SoundAssetName(static_cast<EVestuarioSound>(i));
		const FString Path = FString::Printf(TEXT("/Game/Vestuario/Audio/%s.%s"), *Name, *Name);
		Sounds[i] = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Sounds[i])
		{
			UE_LOG(LogVestuario, Warning, TEXT("Falta el sonido %s (ejecuta Tools/Unreal/build_level.py)"), *Path);
		}
	}

	// Atenuacion 3D comun para todos los sonidos del mundo
	Attenuation = NewObject<USoundAttenuation>(this);
	FSoundAttenuationSettings& Settings = Attenuation->Attenuation;
	Settings.bAttenuate = true;
	Settings.bSpatialize = true;
	Settings.AttenuationShape = EAttenuationShape::Sphere;
	Settings.AttenuationShapeExtents = FVector(150.f, 0.f, 0.f);
	Settings.FalloffDistance = 2200.f;
}

void AVestuarioGameMode::CacheLights()
{
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsWithTag(this, TEXT("Flicker"), Found);
	for (AActor* Actor : Found)
	{
		if (ULightComponent* Light = Actor->FindComponentByClass<ULightComponent>())
		{
			FlickerLights.Add(Light);
			FlickerBaseIntensity.Add(Light->Intensity);
		}
	}
}

USoundBase* AVestuarioGameMode::GetSound(EVestuarioSound Sound) const
{
	const int32 Index = static_cast<int32>(Sound);
	return Sounds.IsValidIndex(Index) ? Sounds[Index].Get() : nullptr;
}

void AVestuarioGameMode::PlaySound3D(EVestuarioSound Sound, const FVector& Location, float Volume, float Pitch)
{
	if (USoundBase* Asset = GetSound(Sound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, Asset, Location, Volume, Pitch, 0.f, Attenuation);
	}
}

void AVestuarioGameMode::PlaySound2D(EVestuarioSound Sound, float Volume, float Pitch)
{
	if (USoundBase* Asset = GetSound(Sound))
	{
		UGameplayStatics::PlaySound2D(this, Asset, Volume, Pitch);
	}
}

void AVestuarioGameMode::ShowMessage(const FText& Text, float Duration)
{
	FVestuarioMessage& Msg = Messages.AddDefaulted_GetRef();
	Msg.Text = Text;
	Msg.Duration = Duration;
	Msg.ExpireTime = GetWorld()->GetTimeSeconds() + Duration;
	if (Messages.Num() > 4)
	{
		Messages.RemoveAt(0);
	}
}

float AVestuarioGameMode::GetElapsedTime() const
{
	return GetWorld()->GetTimeSeconds() - StartTime;
}

void AVestuarioGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Now = GetWorld()->GetTimeSeconds();
	Messages.RemoveAll([Now](const FVestuarioMessage& M) { return M.ExpireTime < Now; });

	UpdateFlicker(Now);
	UpdateDrips(Now);
}

void AVestuarioGameMode::UpdateFlicker(float Now)
{
	for (int32 i = 0; i < FlickerLights.Num(); ++i)
	{
		ULightComponent* Light = FlickerLights[i].Get();
		if (!Light)
		{
			continue;
		}
		const float Noise = FMath::PerlinNoise1D(Now * 2.7f + i * 13.1f);
		float Mult = 0.8f + 0.35f * Noise;
		if (FMath::FRand() < 0.02f)
		{
			Mult = 0.05f; // micro-corte
		}
		Light->SetIntensity(FlickerBaseIntensity[i] * Mult);
	}
}

void AVestuarioGameMode::UpdateDrips(float Now)
{
	if (Now < NextDripTime)
	{
		return;
	}
	NextDripTime = Now + FMath::FRandRange(DripInterval.X, DripInterval.Y);
	if (APawn* Player = GetPlayerPawn())
	{
		const FVector Offset(FMath::FRandRange(-700.f, 700.f), FMath::FRandRange(-500.f, 500.f), FMath::FRandRange(0.f, 150.f));
		PlaySound3D(EVestuarioSound::Drip, Player->GetActorLocation() + Offset, FMath::FRandRange(0.3f, 0.7f), FMath::FRandRange(0.85f, 1.2f));
	}
}

APawn* AVestuarioGameMode::GetPlayerPawn() const
{
	return UGameplayStatics::GetPlayerPawn(this, 0);
}

// ---------------------------------------------------------------------------
// Estado de la partida
// ---------------------------------------------------------------------------

void AVestuarioGameMode::OnPlayerDied()
{
	if (IsGameOver())
	{
		return;
	}
	bPlayerDead = true;
	EndTime = GetElapsedTime();
	if (IsValid(AmbienceAudio))
	{
		AmbienceAudio->FadeOut(1.5f, 0.f);
	}
	GetWorldTimerManager().SetTimer(RestartTimer, this, &AVestuarioGameMode::RestartMap, 5.f, false);
}

void AVestuarioGameMode::OnPlayerEscaped()
{
	if (IsGameOver())
	{
		return;
	}
	bEscaped = true;
	EndTime = GetElapsedTime();
	if (APawn* Player = GetPlayerPawn())
	{
		if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
		{
			Player->DisableInput(PC);
		}
	}
	if (IsValid(AmbienceAudio))
	{
		AmbienceAudio->FadeOut(3.f, 0.f);
	}
	GetWorldTimerManager().SetTimer(RestartTimer, this, &AVestuarioGameMode::RestartMap, 10.f, false);
}

void AVestuarioGameMode::OnPowerRestored()
{
	if (bPowerOn)
	{
		return;
	}
	bPowerOn = true;

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsWithTag(this, TEXT("PowerLight"), Found);
	for (AActor* Actor : Found)
	{
		if (ULightComponent* Light = Actor->FindComponentByClass<ULightComponent>())
		{
			Light->SetVisibility(true);
		}
	}
	Found.Reset();
	UGameplayStatics::GetAllActorsWithTag(this, TEXT("Emergency"), Found);
	for (AActor* Actor : Found)
	{
		if (ULightComponent* Light = Actor->FindComponentByClass<ULightComponent>())
		{
			Light->SetVisibility(false);
		}
	}
	FlickerLights.Reset();
	FlickerBaseIntensity.Reset();

	for (TActorIterator<AExitDoor> It(GetWorld()); It; ++It)
	{
		It->Open();
	}

	PlaySound2D(EVestuarioSound::PowerOn, 1.f);

	// El zumbido de la corriente despierta al enemigo: va directo a por el jugador
	if (APawn* Player = GetPlayerPawn())
	{
		UAISense_Hearing::ReportNoiseEvent(this, Player->GetActorLocation(), 3.f, Player, 0.f, Vestuario::AlarmNoiseTag());
	}

	ShowMessage(LOCTEXT("PowerOn", "\u00A1Ha vuelto la luz! La puerta de salida se ha abierto... \u00A1CORRE!"), 6.f);
}

void AVestuarioGameMode::RestartMap()
{
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}

// ---------------------------------------------------------------------------
// Twitch
// ---------------------------------------------------------------------------

void AVestuarioGameMode::HandleChatCommandInternal(const FString& User, const FString& Command, bool bDebug)
{
	if (IsGameOver())
	{
		return;
	}

	const FString Cmd = Command.ToLower();
	const bool bNoise = Cmd == TEXT("!ruido") || Cmd == TEXT("!noise");
	const bool bHint = Cmd == TEXT("!pista") || Cmd == TEXT("!hint");
	if (!bNoise && !bHint)
	{
		return;
	}

	if (!bDebug)
	{
		const UTwitchSettings* Settings = GetDefault<UTwitchSettings>();
		UTwitchChatSubsystem* Twitch = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTwitchChatSubsystem>() : nullptr;
		if (!Twitch)
		{
			return;
		}
		const FName Key = bNoise ? FName(TEXT("Noise")) : FName(TEXT("Hint"));
		const float Cooldown = bNoise ? Settings->NoiseCooldown : Settings->HintCooldown;
		if (!Twitch->IsCooldownReady(Key, Cooldown) || !Twitch->IsUserReady(User, Settings->PerUserCooldown))
		{
			return;
		}
		Twitch->MarkUsed(Key, User);
	}

	if (bNoise)
	{
		DoChatNoise(bDebug ? FString() : User);
	}
	else
	{
		DoChatHint(bDebug ? FString() : User);
	}
}

void AVestuarioGameMode::DoChatNoise(const FString& User)
{
	APawn* Player = GetPlayerPawn();
	if (!Player)
	{
		return;
	}

	// Busca un punto navegable a 3,5-6 m del jugador
	const FVector Origin = Player->GetActorLocation();
	FVector NoiseLocation = Origin;
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	for (int32 Try = 0; Try < 10; ++Try)
	{
		const FVector Dir = FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f).GetSafeNormal();
		const FVector Candidate = Origin + Dir * FMath::FRandRange(350.f, 600.f);
		NoiseLocation = Candidate;
		FNavLocation NavLocation;
		if (Nav && Nav->ProjectPointToNavigation(Candidate, NavLocation, FVector(150.f, 150.f, 300.f)))
		{
			NoiseLocation = NavLocation.Location;
			break;
		}
	}

	PlaySound3D(EVestuarioSound::Knock, NoiseLocation + FVector(0.f, 0.f, 80.f), 1.4f, FMath::FRandRange(0.8f, 1.1f));
	UAISense_Hearing::ReportNoiseEvent(this, NoiseLocation, 1.5f, Player, 0.f, Vestuario::ChatNoiseTag());

	const bool bShowName = GetDefault<UTwitchSettings>()->bShowUserNames && !User.IsEmpty();
	ShowMessage(bShowName
		? FText::Format(LOCTEXT("ChatNoiseUser", "{0} ha hecho ruido... algo se acerca."), FText::FromString(User))
		: LOCTEXT("ChatNoise", "El chat ha hecho ruido... algo se acerca."), 4.f);
}

void AVestuarioGameMode::DoChatHint(const FString& User)
{
	for (TActorIterator<AClueNote> It(GetWorld()); It; ++It)
	{
		It->Flash(8.f);
	}
	for (TActorIterator<AFusePickup> It(GetWorld()); It; ++It)
	{
		It->Highlight(8.f);
	}
	PlaySound2D(EVestuarioSound::Whisper, 0.8f);

	const bool bShowName = GetDefault<UTwitchSettings>()->bShowUserNames && !User.IsEmpty();
	ShowMessage(bShowName
		? FText::Format(LOCTEXT("ChatHintUser", "{0} te susurra: \"busca lo que brilla\"."), FText::FromString(User))
		: LOCTEXT("ChatHint", "Una voz del chat te susurra: \"busca lo que brilla\"."), 5.f);
}

#undef LOCTEXT_NAMESPACE
