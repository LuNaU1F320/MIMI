#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HttpFwd.h"
#include "IWebSocket.h"
#include "ShowdownBattleRoyaleSubsystem.h"
#include "TimerManager.h"
#include "ControllerInputPollingBridge.generated.h"

class AMyCharacter;

UCLASS()
class PLAYWORLD_API AControllerInputPollingBridge : public AActor
{
	GENERATED_BODY()

public:
	AControllerInputPollingBridge();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale", meta = (ClampMin = "0.1"))
	float StatusPollingInterval = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sync", meta = (ClampMin = "0.05"))
	float WorldStateSyncInterval = 0.1f;

private:
	FTimerHandle PollingTimerHandle;
	FTimerHandle StatusPollingTimerHandle;
	FTimerHandle WorldStateSyncTimerHandle;
	bool bRequestInFlight = false;
	bool bStatusRequestInFlight = false;
	bool bWorldStateRequestInFlight = false;
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
	TSharedPtr<IWebSocket> UnrealWebSocket;
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
	void StopDemoCharacters();
	AMyCharacter* FindExistingCharacter() const;
	AMyCharacter* SpawnCharacterAt(const FVector& Location, const FRotator& Rotation, const TCHAR* NamePrefix) const;
	AMyCharacter* GetOrCreatePlayerCharacter(const FString& PlayerId);
	AMyCharacter* GetOrCreateBotCharacter(const FString& BotId);
	FVector GetPlayerSpawnLocation(int32 PlayerIndex) const;
	void SpawnStaticBots();
	FVector GetRandomBotSpawnLocation(FRandomStream& RandomStream, const TArray<FVector>& ExistingLocations) const;
	bool IsFarEnoughFromExistingBots(const FVector& CandidateLocation, const TArray<FVector>& ExistingLocations) const;
	UShowdownBattleRoyaleSubsystem* GetBattleRoyaleSubsystem() const;
};
