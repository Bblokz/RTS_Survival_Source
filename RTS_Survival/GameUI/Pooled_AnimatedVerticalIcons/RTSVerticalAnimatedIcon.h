#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTSVerticalAnimatedIcon.generated.h"

class UImage;
class USizeBox;
class UTexture2D;
struct FRTSVerticalAnimatedIconDefinition;

/**
 * @brief The pool reuses this widget for every icon; its native layout needs no Blueprint setup.
 * Custom Blueprint layouts must contain M_IconSizeBox and M_IconImage with matching widget types.
 */
UCLASS(BlueprintType, Blueprintable, meta=(DisableNativeTick))
class RTS_SURVIVAL_API UW_RTSVerticalAnimatedIcon : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	/**
	 * @brief Replaces all visual state before a recycled widget becomes visible.
	 * @param Definition Display dimensions and tint for this activation.
	 * @param Texture Already resident artwork; this function never loads an asset.
	 * @return Whether the image and layout were ready to activate.
	 */
	bool ActivateIcon(const FRTSVerticalAnimatedIconDefinition& Definition, UTexture2D* Texture);
	void SetDormant();
	bool IsLayoutReady() const;

private:
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<USizeBox> M_IconSizeBox;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> M_IconImage;

	void Initialize_CreateDefaultLayout();
	bool GetIsValidIconSizeBox() const;
	bool GetIsValidIconImage() const;
};
