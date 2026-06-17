#include "CharacterEquipmentComponent.h"

#include "MyCharacter.h"

UCharacterEquipmentComponent::UCharacterEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCharacterEquipmentComponent::ApplyEffect(EBattleRoyaleEquipmentEffect Effect)
{
	AMyCharacter* OwnerCharacter = GetOwningCharacter();
	if (!OwnerCharacter)
	{
		return;
	}

	switch (Effect)
	{
	case EBattleRoyaleEquipmentEffect::AttackRange:
		OwnerCharacter->ApplyAttackRangeBonus(AttackRangeBonus);
		break;
	case EBattleRoyaleEquipmentEffect::AttackPower:
		OwnerCharacter->ApplyAttackPowerBonus(AttackPowerBonus);
		break;
	case EBattleRoyaleEquipmentEffect::MoveSpeed:
		OwnerCharacter->ApplyMoveSpeedBonus(MoveSpeedBonus);
		break;
	default:
		break;
	}
}

AMyCharacter* UCharacterEquipmentComponent::GetOwningCharacter() const
{
	return Cast<AMyCharacter>(GetOwner());
}
