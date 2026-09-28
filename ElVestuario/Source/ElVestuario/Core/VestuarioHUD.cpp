#include "Core/VestuarioHUD.h"

#include "Core/VestuarioGameMode.h"
#include "Enemy/EnemyAIController.h"
#include "Enemy/EnemyCharacter.h"
#include "Player/VestuarioCharacter.h"
#include "Twitch/TwitchChatSubsystem.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"

#define LOCTEXT_NAMESPACE "Vestuario"

void AVestuarioHUD::DrawCentered(const FString& Text, float Y, UFont* Font, float Scale, const FLinearColor& Color)
{
	float W = 0.f, H = 0.f;
	GetTextSize(Text, W, H, Font, Scale);
	DrawText(Text, FLinearColor(0.f, 0.f, 0.f, Color.A * 0.8f), (Canvas->ClipX - W) * 0.5f + 2.f, Y + 2.f, Font, Scale);
	DrawText(Text, Color, (Canvas->ClipX - W) * 0.5f, Y, Font, Scale);
}

void AVestuarioHUD::DrawRight(const FString& Text, float Y, UFont* Font, float Scale, const FLinearColor& Color)
{
	float W = 0.f, H = 0.f;
	GetTextSize(Text, W, H, Font, Scale);
	DrawText(Text, Color, Canvas->ClipX - W - 24.f, Y, Font, Scale);
}

void AVestuarioHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !GEngine)
	{
		return;
	}

	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float S = H / 1080.f;
	UFont* Large = GEngine->GetLargeFont();
	UFont* Medium = GEngine->GetMediumFont();
	UFont* Small = GEngine->GetSmallFont();

	AVestuarioCharacter* Player = Cast<AVestuarioCharacter>(GetOwningPawn());
	AVestuarioGameMode* GM = GetWorld()->GetAuthGameMode<AVestuarioGameMode>();
	if (!GM)
	{
		return;
	}
	const float Elapsed = GM->GetElapsedTime();

	// --- Miedo: tinte rojo cuando el enemigo esta cerca ---
	if (Player && !Player->IsDead())
	{
		const float Fear = Player->GetFear();
		if (Fear > 0.01f)
		{
			DrawRect(FLinearColor(0.35f, 0.f, 0.f, Fear * 0.22f), 0.f, 0.f, W, H);
		}
	}

	// --- Escondido en la taquilla ---
	if (Player && Player->IsHiddenInLocker() && !Player->IsDead())
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.35f), 0.f, 0.f, W, H);
		DrawCentered(LOCTEXT("Hidden", "Escondido. Aguanta la respiraci\u00F3n...").ToString(), H * 0.12f, Medium, 1.4f * S, FLinearColor(0.8f, 0.8f, 0.8f, 0.9f));
	}

	// --- Punto de mira y texto de interaccion ---
	if (Player && !Player->IsDead() && !GM->HasEscaped())
	{
		const float Dot = 4.f * S;
		const bool bFocus = !Player->GetFocusText().IsEmpty();
		DrawRect(FLinearColor(1.f, 1.f, 1.f, bFocus ? 0.9f : 0.35f), (W - Dot) * 0.5f, (H - Dot) * 0.5f, Dot, Dot);
		if (bFocus)
		{
			DrawCentered(Player->GetFocusText().ToString(), H * 0.5f + 28.f * S, Medium, 1.1f * S, FLinearColor(0.95f, 0.92f, 0.85f, 1.f));
		}

		// Inventario de fusibles
		const TArray<EFuseColor>& Fuses = Player->GetHeldFuses();
		if (Fuses.Num() > 0)
		{
			float X = 40.f * S;
			const float Y = H - 80.f * S;
			DrawText(LOCTEXT("Fuses", "Fusibles:").ToString(), FLinearColor(0.8f, 0.8f, 0.8f), X, Y, Medium, 1.1f * S);
			X += 150.f * S;
			for (int32 i = 0; i < Fuses.Num(); ++i)
			{
				const bool bSelected = i == Player->GetSelectedFuseIndex();
				FLinearColor C = Vestuario::FuseLinearColor(Fuses[i]);
				C.A = bSelected ? 1.f : 0.45f;
				const FString Label = bSelected
					? FString::Printf(TEXT("[%s]"), *Vestuario::FuseName(Fuses[i]).ToString())
					: Vestuario::FuseName(Fuses[i]).ToString();
				DrawText(Label, C, X, Y, Medium, 1.1f * S);
				float TW = 0.f, TH = 0.f;
				GetTextSize(Label, TW, TH, Medium, 1.1f * S);
				X += TW + 24.f * S;
			}
			if (Fuses.Num() > 1)
			{
				DrawText(LOCTEXT("Cycle", "[Q] cambiar").ToString(), FLinearColor(0.6f, 0.6f, 0.6f), 40.f * S, Y + 34.f * S, Small, 1.2f * S);
			}
		}
	}

	// --- Mensajes ---
	{
		const float Now = GetWorld()->GetTimeSeconds();
		float Y = H * 0.2f;
		for (const FVestuarioMessage& Msg : GM->GetMessages())
		{
			const float Alpha = FMath::Clamp(Msg.ExpireTime - Now, 0.f, 1.f);
			DrawCentered(Msg.Text.ToString(), Y, Medium, 1.25f * S, FLinearColor(1.f, 0.9f, 0.75f, Alpha));
			Y += 40.f * S;
		}
	}

	// --- Estado de Twitch ---
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTwitchChatSubsystem* Twitch = GI->GetSubsystem<UTwitchChatSubsystem>())
		{
			const bool bOn = Twitch->GetStatus() == ETwitchStatus::Connected;
			DrawRight(Twitch->GetStatusText().ToString(), 20.f * S, Small, 1.2f * S,
				bOn ? FLinearColor(0.6f, 0.4f, 1.f, 0.9f) : FLinearColor(0.6f, 0.6f, 0.6f, 0.6f));
		}
	}

	// --- Depuracion de la IA (tecla 3) ---
	if (GM->IsDebugAI())
	{
		float Y = 50.f * S;
		for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
		{
			if (const AEnemyAIController* AI = Cast<AEnemyAIController>(It->GetController()))
			{
				DrawRight(FString::Printf(TEXT("Enemigo: %s"), *AI->GetStateDisplayName()), Y, Small, 1.2f * S, FLinearColor::Yellow);
				Y += 22.f * S;
			}
		}
		if (Player)
		{
			DrawRight(FString::Printf(TEXT("Miedo: %.2f"), Player->GetFear()), Y, Small, 1.2f * S, FLinearColor::Yellow);
		}
	}

	// --- Introduccion ---
	if (Elapsed < 9.f && !GM->IsGameOver())
	{
		const float Alpha = FMath::Clamp(9.f - Elapsed, 0.f, 1.f) * FMath::Clamp(Elapsed * 0.7f, 0.f, 1.f);
		DrawCentered(TEXT("EL VESTUARIO"), H * 0.3f, Large, 2.6f * S, FLinearColor(0.85f, 0.1f, 0.08f, Alpha));
		DrawCentered(LOCTEXT("Goal", "Devuelve la luz y sal de aqu\u00ED. \u00C9l no ve... pero te oye.").ToString(),
			H * 0.3f + 80.f * S, Medium, 1.3f * S, FLinearColor(0.9f, 0.9f, 0.9f, Alpha));
		DrawCentered(LOCTEXT("Controls", "WASD moverte \u00B7 Rat\u00F3n mirar \u00B7 Shift correr \u00B7 Ctrl agacharte \u00B7 E usar \u00B7 F linterna \u00B7 Q fusible").ToString(),
			H * 0.3f + 130.f * S, Small, 1.2f * S, FLinearColor(0.7f, 0.7f, 0.7f, Alpha));
		DrawCentered(LOCTEXT("DebugKeys", "1 = simular !ruido \u00B7 2 = simular !pista \u00B7 3 = depurar IA").ToString(),
			H * 0.3f + 160.f * S, Small, 1.2f * S, FLinearColor(0.5f, 0.5f, 0.5f, Alpha));
	}

	// --- Muerte ---
	if (GM->IsPlayerDead())
	{
		const float T = Elapsed - GM->GetEndTime();
		DrawRect(FLinearColor(0.f, 0.f, 0.f, FMath::Clamp((T - 0.8f) * 0.8f, 0.f, 1.f)), 0.f, 0.f, W, H);
		if (T > 1.2f)
		{
			const float A = FMath::Clamp(T - 1.2f, 0.f, 1.f);
			DrawCentered(LOCTEXT("Dead", "TE HA ENCONTRADO").ToString(), H * 0.42f, Large, 2.4f * S, FLinearColor(0.8f, 0.05f, 0.05f, A));
			DrawCentered(LOCTEXT("Retry", "Volviendo a empezar...").ToString(), H * 0.42f + 80.f * S, Medium, 1.1f * S, FLinearColor(0.7f, 0.7f, 0.7f, A));
		}
	}

	// --- Victoria ---
	if (GM->HasEscaped())
	{
		const float T = Elapsed - GM->GetEndTime();
		DrawRect(FLinearColor(0.f, 0.f, 0.f, FMath::Clamp(T * 0.6f, 0.f, 1.f)), 0.f, 0.f, W, H);
		const float A = FMath::Clamp(T - 1.f, 0.f, 1.f);
		DrawCentered(LOCTEXT("Escaped", "HAS ESCAPADO").ToString(), H * 0.4f, Large, 2.4f * S, FLinearColor(0.9f, 0.9f, 0.9f, A));
		const int32 Seconds = FMath::FloorToInt(GM->GetEndTime());
		DrawCentered(FText::Format(LOCTEXT("Time", "Tiempo: {0}:{1}"), FText::AsNumber(Seconds / 60),
			FText::FromString(FString::Printf(TEXT("%02d"), Seconds % 60))).ToString(),
			H * 0.4f + 80.f * S, Medium, 1.2f * S, FLinearColor(0.7f, 0.7f, 0.7f, A));
		DrawCentered(LOCTEXT("ToBeContinued", "...pero el balneario no ha terminado contigo.").ToString(),
			H * 0.4f + 130.f * S, Medium, 1.1f * S, FLinearColor(0.6f, 0.1f, 0.1f, A));
	}
}

#undef LOCTEXT_NAMESPACE
