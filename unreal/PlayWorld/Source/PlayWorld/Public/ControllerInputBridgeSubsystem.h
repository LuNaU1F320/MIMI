#pragma once

#include "CoreMinimal.h"
#include "HttpFwd.h"
#include "ShowdownBattleRoyaleSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"
#include "HAL/PlatformProcess.h"
#include "ControllerInputBridgeSubsystem.generated.h"

class ABattleRoyaleZoneCameraActor;
class AMyCharacter;
class IWebSocket;

USTRUCT(BlueprintType)
struct PLAYWORLD_API FControllerInputBridgeSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demo")
	TSubclassOf<AMyCharacter> PlayerCharacterClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demo")
	TObjectPtr<AMyCharacter> ControlledCharacter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demo", meta = (ClampMin = "1", ClampMax = "4"))
	int32 MaxDemoPlayers = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demo", meta = (ClampMin = "0"))
	int32 MaxDemoBots = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demo")
	FVector2D PlayerSpawnSpacing = FVector2D(250.0f, 250.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demo")
	FString ServerBaseUrl = TEXT("http://localhost:3000");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demo")
	bool bStartServerProcess = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demo", meta = (ClampMin = "0.05"))
	float PollingInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demo")
	bool bLogPollingDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demo", meta = (ClampMin = "0"))
	int32 BotCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demo")
	FVector BotSpawnCenter = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demo")
	FVector2D BotSpawnAreaExtent = FVector2D(1200.0f, 1200.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demo", meta = (ClampMin = "0.0"))
	float MinBotSpawnDistance = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demo")
	FVector PlayerSpawnLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale")
	FBattleRoyaleSettings BattleRoyaleSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale|Camera")
	TSubclassOf<ABattleRoyaleZoneCameraActor> ZoneCameraClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale|Camera")
	TObjectPtr<ABattleRoyaleZoneCameraActor> ZoneCameraActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale|Camera")
	bool bAutoCreateZoneCamera = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale|Camera")
	bool bAutoActivateZoneCamera = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale", meta = (ClampMin = "0.1"))
	float StatusPollingInterval = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sync", meta = (ClampMin = "0.05"))
	float WorldStateSyncInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale|Boundary")
	FVector BoundaryCenter = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale|Boundary", meta = (ClampMin = "100.0"))
	float BoundaryRadius = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale|Boundary", meta = (ClampMin = "0.0"))
	float BoundaryClampMargin = 0.0f;
};

UCLASS()
class PLAYWORLD_API UControllerInputBridgeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UControllerInputBridgeSubsystem();

	virtual void Deinitialize() override;

	void ConfigureAndStart(const FControllerInputBridgeSettings& InSettings);
	void StopBridge();

private:
	UPROPERTY()
	TSubclassOf<AMyCharacter> PlayerCharacterClass;

	UPROPERTY()
	TObjectPtr<AMyCharacter> ControlledCharacter;

	int32 MaxDemoPlayers = 4;
	int32 MaxDemoBots = 20;
	FVector2D PlayerSpawnSpacing = FVector2D(250.0f, 250.0f);
	FString ServerBaseUrl = TEXT("http://localhost:3000");
	bool bStartServerProcess = true;
	float PollingInterval = 0.1f;
	bool bLogPollingDebug = true;
	int32 BotCount = 0;
	FVector BotSpawnCenter = FVector::ZeroVector;
	FVector2D BotSpawnAreaExtent = FVector2D(1200.0f, 1200.0f);
	float MinBotSpawnDistance = 250.0f;
	FVector PlayerSpawnLocation = FVector::ZeroVector;
	FBattleRoyaleSettings BattleRoyaleSettings;

	UPROPERTY()
	TSubclassOf<ABattleRoyaleZoneCameraActor> ZoneCameraClass;

	UPROPERTY()
	TObjectPtr<ABattleRoyaleZoneCameraActor> ZoneCameraActor;

	bool bAutoCreateZoneCamera = true;
	bool bAutoActivateZoneCamera = true;
	float StatusPollingInterval = 0.5f;
	float WorldStateSyncInterval = 0.1f;

	FVector BoundaryCenter = FVector::ZeroVector;
	float BoundaryRadius = 3000.0f;
	float BoundaryClampMargin = 0.0f;

	FTimerHandle PollingTimerHandle;
	FTimerHandle StatusPollingTimerHandle;
	FTimerHandle WorldStateSyncTimerHandle;
	FTimerHandle WebSocketReconnectTimerHandle;
	bool bRequestInFlight = false;
	bool bStatusRequestInFlight = false;
	bool bWorldStateRequestInFlight = false;
	bool bIsStopping = false;
	FProcHandle ServerProcessHandle;
	uint32 ServerProcessId = 0;
	float LastDebugLogTime = -1000.0f;

	UPROPERTY()
	TArray<TObjectPtr<AMyCharacter>> SpawnedBots;

	UPROPERTY()
	TArray<TObjectPtr<AMyCharacter>> DemoPlayerCharacters;

	UPROPERTY()
	TArray<TObjectPtr<AMyCharacter>> DemoBotCharacters;

	TMap<FString, TObjectPtr<AMyCharacter>> PlayerCharactersById;
	TMap<FString, TObjectPtr<AMyCharacter>> BotCharactersById;
	TMap<FString, FVector2D> PlayerInitialPercentageMap;
	TMap<FString, int64> LastEmoteSeqByPlayerId;
	TSharedPtr<IWebSocket> UnrealWebSocket;

	void ApplySettings(const FControllerInputBridgeSettings& InSettings);
	void HandleWebSocketMessage(const FString& MessageString);
	void InitializeDemoCharacters();
	void PollInputs();
	void PollStatus();
	void SendWorldState();
	void HandleInputResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void HandleStatusResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void HandleWorldStateResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void ApplyMoveInput(const FString& PlayerId, float MoveX, float MoveY);
	void ApplyBotMoveInput(const FString& BotId, float MoveX, float MoveY);
	void ApplyEmoteInput(const FString& PlayerId, int64 EmoteSeq);
	void StopDemoCharacters();
	bool ShouldPreserveWinnerControl(const AMyCharacter* Character) const;
	AMyCharacter* FindExistingCharacter() const;
	AMyCharacter* SpawnCharacterAt(const FVector& Location, const FRotator& Rotation, const TCHAR* NamePrefix) const;
	FVector ResolveCharacterSpawnLocation(const FVector& Location) const;
	AMyCharacter* GetOrCreatePlayerCharacter(const FString& PlayerId);
	AMyCharacter* GetOrCreateBotCharacter(const FString& BotId);
	FVector GetPlayerSpawnLocation(int32 PlayerIndex) const;
	void SpawnStaticBots();
	FVector GetRandomBotSpawnLocation(FRandomStream& RandomStream, const TArray<FVector>& ExistingLocations) const;
	bool IsFarEnoughFromExistingBots(const FVector& CandidateLocation, const TArray<FVector>& ExistingLocations) const;
	UShowdownBattleRoyaleSubsystem* GetBattleRoyaleSubsystem() const;
	void EnsureBattleRoyaleZoneCamera();

	FVector ClampLocationToMapCircle(const FVector& Location, float Margin = 0.0f) const;
	FVector GetBoundaryCenter() const;
	float GetMapRadius() const;

	void StartServerProcess();
	void StopServerProcess();
	bool TerminateServerProcessTree(uint32 ProcessId) const;
	void StopStaleServerProcess(const FString& ServerDir) const;
	void ConnectWebSocket();
	void RetryConnectWebSocket();
};
