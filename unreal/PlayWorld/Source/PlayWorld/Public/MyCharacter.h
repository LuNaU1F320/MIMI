
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "MyCharacter.generated.h"

class UBoxComponent;
class UCharacterEquipmentComponent;
class USceneComponent;
class UStaticMeshComponent;
class UZoneDamageReceiverComponent;

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

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ResetForNextRound();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SetAutoAttackEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsAlive() const { return bIsAlive; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetCurrentHP() const { return CurrentHP; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetMaxHP() const { return MaxHP; }

	UFUNCTION(BlueprintCallable, Category = "Input")
	void SetMoveInput(float MoveX, float MoveY);

	UFUNCTION(BlueprintCallable, Category = "Input")
	void SetExternalMovementEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void ApplyAttackRangeBonus(float BonusAmount);


	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void ApplyAttackPowerBonus(float BonusAmount);

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void ApplyMoveSpeedBonus(float BonusAmount);

	UFUNCTION(BlueprintCallable, Category = "BattleRoyale")
	void ApplyZoneDamage(float DamagePerSecond, float DeltaTime);

	UFUNCTION(BlueprintCallable, Category = "Visual")
	void SetOverlayColor(FLinearColor Color);

	UCharacterEquipmentComponent* GetEquipmentComponent() const { return EquipmentComponent; }
	UZoneDamageReceiverComponent* GetZoneDamageReceiverComponent() const { return ZoneDamageReceiverComponent; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment")
	TObjectPtr<UCharacterEquipmentComponent> EquipmentComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BattleRoyale")
	TObjectPtr<UZoneDamageReceiverComponent> ZoneDamageReceiverComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UBoxComponent> AttackBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<USceneComponent> WeaponPivot;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Visual")
	FVector WeaponMeshOffset = FVector(120.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Visual")
	FRotator WeaponMeshRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Visual")
	FVector WeaponMeshScale = FVector(1.0f, 0.12f, 0.12f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.01"))
	float AttackActiveTime = 0.35f;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (ClampMin = "0.0"))
	float ExternalMoveSpeed = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (ClampMin = "0.0"))
	float MinimumExternalMoveSpeed = 900.0f;

private:
	FTimerHandle AutoAttackTimerHandle;
	float TimeUntilNextAttack = 0.0f;
	float AttackActiveTimeRemaining = 0.0f;
	float AttackActiveTimeTotal = 0.0f;
	bool bAutoAttackEnabled = false;
	float ActiveWeaponSwingStartYaw = 0.0f;
	float ActiveWeaponSwingEndYaw = 0.0f;
	float PreviousSwingYaw = 0.0f;
	FVector2D CurrentMoveInput = FVector2D::ZeroVector;
	TSet<TWeakObjectPtr<AMyCharacter>> HitTargetsThisAttack;

	void ApplyCurrentMoveInput();
	void UpdateAttackBoxTransform();
	void ApplyWeaponMeshTransform(float SwingYaw);
	AMyCharacter* FindNearestAttackTarget() const;
	bool IsTargetInAttackDistance(const AMyCharacter* Target) const;
	bool IsTargetInsideForwardArc(const AMyCharacter* Target, float AttackDistance) const;
	void TryAutoAttack();
	void StartAttack();
	void FinishAttack();
	void UpdateWeaponSwing();
	void SweepSectorDamage(float PreviousYaw, float CurrentYaw);
	bool IsTargetInsideSweepSegment(const AMyCharacter* Target, float PreviousYaw, float CurrentYaw, float AttackDistance) const;
	void ApplyExternalMovementSettings();
	void MoveForward(float Value);
	void MoveRight(float Value);
};
