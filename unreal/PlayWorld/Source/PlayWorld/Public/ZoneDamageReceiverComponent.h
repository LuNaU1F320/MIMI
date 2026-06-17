#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ZoneDamageReceiverComponent.generated.h"

class AMyCharacter;

UCLASS(ClassGroup=(BattleRoyale), meta=(BlueprintSpawnableComponent))
class PLAYWORLD_API UZoneDamageReceiverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UZoneDamageReceiverComponent();

	void ApplyZoneDamage(float DamagePerSecond, float DeltaTime);

private:
	AMyCharacter* GetOwningCharacter() const;
};
