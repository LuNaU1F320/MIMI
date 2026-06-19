#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BattleRoyaleMinimapWidget.generated.h"

UCLASS()
class PLAYWORLD_API UBattleRoyaleMinimapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	FVector2D WorldToMinimap(const FVector2D& WorldLocation, const FVector2D& WidgetSize) const;
	void DrawCircle(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& Geometry, const FVector2D& Center, float Radius, const FLinearColor& Color, float Thickness) const;
};
