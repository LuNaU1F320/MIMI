#include "ControllerInputPollingBridge.h"

#include "BattleRoyaleZoneCameraActor.h"
#include "Engine/World.h"
#include "MyCharacter.h"
#include "UObject/ConstructorHelpers.h"

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
}

AControllerInputPollingBridge::AControllerInputPollingBridge()
{
	PrimaryActorTick.bCanEverTick = false;
	PlayerCharacterClass = DefaultPlayerCharacterClass();
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
	Settings.bStartServerProcess = bStartServerProcess;
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
	Settings.BoundaryCenter = BoundaryCenter;
	Settings.BoundaryRadius = BoundaryRadius;
	Settings.BoundaryClampMargin = BoundaryClampMargin;
	return Settings;
}
