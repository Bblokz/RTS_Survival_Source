// W_FormationPicker.h
// Copyright (C) Bas Blokzijl

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/Player/Formation/FormationTypes.h"
#include "W_FormationPicker.generated.h"

class UScaleBox;
class UImage;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UCanvasPanelSlot;

/**
 * Radial‐menu widget for choosing one of several formation types.
 */
UCLASS()
class RTS_SURVIVAL_API UW_FormationPicker : public UUserWidget
{
    GENERATED_BODY()

public:
    // Called once when the widget is constructed
    virtual void NativeOnInitialized() override;

    // Called every frame; only runs our highlight logic when active
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    /** Show the radial menu centered (if possible) on InMousePosition, begin tracking. */
    bool Activate(const FVector2D& InMousePosition);

    /** Hide the menu and stop tracking. */
    UFUNCTION(BlueprintCallable, Category="Formation Picker")
    void Deactivate();

    /**
     * Called by your PlayerController when a primary click happens.
     * Returns the formation under the cursor at the time of click,
     * deactivates & collapses the widget.
     */
    EFormation GetFormationFromPrimaryClick(const FVector2D& ClickedPosition);

protected:
    /** Bound in Designer: the Image widget that shows the radial material. */
    UPROPERTY(meta=(BindWidget))
    UImage* FormationImage;

    // Scale box containing the formation image.
    UPROPERTY(meta=(BindWidget))
    UScaleBox* FormationScaleBox;
    
    /** The list of formation types; the number of entries == number of segments. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Formations")
    TArray<EFormation> FormationTypes;

    /** The master material (with “Segments” and “Index” scalar parameters). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="RadialMaterial", meta=(AllowPrivateAccess="true"))
    UMaterialInterface* RadialMaterial;

    // Radius in pixels that defines the DeadZone in which we do not change the highlighted segment.
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Formations")
    float SphereInnerRadius = 100.f;

private:
    UPROPERTY()
    /** Runtime dynamic material we drive. */
    UMaterialInstanceDynamic* RadialMaterialInstance = nullptr;

    /** Whether we’re currently tracking / visible. */
    bool bIsActive = false;

    /** Screen‐space origin (center) of the radial menu. */
    FVector2D OriginPosition = FVector2D::ZeroVector;

    /** Cached number of segments (FormationTypes.Num()). */
    int32 SegmentCount = 0;

    /** Currently highlighted segment index [0..SegmentCount–1]. */
    int32 FocusedIndex = 0;
};
