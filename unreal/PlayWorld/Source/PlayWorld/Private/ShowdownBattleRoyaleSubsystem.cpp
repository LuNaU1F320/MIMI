#include "ShowdownBattleRoyaleSubsystem.h"

#include "BattleRoyaleMinimapWidget.h"
#include "CharacterEquipmentComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "MyCharacter.h"
#include "SafeZoneVisualizerActor.h"
#include "SupplyDropActor.h"
#include "ZoneDamageReceiverComponent.h"

void UShowdownBattleRoyaleSubsystem::Tick(float DeltaTime)
{
	if (!bBattleRoyaleActive)
	{
		return;
	}

	if (bWarmUpPhase)
	{
		WarmUpTimeRemaining -= DeltaTime;
		if (WarmUpTimeRemaining <= 0.0f)
		{
			bWarmUpPhase = false;
			CurrentPhaseIndex = 0;
			PhaseElapsedTime = 0.0f;
			GenerateInitialZones();
			CreateOrUpdateZoneVisualizers();
			SpawnSupplyForPhase();
			UE_LOG(LogTemp, Log, TEXT("Showdown battle royale warmup finished. Phase 1 active."));
		}
		return;
	}

	PhaseElapsedTime += DeltaTime;

	// If it is the final phase, shrink the safe zone radius gradually to 0
	if (CurrentPhaseIndex == Settings.PhaseCount - 1)
	{
		float Alpha = FMath::Clamp(PhaseElapsedTime / Settings.PhaseDuration, 0.0f, 1.0f);
		float StartRadius = GetPhaseRadius(CurrentPhaseIndex);
		CurrentZone.Radius = FMath::Lerp(StartRadius, 0.0f, Alpha);

		if (CurrentZoneVisualizer)
		{
			CurrentZoneVisualizer->SetZone(CurrentZone.Center, CurrentZone.Radius, FColor::Red, 12.0f);
		}
	}

	ApplyZoneDamage(DeltaTime);

	if (CurrentPhaseIndex < Settings.PhaseCount - 1)
	{
		if (PhaseElapsedTime >= Settings.PhaseDuration)
		{
			AdvancePhase();
		}
	}
}

TStatId UShowdownBattleRoyaleSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UShowdownBattleRoyaleSubsystem, STATGROUP_Tickables);
}

void UShowdownBattleRoyaleSubsystem::Deinitialize()
{
	ResetBattleRoyale();
	Super::Deinitialize();
}

void UShowdownBattleRoyaleSubsystem::ConfigureBattleRoyale(const FBattleRoyaleSettings& InSettings)
{
	Settings = InSettings;
	Settings.PhaseCount = FMath::Clamp(Settings.PhaseCount, 1, 4);
	Settings.PhaseDuration = FMath::Max(1.0f, Settings.PhaseDuration);
	Settings.MapExtent.X = FMath::Max(100.0f, Settings.MapExtent.X);
	Settings.MapExtent.Y = FMath::Max(100.0f, Settings.MapExtent.Y);
}

void UShowdownBattleRoyaleSubsystem::StartBattleRoyale()
{
	if (bBattleRoyaleActive)
	{
		return;
	}

	bBattleRoyaleActive = true;
	bWarmUpPhase = true;
	WarmUpTimeRemaining = 15.0f;
	CurrentPhaseIndex = 0;
	PhaseElapsedTime = 0.0f;
	ClearSupplies();

	// Set temporary giant safe zone during warmup to prevent visualization & damage
	CurrentZone.Center = FVector2D(Settings.MapCenter.X, Settings.MapCenter.Y);
	CurrentZone.Radius = 999999.0f;
	CurrentZone.DamagePerSecond = 0.0f;
	CurrentZone.PhaseIndex = -1;

	// Temporarily destroy or hide visualizers until phase 1 starts
	DestroyRuntimeVisuals();
	CreateMinimapWidget();

	UE_LOG(LogTemp, Log, TEXT("Showdown battle royale started (Warmup 15 seconds)."));
}

void UShowdownBattleRoyaleSubsystem::ResetBattleRoyale()
{
	if (!bBattleRoyaleActive && !CurrentZoneVisualizer && !NextZoneVisualizer && !MinimapWidget && ActiveSupplies.Num() == 0)
	{
		return;
	}

	bBattleRoyaleActive = false;
	bWarmUpPhase = false;
	WarmUpTimeRemaining = 0.0f;
	CurrentPhaseIndex = 0;
	PhaseElapsedTime = 0.0f;
	ClearSupplies();
	DestroyRuntimeVisuals();
}

void UShowdownBattleRoyaleSubsystem::RegisterPlayerCharacter(const FString& PlayerId, AMyCharacter* Character)
{
	if (!PlayerId.IsEmpty() && Character)
	{
		PlayerCharactersById.Add(PlayerId, Character);
	}
}

