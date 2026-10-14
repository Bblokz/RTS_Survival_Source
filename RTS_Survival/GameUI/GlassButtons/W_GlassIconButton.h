// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateTypes.h"

#include "W_GlassIconButton.generated.h"

class UButton;
class UTexture2D;

UENUM(BlueprintType)
enum class EGlassIconButtonAction : uint8
{
	// Only broadcasts OnGlassButtonClicked.
	None,
	// Opens the full-screen map view of the main game UI this button lives in.
	OpenFullMapView
};

DECLARE_MULTICAST_DELEGATE(FOnGlassIconButtonClicked);

/**
 * @brief Round glass button (glass_buttons.py art) that builds its own widget tree, so it can be placed in a
 * designer or created from C++. Set the three state textures; an action makes it work without any wiring.
 */
UCLASS()
class RTS_SURVIVAL_API UW_GlassIconButton : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Used by C++ owners that create the button themselves (the close button of the full map view). */
	void SetupGlassButton(UTexture2D* NormalTexture, UTexture2D* HoveredTexture, UTexture2D* PressedTexture,
	                      const float ButtonSize, const FText& Tooltip);

	FOnGlassIconButtonClicked OnGlassButtonClicked;

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(EditAnywhere, Category = "GlassButton")
	EGlassIconButtonAction M_Action = EGlassIconButtonAction::None;

	UPROPERTY(EditAnywhere, Category = "GlassButton")
	TObjectPtr<UTexture2D> M_NormalTexture;

	UPROPERTY(EditAnywhere, Category = "GlassButton")
	TObjectPtr<UTexture2D> M_HoveredTexture;

	UPROPERTY(EditAnywhere, Category = "GlassButton")
	TObjectPtr<UTexture2D> M_PressedTexture;

	// Edge length of the round button in UI units.
	UPROPERTY(EditAnywhere, Category = "GlassButton", meta = (ClampMin = "16.0"))
	float M_ButtonSize = 40.0f;

	UPROPERTY(EditAnywhere, Category = "GlassButton")
	FText M_Tooltip;

private:
	UPROPERTY()
	TObjectPtr<UButton> M_Button;

	void BuildWidgetTree();
	void ApplyStyle() const;
	FSlateBrush MakeGlassBrush(UTexture2D* Texture) const;

	UFUNCTION()
	void OnButtonClicked();

	void ExecuteAction() const;
};
