#include "ControllerInputBridgeSubsystem.h"

#include "BattleRoyaleZoneCameraActor.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "IWebSocket.h"
#include "Json.h"
#include "WebSocketsModule.h"
#include "MyCharacter.h"
#include "Components/CapsuleComponent.h"
#include "CollisionQueryParams.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "ShowdownBattleRoyaleSubsystem.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	TSubclassOf<AMyCharacter> DefaultPlayerCharacterClass()
	{
		static ConstructorHelpers::FClassFinder<AMyCharacter> PlayerCharacterBP(TEXT("/Game/Level/BP_PlayerCharacter"));
		TSubclassOf<AMyCharacter> CharacterClass = AMyCharacter::StaticClass();
		if (PlayerCharacterBP.Succeeded())
		{
			CharacterClass = PlayerCharacterBP.Class;
		}
		return CharacterClass;
	}

	void AddServerDirCandidate(TArray<FString>& CandidateDirs, const FString& Directory)
	{
		FString FullPath = FPaths::ConvertRelativePathToFull(Directory);
		FPaths::NormalizeDirectoryName(FullPath);
		CandidateDirs.AddUnique(FullPath);
	}

	bool TryResolveSampleServerDir(FString& OutServerDir, FString& OutSearchedDirs)
	{
		TArray<FString> CandidateDirs;
		AddServerDirCandidate(CandidateDirs, FPaths::ProjectDir() / TEXT("../Hackathon_Sample"));
		AddServerDirCandidate(CandidateDirs, FPaths::ProjectDir() / TEXT("Hackathon_Sample"));
		AddServerDirCandidate(CandidateDirs, FPaths::ProjectDir() / TEXT("../../Hackathon_Sample"));
		AddServerDirCandidate(CandidateDirs, FPaths::LaunchDir() / TEXT("Hackathon_Sample"));
		AddServerDirCandidate(CandidateDirs, FPaths::LaunchDir() / TEXT("../Hackathon_Sample"));
		const FString ExecutableDir = FString(FPlatformProcess::BaseDir());
		AddServerDirCandidate(CandidateDirs, ExecutableDir / TEXT("Hackathon_Sample"));
		AddServerDirCandidate(CandidateDirs, ExecutableDir / TEXT("../Hackathon_Sample"));
		AddServerDirCandidate(CandidateDirs, ExecutableDir / TEXT("../../Hackathon_Sample"));
		AddServerDirCandidate(CandidateDirs, ExecutableDir / TEXT("../../../Hackathon_Sample"));

		OutSearchedDirs.Empty();
		for (const FString& CandidateDir : CandidateDirs)
		{
			if (!OutSearchedDirs.IsEmpty())
			{
				OutSearchedDirs += TEXT(", ");
			}
			OutSearchedDirs += CandidateDir;

			const FString ServerScript = CandidateDir / TEXT("server.js");
			if (FPaths::FileExists(ServerScript))
			{
				OutServerDir = CandidateDir;
				return true;
			}
		}

		return false;
	}
}

UControllerInputBridgeSubsystem::UControllerInputBridgeSubsystem()
{
	PlayerCharacterClass = DefaultPlayerCharacterClass();
	ZoneCameraClass = ABattleRoyaleZoneCameraActor::StaticClass();
}

void UControllerInputBridgeSubsystem::Deinitialize()
{
	StopBridge();
	Super::Deinitialize();
}

void UControllerInputBridgeSubsystem::ConfigureAndStart(const FControllerInputBridgeSettings& InSettings)
{
	StopBridge();
	bIsStopping = false;
	ApplySettings(InSettings);

	if (bStartServerProcess)
	{
		StartServerProcess();
	}

	InitializeDemoCharacters();

	if (UWorld* World = GetWorld())
	{
		if (UShowdownBattleRoyaleSubsystem* BattleRoyaleSubsystem = GetBattleRoyaleSubsystem())
		{
			BattleRoyaleSubsystem->ConfigureBattleRoyale(BattleRoyaleSettings);
		}
		EnsureBattleRoyaleZoneCamera();

		// Connect WebSocket with retry handling
		ConnectWebSocket();

		// Keep only the World State Sync Timer (sends Unreal character states to server)
		World->GetTimerManager().SetTimer(
			WorldStateSyncTimerHandle,
			this,
			&UControllerInputBridgeSubsystem::SendWorldState,
			WorldStateSyncInterval,
			true);
	}
}

void UControllerInputBridgeSubsystem::StopBridge()
{
	bIsStopping = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PollingTimerHandle);
		World->GetTimerManager().ClearTimer(StatusPollingTimerHandle);
		World->GetTimerManager().ClearTimer(WorldStateSyncTimerHandle);
		World->GetTimerManager().ClearTimer(WebSocketReconnectTimerHandle);
	}

	if (UnrealWebSocket.IsValid())
	{
		if (UnrealWebSocket->IsConnected())
		{
			UnrealWebSocket->Close();
		}
		UnrealWebSocket.Reset();
	}

	// Terminate Node.js server process
	StopServerProcess();

	bRequestInFlight = false;
	bStatusRequestInFlight = false;
	bWorldStateRequestInFlight = false;
	LastDebugLogTime = -1000.0f;
	SpawnedBots.Reset();
	DemoPlayerCharacters.Reset();
	DemoBotCharacters.Reset();
	PlayerCharactersById.Reset();
	BotCharactersById.Reset();
	PlayerInitialPercentageMap.Reset();
}

