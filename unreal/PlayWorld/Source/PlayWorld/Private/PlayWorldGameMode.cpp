#include "PlayWorldGameMode.h"

APlayWorldGameMode::APlayWorldGameMode()
{
	DefaultPawnClass = nullptr;
	HUDClass = nullptr;
	SpectatorClass = nullptr;
	bStartPlayersAsSpectators = true;
}

APawn* APlayWorldGameMode::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	return nullptr;
}

APawn* APlayWorldGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	return nullptr;
}
