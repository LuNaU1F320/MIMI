#pragma once

#include "Camera/CameraComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BattleRoyaleZoneCameraActor.generated.h"

class USceneComponent;

UCLASS()
class PLAYWORLD_API ABattleRoyaleZoneCameraActor : public AActor
{
	GENERATED_BODY()

public:
	ABattleRoyaleZoneCameraActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	void SetAutoActivateForPlayer0(bool bInAutoActivate);
	void ActivateForPlayer0();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BattleRoyale|Camera")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BattleRoyale|Camera")
	TObjectPtr<UCameraComponent> CameraComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale|Camera")
	bool bAutoActivateForPlayer0 = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale|Camera")
	TEnumAsByte<ECameraProjectionMode::Type> ProjectionMode = ECameraProjectionMode::Perspective;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale|Camera", meta = (ClampMin = "5.0", ClampMax = "170.0"))
	float PerspectiveFieldOfView = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale|Camera")
	FRotator QuarterViewRotation = FRotator(-50.473487f, -0.166535f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale|Camera")
	bool bAlignMapToScreenAxes = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale|Camera")
	bool bFitPitchToViewportAspect = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale|Camera", meta = (ClampMin = "100.0"))
	float CameraDistance = 45000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale|Camera", meta = (ClampMin = "0.0"))
	float OrthoPadding = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale|Camera", meta = (ClampMin = "0.1"))
	float ViewportFillAmount = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale|Camera")
	bool bCoverViewportWithMap = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale|Camera")
	bool bUseInitialViewExtentOverride = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale|Camera", meta = (ClampMin = "100.0"))
	FVector2D InitialViewExtentOverride = FVector2D(3000.0f, 3000.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale|Camera", meta = (ClampMin = "100.0"))
	float MinOrthoWidth = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale|Camera", meta = (ClampMin = "100.0"))
	float MaxOrthoWidth = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale|Camera", meta = (ClampMin = "0.0"))
	float LocationInterpSpeed = 3.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale|Camera", meta = (ClampMin = "0.0"))
	float OrthoWidthInterpSpeed = 3.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BattleRoyale|Camera")
	float TargetZ = 0.0f;

private:
	FRotator GetDesiredRotation(const FVector2D& ViewExtent) const;
	float GetDesiredOrthoWidth(const FVector2D& ViewExtent, const FRotator& DesiredRotation) const;
	float GetDesiredPerspectiveDistance(const FVector2D& ViewExtent, const FRotator& DesiredRotation) const;
	FVector2D GetDesiredViewExtent(const class UShowdownBattleRoyaleSubsystem& BattleRoyaleSubsystem) const;
	float GetViewportAspectRatio() const;
	void UpdateCameraFromBattleRoyale(float DeltaTime, bool bSnap);
};