void UControllerInputBridgeSubsystem::ApplySettings(const FControllerInputBridgeSettings& InSettings)
{
	PlayerCharacterClass = InSettings.PlayerCharacterClass;
	if (!PlayerCharacterClass || PlayerCharacterClass == AMyCharacter::StaticClass())
	{
		PlayerCharacterClass = DefaultPlayerCharacterClass();
	}
	ControlledCharacter = InSettings.ControlledCharacter;
	MaxDemoPlayers = FMath::Clamp(InSettings.MaxDemoPlayers, 1, 64);
	MaxDemoBots = FMath::Max(0, InSettings.MaxDemoBots);
	PlayerSpawnSpacing = InSettings.PlayerSpawnSpacing;
	ServerBaseUrl = InSettings.ServerBaseUrl;
	bStartServerProcess = InSettings.bStartServerProcess;
	PollingInterval = FMath::Max(0.05f, InSettings.PollingInterval);
	bLogPollingDebug = InSettings.bLogPollingDebug;
	BotCount = FMath::Max(0, InSettings.BotCount);
	BotSpawnCenter = InSettings.BotSpawnCenter;
	BotSpawnAreaExtent = InSettings.BotSpawnAreaExtent;
	MinBotSpawnDistance = FMath::Max(0.0f, InSettings.MinBotSpawnDistance);
	PlayerSpawnLocation = InSettings.PlayerSpawnLocation;
	BattleRoyaleSettings = InSettings.BattleRoyaleSettings;
	ZoneCameraClass = InSettings.ZoneCameraClass;
	if (!ZoneCameraClass)
	{
		ZoneCameraClass = ABattleRoyaleZoneCameraActor::StaticClass();
	}
	ZoneCameraActor = InSettings.ZoneCameraActor;
	bAutoCreateZoneCamera = InSettings.bAutoCreateZoneCamera;
	bAutoActivateZoneCamera = InSettings.bAutoActivateZoneCamera;
	StatusPollingInterval = FMath::Max(0.1f, InSettings.StatusPollingInterval);
	WorldStateSyncInterval = FMath::Max(0.05f, InSettings.WorldStateSyncInterval);
}

void UControllerInputBridgeSubsystem::InitializeDemoCharacters()
{
	if (!PlayerCharacterClass)
	{
		PlayerCharacterClass = DefaultPlayerCharacterClass();
	}

	if (!ControlledCharacter)
	{
		ControlledCharacter = FindExistingCharacter();
	}

	if (ControlledCharacter)
	{
		ControlledCharacter->SetMoveInput(0.0f, 0.0f);
		ControlledCharacter->SetExternalMovementEnabled(true);
		DemoPlayerCharacters.AddUnique(ControlledCharacter);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("No initial ControlledCharacter found. Player character will be spawned dynamically when they connect."));
	}

	SpawnStaticBots();
}

void UControllerInputBridgeSubsystem::PollInputs()
{
	if (bRequestInFlight)
	{
		return;
	}

	FString BaseUrl = ServerBaseUrl;
	BaseUrl.RemoveFromEnd(TEXT("/"));

	const auto Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(FString::Printf(TEXT("%s/api/unreal/inputs"), *BaseUrl));
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->OnProcessRequestComplete().BindUObject(this, &UControllerInputBridgeSubsystem::HandleInputResponse);

	bRequestInFlight = true;
	if (!Request->ProcessRequest())
	{
		bRequestInFlight = false;
		StopDemoCharacters();
	}
}

void UControllerInputBridgeSubsystem::PollStatus()
{
	if (bStatusRequestInFlight)
	{
		return;
	}

	FString BaseUrl = ServerBaseUrl;
	BaseUrl.RemoveFromEnd(TEXT("/"));

	const auto Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(FString::Printf(TEXT("%s/api/status"), *BaseUrl));
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->OnProcessRequestComplete().BindUObject(this, &UControllerInputBridgeSubsystem::HandleStatusResponse);

	bStatusRequestInFlight = true;
	if (!Request->ProcessRequest())
	{
		bStatusRequestInFlight = false;
	}
}

