// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MissionObjectivePlateWidgets.generated.h"

class UBorder;
class UButton;
class UHorizontalBox;
class UImage;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USizeBox;
class UTextBlock;
class UWidget;

/**
 * @brief The C++ built objective plate of one mission widget (design A1-c): gunmetal plate, bolted enamel shield
 * with its own glass icon image, MAJOR / BONUS labels, knurled expand knob and the OK / Next button.
 * Built once by UW_Mission; the mission's own title and description text blocks are moved into it.
 */
USTRUCT()
struct FMissionObjectivePlateWidgets
{
	GENERATED_BODY()

	// The plate lives as long as the widget tree that owns it, so a set root means a complete plate.
	bool GetIsBuilt() const { return Root != nullptr; }

	UPROPERTY()
	TObjectPtr<UWidget> Root = nullptr;

	// Left padding of the whole plate: bonus objectives sit indented under the major ones.
	UPROPERTY()
	TObjectPtr<UBorder> IndentBorder = nullptr;

	// Width of the plate: fixed when expanded so the description wraps, a minimum when only the title shows.
	UPROPERTY()
	TObjectPtr<USizeBox> PlateSizeBox = nullptr;

	// Offsets the plate to the right so the shield overlaps its left edge.
	UPROPERTY()
	TObjectPtr<UBorder> PlateOffsetBorder = nullptr;

	UPROPERTY()
	TObjectPtr<UBorder> PlateBorder = nullptr;

	UPROPERTY()
	TObjectPtr<USizeBox> ShieldSizeBox = nullptr;

	UPROPERTY()
	TObjectPtr<UImage> ShieldImage = nullptr;

	UPROPERTY()
	TObjectPtr<USizeBox> IconSizeBox = nullptr;

	// The glass star / diamond: its own image and material instance so its glass can be tuned apart from the shield.
	UPROPERTY()
	TObjectPtr<UImage> IconImage = nullptr;

	// Expanded: MAJOR OBJECTIVE / BONUS OBJECTIVE above the title.
	UPROPERTY()
	TObjectPtr<UTextBlock> LabelAbove = nullptr;

	// Title view of a bonus objective: BONUS in front of the title.
	UPROPERTY()
	TObjectPtr<UTextBlock> LabelInline = nullptr;

	UPROPERTY()
	TObjectPtr<UButton> HeaderButton = nullptr;

	UPROPERTY()
	TObjectPtr<UButton> KnobButton = nullptr;

	UPROPERTY()
	TObjectPtr<USizeBox> KnobSizeBox = nullptr;

	UPROPERTY()
	TObjectPtr<UWidget> Groove = nullptr;

	UPROPERTY()
	TObjectPtr<UWidget> DescriptionHolder = nullptr;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> ConfirmRow = nullptr;

	UPROPERTY()
	TObjectPtr<UButton> ConfirmButton = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> ConfirmText = nullptr;

	// Normal / hovered / pressed knob materials; PointUp flips with the expanded state.
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> KnobMaterials;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> MajorShieldMaterial = nullptr;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BonusShieldMaterial = nullptr;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> MajorIconMaterial = nullptr;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BonusIconMaterial = nullptr;
};
