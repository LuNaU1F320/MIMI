#include "ControllerInputPollingBridge.h"

#include "BattleRoyaleZoneCameraActor.h"
#include "Engine/World.h"
#include "MyCharacter.h"

AControllerInputPollingBridge::AControllerInputPollingBridge()
{
	PrimaryActorTick.bCanEverTick = false;
	PlayerCharacterClass = AMyCharacter::StaticClass();
	ZoneCameraClass = ABattleRoyaleZoneCameraActor::StaticClass();
}

void AControllerInputPollingBridge::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		if (UControllerInputBridgeSubsystem* BridgeSubsystem = World->GetSubsystem<UControllerInputBridgeSubsystem>())
		{
			BridgeSubsystem->ConfigureAndStart(MakeBridgeSettings());
		}
	}
}

void AControllerInputPollingBridge::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UControllerInputBridgeSubsystem* BridgeSubsystem = World->GetSubsystem<UControllerInputBridgeSubsystem>())
		{
			BridgeSubsystem->StopBridge();
		}
	}

	Super::EndPlay(EndPlayReason);
}

FControllerInputBridgeSettings AControllerInputPollingBridge::MakeBridgeSettings() const
{
	FControllerInputBridgeSettings Settings;
	Settings.PlayerCharacterClass = PlayerCharacterClass;
	Settings.ControlledCharacter = ControlledCharacter;
	Settings.MaxDemoPlayers = MaxDemoPlayers;
	Settings.MaxDemoBots = MaxDemoBots;
	Settings.PlayerSpawnSpacing = PlayerSpawnSpacing;
	Settings.ServerBaseUrl = ServerBaseUrl;
	Settings.PollingInterval = PollingInterval;
	Settings.bLogPollingDebug = bLogPollingDebug;
	Settings.BotCount = BotCount;
	Settings.BotSpawnCenter = BotSpawnCenter;
	Settings.BotSpawnAreaExtent = BotSpawnAreaExtent;
	Settings.MinBotSpawnDistance = MinBotSpawnDistance;
	Settings.PlayerSpawnLocation = PlayerSpawnLocation;
	Settings.BattleRoyaleSettings = BattleRoyaleSettings;
	Settings.ZoneCameraClass = ZoneCameraClass;
	Settings.ZoneCameraActor = ZoneCameraActor;
	Settings.bAutoCreateZoneCamera = bAutoCreateZoneCamera;
	Settings.bAutoActivateZoneCamera = bAutoActivateZoneCamera;
	Settings.StatusPollingInterval = StatusPollingInterval;
	Settings.WorldStateSyncInterval = WorldStateSyncInterval;
	return Settings;
}