void UControllerInputBridgeSubsystem::SendWorldState()
{
	if (!UnrealWebSocket.IsValid() || !UnrealWebSocket->IsConnected())
	{
		return;
	}

	TArray<TSharedPtr<FJsonValue>> PlayerSnapshots;
	const auto AddCharacterSnapshot = [&PlayerSnapshots](const FString& PlayerId, AMyCharacter* Character)
	{
		if (PlayerId.IsEmpty() || !Character)
		{
			return;
		}

		const FVector Location = Character->GetActorLocation();
		const TSharedRef<FJsonObject> Snapshot = MakeShared<FJsonObject>();
		Snapshot->SetStringField(TEXT("playerId"), PlayerId);
		Snapshot->SetNumberField(TEXT("worldX"), Location.X);
		Snapshot->SetNumberField(TEXT("worldY"), Location.Y);
		Snapshot->SetNumberField(TEXT("hp"), Character->GetCurrentHP());
		Snapshot->SetNumberField(TEXT("maxHp"), Character->GetMaxHP());
		Snapshot->SetBoolField(TEXT("alive"), Character->IsAlive());
		PlayerSnapshots.Add(MakeShared<FJsonValueObject>(Snapshot));
	};

	for (const TPair<FString, TObjectPtr<AMyCharacter>>& PlayerPair : PlayerCharactersById)
	{
		AddCharacterSnapshot(PlayerPair.Key, PlayerPair.Value);
	}

	for (const TPair<FString, TObjectPtr<AMyCharacter>>& BotPair : BotCharactersById)
	{
		AddCharacterSnapshot(BotPair.Key, BotPair.Value);
	}

	if (PlayerSnapshots.Num() == 0)
	{
		return;
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetStringField(TEXT("type"), TEXT("worldState"));
	RootObject->SetArrayField(TEXT("players"), PlayerSnapshots);
	RootObject->SetNumberField(TEXT("timestamp"), FDateTime::UtcNow().ToUnixTimestamp() * 1000.0);

	FString Body;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	if (FJsonSerializer::Serialize(RootObject, Writer))
	{
		UnrealWebSocket->Send(Body);
	}
}

void UControllerInputBridgeSubsystem::HandleStatusResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	bStatusRequestInFlight = false;

	if (!bWasSuccessful || !Response.IsValid() || Response->GetResponseCode() < 200 || Response->GetResponseCode() >= 300)
	{
		return;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		return;
	}

	FString GameState;
	if (!RootObject->TryGetStringField(TEXT("gameState"), GameState))
	{
		return;
	}

	// Cache initial spawn percentages from server players list
	const TArray<TSharedPtr<FJsonValue>>* PlayersArray = nullptr;
	if (RootObject->TryGetArrayField(TEXT("players"), PlayersArray) && PlayersArray)
	{
		for (const TSharedPtr<FJsonValue>& PlayerValue : *PlayersArray)
		{
			const TSharedPtr<FJsonObject> PlayerObject = PlayerValue.IsValid() ? PlayerValue->AsObject() : nullptr;
			if (PlayerObject.IsValid())
			{
				FString PlayerId;
				double PosX = 50.0;
				double PosY = 50.0;
				if (PlayerObject->TryGetStringField(TEXT("playerId"), PlayerId))
				{
					PlayerObject->TryGetNumberField(TEXT("posX"), PosX);
					PlayerObject->TryGetNumberField(TEXT("posY"), PosY);
					PlayerInitialPercentageMap.FindOrAdd(PlayerId) = FVector2D(PosX, PosY);

					AMyCharacter* Character = nullptr;
					if (TObjectPtr<AMyCharacter>* FoundChar = PlayerCharactersById.Find(PlayerId))
					{
						Character = FoundChar->Get();
					}
					else if (TObjectPtr<AMyCharacter>* FoundBot = BotCharactersById.Find(PlayerId))
					{
						Character = FoundBot->Get();
					}

					if (Character)
					{
						FString ColorHex;
						if (PlayerObject->TryGetStringField(TEXT("color"), ColorHex))
						{
							FLinearColor ColorVal = FLinearColor(FColor::FromHex(ColorHex));
							Character->SetOverlayColor(ColorVal);
						}
					}
				}
			}
		}
	}

	if (UShowdownBattleRoyaleSubsystem* BattleRoyaleSubsystem = GetBattleRoyaleSubsystem())
	{
		if (GameState.Equals(TEXT("Playing"), ESearchCase::IgnoreCase))
		{
			BattleRoyaleSubsystem->StartBattleRoyale();
		}
		else if (!BattleRoyaleSubsystem->IsBattleRoyaleCompleted())
		{
			BattleRoyaleSubsystem->ResetBattleRoyale();
		}
	}
}

void UControllerInputBridgeSubsystem::HandleWorldStateResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	bWorldStateRequestInFlight = false;

	if (bLogPollingDebug && (!bWasSuccessful || !Response.IsValid() || Response->GetResponseCode() < 200 || Response->GetResponseCode() >= 300))
	{
		const int32 ResponseCode = Response.IsValid() ? Response->GetResponseCode() : 0;
		UE_LOG(LogTemp, Warning, TEXT("ControllerInputBridgeSubsystem world-state sync failed. Success=%s ResponseCode=%d"),
			bWasSuccessful ? TEXT("true") : TEXT("false"),
			ResponseCode);
	}
}

void UControllerInputBridgeSubsystem::HandleInputResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	bRequestInFlight = false;

	if (!bWasSuccessful || !Response.IsValid() || Response->GetResponseCode() < 200 || Response->GetResponseCode() >= 300)
	{
		if (bLogPollingDebug)
		{
			const int32 ResponseCode = Response.IsValid() ? Response->GetResponseCode() : 0;
			UE_LOG(LogTemp, Warning, TEXT("ControllerInputBridgeSubsystem input poll failed. Success=%s ResponseCode=%d"),
				bWasSuccessful ? TEXT("true") : TEXT("false"),
				ResponseCode);
		}
		StopDemoCharacters();
		return;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		if (bLogPollingDebug)
		{
			UE_LOG(LogTemp, Warning, TEXT("ControllerInputBridgeSubsystem could not parse /api/unreal/inputs response: %s"), *Response->GetContentAsString());
		}
		StopDemoCharacters();
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* Inputs = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("inputs"), Inputs) || !Inputs || Inputs->Num() == 0)
	{
		if (bLogPollingDebug)
		{
			UE_LOG(LogTemp, Warning, TEXT("ControllerInputBridgeSubsystem received no inputs from sample server."));
		}
		StopDemoCharacters();
		return;
	}

	TSet<FString> SeenPlayerIds;
	TSet<FString> SeenBotIds;
	int32 AppliedPlayerCount = 0;
	int32 AppliedBotCount = 0;
	int32 NonZeroInputCount = 0;
	for (const TSharedPtr<FJsonValue>& InputValue : *Inputs)
	{
		if (AppliedPlayerCount >= MaxDemoPlayers && AppliedBotCount >= MaxDemoBots)
		{
			break;
		}

		const TSharedPtr<FJsonObject> InputObject = InputValue.IsValid() ? InputValue->AsObject() : nullptr;
		if (!InputObject.IsValid())
		{
			continue;
		}

		FString PlayerId;
		InputObject->TryGetStringField(TEXT("playerId"), PlayerId);
		if (PlayerId.IsEmpty())
		{
			continue;
		}

		double MoveX = 0.0;
		double MoveY = 0.0;
		if (InputObject->TryGetNumberField(TEXT("moveX"), MoveX) && InputObject->TryGetNumberField(TEXT("moveY"), MoveY))
		{
			if (!FMath::IsNearlyZero(static_cast<float>(MoveX)) || !FMath::IsNearlyZero(static_cast<float>(MoveY)))
			{
				++NonZeroInputCount;
			}

			if (PlayerId.StartsWith(TEXT("bot_")))
			{
				if (AppliedBotCount >= MaxDemoBots)
				{
					continue;
				}

				ApplyBotMoveInput(PlayerId, static_cast<float>(MoveX), static_cast<float>(MoveY));
				SeenBotIds.Add(PlayerId);
				++AppliedBotCount;
			}
			else
			{
				if (AppliedPlayerCount >= MaxDemoPlayers)
				{
					continue;
				}

				ApplyMoveInput(PlayerId, static_cast<float>(MoveX), static_cast<float>(MoveY));
				SeenPlayerIds.Add(PlayerId);
				++AppliedPlayerCount;
			}
		}
	}

	if (bLogPollingDebug)
	{
		const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		if (CurrentTime - LastDebugLogTime >= 1.0f)
		{
			LastDebugLogTime = CurrentTime;
			UE_LOG(LogTemp, Warning, TEXT("ControllerInputBridgeSubsystem inputs=%d appliedPlayers=%d/%d appliedBots=%d/%d nonZero=%d"),
				Inputs->Num(),
				AppliedPlayerCount,
				MaxDemoPlayers,
				AppliedBotCount,
				MaxDemoBots,
				NonZeroInputCount);
		}
	}

	for (const TPair<FString, TObjectPtr<AMyCharacter>>& PlayerPair : PlayerCharactersById)
	{
		if (!SeenPlayerIds.Contains(PlayerPair.Key) && PlayerPair.Value)
		{
			if (!ShouldPreserveWinnerControl(PlayerPair.Value.Get()))
			{
				PlayerPair.Value->SetMoveInput(0.0f, 0.0f);
			}
		}
	}

	for (const TPair<FString, TObjectPtr<AMyCharacter>>& BotPair : BotCharactersById)
	{
		if (!SeenBotIds.Contains(BotPair.Key) && BotPair.Value)
		{
			if (!ShouldPreserveWinnerControl(BotPair.Value.Get()))
			{
				BotPair.Value->SetMoveInput(0.0f, 0.0f);
			}
		}
	}
}

