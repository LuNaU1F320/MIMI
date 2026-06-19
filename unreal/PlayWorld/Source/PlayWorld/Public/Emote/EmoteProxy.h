// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EmoteProxy.generated.h"

/**
 * 
 */

class UImage;

UCLASS()
class PLAYWORLD_API UEmoteProxy : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> SpriteImage;
	
	UPROPERTY(Transient, BlueprintReadOnly, meta = (BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> FadeInOutAnim;
	
};
