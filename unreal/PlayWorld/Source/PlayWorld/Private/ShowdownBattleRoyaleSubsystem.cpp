#include "ShowdownBattleRoyaleSubsystem.h"

#include "BattleRoyaleMinimapWidget.h"
#include "CharacterEquipmentComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
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

	if (bBattleRoyaleCompleted)
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
			PreviousZone = GetMapZone();
			CurrentZone = PreviousZone;
			CreateOrUpdateZoneVisualizers();
			SpawnSupplyForPhase();
			UE_LOG(LogTemp, Log, TEXT("Showdown battle royale warmup finished. Phase 1 active."));
		}
		return;
	}

	PhaseElapsedTime += DeltaTime;

	float WaitDuration = FMath::Max(0.0f, Settings.PhaseDuration - 5.0f);
	float ShrinkDuration = FMath::Min(5.0f, Settings.PhaseDuration);

	if (CurrentPhaseIndex == Settings.PhaseCount - 1)
	{
		// Final phase: shrink to 0.0f
		if (PhaseElapsedTime < WaitDuration)
		{
			CurrentZone = PreviousZone;
		}
		else
		{
			float Alpha = FMath::Clamp((PhaseElapsedTime - WaitDuration) / ShrinkDuration, 0.0f, 1.0f);
			CurrentZone.Center = PreviousZone.Center;
			CurrentZone.Radius = FMath::Lerp(PreviousZone.Radius, 0.0f, Alpha);
		}

		if (CurrentZoneVisualizer)
		{
			CurrentZoneVisualizer->SetZone(CurrentZone.Center, CurrentZone.Radius, FColor::Red, 12.0f, true);
		}
	}
	else
	{
		// Normal phases: shrink from PreviousZone to NextZone
		if (PhaseElapsedTime < WaitDuration)
		{
			CurrentZone = PreviousZone;
		}
		else
		{
			float Alpha = FMath::Clamp((PhaseElapsedTime - WaitDuration) / ShrinkDuration, 0.0f, 1.0f);
			CurrentZone.Center = FMath::Lerp(PreviousZone.Center, NextZone.Center, Alpha);
			CurrentZone.Radius = FMath::Lerp(PreviousZone.Radius, NextZone.Radius, Alpha);
		}

		if (CurrentZoneVisualizer)
		{
			CurrentZoneVisualizer->SetZone(CurrentZone.Center, CurrentZone.Radius, FColor::Green, 10.0f, true);
		}
	}

	ApplyZoneDamage(DeltaTime);
	UpdateWinnerState();

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
	if (bBattleRoyaleActive && !bBattleRoyaleCompleted)
	{
		return;
	}

	if (bBattleRoyaleCompleted)
	{
		ResetBattleRoyale();
	}

	bBattleRoyaleActive = true;
	bBattleRoyaleCompleted = false;
	bWarmUpPhase = true;
	WarmUpTimeRemaining = 15.0f;
	CurrentPhaseIndex = 0;
	PhaseElapsedTime = 0.0f;
	WinnerCharacter.Reset();
	ClearSupplies();
	GenerateInitialZones();
	DestroyRuntimeVisuals();
	CreateOrUpdateZoneVisualizers();
	CreateMinimapWidget();
	SetAutoAttackForRegisteredCharacters(true);

	UE_LOG(LogTemp, Log, TEXT("Showdown battle royale started (Warmup 15 seconds, first safe zone visible)."));
}

void UShowdownBattleRoyaleSubsystem::ResetBattleRoyale()
{
	SetAutoAttackForRegisteredCharacters(false);

	if (!bBattleRoyaleActive && !CurrentZoneVisualizer && !NextZoneVisualizer && !MinimapWidget && ActiveSupplies.Num() == 0)
	{
		return;
	}

	bBattleRoyaleActive = false;
	bBattleRoyaleCompleted = false;
	bWarmUpPhase = false;
	WarmUpTimeRemaining = 0.0f;
	CurrentPhaseIndex = 0;
	PhaseElapsedTime = 0.0f;
	WinnerCharacter.Reset();
	ClearSupplies();
	DestroyRuntimeVisuals();
}

void UShowdownBattleRoyaleSubsystem::RegisterPlayerCharacter(const FString& PlayerId, AMyCharacter* Character)
{
	if (!PlayerId.IsEmpty() && Character)
	{
		PlayerCharactersById.Add(PlayerId, Character);
		Character->SetAutoAttackEnabled(bBattleRoyaleActive && !bBattleRoyaleCompleted);
	}
}

void UShowdownBattleRoyaleSubsystem::RegisterBotCharacter(const FString& BotId, AMyCharacter* Character)
{
	if (!BotId.IsEmpty() && Character)
	{
		BotCharactersById.Add(BotId, Character);
		Character->SetAutoAttackEnabled(bBattleRoyaleActive && !bBattleRoyaleCompleted);
	}
}