void UControllerInputBridgeSubsystem::ApplyMoveInput(const FString& PlayerId, float MoveX, float MoveY)
{
	AMyCharacter* PlayerCharacter = GetOrCreatePlayerCharacter(PlayerId);
	if (!PlayerCharacter)
	{
		return;
	}

	PlayerCharacter->SetMoveInput(MoveX, MoveY);
}

void UControllerInputBridgeSubsystem::ApplyBotMoveInput(const FString& BotId, float MoveX, float MoveY)
{
	AMyCharacter* BotCharacter = GetOrCreateBotCharacter(BotId);
	if (!BotCharacter)
	{
		return;
	}

	BotCharacter->SetMoveInput(MoveX, MoveY);
}

void UControllerInputBridgeSubsystem::ApplyEmoteInput(const FString& PlayerId, int64 EmoteSeq)
{
	AMyCharacter* PlayerCharacter = GetOrCreatePlayerCharacter(PlayerId);
	if (!PlayerCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ControllerInputBridge] ApplyEmoteInput: Player character not found for ID: %s"), *PlayerId);
		return;
	}

	// If emoteSeq is reset to 0 (e.g. from mobile stopping input), sync our record to 0
	if (EmoteSeq == 0)
	{
		LastEmoteSeqByPlayerId.Add(PlayerId, 0);
		return;
	}

	int64* LastSeq = LastEmoteSeqByPlayerId.Find(PlayerId);
	if (!LastSeq)
	{
		LastEmoteSeqByPlayerId.Add(PlayerId, EmoteSeq);
		UE_LOG(LogTemp, Log, TEXT("[ControllerInputBridge] First emote seq for player %s is %lld"), *PlayerId, EmoteSeq);
		
		// If the first received sequence is already greater than 0, play it immediately
		if (EmoteSeq > 0)
		{
			UE_LOG(LogTemp, Log, TEXT("[ControllerInputBridge] Playing initial emote for player %s (seq: %lld)"), *PlayerId, EmoteSeq);
			PlayerCharacter->PlayEmote(TEXT("Default"), 1.0f);
		}
		return;
	}

	if (EmoteSeq > *LastSeq)
	{
		UE_LOG(LogTemp, Log, TEXT("[ControllerInputBridge] Emote seq advanced for player %s: %lld -> %lld. Playing emote."), *PlayerId, *LastSeq, EmoteSeq);
		LastEmoteSeqByPlayerId.Add(PlayerId, EmoteSeq);
		PlayerCharacter->PlayEmote(TEXT("Default"), 1.0f);
	}
}

void UControllerInputBridgeSubsystem::StopDemoCharacters()
{
	for (const TObjectPtr<AMyCharacter>& DemoPlayerCharacter : DemoPlayerCharacters)
	{
		if (DemoPlayerCharacter.Get())
		{
			if (!ShouldPreserveWinnerControl(DemoPlayerCharacter.Get()))
			{
				DemoPlayerCharacter->SetMoveInput(0.0f, 0.0f);
			}
		}
	}

	for (const TObjectPtr<AMyCharacter>& DemoBotCharacter : DemoBotCharacters)
	{
		if (DemoBotCharacter.Get())
		{
			if (!ShouldPreserveWinnerControl(DemoBotCharacter.Get()))
			{
				DemoBotCharacter->SetMoveInput(0.0f, 0.0f);
			}
		}
	}
}

bool UControllerInputBridgeSubsystem::ShouldPreserveWinnerControl(const AMyCharacter* Character) const
{
	if (!Character)
	{
		return false;
	}

	const UShowdownBattleRoyaleSubsystem* BattleRoyaleSubsystem = GetBattleRoyaleSubsystem();
	return BattleRoyaleSubsystem
		&& BattleRoyaleSubsystem->IsBattleRoyaleCompleted()
		&& BattleRoyaleSubsystem->GetWinnerCharacter() == Character;
}

