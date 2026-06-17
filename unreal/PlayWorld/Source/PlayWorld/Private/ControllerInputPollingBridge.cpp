#include "ControllerInputPollingBridge.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Json.h"
#include "WebSocketsModule.h"
#include "MyCharacter.h"
#include "Misc/DateTime.h"
#include "ShowdownBattleRoyaleSubsystem.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "TimerManager.h"

AControllerInputPollingBridge::AControllerInputPollingBridge()
{
	PrimaryActorTick.bCanEverTick = false;
	PlayerCharacterClass = AMyCharacter::StaticClass();
}

void AControllerInputPollingBridge::BeginPlay()
{
	Super::BeginPlay();

	InitializeDemoCharacters();

	if (UWorld* World = GetWorld())
	{
		if (UShowdownBattleRoyaleSubsystem* BattleRoyaleSubsystem = GetBattleRoyaleSubsystem())
		{
			BattleRoyaleSubsystem->ConfigureBattleRoyale(BattleRoyaleSettings);
		}

		// Initialize WebSocket Connection
		FWebSocketsModule& WebSocketsModule = FWebSocketsModule::Get();
		FString BaseWsUrl = ServerBaseUrl;
		BaseWsUrl.ReplaceInline(TEXT("http://"), TEXT("ws://"));
		BaseWsUrl.ReplaceInline(TEXT("https://"), TEXT("wss://"));
		BaseWsUrl.RemoveFromEnd(TEXT("/"));

		UnrealWebSocket = WebSocketsModule.CreateWebSocket(FString::Printf(TEXT("%s/ws/unreal"), *BaseWsUrl), TEXT("ws"));

		UnrealWebSocket->OnConnected().AddLambda([this]()
		{
			UE_LOG(LogTemp, Warning, TEXT("[Unreal WS] Connected to backend WebSocket."));
		});

		UnrealWebSocket->OnConnectionError().AddLambda([](const FString& Error)
		{
			UE_LOG(LogTemp, Error, TEXT("[Unreal WS] Connection Error: %s"), *Error);
		});

		UnrealWebSocket->OnClosed().AddLambda([](int32 StatusCode, const FString& Reason, bool bWasClean)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Unreal WS] Connection Closed: %s"), *Reason);
		});

		UnrealWebSocket->OnMessage().AddUObject(this, &AControllerInputPollingBridge::HandleWebSocketMessage);

		UnrealWebSocket->Connect();

		// Keep only the World State Sync Timer (sends Unreal character states to server)
		World->GetTimerManager().SetTimer(
			WorldStateSyncTimerHandle,
			this,
			&AControllerInputPollingBridge::SendWorldState,
			WorldStateSyncInterval,
			true);
	}
}

void AControllerInputPollingBridge::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UnrealWebSocket.IsValid() && UnrealWebSocket->IsConnected())
	{
		UnrealWebSocket->Close();
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WorldStateSyncTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void AControllerInputPollingBridge::InitializeDemoCharacters()
{
	if (!PlayerCharacterClass)
	{
		PlayerCharacterClass = AMyCharacter::StaticClass();
	}

	if (!ControlledCharacter)
	{
		ControlledCharacter = FindExistingCharacter();
	}

	if (!ControlledCharacter)
	{
		ControlledCharacter = SpawnCharacterAt(PlayerSpawnLocation, FRotator::ZeroRotator, TEXT("DemoPlayer"));
	}

	if (!ControlledCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("ControllerInputPollingBridge could not find or spawn a controlled AMyCharacter."));
		return;
	}

	ControlledCharacter->SetMoveInput(0.0f, 0.0f);
	ControlledCharacter->SetExternalMovementEnabled(true);
	DemoPlayerCharacters.AddUnique(ControlledCharacter);
	SpawnStaticBots();
}

void AControllerInputPollingBridge::PollInputs()
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
	Request->OnProcessRequestComplete().BindUObject(this, &AControllerInputPollingBridge::HandleInputResponse);

	bRequestInFlight = true;
	if (!Request->ProcessRequest())
	{
		bRequestInFlight = false;
		StopDemoCharacters();
	}
}

void AControllerInputPollingBridge::PollStatus()
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
	Request->OnProcessRequestComplete().BindUObject(this, &AControllerInputPollingBridge::HandleStatusResponse);

	bStatusRequestInFlight = true;
	if (!Request->ProcessRequest())
	{
		bStatusRequestInFlight = false;
	}
}

void AControllerInputPollingBridge::SendWorldState()
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

void AControllerInputPollingBridge::HandleStatusResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
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
		else
		{
			BattleRoyaleSubsystem->ResetBattleRoyale();
		}
	}
}

void AControllerInputPollingBridge::HandleWorldStateResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	bWorldStateRequestInFlight = false;

	if (bLogPollingDebug && (!bWasSuccessful || !Response.IsValid() || Response->GetResponseCode() < 200 || Response->GetResponseCode() >= 300))
	{
		const int32 ResponseCode = Response.IsValid() ? Response->GetResponseCode() : 0;
		UE_LOG(LogTemp, Warning, TEXT("ControllerInputPollingBridge world-state sync failed. Success=%s ResponseCode=%d"),
			bWasSuccessful ? TEXT("true") : TEXT("false"),
			ResponseCode);
	}
}

