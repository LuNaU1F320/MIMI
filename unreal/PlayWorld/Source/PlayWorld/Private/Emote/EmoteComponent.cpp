// Fill out your copyright notice in the Description page of Project Settings.


#include "Emote/EmoteComponent.h"

#include "Materials/MaterialInstance.h"
#include "Components/Image.h"
#include "Emote/EmoteProxy.h"

void UEmoteComponent::PlayEmote(FString EmoteName, float PlayRate)
{
	UUserWidget* WidgetPtr = GetWidget();
	if (!WidgetPtr)
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s: Widget is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	UEmoteProxy* EmoteProxy = Cast<UEmoteProxy>(WidgetPtr);
	if (!EmoteProxy)
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s: The Widget is not EmoteProxy widget."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	
	auto MaterialAsset = EmoteMaterial.Find(EmoteName);
	if (!MaterialAsset)
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s: Material Asset is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	
	EmoteProxy->SpriteImage->SetBrushFromMaterial(*MaterialAsset);
	
	EmoteProxy->PlayAnimation(EmoteProxy->FadeInOutAnim, 0.f, 1, EUMGSequencePlayMode::Forward, PlayRate);
}