AMyCharacter* UControllerInputBridgeSubsystem::FindExistingCharacter() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AMyCharacter> It(World); It; ++It)
	{
		AMyCharacter* Character = *It;
		bool bIsSpawnedBot = false;
		for (const TObjectPtr<AMyCharacter>& SpawnedBot : SpawnedBots)
		{
			if (SpawnedBot.Get() == Character)
			{
				bIsSpawnedBot = true;
				break;
			}
		}

		if (Character && !bIsSpawnedBot)
		{
			return Character;
		}
	}

	return nullptr;
}

AMyCharacter* UControllerInputBridgeSubsystem::SpawnCharacterAt(const FVector& Location, const FRotator& Rotation, const TCHAR* NamePrefix) const
{
	UWorld* World = GetWorld();
	if (!World || !PlayerCharacterClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = MakeUniqueObjectName(World, PlayerCharacterClass, FName(NamePrefix));
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	return World->SpawnActor<AMyCharacter>(PlayerCharacterClass, ResolveCharacterSpawnLocation(Location), Rotation, SpawnParams);
}

FVector UControllerInputBridgeSubsystem::ResolveCharacterSpawnLocation(const FVector& Location) const
{
	UWorld* World = GetWorld();
	if (!World || !PlayerCharacterClass)
	{
		return Location;
	}

	float CapsuleHalfHeight = 88.0f;
	if (const AMyCharacter* DefaultCharacter = PlayerCharacterClass->GetDefaultObject<AMyCharacter>())
	{
		if (const UCapsuleComponent* Capsule = DefaultCharacter->GetCapsuleComponent())
		{
			CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		}
	}

	FHitResult Hit;
	const FVector TraceStart(Location.X, Location.Y, Location.Z + 5000.0f);
	const FVector TraceEnd(Location.X, Location.Y, Location.Z - 5000.0f);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MimiSpawnGroundTrace), false);
	if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
	{
		return FVector(Location.X, Location.Y, Hit.Location.Z + CapsuleHalfHeight + 2.0f);
	}

	return FVector(Location.X, Location.Y, Location.Z + CapsuleHalfHeight + 2.0f);
}

AMyCharacter* UControllerInputBridgeSubsystem::GetOrCreatePlayerCharacter(const FString& PlayerId)
{
	if (TObjectPtr<AMyCharacter>* ExistingCharacter = PlayerCharactersById.Find(PlayerId))
	{
		return ExistingCharacter->Get();
	}

	const int32 PlayerIndex = PlayerCharactersById.Num();
	if (PlayerIndex >= MaxDemoPlayers)
	{
		return nullptr;
	}

	AMyCharacter* PlayerCharacter = nullptr;
	if (PlayerIndex == 0)
	{
		if (!ControlledCharacter)
		{
			ControlledCharacter = FindExistingCharacter();
		}

		PlayerCharacter = ControlledCharacter;
	}

	if (!PlayerCharacter)
	{
		FVector SpawnLoc;
		if (FVector2D* InitialPercent = PlayerInitialPercentageMap.Find(PlayerId))
		{
			float WorldY = (InitialPercent->X / 100.0f) * 6000.0f - 3000.0f;
			float WorldX = ((100.0f - InitialPercent->Y) / 100.0f) * 6000.0f - 3000.0f;
			SpawnLoc = FVector(WorldX, WorldY, 0.0f);
		}
		else
		{
			SpawnLoc = GetPlayerSpawnLocation(PlayerIndex);
		}
		PlayerCharacter = SpawnCharacterAt(SpawnLoc, FRotator::ZeroRotator, TEXT("DemoPlayer"));
	}

	if (PlayerCharacter)
	{
		PlayerCharacter->SetExternalMovementEnabled(true);
		PlayerCharacter->SetMoveInput(0.0f, 0.0f);
		DemoPlayerCharacters.AddUnique(PlayerCharacter);
		PlayerCharactersById.Add(PlayerId, PlayerCharacter);
		if (PlayerIndex == 0 && !ControlledCharacter)
		{
			ControlledCharacter = PlayerCharacter;
		}
		if (UShowdownBattleRoyaleSubsystem* BattleRoyaleSubsystem = GetBattleRoyaleSubsystem())
		{
			BattleRoyaleSubsystem->RegisterPlayerCharacter(PlayerId, PlayerCharacter);
		}
		UE_LOG(LogTemp, Log, TEXT("Mapped sample player %s to %s."), *PlayerId, *PlayerCharacter->GetName());
	}

	return PlayerCharacter;
}