void AControllerInputPollingBridge::HandleInputResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	bRequestInFlight = false;

	if (!bWasSuccessful || !Response.IsValid() || Response->GetResponseCode() < 200 || Response->GetResponseCode() >= 300)
	{
		if (bLogPollingDebug)
		{
			const int32 ResponseCode = Response.IsValid() ? Response->GetResponseCode() : 0;
			UE_LOG(LogTemp, Warning, TEXT("ControllerInputPollingBridge input poll failed. Success=%s ResponseCode=%d"),
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
			UE_LOG(LogTemp, Warning, TEXT("ControllerInputPollingBridge could not parse /api/unreal/inputs response: %s"), *Response->GetContentAsString());
		}
		StopDemoCharacters();
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* Inputs = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("inputs"), Inputs) || !Inputs || Inputs->Num() == 0)
	{
		if (bLogPollingDebug)
		{
			UE_LOG(LogTemp, Warning, TEXT("ControllerInputPollingBridge received no inputs from sample server."));
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
			UE_LOG(LogTemp, Warning, TEXT("ControllerInputPollingBridge inputs=%d appliedPlayers=%d/%d appliedBots=%d/%d nonZero=%d"),
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
			PlayerPair.Value->SetMoveInput(0.0f, 0.0f);
		}
	}

	for (const TPair<FString, TObjectPtr<AMyCharacter>>& BotPair : BotCharactersById)
	{
		if (!SeenBotIds.Contains(BotPair.Key) && BotPair.Value)
		{
			BotPair.Value->SetMoveInput(0.0f, 0.0f);
		}
	}
}

void AControllerInputPollingBridge::ApplyMoveInput(const FString& PlayerId, float MoveX, float MoveY)
{
	AMyCharacter* PlayerCharacter = GetOrCreatePlayerCharacter(PlayerId);
	if (!PlayerCharacter)
	{
		return;
	}

	PlayerCharacter->SetMoveInput(MoveX, MoveY);
}

void AControllerInputPollingBridge::ApplyBotMoveInput(const FString& BotId, float MoveX, float MoveY)
{
	AMyCharacter* BotCharacter = GetOrCreateBotCharacter(BotId);
	if (!BotCharacter)
	{
		return;
	}

	BotCharacter->SetMoveInput(MoveX, MoveY);
}

void AControllerInputPollingBridge::StopDemoCharacters()
{
	for (const TObjectPtr<AMyCharacter>& DemoPlayerCharacter : DemoPlayerCharacters)
	{
		if (DemoPlayerCharacter.Get())
		{
			DemoPlayerCharacter->SetMoveInput(0.0f, 0.0f);
		}
	}

	for (const TObjectPtr<AMyCharacter>& DemoBotCharacter : DemoBotCharacters)
	{
		if (DemoBotCharacter.Get())
		{
			DemoBotCharacter->SetMoveInput(0.0f, 0.0f);
		}
	}
}

AMyCharacter* AControllerInputPollingBridge::FindExistingCharacter() const
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

AMyCharacter* AControllerInputPollingBridge::SpawnCharacterAt(const FVector& Location, const FRotator& Rotation, const TCHAR* NamePrefix) const
{
	UWorld* World = GetWorld();
	if (!World || !PlayerCharacterClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = MakeUniqueObjectName(World, PlayerCharacterClass, FName(NamePrefix));
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	return World->SpawnActor<AMyCharacter>(PlayerCharacterClass, Location, Rotation, SpawnParams);
}

AMyCharacter* AControllerInputPollingBridge::GetOrCreatePlayerCharacter(const FString& PlayerId)
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
		if (UShowdownBattleRoyaleSubsystem* BattleRoyaleSubsystem = GetBattleRoyaleSubsystem())
		{
			BattleRoyaleSubsystem->RegisterPlayerCharacter(PlayerId, PlayerCharacter);
		}
		UE_LOG(LogTemp, Log, TEXT("Mapped sample player %s to %s."), *PlayerId, *PlayerCharacter->GetName());
	}

	return PlayerCharacter;
}

AMyCharacter* AControllerInputPollingBridge::GetOrCreateBotCharacter(const FString& BotId)
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
		RandomStream.Initialize(GetTypeHash(BotId) ^ GetTypeHash(GetActorLocation()) ^ FMath::Rand());

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

FVector AControllerInputPollingBridge::GetPlayerSpawnLocation(int32 PlayerIndex) const
{
	const int32 Column = PlayerIndex % 2;
	const int32 Row = PlayerIndex / 2;
	return PlayerSpawnLocation + FVector(Column * PlayerSpawnSpacing.X, Row * PlayerSpawnSpacing.Y, 0.0f);
}

void AControllerInputPollingBridge::SpawnStaticBots()
{
	if (BotCount <= 0)
	{
		return;
	}

	FRandomStream RandomStream;
	RandomStream.Initialize(GetTypeHash(GetActorLocation()) ^ FDateTime::Now().GetMillisecond() ^ FMath::Rand());
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

FVector AControllerInputPollingBridge::GetRandomBotSpawnLocation(FRandomStream& RandomStream, const TArray<FVector>& ExistingLocations) const
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

bool AControllerInputPollingBridge::IsFarEnoughFromExistingBots(const FVector& CandidateLocation, const TArray<FVector>& ExistingLocations) const
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

UShowdownBattleRoyaleSubsystem* AControllerInputPollingBridge::GetBattleRoyaleSubsystem() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UShowdownBattleRoyaleSubsystem>() : nullptr;
}

void AControllerInputPollingBridge::HandleWebSocketMessage(const FString& MessageString)
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
		const TArray<TSharedPtr<FJsonValue>>* Inputs = nullptr;
		if (!RootObject->TryGetArrayField(TEXT("inputs"), Inputs) || !Inputs || Inputs->Num() == 0)
		{
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

							// Spawn characters/bots if not already created when receiving status update
							if (PlayerId.StartsWith(TEXT("bot_")))
							{
								GetOrCreateBotCharacter(PlayerId);
							}
							else
							{
								GetOrCreatePlayerCharacter(PlayerId);
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
				else
				{
					BattleRoyaleSubsystem->ResetBattleRoyale();
				}
			}
		}
	}
}
