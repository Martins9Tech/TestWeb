#include "Twitch/TwitchChatSubsystem.h"

#include "ElVestuario.h"
#include "Twitch/TwitchSettings.h"

#include "Engine/GameInstance.h"
#include "HAL/PlatformTime.h"
#include "IWebSocket.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"
#include "TimerManager.h"
#include "WebSocketsModule.h"

namespace TwitchConst
{
	const TCHAR* TwitchIrcUrl = TEXT("wss://irc-ws.chat.twitch.tv:443");
}

void UTwitchChatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FModuleManager::Get().LoadModuleChecked<FWebSocketsModule>(TEXT("WebSockets"));

	const UTwitchSettings* Settings = GetDefault<UTwitchSettings>();
	FString StartChannel = Settings->Channel;

	// Permite arrancar con: ElVestuario.exe -twitch=micanal
	FString CmdChannel;
	if (FParse::Value(FCommandLine::Get(), TEXT("twitch="), CmdChannel))
	{
		StartChannel = CmdChannel;
	}

	if (Settings->bAutoConnect && !StartChannel.IsEmpty())
	{
		Connect(StartChannel);
	}
	else
	{
		UE_LOG(LogVestuario, Log, TEXT("Twitch: sin canal configurado, el juego funciona sin chat."));
	}
}

void UTwitchChatSubsystem::Deinitialize()
{
	Disconnect();
	Super::Deinitialize();
}

void UTwitchChatSubsystem::Connect(const FString& InChannel)
{
	Disconnect();

	Channel = InChannel.TrimStartAndEnd().ToLower();
	Channel.RemoveFromStart(TEXT("#"));
	if (Channel.IsEmpty())
	{
		return;
	}

	bWantConnection = true;
	ReconnectAttempts = 0;
	OpenSocket();
}

void UTwitchChatSubsystem::Disconnect()
{
	bWantConnection = false;
	if (UGameInstance* GI = GetGameInstance())
	{
		GI->GetTimerManager().ClearTimer(ReconnectTimer);
	}
	if (Socket.IsValid())
	{
		Socket->OnConnected().Clear();
		Socket->OnConnectionError().Clear();
		Socket->OnClosed().Clear();
		Socket->OnMessage().Clear();
		if (Socket->IsConnected())
		{
			Socket->Close();
		}
		Socket.Reset();
	}
	Pending.Reset();
	Status = ETwitchStatus::Disabled;
}

void UTwitchChatSubsystem::OpenSocket()
{
	if (Socket.IsValid())
	{
		Socket->OnConnected().RemoveAll(this);
		Socket->OnConnectionError().RemoveAll(this);
		Socket->OnClosed().RemoveAll(this);
		Socket->OnMessage().RemoveAll(this);
		Socket.Reset();
	}
	Pending.Reset();
	Status = ReconnectAttempts > 0 ? ETwitchStatus::Reconnecting : ETwitchStatus::Connecting;
	UE_LOG(LogVestuario, Log, TEXT("Twitch: conectando a #%s..."), *Channel);

	Socket = FWebSocketsModule::Get().CreateWebSocket(TwitchConst::TwitchIrcUrl);
	Socket->OnConnected().AddUObject(this, &UTwitchChatSubsystem::HandleConnected);
	Socket->OnConnectionError().AddUObject(this, &UTwitchChatSubsystem::HandleConnectionError);
	Socket->OnClosed().AddUObject(this, &UTwitchChatSubsystem::HandleClosed);
	Socket->OnMessage().AddUObject(this, &UTwitchChatSubsystem::HandleMessage);
	Socket->Connect();
}

void UTwitchChatSubsystem::HandleConnected()
{
	// Login anonimo: Twitch acepta cualquier "justinfanNNNN" sin contrasena real.
	const int32 AnonId = FMath::RandRange(10000, 99999);
	Send(TEXT("PASS SCHMOOPIIE"));
	Send(FString::Printf(TEXT("NICK justinfan%d"), AnonId));
	Send(FString::Printf(TEXT("JOIN #%s"), *Channel));
}

void UTwitchChatSubsystem::HandleConnectionError(const FString& Error)
{
	UE_LOG(LogVestuario, Warning, TEXT("Twitch: error de conexion: %s"), *Error);
	ScheduleReconnect();
}

void UTwitchChatSubsystem::HandleClosed(int32 StatusCode, const FString& Reason, bool bWasClean)
{
	UE_LOG(LogVestuario, Warning, TEXT("Twitch: conexion cerrada (%d) %s"), StatusCode, *Reason);
	ScheduleReconnect();
}

