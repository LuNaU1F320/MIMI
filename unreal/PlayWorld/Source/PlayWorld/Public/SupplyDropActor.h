#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SupplyDropActor.generated.h"

class AMyCharacter;
class USphereComponent;
class UStaticMeshComponent;

UCLASS()
class PLAYWORLD_API ASupplyDropActor : public AActor
{
	GENERATED_BODY()

public:
	ASupplyDropActor();

	FVector2D GetMapLocation2D() const;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Supply")
	TObjectPtr<UStaticMeshComponent> SupplyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Supply")
	TObjectPtr<USphereComponent> PickupSphere;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Supply", meta = (ClampMin = "0.0"))
	float CaptureTime = 0.0f;

private:
	UFUNCTION()
	void HandlePickupOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
