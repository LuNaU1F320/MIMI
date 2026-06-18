// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "EmoteComponent.generated.h"

/**
 * 
 */

class UMaterialInstance;

UCLASS()
class PLAYWORLD_API UEmoteComponent : public UWidgetComponent
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emote")
	TMap<FString, UMaterialInstance*> EmoteMaterial;

public:
	UFUNCTION(BlueprintCallable, Category = "Emote")
	void PlayEmote(FString EmoteName, float PlayRate);
private:
	
};
