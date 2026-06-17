// Copyright Epic Games, Inc. All Rights Reserved.

#include "MyCharacter.h"

#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "CharacterEquipmentComponent.h"
#include "ZoneDamageReceiverComponent.h"

AMyCharacter::AMyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(34.0f, 88.0f);
	GetCapsuleComponent()->SetHiddenInGame(false);

	GetMesh()->SetVisibility(false);
	GetMesh()->SetHiddenInGame(true);

	ForwardArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("ForwardArrow"));
	ForwardArrow->SetupAttachment(GetRootComponent());
	ForwardArrow->SetRelativeLocation(FVector(80.0f, 0.0f, 80.0f));
	ForwardArrow->SetArrowSize(1.5f);
	ForwardArrow->ArrowColor = FColor::Green;
	ForwardArrow->SetHiddenInGame(false);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(GetRootComponent());
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetRelativeScale3D(FVector(0.68f, 0.68f, 0.88f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CapsuleMesh(TEXT("/Engine/EditorResources/S_TriggerCapsule.S_TriggerCapsule"));
	if (CapsuleMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(CapsuleMesh.Object);
	}
	else
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		if (CylinderMesh.Succeeded())
		{
			BodyMesh->SetStaticMesh(CylinderMesh.Object);
		}
	}

	AttackBox = CreateDefaultSubobject<UBoxComponent>(TEXT("AttackBox"));
	AttackBox->SetupAttachment(GetRootComponent());
	AttackBox->SetCollisionObjectType(ECC_WorldDynamic);
	AttackBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AttackBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	AttackBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	AttackBox->SetGenerateOverlapEvents(true);
	AttackBox->SetHiddenInGame(true);
	AttackBox->SetVisibility(false);

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(GetRootComponent());
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetHiddenInGame(true);
	WeaponMesh->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		WeaponMesh->SetStaticMesh(CubeMesh.Object);
	}

	EquipmentComponent = CreateDefaultSubobject<UCharacterEquipmentComponent>(TEXT("EquipmentComponent"));
	ZoneDamageReceiverComponent = CreateDefaultSubobject<UZoneDamageReceiverComponent>(TEXT("ZoneDamageReceiverComponent"));

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
	GetCharacterMovement()->bRunPhysicsWithNoController = true;

	UpdateAttackBoxTransform();
}

void AMyCharacter::BeginPlay()
{
	Super::BeginPlay();

	CurrentHP = FMath::Clamp(CurrentHP, 0.0f, MaxHP);
	bIsAlive = CurrentHP > 0.0f;
	FinishAttack();

	if (bIsAlive)
	{
		GetWorldTimerManager().SetTimer(AutoAttackTimerHandle, this, &AMyCharacter::TryAutoAttack, AutoAttackInterval, true);
	}

	if (bEnableWASDMovement && bAutoPossessForWASDTest)
	{
		if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
		{
			PlayerController->Possess(this);
		}
	}
}

void AMyCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	UpdateAttackBoxTransform();
}

void AMyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	ApplyCurrentMoveInput();

	if (AttackActiveTimeRemaining > 0.0f)
	{
		AttackActiveTimeRemaining -= DeltaTime;
		UpdateWeaponSwing();

		if (AttackActiveTimeRemaining <= 0.0f)
		{
			FinishAttack();
		}
	}
}

void AMyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &AMyCharacter::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &AMyCharacter::MoveRight);
}

void AMyCharacter::SetMoveInput(float MoveX, float MoveY)
{
	CurrentMoveInput = FVector2D(
		FMath::Clamp(MoveX, -1.0f, 1.0f),
		FMath::Clamp(MoveY, -1.0f, 1.0f));

	if (CurrentMoveInput.SizeSquared() > 1.0f)
	{
		CurrentMoveInput.Normalize();
	}
}

void AMyCharacter::SetExternalMovementEnabled(bool bEnabled)
{
	bEnableWASDMovement = bEnabled;

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->bRunPhysicsWithNoController = bEnabled;
		MovementComponent->Activate(true);
	}
}

