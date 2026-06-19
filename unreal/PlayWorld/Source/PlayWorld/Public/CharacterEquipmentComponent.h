#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CharacterEquipmentComponent.generated.h"

class AMyCharacter;

UENUM(BlueprintType)
enum class EBattleRoyaleEquipmentEffect : uint8
{
	AttackRange,
	AttackPower,
	MoveSpeed
};

UCLASS(ClassGroup=(BattleRoyale), meta=(BlueprintSpawnableComponent))
class PLAYWORLD_API UCharacterEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCharacterEquipmentComponent();

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void ApplyEffect(EBattleRoyaleEquipmentEffect Effect);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
	float AttackRangeBonus = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
	float AttackPowerBonus = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
	float MoveSpeedBonus = 80.0f;

private:
	AMyCharacter* GetOwningCharacter() const;
};