void UTwitchChatSubsystem::ScheduleReconnect()
{
	if (!bWantConnection)
	{
		return;
	}
	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance && GameInstance->GetTimerManager().IsTimerActive(ReconnectTimer))
	{
		return; // ya hay una reconexion pendiente (error + cierre llegan juntos)
	}
	Status = ETwitchStatus::Reconnecting;
	++ReconnectAttempts;
	const float Delay = FMath::Min(60.f, 2.5f * FMath::Pow(2.f, static_cast<float>(FMath::Min(ReconnectAttempts, 5))));
	if (UGameInstance* GI = GetGameInstance())
	{
		GI->GetTimerManager().SetTimer(ReconnectTimer, this, &UTwitchChatSubsystem::OpenSocket, Delay, false);
	}
}

void UTwitchChatSubsystem::Send(const FString& Line)
{
	if (Socket.IsValid() && Socket->IsConnected())
	{
		Socket->Send(Line + TEXT("\r\n"));
	}
}

void UTwitchChatSubsystem::HandleMessage(const FString& Message)
{
	Pending += Message;
	int32 NewLine = INDEX_NONE;
	while (Pending.FindChar(TEXT('\n'), NewLine))
	{
		FString Line = Pending.Left(NewLine);
		Pending.RightChopInline(NewLine + 1);
		Line.TrimEndInline();
		if (!Line.IsEmpty())
		{
			ProcessLine(Line);
		}
	}
}

void UTwitchChatSubsystem::ProcessLine(const FString& Line)
{
	if (Line.StartsWith(TEXT("PING")))
	{
		Send(TEXT("PONG :tmi.twitch.tv"));
		return;
	}

	// Formato: ":usuario!usuario@usuario.tmi.twitch.tv PRIVMSG #canal :mensaje"
	const int32 PrivIdx = Line.Find(TEXT(" PRIVMSG #"), ESearchCase::CaseSensitive);
	if (PrivIdx == INDEX_NONE)
	{
		if (Line.Contains(TEXT(" JOIN #")) || Line.Contains(TEXT(" 366 ")))
		{
			if (Status != ETwitchStatus::Connected)
			{
				UE_LOG(LogVestuario, Log, TEXT("Twitch: conectado al chat de #%s"), *Channel);
			}
			Status = ETwitchStatus::Connected;
			ReconnectAttempts = 0;
		}
		return;
	}

	FString User;
	int32 Bang = INDEX_NONE;
	if (Line.FindChar(TEXT('!'), Bang) && Bang > 1 && Bang < PrivIdx)
	{
		User = Line.Mid(1, Bang - 1);
	}

	const int32 MsgIdx = Line.Find(TEXT(" :"), ESearchCase::CaseSensitive, ESearchDir::FromStart, PrivIdx);
	if (MsgIdx == INDEX_NONE)
	{
		return;
	}
	const FString Text = Line.Mid(MsgIdx + 2).TrimStartAndEnd();
	if (!Text.StartsWith(TEXT("!")))
	{
		return;
	}

	FString Command = Text;
	int32 Space = INDEX_NONE;
	if (Text.FindChar(TEXT(' '), Space))
	{
		Command = Text.Left(Space);
	}
	Command.ToLowerInline();

	OnChatCommand.Broadcast(User, Command);
}

FText UTwitchChatSubsystem::GetStatusText() const
{
	switch (Status)
	{
	case ETwitchStatus::Connected:
		return FText::Format(NSLOCTEXT("Vestuario", "TwitchOn", "Twitch: #{0}"), FText::FromString(Channel));
	case ETwitchStatus::Connecting:
		return NSLOCTEXT("Vestuario", "TwitchConnecting", "Twitch: conectando...");
	case ETwitchStatus::Reconnecting:
		return NSLOCTEXT("Vestuario", "TwitchRetry", "Twitch: reconectando...");
	default:
		return NSLOCTEXT("Vestuario", "TwitchOff", "Twitch: sin conectar (teclas 1 y 2 simulan el chat)");
	}
}

bool UTwitchChatSubsystem::IsCooldownReady(FName Key, float CooldownSeconds) const
{
	const double* Last = CooldownTimes.Find(Key);
	return !Last || FPlatformTime::Seconds() - *Last >= CooldownSeconds;
}

bool UTwitchChatSubsystem::IsUserReady(const FString& User, float CooldownSeconds) const
{
	const double* Last = UserTimes.Find(User);
	return !Last || FPlatformTime::Seconds() - *Last >= CooldownSeconds;
}

void UTwitchChatSubsystem::MarkUsed(FName Key, const FString& User)
{
	const double Now = FPlatformTime::Seconds();
	CooldownTimes.Add(Key, Now);
	if (!User.IsEmpty())
	{
		UserTimes.Add(User, Now);
	}
}