AMyCharacter* UControllerInputBridgeSubsystem::GetOrCreateBotCharacter(const FString& BotId)
{
	if (TObjectPtr<AMyCharacter>* ExistingCharacter = BotCharactersById.Find(BotId))
	{
		return ExistingCharacter->Get();
	}

	const int32 BotIndex = BotCharactersById.Num();
	if (BotIndex >= MaxDemoBots)
	{
		return nullptr;
	}

	FVector SpawnLoc;
	if (FVector2D* InitialPercent = PlayerInitialPercentageMap.Find(BotId))
	{
		float WorldY = (InitialPercent->X / 100.0f) * 6000.0f - 3000.0f;
		float WorldX = ((100.0f - InitialPercent->Y) / 100.0f) * 6000.0f - 3000.0f;
		SpawnLoc = FVector(WorldX, WorldY, 0.0f);
	}
	else
	{
		FRandomStream RandomStream;
		RandomStream.Initialize(GetTypeHash(BotId) ^ GetTypeHash(BotSpawnCenter) ^ FMath::Rand());

		TArray<FVector> ExistingBotLocations;
		ExistingBotLocations.Reserve(DemoBotCharacters.Num());
		for (const TObjectPtr<AMyCharacter>& DemoBotCharacter : DemoBotCharacters)
		{
			if (DemoBotCharacter.Get())
			{
				ExistingBotLocations.Add(DemoBotCharacter->GetActorLocation());
			}
		}
		SpawnLoc = GetRandomBotSpawnLocation(RandomStream, ExistingBotLocations);
	}

	AMyCharacter* BotCharacter = SpawnCharacterAt(SpawnLoc, FRotator::ZeroRotator, TEXT("WebBot"));
	if (BotCharacter)
	{
		BotCharacter->SetExternalMovementEnabled(true);
		BotCharacter->SetMoveInput(0.0f, 0.0f);
		DemoBotCharacters.AddUnique(BotCharacter);
		BotCharactersById.Add(BotId, BotCharacter);
		if (UShowdownBattleRoyaleSubsystem* BattleRoyaleSubsystem = GetBattleRoyaleSubsystem())
		{
			BattleRoyaleSubsystem->RegisterBotCharacter(BotId, BotCharacter);
		}
		UE_LOG(LogTemp, Log, TEXT("Mapped sample bot %s to %s."), *BotId, *BotCharacter->GetName());
	}

	return BotCharacter;
}

FVector UControllerInputBridgeSubsystem::GetPlayerSpawnLocation(int32 PlayerIndex) const
{
	const int32 Column = PlayerIndex % 2;
	const int32 Row = PlayerIndex / 2;
	return PlayerSpawnLocation + FVector(Column * PlayerSpawnSpacing.X, Row * PlayerSpawnSpacing.Y, 0.0f);
}

void UControllerInputBridgeSubsystem::SpawnStaticBots()
{
	if (BotCount <= 0)
	{
		return;
	}

	FRandomStream RandomStream;
	RandomStream.Initialize(GetTypeHash(BotSpawnCenter) ^ FDateTime::Now().GetMillisecond() ^ FMath::Rand());
	TArray<FVector> SpawnLocations;
	SpawnLocations.Reserve(BotCount);

	for (int32 Index = 0; Index < BotCount; ++Index)
	{
		const FVector SpawnLocation = GetRandomBotSpawnLocation(RandomStream, SpawnLocations);
		const FVector LookTarget = ControlledCharacter ? ControlledCharacter->GetActorLocation() : BotSpawnCenter;
		const FRotator SpawnRotation = (LookTarget - SpawnLocation).Rotation();

		AMyCharacter* Bot = SpawnCharacterAt(SpawnLocation, FRotator(0.0f, SpawnRotation.Yaw, 0.0f), TEXT("StaticBot"));
		if (Bot)
		{
			Bot->SetExternalMovementEnabled(true);
			Bot->SetMoveInput(0.0f, 0.0f);
			SpawnedBots.Add(Bot);
			SpawnLocations.Add(SpawnLocation);
		}
	}
}

FVector UControllerInputBridgeSubsystem::GetRandomBotSpawnLocation(FRandomStream& RandomStream, const TArray<FVector>& ExistingLocations) const
{
	const FVector2D SafeExtent(
		FMath::Max(0.0f, BotSpawnAreaExtent.X),
		FMath::Max(0.0f, BotSpawnAreaExtent.Y));

	for (int32 Attempt = 0; Attempt < 20; ++Attempt)
	{
		const FVector CandidateLocation = BotSpawnCenter + FVector(
			RandomStream.FRandRange(-SafeExtent.X, SafeExtent.X),
			RandomStream.FRandRange(-SafeExtent.Y, SafeExtent.Y),
			0.0f);

		if (IsFarEnoughFromExistingBots(CandidateLocation, ExistingLocations))
		{
			return CandidateLocation;
		}
	}

	return BotSpawnCenter + FVector(
		RandomStream.FRandRange(-SafeExtent.X, SafeExtent.X),
		RandomStream.FRandRange(-SafeExtent.Y, SafeExtent.Y),
		0.0f);
}

bool UControllerInputBridgeSubsystem::IsFarEnoughFromExistingBots(const FVector& CandidateLocation, const TArray<FVector>& ExistingLocations) const
{
	const float MinDistanceSquared = FMath::Square(MinBotSpawnDistance);
	for (const FVector& ExistingLocation : ExistingLocations)
	{
		if (FVector::DistSquared2D(CandidateLocation, ExistingLocation) < MinDistanceSquared)
		{
			return false;
		}
	}

	if (ControlledCharacter && FVector::DistSquared2D(CandidateLocation, ControlledCharacter->GetActorLocation()) < MinDistanceSquared)
	{
		return false;
	}

	return true;
}

UShowdownBattleRoyaleSubsystem* UControllerInputBridgeSubsystem::GetBattleRoyaleSubsystem() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UShowdownBattleRoyaleSubsystem>() : nullptr;
}

