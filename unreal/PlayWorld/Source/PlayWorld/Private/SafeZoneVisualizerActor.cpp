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

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Origin(Center.X, Center.Y, 12.0f);
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

void ASafeZoneVisualizerActor::SetZone(const FVector2D& InCenter, float InRadius, const FColor& InColor, float InLineThickness)
{
	Center = InCenter;
	Radius = InRadius;
	Color = InColor;
	LineThickness = InLineThickness;
	SetActorLocation(FVector(Center.X, Center.Y, 0.0f));
}