void UShowdownBattleRoyaleSubsystem::RegisterBotCharacter(const FString& BotId, AMyCharacter* Character)
{
	if (!BotId.IsEmpty() && Character)
	{
		BotCharactersById.Add(BotId, Character);
	}
}

bool UShowdownBattleRoyaleSubsystem::IsRegisteredPlayer(const AMyCharacter* Character) const
{
	for (const TPair<FString, TWeakObjectPtr<AMyCharacter>>& PlayerPair : PlayerCharactersById)
	{
		if (PlayerPair.Value.Get() == Character)
		{
			return true;
		}
	}

	return false;
}

bool UShowdownBattleRoyaleSubsystem::IsRegisteredBot(const AMyCharacter* Character) const
{
	for (const TPair<FString, TWeakObjectPtr<AMyCharacter>>& BotPair : BotCharactersById)
	{
		if (BotPair.Value.Get() == Character)
		{
			return true;
		}
	}

	return false;
}

void UShowdownBattleRoyaleSubsystem::TryPickupSupply(ASupplyDropActor* SupplyDrop, AMyCharacter* Character)
{
	if (!bBattleRoyaleActive || !SupplyDrop || !Character || !IsRegisteredPlayer(Character))
	{
		return;
	}

	ApplyRandomEquipmentEffect(Character);
	ActiveSupplies.Remove(SupplyDrop);
	SupplyDrop->Destroy();
}

void UShowdownBattleRoyaleSubsystem::GenerateInitialZones()
{
	CurrentZone = GenerateZoneForPhase(0, nullptr);
	NextZone = GenerateZoneForPhase(FMath::Min(1, Settings.PhaseCount - 1), &CurrentZone);
}

FSafeZoneState UShowdownBattleRoyaleSubsystem::GenerateZoneForPhase(int32 PhaseIndex, const FSafeZoneState* PreviousZone) const
{
	FSafeZoneState Zone;
	Zone.PhaseIndex = PhaseIndex;
	Zone.Radius = GetPhaseRadius(PhaseIndex);
	Zone.DamagePerSecond = GetPhaseDamage(PhaseIndex);

	if (!PreviousZone)
	{
		Zone.Center = FVector2D(Settings.MapCenter.X, Settings.MapCenter.Y);
		return Zone;
	}

	const float MaxOffset = FMath::Max(0.0f, PreviousZone->Radius - Zone.Radius);
	const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
	const float Distance = FMath::FRandRange(0.0f, MaxOffset);
	Zone.Center = PreviousZone->Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Distance;

	Zone.Center.X = FMath::Clamp(Zone.Center.X, Settings.MapCenter.X - Settings.MapExtent.X + Zone.Radius, Settings.MapCenter.X + Settings.MapExtent.X - Zone.Radius);
	Zone.Center.Y = FMath::Clamp(Zone.Center.Y, Settings.MapCenter.Y - Settings.MapExtent.Y + Zone.Radius, Settings.MapCenter.Y + Settings.MapExtent.Y - Zone.Radius);
	return Zone;
}

void UShowdownBattleRoyaleSubsystem::AdvancePhase()
{
	PhaseElapsedTime = 0.0f;

	if (CurrentPhaseIndex >= Settings.PhaseCount - 1)
	{
		return;
	}

	++CurrentPhaseIndex;
	CurrentZone = NextZone;
	CurrentZone.PhaseIndex = CurrentPhaseIndex;
	CurrentZone.DamagePerSecond = GetPhaseDamage(CurrentPhaseIndex);
	NextZone = GenerateZoneForPhase(FMath::Min(CurrentPhaseIndex + 1, Settings.PhaseCount - 1), &CurrentZone);
	CreateOrUpdateZoneVisualizers();
	SpawnSupplyForPhase();
	UE_LOG(LogTemp, Log, TEXT("Advanced safe zone phase to %d."), CurrentPhaseIndex + 1);
}

void UShowdownBattleRoyaleSubsystem::ApplyZoneDamage(float DeltaTime)
{
	auto ApplyToCharacter = [this, DeltaTime](const TWeakObjectPtr<AMyCharacter>& CharacterPtr)
	{
		AMyCharacter* Character = CharacterPtr.Get();
		if (!Character || !Character->IsAlive())
		{
			return;
		}

		const FVector Location = Character->GetActorLocation();
		const float DistanceSquared = FVector2D::DistSquared(FVector2D(Location.X, Location.Y), CurrentZone.Center);
		if (DistanceSquared <= FMath::Square(CurrentZone.Radius))
		{
			return;
		}

		if (UZoneDamageReceiverComponent* DamageReceiver = Character->GetZoneDamageReceiverComponent())
		{
			DamageReceiver->ApplyZoneDamage(CurrentZone.DamagePerSecond, DeltaTime);
		}
	};

	for (const TPair<FString, TWeakObjectPtr<AMyCharacter>>& PlayerPair : PlayerCharactersById)
	{
		ApplyToCharacter(PlayerPair.Value);
	}

	for (const TPair<FString, TWeakObjectPtr<AMyCharacter>>& BotPair : BotCharactersById)
	{
		ApplyToCharacter(BotPair.Value);
	}
}

