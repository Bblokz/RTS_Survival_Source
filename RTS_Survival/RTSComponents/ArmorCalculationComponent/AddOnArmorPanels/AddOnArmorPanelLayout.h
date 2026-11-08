// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetUserData.h"

#include "AddOnArmorPanelLayout.generated.h"

class UStaticMesh;

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

	// This panel cut out as it hangs on its own side (not turned around), for panels shaped unlike the typical one
	// (end plates with a chamfer or nose). Null: the panel flies off as the layout's shared debris mesh.
	// Set by RTS.AddOnArmor.ExtractPanelDebris -OwnPanel.
	UPROPERTY(VisibleAnywhere, Category="AddOnArmor")
	TObjectPtr<UStaticMesh> OwnDebrisMesh;
};

/**
 * @brief Written onto a hull static mesh by RTS.AddOnArmor.BuildPanelHull; UAddOnArmorPanelsComponent reads it to
 * know which panels it can hide and where they sit. Array index = bit in the hidden-panel mask = value in the
 * panel geometry's PanelIndexUvChannel. Also holds the debris meshes thrown off as Niagara debris whenever a panel
 * is shot off: one shared left-side panel, and optionally a panel's own mesh (see FAddOnArmorPanel::OwnDebrisMesh).
 */
UCLASS()
class RTS_SURVIVAL_API UAddOnArmorPanelLayout : public UAssetUserData
{
	GENERATED_BODY()

public:
	const TArray<FAddOnArmorPanel>& GetPanels() const { return M_Panels; }
	UStaticMesh* GetDebrisMesh() const { return M_DebrisMesh; }

	/**
	 * @brief Panels with their own debris mesh fly off as themselves; the rest share the left-side debris panel.
	 * @param PanelIndex Layout index of the panel.
	 * @param bOutIsOwnSideMesh True when the mesh is the panel's own, already on its side (no turn for right panels).
	 * @return The mesh to throw for this panel, or null when the hull has none.
	 */
	UStaticMesh* GetDebrisMeshForPanel(const int32 PanelIndex, bool& bOutIsOwnSideMesh) const
	{
		UStaticMesh* const OwnMesh = M_Panels.IsValidIndex(PanelIndex) ? M_Panels[PanelIndex].OwnDebrisMesh.Get() : nullptr;
		bOutIsOwnSideMesh = OwnMesh != nullptr;
		return bOutIsOwnSideMesh ? OwnMesh : M_DebrisMesh.Get();
	}

#if WITH_EDITOR
	void SetPanels(const TArray<FAddOnArmorPanel>& Panels) { M_Panels = Panels; }
	void SetDebrisMesh(UStaticMesh* DebrisMesh) { M_DebrisMesh = DebrisMesh; }

	void SetOwnDebrisMesh(const int32 PanelIndex, UStaticMesh* DebrisMesh)
	{
		if (M_Panels.IsValidIndex(PanelIndex))
		{
			M_Panels[PanelIndex].OwnDebrisMesh = DebrisMesh;
		}
	}
#endif

private:
	UPROPERTY(VisibleAnywhere, Category="AddOnArmor")
	TArray<FAddOnArmorPanel> M_Panels;

	// One left-side panel as its own static mesh: pivot at the panel centre, in the hull mesh's axes. A hit throws one
	// per hidden panel (turned around for right-side panels). Set by RTS.AddOnArmor.SetPanelDebrisMesh.
	UPROPERTY(VisibleAnywhere, Category="AddOnArmor")
	TObjectPtr<UStaticMesh> M_DebrisMesh;
};
