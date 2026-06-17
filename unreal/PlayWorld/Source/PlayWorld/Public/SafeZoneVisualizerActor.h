#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SafeZoneVisualizerActor.generated.h"

UCLASS()
class PLAYWORLD_API ASafeZoneVisualizerActor : public AActor
{
	GENERATED_BODY()

public:
	ASafeZoneVisualizerActor();

	virtual void Tick(float DeltaTime) override;

	void SetZone(const FVector2D& InCenter, float InRadius, const FColor& InColor, float InLineThickness);

protected:
	virtual void BeginPlay() override;

private:
	FVector2D Center = FVector2D::ZeroVector;
	float Radius = 1000.0f;
	FColor Color = FColor::Green;
	float LineThickness = 8.0f;
};
