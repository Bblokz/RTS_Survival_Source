// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TacticalUnitIconSettings.h"

#include "W_TacticalUnitIconCorners.generated.h"

class UOverlay;
class UW_TacticalUnitIcon;

/**
 * @brief Lay it over a unit image (fill the same overlay as the image) to show up to two tactical glass icons in the
 * bottom-left and bottom-right corners. Builds its own tree and never takes hover or clicks from the image.
 * Experimental and currently not placed anywhere; see Docs/UI/TacticalUnitIcons.md.
 */
UCLASS()
class RTS_SURVIVAL_API UW_TacticalUnitIconCorners : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	/** A side whose settings have no icon texture is hidden. */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "TacticalIcon")
	void SetTacticalIcons(const FTacticalUnitIconSettings& LeftIcon, const FTacticalUnitIconSettings& RightIcon);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "TacticalIcon")
	void ClearTacticalIcons();

protected:
	virtual void NativePreConstruct() override;

	UPROPERTY(EditAnywhere, Category = "TacticalIcon")
	FTacticalUnitIconSettings M_LeftIcon;

	UPROPERTY(EditAnywhere, Category = "TacticalIcon")
	FTacticalUnitIconSettings M_RightIcon;

	// Edge length of each plate in UI units.
	UPROPERTY(EditAnywhere, Category = "TacticalIcon", meta = (ClampMin = "8.0"))
	float M_IconSize = 32.0f;

	// Distance of each plate from the bottom and side edges of the unit image, in UI units.
	UPROPERTY(EditAnywhere, Category = "TacticalIcon", meta = (ClampMin = "0.0"))
	float M_CornerInset = 3.0f;

private:
	UPROPERTY()
	TObjectPtr<UW_TacticalUnitIcon> M_LeftIconWidget;

	UPROPERTY()
	TObjectPtr<UW_TacticalUnitIcon> M_RightIconWidget;

	void BuildWidgetTree();
	UW_TacticalUnitIcon* AddCornerIcon(UOverlay& Overlay, const EHorizontalAlignment HorizontalAlignment) const;
	void ApplyIcons() const;

	bool GetIsValidLeftIconWidget() const;
	bool GetIsValidRightIconWidget() const;
};