void UShowdownBattleRoyaleSubsystem::SpawnSupplyForPhase()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ASupplyDropActor* Supply = World->SpawnActor<ASupplyDropActor>(ASupplyDropActor::StaticClass(), GetRandomMapLocation(), FRotator::ZeroRotator, SpawnParams);
	if (Supply)
	{
		ActiveSupplies.Add(Supply);
	}
}

void UShowdownBattleRoyaleSubsystem::ClearSupplies()
{
	for (const TWeakObjectPtr<ASupplyDropActor>& SupplyPtr : ActiveSupplies)
	{
		if (ASupplyDropActor* Supply = SupplyPtr.Get())
		{
			Supply->Destroy();
		}
	}

	ActiveSupplies.Reset();
}

void UShowdownBattleRoyaleSubsystem::CreateOrUpdateZoneVisualizers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!CurrentZoneVisualizer)
	{
		CurrentZoneVisualizer = World->SpawnActor<ASafeZoneVisualizerActor>();
	}
	if (!NextZoneVisualizer)
	{
		NextZoneVisualizer = World->SpawnActor<ASafeZoneVisualizerActor>();
	}

	if (CurrentZoneVisualizer)
	{
		CurrentZoneVisualizer->SetZone(CurrentZone.Center, CurrentZone.Radius, FColor::Green, 10.0f);
	}
	if (NextZoneVisualizer)
	{
		NextZoneVisualizer->SetZone(NextZone.Center, NextZone.Radius, FColor::Yellow, 4.0f);
	}
}

void UShowdownBattleRoyaleSubsystem::CreateMinimapWidget()
{
	if (MinimapWidget)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0);
	if (!PlayerController)
	{
		return;
	}

	MinimapWidget = CreateWidget<UBattleRoyaleMinimapWidget>(PlayerController, UBattleRoyaleMinimapWidget::StaticClass());
	if (MinimapWidget)
	{
		MinimapWidget->AddToViewport();
	}
}

void UShowdownBattleRoyaleSubsystem::DestroyRuntimeVisuals()
{
	if (CurrentZoneVisualizer)
	{
		CurrentZoneVisualizer->Destroy();
		CurrentZoneVisualizer = nullptr;
	}
	if (NextZoneVisualizer)
	{
		NextZoneVisualizer->Destroy();
		NextZoneVisualizer = nullptr;
	}
	if (MinimapWidget)
	{
		MinimapWidget->RemoveFromParent();
		MinimapWidget = nullptr;
	}
}

FVector UShowdownBattleRoyaleSubsystem::GetRandomMapLocation() const
{
	return Settings.MapCenter + FVector(
		FMath::FRandRange(-Settings.MapExtent.X, Settings.MapExtent.X),
		FMath::FRandRange(-Settings.MapExtent.Y, Settings.MapExtent.Y),
		80.0f);
}

float UShowdownBattleRoyaleSubsystem::GetInitialZoneRadius() const
{
	return FMath::Min(Settings.MapExtent.X, Settings.MapExtent.Y);
}

float UShowdownBattleRoyaleSubsystem::GetPhaseRadius(int32 PhaseIndex) const
{
	if (Settings.PhaseCount <= 1)
	{
		return GetInitialZoneRadius();
	}

	const float Alpha = static_cast<float>(PhaseIndex) / static_cast<float>(Settings.PhaseCount - 1);
	return FMath::Lerp(GetInitialZoneRadius(), GetInitialZoneRadius() * 0.25f, Alpha);
}

float UShowdownBattleRoyaleSubsystem::GetPhaseDamage(int32 PhaseIndex) const
{
	return DamageByPhase.IsValidIndex(PhaseIndex) ? DamageByPhase[PhaseIndex] : DamageByPhase.Last();
}

void UShowdownBattleRoyaleSubsystem::ApplyRandomEquipmentEffect(AMyCharacter* Character)
{
	if (!Character)
	{
		return;
	}

	UCharacterEquipmentComponent* Equipment = Character->GetEquipmentComponent();
	if (!Equipment)
	{
		return;
	}

	const int32 EffectIndex = FMath::RandRange(0, 2);
	Equipment->ApplyEffect(static_cast<EBattleRoyaleEquipmentEffect>(EffectIndex));
}
