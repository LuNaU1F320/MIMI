#include "BattleRoyaleZoneCameraActor.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "ShowdownBattleRoyaleSubsystem.h"

ABattleRoyaleZoneCameraActor::ABattleRoyaleZoneCameraActor()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(SceneRoot);
	CameraComponent->ProjectionMode = ProjectionMode;
	CameraComponent->FieldOfView = PerspectiveFieldOfView;
	CameraComponent->OrthoWidth = 7000.0f;
}

void ABattleRoyaleZoneCameraActor::BeginPlay()
{
	Super::BeginPlay();

	UpdateCameraFromBattleRoyale(0.0f, true);

	if (bAutoActivateForPlayer0)
	{
		ActivateForPlayer0();
	}
}

void ABattleRoyaleZoneCameraActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateCameraFromBattleRoyale(DeltaTime, false);
}

void ABattleRoyaleZoneCameraActor::SetAutoActivateForPlayer0(bool bInAutoActivate)
{
	bAutoActivateForPlayer0 = bInAutoActivate;
}

void ABattleRoyaleZoneCameraActor::ActivateForPlayer0()
{
	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		PlayerController->SetViewTarget(this);
	}
}

FRotator ABattleRoyaleZoneCameraActor::GetDesiredRotation(const FVector2D& ViewExtent) const
{
	FRotator DesiredRotation = QuarterViewRotation;
	if (bAlignMapToScreenAxes)
	{
		DesiredRotation.Yaw = 0.0f;
		DesiredRotation.Roll = 0.0f;
	}

	if (!bFitPitchToViewportAspect)
	{
		return DesiredRotation;
	}

	const float AspectRatio = GetViewportAspectRatio();
	const float ExtentX = FMath::Max(1.0f, ViewExtent.X);
	const float ExtentY = FMath::Max(1.0f, ViewExtent.Y);
	const float DesiredSinPitch = FMath::Clamp(ExtentY / (ExtentX * AspectRatio), 0.1f, 1.0f);
	DesiredRotation.Pitch = -FMath::RadiansToDegrees(FMath::Asin(DesiredSinPitch));
	return DesiredRotation;
}

float ABattleRoyaleZoneCameraActor::GetDesiredOrthoWidth(const FVector2D& ViewExtent, const FRotator& DesiredRotation) const
{
	const FVector RightVector = FRotationMatrix(DesiredRotation).GetScaledAxis(EAxis::Y);
	const FVector UpVector = FRotationMatrix(DesiredRotation).GetScaledAxis(EAxis::Z);
	const float ProjectedWidth = 2.0f * (FMath::Abs(RightVector.X) * ViewExtent.X + FMath::Abs(RightVector.Y) * ViewExtent.Y);
	const float ProjectedHeight = 2.0f * (FMath::Abs(UpVector.X) * ViewExtent.X + FMath::Abs(UpVector.Y) * ViewExtent.Y);
	const float WidthToFitHeight = ProjectedHeight * GetViewportAspectRatio();
	const float FramingWidth = bCoverViewportWithMap
		? FMath::Min(ProjectedWidth, WidthToFitHeight)
		: FMath::Max(ProjectedWidth, WidthToFitHeight);
	const float DesiredWidth = (FramingWidth + OrthoPadding) / FMath::Max(0.1f, ViewportFillAmount);
	const float UpperBound = FMath::Max(MinOrthoWidth, MaxOrthoWidth);
	return FMath::Clamp(DesiredWidth, MinOrthoWidth, UpperBound);
}