void AMyCharacter::ApplyAttackRangeBonus(float BonusAmount)
{
	AttackBoxForwardOffset += BonusAmount;
	AttackBoxExtent.X += BonusAmount * 0.5f;
	UpdateAttackBoxTransform();
	UE_LOG(LogTemp, Log, TEXT("%s gained attack range +%.1f."), *GetName(), BonusAmount);
}

void AMyCharacter::ApplyAttackPowerBonus(float BonusAmount)
{
	AttackPower += BonusAmount;
	UE_LOG(LogTemp, Log, TEXT("%s gained attack power +%.1f."), *GetName(), BonusAmount);
}

void AMyCharacter::ApplyMoveSpeedBonus(float BonusAmount)
{
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->MaxWalkSpeed += BonusAmount;
		UE_LOG(LogTemp, Log, TEXT("%s gained move speed +%.1f. MaxWalkSpeed: %.1f"), *GetName(), BonusAmount, MovementComponent->MaxWalkSpeed);
	}
}

void AMyCharacter::ApplyZoneDamage(float DamagePerSecond, float DeltaTime)
{
	TakeAutoAttackDamage(DamagePerSecond * DeltaTime);
}

void AMyCharacter::TryAutoAttack()
{
	if (!bIsAlive || AttackActiveTimeRemaining > 0.0f)
	{
		return;
	}

	const float DeltaTime = GetWorld() ? GetWorld()->GetTimerManager().GetTimerRate(AutoAttackTimerHandle) : AutoAttackInterval;
	TimeUntilNextAttack -= DeltaTime;
	if (TimeUntilNextAttack > 0.0f)
	{
		return;
	}

	AMyCharacter* Target = FindNearestAttackTarget();
	if (!Target || !IsTargetInAttackDistance(Target))
	{
		return;
	}

	const FVector DirectionToTarget = Target->GetActorLocation() - GetActorLocation();
	const FRotator TargetRotation = DirectionToTarget.Rotation();
	SetActorRotation(FRotator(0.0f, TargetRotation.Yaw, 0.0f));

	StartAttack();
	TimeUntilNextAttack = AttackCooldown;
}

void AMyCharacter::TakeAutoAttackDamage(float DamageAmount)
{
	if (!bIsAlive || DamageAmount <= 0.0f)
	{
		return;
	}

	CurrentHP = FMath::Max(0.0f, CurrentHP - DamageAmount);
	UE_LOG(LogTemp, Log, TEXT("%s took %.1f damage. HP: %.1f / %.1f"), *GetName(), DamageAmount, CurrentHP, MaxHP);

	if (CurrentHP > 0.0f)
	{
		return;
	}

	bIsAlive = false;
	FinishAttack();
	GetWorldTimerManager().ClearTimer(AutoAttackTimerHandle);
	SetActorEnableCollision(false);
	UE_LOG(LogTemp, Log, TEXT("%s died."), *GetName());
}

void AMyCharacter::UpdateAttackBoxTransform()
{
	if (!AttackBox)
	{
		return;
	}

	AttackBox->SetBoxExtent(AttackBoxExtent);
	AttackBox->SetRelativeLocation(FVector(AttackBoxForwardOffset, 0.0f, 0.0f));
	AttackBox->SetRelativeRotation(FRotator::ZeroRotator);

	if (WeaponMesh)
	{
		WeaponMesh->SetRelativeLocation(FVector(AttackBoxForwardOffset, 0.0f, 0.0f));
		WeaponMesh->SetRelativeScale3D((AttackBoxExtent * 2.0f) / 100.0f);
	}
}

