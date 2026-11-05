// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetUserData.h"

#include "AddOnArmorPanelLayout.generated.h"

namespace AddOnArmorPanelMask
{
	// UV channel holding each panel's index; vehicle hulls generate their lightmap UVs into channel 2.
	inline constexpr int32 PanelIndexUvChannel = 3;

	// Custom primitive data float with one bit per hidden panel; 0 is the faction flag, 4-7 the team colour.
	inline constexpr int32 HiddenPanelsPrimitiveDataIndex = 8;

	// A float holds integers exactly up to 2^24; 20 bits keep the material's floor/exp2 decode exact.
	inline constexpr int32 MaxPanelsPerMesh = 20;

	// Material slot of the panel geometry; its material clips hidden panels through the opacity mask.
	inline const FName PanelMaterialSlotName(TEXT("AddOnArmorPanels"));

	// Scalar parameter the panel material reads from HiddenPanelsPrimitiveDataIndex.
	inline const FName HiddenPanelsParameterName(TEXT("AddOnArmorHiddenPanels"));
}

UENUM(BlueprintType)
enum class EAddOnArmorPanelSide : uint8
{
	Left,
	Right
};

USTRUCT()
struct FAddOnArmorPanel
{
	GENERATED_BODY()

	// Panel centre in the static mesh's space.
	UPROPERTY(VisibleAnywhere, Category="AddOnArmor")
	FVector MeshSpaceCentre = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category="AddOnArmor")
	EAddOnArmorPanelSide Side = EAddOnArmorPanelSide::Left;
};

/**
 * @brief Written onto a hull static mesh by RTS.AddOnArmor.BuildPanelHull; UAddOnArmorPanelsComponent reads it to
 * know which panels it can hide and where they sit. Array index = bit in the hidden-panel mask = value in the
 * panel geometry's PanelIndexUvChannel.
 */
UCLASS()
class RTS_SURVIVAL_API UAddOnArmorPanelLayout : public UAssetUserData
{
	GENERATED_BODY()

public:
	const TArray<FAddOnArmorPanel>& GetPanels() const { return M_Panels; }

#if WITH_EDITOR
	void SetPanels(const TArray<FAddOnArmorPanel>& Panels) { M_Panels = Panels; }
#endif

private:
	UPROPERTY(VisibleAnywhere, Category="AddOnArmor")
	TArray<FAddOnArmorPanel> M_Panels;
};
