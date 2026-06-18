#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ShowdownBattleRoyaleSubsystem.generated.h"

class AMyCharacter;
class ASafeZoneVisualizerActor;
class ASupplyDropActor;
class UBattleRoyaleMinimapWidget;
class UCharacterEquipmentComponent;

USTRUCT(BlueprintType)
struct PLAYWORLD_API FBattleRoyaleSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale")
	FVector MapCenter = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale")
	FVector2D MapExtent = FVector2D(3000.0f, 3000.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale", meta = (ClampMin = "1.0"))
	float PhaseDuration = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale", meta = (ClampMin = "1", ClampMax = "4"))
	int32 PhaseCount = 4;
};

USTRUCT(BlueprintType)
struct PLAYWORLD_API FSafeZoneState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "BattleRoyale")
	FVector2D Center = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "BattleRoyale")
	float Radius = 1000.0f;

	UPROPERTY(BlueprintReadOnly, Category = "BattleRoyale")
	int32 PhaseIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "BattleRoyale")
	float DamagePerSecond = 0.0f;
};

UCLASS()
class PLAYWORLD_API UShowdownBattleRoyaleSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return true; }
	virtual void Deinitialize() override;

	void ConfigureBattleRoyale(const FBattleRoyaleSettings& InSettings);
	void StartBattleRoyale();
	void ResetBattleRoyale();

	void RegisterPlayerCharacter(const FString& PlayerId, AMyCharacter* Character);
	void RegisterBotCharacter(const FString& BotId, AMyCharacter* Character);

	bool IsRegisteredPlayer(const AMyCharacter* Character) const;
	bool IsRegisteredBot(const AMyCharacter* Character) const;

	const TMap<FString, TWeakObjectPtr<AMyCharacter>>& GetPlayers() const { return PlayerCharactersById; }
	const TMap<FString, TWeakObjectPtr<AMyCharacter>>& GetBots() const { return BotCharactersById; }
	const FSafeZoneState& GetCurrentZone() const { return CurrentZone; }
	const FSafeZoneState& GetNextZone() const { return NextZone; }
	const TArray<TWeakObjectPtr<ASupplyDropActor>>& GetActiveSupplies() const { return ActiveSupplies; }
	const FBattleRoyaleSettings& GetSettings() const { return Settings; }
	bool IsBattleRoyaleActive() const { return bBattleRoyaleActive; }
	bool IsBattleRoyaleCompleted() const { return bBattleRoyaleCompleted; }
	bool IsWarmUpPhase() const { return bWarmUpPhase; }
	int32 GetCurrentPhaseIndex() const { return CurrentPhaseIndex; }
	float GetPhaseElapsedTime() const { return PhaseElapsedTime; }
	FVector2D GetCameraViewCenter() const;
	float GetCameraViewRadius() const;
	FVector2D GetCameraViewExtent() const;
	AMyCharacter* GetWinnerCharacter() const { return WinnerCharacter.Get(); }

	void TryPickupSupply(ASupplyDropActor* SupplyDrop, AMyCharacter* Character);

private:
	FSafeZoneState GetMapZone() const;

	FBattleRoyaleSettings Settings;
	bool bBattleRoyaleActive = false;
	bool bBattleRoyaleCompleted = false;
	bool bWarmUpPhase = false;
	float WarmUpTimeRemaining = 0.0f;
	int32 CurrentPhaseIndex = 0;
	float PhaseElapsedTime = 0.0f;
	FSafeZoneState PreviousZone;
	FSafeZoneState CurrentZone;
	FSafeZoneState NextZone;

	TMap<FString, TWeakObjectPtr<AMyCharacter>> PlayerCharactersById;
	TMap<FString, TWeakObjectPtr<AMyCharacter>> BotCharactersById;
	TArray<TWeakObjectPtr<ASupplyDropActor>> ActiveSupplies;
	TWeakObjectPtr<AMyCharacter> WinnerCharacter;

	UPROPERTY()
	TObjectPtr<ASafeZoneVisualizerActor> CurrentZoneVisualizer;

	UPROPERTY()
	TObjectPtr<ASafeZoneVisualizerActor> NextZoneVisualizer;

	UPROPERTY()
	TObjectPtr<UBattleRoyaleMinimapWidget> MinimapWidget;

	TArray<float> DamageByPhase = { 3.0f, 6.0f, 10.0f, 15.0f };

	void GenerateInitialZones();
	FSafeZoneState GenerateZoneForPhase(int32 PhaseIndex, const FSafeZoneState* InPreviousZone) const;
	void AdvancePhase();
	void ApplyZoneDamage(float DeltaTime);
	void SpawnSupplyForPhase();
	void ClearSupplies();
	void CreateOrUpdateZoneVisualizers();
	void CreateMinimapWidget();
	void DestroyRuntimeVisuals();
	FVector GetRandomMapLocation() const;
	float GetInitialZoneRadius() const;
	float GetPhaseRadius(int32 PhaseIndex) const;
	float GetPhaseDamage(int32 PhaseIndex) const;
	void UpdateWinnerState();
	void CompleteBattleRoyale(AMyCharacter* Winner);
	void ApplyRandomEquipmentEffect(AMyCharacter* Character);
};