AMyCharacter* AMyCharacter::FindNearestAttackTarget() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const float AttackDistance = FMath::Max(0.0f, AttackBoxForwardOffset) + FMath::Max(0.0f, AttackBoxExtent.X);
	TArray<FOverlapResult> OverlapResults;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AutoAttackTargetSearch), false, this);
	QueryParams.AddIgnoredActor(this);

	const bool bHasOverlap = World->OverlapMultiByObjectType(
		OverlapResults,
		GetActorLocation(),
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(AttackDistance),
		QueryParams);

	if (!bHasOverlap)
	{
		return nullptr;
	}

	AMyCharacter* NearestTarget = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();

	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AMyCharacter* Candidate = Cast<AMyCharacter>(OverlapResult.GetActor());
		if (!Candidate || Candidate == this || !Candidate->IsAlive())
		{
			continue;
		}

		if (!IsTargetInsideForwardArc(Candidate, AttackDistance))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			NearestTarget = Candidate;
		}
	}

	return NearestTarget;
}

bool AMyCharacter::IsTargetInAttackDistance(const AMyCharacter* Target) const
{
	if (!Target)
	{
		return false;
	}

	const float AttackDistance = FMath::Max(0.0f, AttackBoxForwardOffset) + FMath::Max(0.0f, AttackBoxExtent.X);
	return IsTargetInsideForwardArc(Target, AttackDistance);
}

bool AMyCharacter::IsTargetInsideForwardArc(const AMyCharacter* Target, float AttackDistance) const
{
	if (!Target)
	{
		return false;
	}

	const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	const FVector ToTarget2D(ToTarget.X, ToTarget.Y, 0.0f);
	const float DistanceSquared = ToTarget2D.SizeSquared();
	if (DistanceSquared > FMath::Square(AttackDistance))
	{
		return false;
	}

	const FVector LocalDirection = GetActorTransform().InverseTransformVectorNoScale(ToTarget2D.GetSafeNormal());
	const float TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(LocalDirection.Y, LocalDirection.X));
	const float MinYaw = FMath::Min(WeaponSwingStartYaw, WeaponSwingEndYaw);
	const float MaxYaw = FMath::Max(WeaponSwingStartYaw, WeaponSwingEndYaw);
	return TargetYaw >= MinYaw && TargetYaw <= MaxYaw;
}

void AMyCharacter::StartAttack()
{
	if (!AttackBox)
	{
		return;
	}

	UpdateAttackBoxTransform();
	AttackActiveTimeRemaining = AttackActiveTime;
	AttackActiveTimeTotal = AttackActiveTime;
	PreviousSwingYaw = WeaponSwingStartYaw;
	HitTargetsThisAttack.Reset();

	if (WeaponMesh)
	{
		WeaponMesh->SetHiddenInGame(false);
		WeaponMesh->SetVisibility(true);
		WeaponMesh->SetRelativeRotation(FRotator(0.0f, WeaponSwingStartYaw, 0.0f));
	}

	AttackBox->SetHiddenInGame(false);
	AttackBox->SetVisibility(true);
	AttackBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	AttackBox->UpdateOverlaps();

	UE_LOG(LogTemp, Log, TEXT("%s attacks."), *GetName());
	SweepSectorDamage(PreviousSwingYaw, PreviousSwingYaw);
}

void AMyCharacter::FinishAttack()
{
	AttackActiveTimeRemaining = 0.0f;

	if (!AttackBox)
	{
		return;
	}

	AttackBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AttackBox->SetHiddenInGame(true);
	AttackBox->SetVisibility(false);
	HitTargetsThisAttack.Reset();

	if (WeaponMesh)
	{
		WeaponMesh->SetHiddenInGame(true);
		WeaponMesh->SetVisibility(false);
		WeaponMesh->SetRelativeRotation(FRotator::ZeroRotator);
	}
}

void AMyCharacter::UpdateWeaponSwing()
{
	if (!WeaponMesh || AttackActiveTimeTotal <= 0.0f)
	{
		return;
	}

	const float Alpha = 1.0f - FMath::Clamp(AttackActiveTimeRemaining / AttackActiveTimeTotal, 0.0f, 1.0f);
	const float SwingYaw = FMath::Lerp(WeaponSwingStartYaw, WeaponSwingEndYaw, Alpha);
	WeaponMesh->SetRelativeRotation(FRotator(0.0f, SwingYaw, 0.0f));

	if (AttackBox)
	{
		AttackBox->SetRelativeRotation(FRotator(0.0f, SwingYaw, 0.0f));
	}

	SweepSectorDamage(PreviousSwingYaw, SwingYaw);
	PreviousSwingYaw = SwingYaw;
}