void UShowdownBattleRoyaleSubsystem::SetAutoAttackForRegisteredCharacters(bool bEnabled)
{
	for (const TPair<FString, TWeakObjectPtr<AMyCharacter>>& PlayerPair : PlayerCharactersById)
	{
		if (AMyCharacter* Character = PlayerPair.Value.Get())
		{
			Character->SetAutoAttackEnabled(bEnabled);
		}
	}

	for (const TPair<FString, TWeakObjectPtr<AMyCharacter>>& BotPair : BotCharactersById)
	{
		if (AMyCharacter* Character = BotPair.Value.Get())
		{
			Character->SetAutoAttackEnabled(bEnabled);
		}
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

FVector2D UShowdownBattleRoyaleSubsystem::GetCameraViewCenter() const
{
	if (bBattleRoyaleActive && (!bWarmUpPhase || bBattleRoyaleCompleted))
	{
		return CurrentZone.Center;
	}

	return FVector2D(Settings.MapCenter.X, Settings.MapCenter.Y);
}

float UShowdownBattleRoyaleSubsystem::GetCameraViewRadius() const
{
	if (bBattleRoyaleActive && (!bWarmUpPhase || bBattleRoyaleCompleted))
	{
		return CurrentZone.Radius;
	}

	return FVector2D(Settings.MapExtent.X, Settings.MapExtent.Y).Size();
}

FVector2D UShowdownBattleRoyaleSubsystem::GetCameraViewExtent() const
{
	if (bBattleRoyaleActive && (!bWarmUpPhase || bBattleRoyaleCompleted))
	{
		return FVector2D(CurrentZone.Radius, CurrentZone.Radius);
	}

	return Settings.MapExtent;
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
	PreviousZone = GetMapZone();
	CurrentZone = PreviousZone;
	NextZone = GenerateZoneForPhase(0, &CurrentZone);
}

FSafeZoneState UShowdownBattleRoyaleSubsystem::GenerateZoneForPhase(int32 PhaseIndex, const FSafeZoneState* InPreviousZone) const
{
	FSafeZoneState Zone;
	Zone.PhaseIndex = PhaseIndex;
	Zone.Radius = GetPhaseRadius(PhaseIndex);
	Zone.DamagePerSecond = GetPhaseDamage(PhaseIndex);

	if (!InPreviousZone)
	{
		Zone.Center = FVector2D(Settings.MapCenter.X, Settings.MapCenter.Y);
		return Zone;
	}

	const float MaxOffset = FMath::Max(0.0f, InPreviousZone->Radius - Zone.Radius);
	const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
	const float Distance = FMath::FRandRange(0.0f, MaxOffset);
	Zone.Center = InPreviousZone->Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Distance;

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

	PreviousZone = NextZone;
	++CurrentPhaseIndex;

	CurrentZone = PreviousZone;
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
		if (bWarmUpPhase)
		{
			CurrentZoneVisualizer->SetZone(CurrentZone.Center, 0.0f, FColor::Green, 0.0f, false);
		}
		else if (CurrentPhaseIndex == Settings.PhaseCount - 1)
		{
			CurrentZoneVisualizer->SetZone(CurrentZone.Center, CurrentZone.Radius, FColor::Red, 12.0f, true);
		}
		else
		{
			CurrentZoneVisualizer->SetZone(CurrentZone.Center, CurrentZone.Radius, FColor::Green, 10.0f, true);
		}
	}

	if (NextZoneVisualizer)
	{
		if (bWarmUpPhase)
		{
			NextZoneVisualizer->SetZone(NextZone.Center, NextZone.Radius, FColor::Yellow, 4.0f, false);
		}
		else if (CurrentPhaseIndex == Settings.PhaseCount - 1)
		{
			NextZoneVisualizer->SetZone(NextZone.Center, 0.0f, FColor::Yellow, 0.0f, false);
		}
		else
		{
			NextZoneVisualizer->SetZone(NextZone.Center, NextZone.Radius, FColor::Yellow, 4.0f, false);
		}
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

void UShowdownBattleRoyaleSubsystem::UpdateWinnerState()
{
	if (!bBattleRoyaleActive || bWarmUpPhase || bBattleRoyaleCompleted)
	{
		return;
	}

	AMyCharacter* LastAliveCharacter = nullptr;
	int32 AliveCount = 0;
	auto CountAliveCharacter = [&AliveCount, &LastAliveCharacter](const TWeakObjectPtr<AMyCharacter>& CharacterPtr)
	{
		AMyCharacter* Character = CharacterPtr.Get();
		if (Character && Character->IsAlive())
		{
			++AliveCount;
			LastAliveCharacter = Character;
		}
	};

	for (const TPair<FString, TWeakObjectPtr<AMyCharacter>>& PlayerPair : PlayerCharactersById)
	{
		CountAliveCharacter(PlayerPair.Value);
	}

	for (const TPair<FString, TWeakObjectPtr<AMyCharacter>>& BotPair : BotCharactersById)
	{
		CountAliveCharacter(BotPair.Value);
	}

	if (AliveCount == 1)
	{
		CompleteBattleRoyale(LastAliveCharacter);
	}
}

void UShowdownBattleRoyaleSubsystem::CompleteBattleRoyale(AMyCharacter* Winner)
{
	bBattleRoyaleCompleted = true;
	bWarmUpPhase = false;
	WinnerCharacter = Winner;

	if (Winner)
	{
		Winner->SetExternalMovementEnabled(true);
		UE_LOG(LogTemp, Log, TEXT("Showdown battle royale completed. Winner remains controllable: %s."), *Winner->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("Showdown battle royale completed without a winner."));
	}
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

FSafeZoneState UShowdownBattleRoyaleSubsystem::GetMapZone() const
{
	FSafeZoneState MapZone;
	MapZone.Center = FVector2D(Settings.MapCenter.X, Settings.MapCenter.Y);
	MapZone.Radius = FVector2D(Settings.MapExtent.X, Settings.MapExtent.Y).Size();
	MapZone.PhaseIndex = -1;
	MapZone.DamagePerSecond = 0.0f;
	return MapZone;
}