void UControllerInputBridgeSubsystem::EnsureBattleRoyaleZoneCamera()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!ZoneCameraActor)
	{
		for (TActorIterator<ABattleRoyaleZoneCameraActor> It(World); It; ++It)
		{
			ZoneCameraActor = *It;
			break;
		}
	}

	if (!ZoneCameraActor && bAutoCreateZoneCamera)
	{
		if (!ZoneCameraClass)
		{
			ZoneCameraClass = ABattleRoyaleZoneCameraActor::StaticClass();
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = MakeUniqueObjectName(World, ZoneCameraClass, FName(TEXT("BattleRoyaleZoneCamera")));
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ZoneCameraActor = World->SpawnActor<ABattleRoyaleZoneCameraActor>(ZoneCameraClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	}

	if (ZoneCameraActor)
	{
		ZoneCameraActor->SetAutoActivateForPlayer0(bAutoActivateZoneCamera);
		if (bAutoActivateZoneCamera)
		{
			ZoneCameraActor->ActivateForPlayer0();
		}
	}
}

void UControllerInputBridgeSubsystem::HandleWebSocketMessage(const FString& MessageString)
{
	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(MessageString);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		return;
	}

	FString MsgType;
	if (!RootObject->TryGetStringField(TEXT("type"), MsgType))
	{
		return;
	}

	if (MsgType.Equals(TEXT("inputsUpdated"), ESearchCase::IgnoreCase))
	{
		bool bFullSnapshot = false;
		RootObject->TryGetBoolField(TEXT("fullSnapshot"), bFullSnapshot);

		const TArray<TSharedPtr<FJsonValue>>* Inputs = nullptr;
		if (!RootObject->TryGetArrayField(TEXT("inputs"), Inputs) || !Inputs || Inputs->Num() == 0)
		{
			if (bFullSnapshot)
			{
				StopDemoCharacters();
			}
			return;
		}

		TSet<FString> SeenPlayerIds;
		TSet<FString> SeenBotIds;
		int32 AppliedPlayerCount = 0;
		int32 AppliedBotCount = 0;

		for (const TSharedPtr<FJsonValue>& InputValue : *Inputs)
		{
			if (AppliedPlayerCount >= MaxDemoPlayers && AppliedBotCount >= MaxDemoBots)
			{
				break;
			}

			const TSharedPtr<FJsonObject> InputObject = InputValue.IsValid() ? InputValue->AsObject() : nullptr;
			if (!InputObject.IsValid())
			{
				continue;
			}

			FString PlayerId;
			InputObject->TryGetStringField(TEXT("playerId"), PlayerId);
			if (PlayerId.IsEmpty())
			{
				continue;
			}

			double MoveX = 0.0;
			double MoveY = 0.0;
			if (InputObject->TryGetNumberField(TEXT("moveX"), MoveX) && InputObject->TryGetNumberField(TEXT("moveY"), MoveY))
			{
				if (PlayerId.StartsWith(TEXT("bot_")))
				{
					if (AppliedBotCount >= MaxDemoBots)
					{
						continue;
					}
					ApplyBotMoveInput(PlayerId, static_cast<float>(MoveX), static_cast<float>(MoveY));
					SeenBotIds.Add(PlayerId);
					++AppliedBotCount;
				}
				else
				{
					if (AppliedPlayerCount >= MaxDemoPlayers)
					{
						continue;
					}
					ApplyMoveInput(PlayerId, static_cast<float>(MoveX), static_cast<float>(MoveY));
					SeenPlayerIds.Add(PlayerId);
					++AppliedPlayerCount;
				}
			}

			double EmoteSeqDouble = 0.0;
			if (InputObject->TryGetNumberField(TEXT("emoteSeq"), EmoteSeqDouble))
			{
				if (!PlayerId.StartsWith(TEXT("bot_")))
				{
					ApplyEmoteInput(PlayerId, static_cast<int64>(EmoteSeqDouble));
				}
			}
		}

		if (bFullSnapshot)
		{
			for (const TPair<FString, TObjectPtr<AMyCharacter>>& PlayerPair : PlayerCharactersById)
			{
				if (!SeenPlayerIds.Contains(PlayerPair.Key) && PlayerPair.Value)
				{
					if (!ShouldPreserveWinnerControl(PlayerPair.Value.Get()))
					{
						PlayerPair.Value->SetMoveInput(0.0f, 0.0f);
					}
				}
			}

			for (const TPair<FString, TObjectPtr<AMyCharacter>>& BotPair : BotCharactersById)
			{
				if (!SeenBotIds.Contains(BotPair.Key) && BotPair.Value)
				{
					if (!ShouldPreserveWinnerControl(BotPair.Value.Get()))
					{
						BotPair.Value->SetMoveInput(0.0f, 0.0f);
					}
				}
			}
		}
	}
	else if (MsgType.Equals(TEXT("resetGame"), ESearchCase::IgnoreCase))
	{
		StopDemoCharacters();

		if (UShowdownBattleRoyaleSubsystem* BattleRoyaleSubsystem = GetBattleRoyaleSubsystem())
		{
			BattleRoyaleSubsystem->ResetBattleRoyale();
		}
	}
	else if (MsgType.Equals(TEXT("gameStateChanged"), ESearchCase::IgnoreCase))
	{
		FString GameState;
		if (RootObject->TryGetStringField(TEXT("gameState"), GameState))
		{
			// Cache initial spawn percentages from server players list
			const TArray<TSharedPtr<FJsonValue>>* PlayersArray = nullptr;
			if (RootObject->TryGetArrayField(TEXT("players"), PlayersArray) && PlayersArray)
			{
				for (const TSharedPtr<FJsonValue>& PlayerValue : *PlayersArray)
				{
					const TSharedPtr<FJsonObject> PlayerObject = PlayerValue.IsValid() ? PlayerValue->AsObject() : nullptr;
					if (PlayerObject.IsValid())
					{
						FString PlayerId;
						double PosX = 50.0;
						double PosY = 50.0;
						if (PlayerObject->TryGetStringField(TEXT("playerId"), PlayerId))
						{
							PlayerObject->TryGetNumberField(TEXT("posX"), PosX);
							PlayerObject->TryGetNumberField(TEXT("posY"), PosY);
							PlayerInitialPercentageMap.FindOrAdd(PlayerId) = FVector2D(PosX, PosY);

							AMyCharacter* Character = nullptr;
							// Spawn characters/bots if not already created when receiving status update
							if (PlayerId.StartsWith(TEXT("bot_")))
							{
								Character = GetOrCreateBotCharacter(PlayerId);
							}
							else
							{
								Character = GetOrCreatePlayerCharacter(PlayerId);
							}

							if (Character)
							{
								FString ColorHex;
								if (PlayerObject->TryGetStringField(TEXT("color"), ColorHex))
								{
									FLinearColor ColorVal = FLinearColor(FColor::FromHex(ColorHex));
									Character->SetOverlayColor(ColorVal);
								}
							}
						}
					}
				}
			}

			if (UShowdownBattleRoyaleSubsystem* BattleRoyaleSubsystem = GetBattleRoyaleSubsystem())
			{
				if (GameState.Equals(TEXT("Playing"), ESearchCase::IgnoreCase))
				{
					BattleRoyaleSubsystem->StartBattleRoyale();
				}
				else if (!BattleRoyaleSubsystem->IsBattleRoyaleCompleted())
				{
					BattleRoyaleSubsystem->ResetBattleRoyale();
				}
			}
		}
	}
}

