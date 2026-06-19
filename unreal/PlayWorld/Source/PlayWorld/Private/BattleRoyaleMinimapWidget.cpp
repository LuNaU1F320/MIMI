#include "BattleRoyaleMinimapWidget.h"

#include "MyCharacter.h"
#include "Rendering/DrawElements.h"
#include "ShowdownBattleRoyaleSubsystem.h"
#include "SupplyDropActor.h"

int32 UBattleRoyaleMinimapWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const FVector2D WidgetSize = AllottedGeometry.GetLocalSize();
	UWorld* World = GetWorld();
	const UShowdownBattleRoyaleSubsystem* Subsystem = World ? World->GetSubsystem<UShowdownBattleRoyaleSubsystem>() : nullptr;
	if (!Subsystem || !Subsystem->IsBattleRoyaleActive())
	{
		return BaseLayer;
	}

	const FPaintGeometry PaintGeometry = AllottedGeometry.ToPaintGeometry();
	const FVector2D TopLeft(12.0f, 12.0f);
	const FVector2D Size(220.0f, 220.0f);
	const FVector2D BottomRight = TopLeft + Size;
	const FVector2D Center = TopLeft + Size * 0.5f;

	TArray<FVector2D> BorderPoints = {
		TopLeft,
		FVector2D(BottomRight.X, TopLeft.Y),
		BottomRight,
		FVector2D(TopLeft.X, BottomRight.Y),
		TopLeft
	};
	FSlateDrawElement::MakeLines(OutDrawElements, BaseLayer + 1, PaintGeometry, BorderPoints, ESlateDrawEffect::None, FLinearColor::White, true, 2.0f);

	const FBattleRoyaleSettings& Settings = Subsystem->GetSettings();
	const float Scale = FMath::Min(Size.X / (Settings.MapExtent.X * 2.0f), Size.Y / (Settings.MapExtent.Y * 2.0f));

	// Draw Next (Yellow) Zone if we are not in the final phase
	if (!Subsystem->IsWarmUpPhase() && Subsystem->GetCurrentPhaseIndex() == Settings.PhaseCount - 1)
	{
		// Hide Next Zone in final phase
	}
	else
	{
		const FVector2D NextZoneCenter = WorldToMinimap(Subsystem->GetNextZone().Center, Size) + TopLeft;
		DrawCircle(OutDrawElements, BaseLayer + 2, AllottedGeometry, NextZoneCenter, Subsystem->GetNextZone().Radius * Scale, FLinearColor(1.0f, 0.9f, 0.1f, 0.65f), 2.0f);
	}

	// Draw Current (Green/Red) Zone if we are not in warmup phase
	if (!Subsystem->IsWarmUpPhase())
	{
		const FVector2D CurrentZoneCenter = WorldToMinimap(Subsystem->GetCurrentZone().Center, Size) + TopLeft;
		FLinearColor ZoneColor = (Subsystem->GetCurrentPhaseIndex() == Settings.PhaseCount - 1) ? FLinearColor(1.0f, 0.1f, 0.1f, 0.85f) : FLinearColor(0.1f, 1.0f, 0.2f, 0.85f);
		DrawCircle(OutDrawElements, BaseLayer + 3, AllottedGeometry, CurrentZoneCenter, Subsystem->GetCurrentZone().Radius * Scale, ZoneColor, 3.0f);
	}

	auto DrawPoint = [&](const FVector2D& WorldLocation, const FLinearColor& Color, float HalfSize)
	{
		const FVector2D Point = WorldToMinimap(WorldLocation, Size) + TopLeft;
		TArray<FVector2D> Points = {
			Point + FVector2D(-HalfSize, -HalfSize),
			Point + FVector2D(HalfSize, -HalfSize),
			Point + FVector2D(HalfSize, HalfSize),
			Point + FVector2D(-HalfSize, HalfSize),
			Point + FVector2D(-HalfSize, -HalfSize)
		};
		FSlateDrawElement::MakeLines(OutDrawElements, BaseLayer + 4, PaintGeometry, Points, ESlateDrawEffect::None, Color, true, 2.0f);
	};

	for (const TPair<FString, TWeakObjectPtr<AMyCharacter>>& PlayerPair : Subsystem->GetPlayers())
	{
		if (const AMyCharacter* Character = PlayerPair.Value.Get())
		{
			if (!Character->IsAlive())
			{
				continue;
			}

			const FVector Location = Character->GetActorLocation();
			DrawPoint(FVector2D(Location.X, Location.Y), FLinearColor::Green, 4.0f);
		}
	}

	for (const TPair<FString, TWeakObjectPtr<AMyCharacter>>& BotPair : Subsystem->GetBots())
	{
		if (const AMyCharacter* Character = BotPair.Value.Get())
		{
			if (!Character->IsAlive())
			{
				continue;
			}

			const FVector Location = Character->GetActorLocation();
			DrawPoint(FVector2D(Location.X, Location.Y), FLinearColor::Red, 3.0f);
		}
	}

	for (const TWeakObjectPtr<ASupplyDropActor>& SupplyPtr : Subsystem->GetActiveSupplies())
	{
		if (const ASupplyDropActor* Supply = SupplyPtr.Get())
		{
			DrawPoint(Supply->GetMapLocation2D(), FLinearColor(0.1f, 0.6f, 1.0f, 1.0f), 5.0f);
		}
	}

	return BaseLayer + 5;
}

FVector2D UBattleRoyaleMinimapWidget::WorldToMinimap(const FVector2D& WorldLocation, const FVector2D& WidgetSize) const
{
	const UWorld* World = GetWorld();
	const UShowdownBattleRoyaleSubsystem* Subsystem = World ? World->GetSubsystem<UShowdownBattleRoyaleSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return WidgetSize * 0.5f;
	}

	const FBattleRoyaleSettings& Settings = Subsystem->GetSettings();
	const FVector2D Center(Settings.MapCenter.X, Settings.MapCenter.Y);
	const FVector2D Normalized(
		(WorldLocation.X - Center.X + Settings.MapExtent.X) / (Settings.MapExtent.X * 2.0f),
		(WorldLocation.Y - Center.Y + Settings.MapExtent.Y) / (Settings.MapExtent.Y * 2.0f));

	return FVector2D(
		FMath::Clamp(Normalized.X, 0.0f, 1.0f) * WidgetSize.X,
		FMath::Clamp(Normalized.Y, 0.0f, 1.0f) * WidgetSize.Y);
}

void UBattleRoyaleMinimapWidget::DrawCircle(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& Geometry, const FVector2D& Center, float Radius, const FLinearColor& Color, float Thickness) const
{
	TArray<FVector2D> Points;
	const int32 Segments = 64;
	Points.Reserve(Segments + 1);
	for (int32 Index = 0; Index <= Segments; ++Index)
	{
		const float Angle = (2.0f * PI * Index) / Segments;
		Points.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
	}

	FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, Thickness);
}
