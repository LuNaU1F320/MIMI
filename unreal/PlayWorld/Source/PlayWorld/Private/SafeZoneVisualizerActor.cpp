#include "SafeZoneVisualizerActor.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"

ASafeZoneVisualizerActor::ASafeZoneVisualizerActor()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ASafeZoneVisualizerActor::BeginPlay()
{
	Super::BeginPlay();
}

void ASafeZoneVisualizerActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (Radius <= 0.0f || LineThickness <= 0.0f)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Origin(Center.X, Center.Y, DrawZOffset);

	if (bHighlightInterior && Radius > 0.0f)
	{
		for (float RingRadius = HighlightRingSpacing; RingRadius < Radius; RingRadius += HighlightRingSpacing)
		{
			DrawDebugCircle(
				World,
				Origin,
				RingRadius,
				96,
				HighlightColor,
				false,
				0.0f,
				0,
				HighlightLineThickness,
				FVector::ForwardVector,
				FVector::RightVector,
				false);
		}
	}

	DrawDebugCircle(
		World,
		Origin,
		Radius,
		128,
		Color,
		false,
		0.0f,
		0,
		LineThickness,
		FVector::ForwardVector,
		FVector::RightVector,
		false);
}

void ASafeZoneVisualizerActor::SetZone(const FVector2D& InCenter, float InRadius, const FColor& InColor, float InLineThickness, bool bInHighlightInterior)
{
	Center = InCenter;
	Radius = InRadius;
	Color = InColor;
	LineThickness = InLineThickness;
	bHighlightInterior = bInHighlightInterior;
	SetActorLocation(FVector(Center.X, Center.Y, 0.0f));
}
