#pragma once

#include "CoreMinimal.h"
#include "ControllerInputBridgeSubsystem.h"
#include "GameFramework/Actor.h"
#include "ControllerInputPollingBridge.generated.h"

class ABattleRoyaleZoneCameraActor;
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

private:
	FControllerInputBridgeSettings MakeBridgeSettings() const;
};
