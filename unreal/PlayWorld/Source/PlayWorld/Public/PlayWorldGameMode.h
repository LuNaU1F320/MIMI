#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PlayWorldGameMode.generated.h"

UCLASS()
class PLAYWORLD_API APlayWorldGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APlayWorldGameMode();

	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;
};