void AMyCharacter::SweepSectorDamage(float PreviousYaw, float CurrentYaw)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float AttackDistance = FMath::Max(0.0f, AttackBoxForwardOffset) + FMath::Max(0.0f, AttackBoxExtent.X);
	const FVector Origin = GetActorLocation();

	TArray<FOverlapResult> OverlapResults;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AutoAttackSector), false, this);
	QueryParams.AddIgnoredActor(this);

	const bool bHasOverlap = World->OverlapMultiByObjectType(
		OverlapResults,
		Origin,
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(AttackDistance),
		QueryParams);

	const FVector Forward = GetActorForwardVector();
	const FVector LeftEdge = FRotator(0.0f, FMath::Min(PreviousYaw, CurrentYaw), 0.0f).RotateVector(Forward);
	const FVector RightEdge = FRotator(0.0f, FMath::Max(PreviousYaw, CurrentYaw), 0.0f).RotateVector(Forward);
	const FVector CurrentEdge = FRotator(0.0f, CurrentYaw, 0.0f).RotateVector(Forward);
	DrawDebugLine(World, Origin, Origin + LeftEdge * AttackDistance, FColor::Yellow, false, 0.08f, 0, 2.0f);
	DrawDebugLine(World, Origin, Origin + RightEdge * AttackDistance, FColor::Yellow, false, 0.08f, 0, 2.0f);
	DrawDebugLine(World, Origin, Origin + CurrentEdge * AttackDistance, FColor::Red, false, 0.08f, 0, 3.0f);
	DrawDebugSphere(World, Origin, AttackDistance, 24, FColor::Orange, false, 0.08f, 0, 1.0f);

	if (!bHasOverlap)
	{
		return;
	}

	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AMyCharacter* Target = Cast<AMyCharacter>(OverlapResult.GetActor());
		if (!Target || Target == this || !Target->IsAlive() || HitTargetsThisAttack.Contains(Target))
		{
			continue;
		}

		if (!IsTargetInsideSweepSegment(Target, PreviousYaw, CurrentYaw, AttackDistance))
		{
			continue;
		}

		HitTargetsThisAttack.Add(Target);
		Target->TakeAutoAttackDamage(AttackPower);
	}
}

bool AMyCharacter::IsTargetInsideSweepSegment(const AMyCharacter* Target, float PreviousYaw, float CurrentYaw, float AttackDistance) const
{
	if (!Target)
	{
		return false;
	}

	const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	const FVector ToTarget2D(ToTarget.X, ToTarget.Y, 0.0f);
	const float DistanceSquared = ToTarget2D.SizeSquared();
	if (DistanceSquared > FMath::Square(AttackDistance))
	{
		return false;
	}

	const FVector LocalDirection = GetActorTransform().InverseTransformVectorNoScale(ToTarget2D.GetSafeNormal());
	const float TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(LocalDirection.Y, LocalDirection.X));
	const float MinYaw = FMath::Min(PreviousYaw, CurrentYaw) - AttackSweepPaddingDegrees;
	const float MaxYaw = FMath::Max(PreviousYaw, CurrentYaw) + AttackSweepPaddingDegrees;
	return TargetYaw >= MinYaw && TargetYaw <= MaxYaw;
}

void AMyCharacter::ApplyCurrentMoveInput()
{
	if (!bEnableWASDMovement || !bIsAlive || CurrentMoveInput.IsNearlyZero())
	{
		return;
	}

	AddMovementInput(FVector::ForwardVector, CurrentMoveInput.Y, true);
	AddMovementInput(FVector::RightVector, CurrentMoveInput.X, true);
}

void AMyCharacter::MoveForward(float Value)
{
	SetMoveInput(CurrentMoveInput.X, Value);
}

void AMyCharacter::MoveRight(float Value)
{
	SetMoveInput(Value, CurrentMoveInput.Y);
}
