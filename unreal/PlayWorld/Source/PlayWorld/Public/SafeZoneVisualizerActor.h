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

	void SetZone(const FVector2D& InCenter, float InRadius, const FColor& InColor, float InLineThickness, bool bInHighlightInterior = true);

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale|Zone", meta = (AllowPrivateAccess = "true"))
	FVector2D Center = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale|Zone", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float Radius = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale|Zone", meta = (AllowPrivateAccess = "true"))
	FColor Color = FColor::Green;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale|Zone", meta = (ClampMin = "0.1", AllowPrivateAccess = "true"))
	float LineThickness = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale|Highlight", meta = (AllowPrivateAccess = "true"))
	bool bHighlightInterior = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale|Highlight", meta = (AllowPrivateAccess = "true"))
	FColor HighlightColor = FColor(0, 180, 80, 80);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale|Highlight", meta = (ClampMin = "50.0", AllowPrivateAccess = "true"))
	float HighlightRingSpacing = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale|Highlight", meta = (ClampMin = "0.1", AllowPrivateAccess = "true"))
	float HighlightLineThickness = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale|Highlight", meta = (AllowPrivateAccess = "true"))
	float DrawZOffset = 12.0f;
};
