#include "ZoneDamageReceiverComponent.h"

#include "MyCharacter.h"

UZoneDamageReceiverComponent::UZoneDamageReceiverComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UZoneDamageReceiverComponent::ApplyZoneDamage(float DamagePerSecond, float DeltaTime)
{
	if (AMyCharacter* OwnerCharacter = GetOwningCharacter())
	{
		OwnerCharacter->ApplyZoneDamage(DamagePerSecond, DeltaTime);
	}
}

AMyCharacter* UZoneDamageReceiverComponent::GetOwningCharacter() const
{
	return Cast<AMyCharacter>(GetOwner());
}
