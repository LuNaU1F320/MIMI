
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "MyCharacter.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

UCLASS()
class PLAYWORLD_API AMyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AMyCharacter();

	virtual void Tick(float DeltaTime) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void TakeAutoAttackDamage(float DamageAmount);

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsAlive() const { return bIsAlive; }

	UFUNCTION(BlueprintCallable, Category = "Input")
	void SetMoveInput(float MoveX, float MoveY);

	UFUNCTION(BlueprintCallable, Category = "Input")
	void SetExternalMovementEnabled(bool bEnabled);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UBoxComponent> AttackBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "1.0"))
	float MaxHP = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0"))
	float CurrentHP = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0"))
	float AttackPower = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.01"))
	float AttackCooldown = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.05"))
	float AutoAttackInterval = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	FVector AttackBoxExtent = FVector(120.0f, 50.0f, 50.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float AttackBoxForwardOffset = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.01"))
	float AttackActiveTime = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float WeaponSwingStartYaw = -60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float WeaponSwingEndYaw = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0"))
	float AttackSweepPaddingDegrees = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	bool bIsAlive = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bEnableWASDMovement = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bAutoPossessForWASDTest = false;

private:
	FTimerHandle AutoAttackTimerHandle;
	float TimeUntilNextAttack = 0.0f;
	float AttackActiveTimeRemaining = 0.0f;
	float AttackActiveTimeTotal = 0.0f;
	float PreviousSwingYaw = 0.0f;
	FVector2D CurrentMoveInput = FVector2D::ZeroVector;
	TSet<TWeakObjectPtr<AMyCharacter>> HitTargetsThisAttack;

	void ApplyCurrentMoveInput();
	void UpdateAttackBoxTransform();
	AMyCharacter* FindNearestAttackTarget() const;
	bool IsTargetInAttackDistance(const AMyCharacter* Target) const;
	bool IsTargetInsideForwardArc(const AMyCharacter* Target, float AttackDistance) const;
	void TryAutoAttack();
	void StartAttack();
	void FinishAttack();
	void UpdateWeaponSwing();
	void SweepSectorDamage(float PreviousYaw, float CurrentYaw);
	bool IsTargetInsideSweepSegment(const AMyCharacter* Target, float PreviousYaw, float CurrentYaw, float AttackDistance) const;
	void MoveForward(float Value);
	void MoveRight(float Value);
};