float ABattleRoyaleZoneCameraActor::GetDesiredPerspectiveDistance(const FVector2D& ViewExtent, const FRotator& DesiredRotation) const
{
	const FVector RightVector = FRotationMatrix(DesiredRotation).GetScaledAxis(EAxis::Y);
	const FVector UpVector = FRotationMatrix(DesiredRotation).GetScaledAxis(EAxis::Z);
	const float ProjectedWidth = 2.0f * (FMath::Abs(RightVector.X) * ViewExtent.X + FMath::Abs(RightVector.Y) * ViewExtent.Y);
	const float ProjectedHeight = 2.0f * (FMath::Abs(UpVector.X) * ViewExtent.X + FMath::Abs(UpVector.Y) * ViewExtent.Y);
	const float RequiredViewHeight = bCoverViewportWithMap
		? FMath::Min(ProjectedHeight, ProjectedWidth / GetViewportAspectRatio())
		: FMath::Max(ProjectedHeight, ProjectedWidth / GetViewportAspectRatio());
	const float HalfFovRadians = FMath::DegreesToRadians(FMath::Clamp(PerspectiveFieldOfView, 5.0f, 170.0f) * 0.5f);
	const float FitDistance = (RequiredViewHeight * 0.5f) / FMath::Max(0.01f, FMath::Tan(HalfFovRadians));
	return FMath::Max(CameraDistance, FitDistance / FMath::Max(0.1f, ViewportFillAmount));
}

FVector2D ABattleRoyaleZoneCameraActor::GetDesiredViewExtent(const UShowdownBattleRoyaleSubsystem& BattleRoyaleSubsystem) const
{
	const bool bZonePhaseActive = BattleRoyaleSubsystem.IsBattleRoyaleActive() && BattleRoyaleSubsystem.GetCurrentZone().PhaseIndex >= 0;
	if (!bZonePhaseActive && bUseInitialViewExtentOverride)
	{
		return FVector2D(
			FMath::Max(100.0f, InitialViewExtentOverride.X),
			FMath::Max(100.0f, InitialViewExtentOverride.Y));
	}

	return BattleRoyaleSubsystem.GetCameraViewExtent();
}

float ABattleRoyaleZoneCameraActor::GetViewportAspectRatio() const
{
	int32 ViewportSizeX = 0;
	int32 ViewportSizeY = 0;
	if (const APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		PlayerController->GetViewportSize(ViewportSizeX, ViewportSizeY);
	}

	if (ViewportSizeX <= 0 || ViewportSizeY <= 0)
	{
		return 16.0f / 9.0f;
	}

	return static_cast<float>(ViewportSizeX) / static_cast<float>(ViewportSizeY);
}

void ABattleRoyaleZoneCameraActor::UpdateCameraFromBattleRoyale(float DeltaTime, bool bSnap)
{
	UWorld* World = GetWorld();
	const UShowdownBattleRoyaleSubsystem* BattleRoyaleSubsystem = World ? World->GetSubsystem<UShowdownBattleRoyaleSubsystem>() : nullptr;
	if (!BattleRoyaleSubsystem || !CameraComponent)
	{
		return;
	}

	const FVector2D ViewCenter = BattleRoyaleSubsystem->GetCameraViewCenter();
	const FVector2D ViewExtent = GetDesiredViewExtent(*BattleRoyaleSubsystem);
	const FRotator DesiredRotation = GetDesiredRotation(ViewExtent);
	const float DesiredDistance = ProjectionMode == ECameraProjectionMode::Perspective
		? GetDesiredPerspectiveDistance(ViewExtent, DesiredRotation)
		: CameraDistance;
	const FVector DesiredLocation = FVector(ViewCenter.X, ViewCenter.Y, TargetZ) - DesiredRotation.Vector() * DesiredDistance;
	const float DesiredOrthoWidth = GetDesiredOrthoWidth(ViewExtent, DesiredRotation);

	SetActorRotation(DesiredRotation);
	CameraComponent->ProjectionMode = ProjectionMode;
	CameraComponent->FieldOfView = PerspectiveFieldOfView;

	if (bSnap || DeltaTime <= 0.0f)
	{
		SetActorLocation(DesiredLocation);
		CameraComponent->OrthoWidth = DesiredOrthoWidth;
		return;
	}

	SetActorLocation(FMath::VInterpTo(GetActorLocation(), DesiredLocation, DeltaTime, LocationInterpSpeed));
	CameraComponent->OrthoWidth = FMath::FInterpTo(CameraComponent->OrthoWidth, DesiredOrthoWidth, DeltaTime, OrthoWidthInterpSpeed);
}