void UControllerInputBridgeSubsystem::StartServerProcess()
{
	if (ServerProcessHandle.IsValid())
	{
		return;
	}

	FString SearchedDirs;
	FString ServerDir;
	if (!TryResolveSampleServerDir(ServerDir, SearchedDirs))
	{
		UE_LOG(LogTemp, Error, TEXT("[Subsystem] Failed to start Node.js server process. Could not find Hackathon_Sample/server.js. Searched: %s"), *SearchedDirs);
		return;
	}

	FString ServerScript = ServerDir / TEXT("server.js");

	FString Executable = TEXT("C:\\Program Files\\nodejs\\node.exe");
	if (!FPaths::FileExists(Executable))
	{
		Executable = TEXT("node.exe"); // Fallback to PATH search
	}
	FString Params = FString::Printf(TEXT("\"%s\""), *ServerScript);

	uint32 ProcessId = 0;
	ServerProcessHandle = FPlatformProcess::CreateProc(
		*Executable,
		*Params,
		false, // bLaunchDetached = false (child process)
		true,  // bLaunchHidden = true
		true,  // bLaunchReallyHidden = true
		&ProcessId,
		0,
		*ServerDir,
		nullptr
	);

	if (ServerProcessHandle.IsValid())
	{
		ServerProcessId = ProcessId;
		UE_LOG(LogTemp, Warning, TEXT("[Subsystem] Started Node.js server process (PID: %d)."), ProcessId);

		// Open Host Dashboard Webpage
		FString HostUrl = ServerBaseUrl;
		HostUrl.RemoveFromEnd(TEXT("/"));
		HostUrl += TEXT("/host.html");
		FPlatformProcess::LaunchURL(*HostUrl, nullptr, nullptr);
	}
	else
	{
		ServerProcessId = 0;
		UE_LOG(LogTemp, Error, TEXT("[Subsystem] Failed to start Node.js server process. Make sure Node.js is installed and server script exists: %s"), *ServerScript);
	}
}

void UControllerInputBridgeSubsystem::StopServerProcess()
{
	if (ServerProcessHandle.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Subsystem] Terminating Node.js server process."));
		FPlatformProcess::TerminateProc(ServerProcessHandle);
		FPlatformProcess::CloseProc(ServerProcessHandle);
		ServerProcessHandle.Reset();
		ServerProcessId = 0;
	}
}

void UControllerInputBridgeSubsystem::ConnectWebSocket()
{
	if (bIsStopping || !IsValid(this) || IsUnreachable())
	{
		return;
	}

	if (!UnrealWebSocket.IsValid())
	{
		FWebSocketsModule& WebSocketsModule = FWebSocketsModule::Get();
		FString BaseWsUrl = ServerBaseUrl;
		BaseWsUrl.ReplaceInline(TEXT("http://"), TEXT("ws://"));
		BaseWsUrl.ReplaceInline(TEXT("https://"), TEXT("wss://"));
		BaseWsUrl.RemoveFromEnd(TEXT("/"));

		UnrealWebSocket = WebSocketsModule.CreateWebSocket(FString::Printf(TEXT("%s/ws/unreal"), *BaseWsUrl), TEXT("ws"));

		UnrealWebSocket->OnConnected().AddLambda([this]()
		{
			if (bIsStopping || !IsValid(this) || IsUnreachable())
			{
				return;
			}
			UE_LOG(LogTemp, Warning, TEXT("[Unreal WS] Connected to backend WebSocket."));
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().ClearTimer(WebSocketReconnectTimerHandle);
			}
		});

		UnrealWebSocket->OnConnectionError().AddLambda([this](const FString& Error)
		{
			if (bIsStopping || !IsValid(this) || IsUnreachable())
			{
				return;
			}
			UE_LOG(LogTemp, Error, TEXT("[Unreal WS] Connection Error: %s. Retrying in 2 seconds..."), *Error);
			RetryConnectWebSocket();
		});

		UnrealWebSocket->OnClosed().AddLambda([this](int32 StatusCode, const FString& Reason, bool bWasClean)
		{
			if (bIsStopping || !IsValid(this) || IsUnreachable())
			{
				return;
			}
			UE_LOG(LogTemp, Warning, TEXT("[Unreal WS] Connection Closed: %s. Retrying in 2 seconds..."), *Reason);
			RetryConnectWebSocket();
		});

		UnrealWebSocket->OnMessage().AddUObject(this, &UControllerInputBridgeSubsystem::HandleWebSocketMessage);
	}

	if (UnrealWebSocket.IsValid() && !UnrealWebSocket->IsConnected())
	{
		UnrealWebSocket->Connect();
	}
}

void UControllerInputBridgeSubsystem::RetryConnectWebSocket()
{
	if (bIsStopping || !IsValid(this) || IsUnreachable())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World && !World->bIsTearingDown && !WebSocketReconnectTimerHandle.IsValid())
	{
		World->GetTimerManager().SetTimer(
			WebSocketReconnectTimerHandle,
			this,
			&UControllerInputBridgeSubsystem::ConnectWebSocket,
			2.0f,
			false);
	}
}
