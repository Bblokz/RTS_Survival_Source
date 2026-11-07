// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AddOnArmorPanelAudioSubsystem.h"
#include "AddOnArmorPanelLayout.h"
#include "RTS_Survival/RTSComponents/ArmorComponent/Armor.h"

#include "AddOnArmorPanelsComponent.generated.h"

class UArmorCalculation;
class UMeshComponent;
class UNiagaraSystem;
class UStaticMesh;
class UStaticMeshComponent;
struct FModuleStateChange;
struct FVehicleModuleSnapshot;

/** @brief One add-on armour zone: the panels on one hull side and the plates they protect. */
USTRUCT(BlueprintType)
struct FAddOnArmorPanelZoneSettings
{
	GENERATED_BODY()

	// Hull side whose panels (from the hull mesh's panel layout) belong to this zone.
	UPROPERTY(EditDefaultsOnly, Category="AddOnArmor")
	EAddOnArmorPanelSide Side = EAddOnArmorPanelSide::Left;

	// Registered hull plate types behind the panels; each gets AddOnArmorValue on top of its structural armour.
	UPROPERTY(EditDefaultsOnly, Category="AddOnArmor")
	TArray<EArmorPlate> CoveredPlateTypes;

	UPROPERTY(EditDefaultsOnly, Category="AddOnArmor", meta=(ClampMin="0"))
	float AddOnArmorValue = 15.f;
};

/** @brief Runtime state of one installed zone. */
struct FAddOnArmorPanelZoneState
{
	int32 ModuleId = INDEX_NONE;

	// Indices into the hull mesh's panel layout.
	TArray<int32, TInlineAllocator<AddOnArmorPanelMask::MaxPanelsPerMesh>> PanelIndices;

	// Hidden panels in the order they were hidden; repairs bring back the most recent first.
	TArray<int32, TInlineAllocator<AddOnArmorPanelMask::MaxPanelsPerMesh>> HiddenPanelOrder;

	// Last impact on the zone in the owner's space; the next panel to hide is the intact one closest to it.
	FVector LastHitPosition = FVector::ZeroVector;
	bool bHasLastHit = false;
};

/**
 * @brief Add to a tank whose hull mesh was built by RTS.AddOnArmor.BuildPanelHull. The tank installs one AddOnArmor
 * module per zone when its Blueprint finished the module setup; the zone's damage then hides panels of its side
 * (closest to the hits first, all of them once destroyed) and repairs bring them back. Every panel that damage hides
 * flies off as Niagara debris: the hull's debris panel mesh (see its panel layout) in one pooled burst per hit, with
 * one pooled clang per hit for the tank's weight class (UAddOnArmorPanelAudioSubsystem).
 * Structural plate values in the Blueprint's armour setup must not include the add-on armour.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UAddOnArmorPanelsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAddOnArmorPanelsComponent();

	/**
	 * @brief Called by the tank right before module finalization, a tick after its Blueprint module setup.
	 * @param ArmorCalculation The tank's armour; the hull mesh must already be registered on it.
	 */
	void InstallAddOnArmorZones(UArmorCalculation* ArmorCalculation);

	/** @brief Keeps the panels in line with a committed AddOnArmor transition (damage, repair, service). */
	void OnAddOnArmorModuleStateChanged(const FModuleStateChange& Change);

	/** @brief Rebuilds every zone's panels from the module state; loads emit no transitions. */
	void SyncAllZonesFromModuleState();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(EditDefaultsOnly, Category="AddOnArmor")
	TArray<FAddOnArmorPanelZoneSettings> M_ZoneSettings;

	// Throws the shot-off panels; its User.DebrisMesh is set per burst to the hull's debris panel.
	UPROPERTY(EditDefaultsOnly, Category="AddOnArmor")
	TSoftObjectPtr<UNiagaraSystem> M_DebrisSystem;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> M_LoadedDebrisSystem;

	// One panel as its own mesh, from the panel layout; null when the hull has none (no debris then).
	UPROPERTY()
	TObjectPtr<UStaticMesh> M_DebrisMesh;

	// The hull mesh carrying the panel layout and the panel material slot.
	UPROPERTY()
	TWeakObjectPtr<UStaticMeshComponent> M_PanelMesh;

	// Registered armour mesh whose plates the zones cover: the panel mesh itself, or the mesh it is attached to.
	UPROPERTY()
	TWeakObjectPtr<UMeshComponent> M_ArmorMesh;

	UPROPERTY()
	TWeakObjectPtr<UArmorCalculation> M_ArmorCalculation;

	// Panel centres in the owner's space, indexed like the layout; module hit positions use the same space.
	TArray<FVector, TInlineAllocator<AddOnArmorPanelMask::MaxPanelsPerMesh>> M_PanelCentres;

	// Panel sides in the panel mesh's own space, indexed like the layout; the debris mesh is a left-side panel.
	TArray<EAddOnArmorPanelSide, TInlineAllocator<AddOnArmorPanelMask::MaxPanelsPerMesh>> M_PanelMeshSides;

	TArray<FAddOnArmorPanelZoneState> M_Zones;

	// Which armour-off sound the tank plays, from its subtype when the zones install.
	EAddOnArmorOffSoundWeight M_ArmorOffSoundWeight = EAddOnArmorOffSoundWeight::Medium;

	// Bit per hidden panel, written to the panel mesh's custom primitive data.
	int32 M_HiddenPanelMask = 0;

	FDelegateHandle M_ZoneHitHandle;

	bool GetIsValidPanelMesh() const;
	bool GetIsValidArmorCalculation() const;
	bool GetIsValidDebrisSystem() const;

	/** @return The owner's static mesh component whose mesh carries a panel layout, or null. */
	UStaticMeshComponent* FindPanelMesh(const UAddOnArmorPanelLayout*& OutLayout) const;
	/** @return The panel mesh when registered for armour, else its nearest registered attach parent, or null. */
	UMeshComponent* FindArmorMesh(const UArmorCalculation& ArmorCalculation) const;
	bool CachePanelCentres(const UStaticMeshComponent& PanelMesh, const UAddOnArmorPanelLayout& Layout);
	void CacheDebris(const UAddOnArmorPanelLayout& Layout);
	void CacheArmorOffSoundWeight();

	/**
	 * @brief Registers the zone's add-on armour on its plates and installs its module with that coverage.
	 * @param Settings Zone to install; its panels are the cached panels on its side of the vehicle.
	 * @return False when the zone has no panels or plates, or the armour rejected the module or its coverage.
	 */
	bool InstallZone(const FAddOnArmorPanelZoneSettings& Settings);

	void OnAddOnArmorZoneHit(int32 ModuleId, const FVector& HullLocalHitPosition);
	int32 FindZoneIndexByModuleId(int32 ModuleId) const;
	/**
	 * @brief Hides or restores the zone's panels to match its module state.
	 * @param ZoneIndex Zone to sync.
	 * @param bThrowDebris Whether newly hidden panels fly off as debris; false for loads, which only restore a state.
	 */
	void SyncZoneFromModuleState(int32 ZoneIndex, bool bThrowDebris);

	/**
	 * @brief Damage hides panels in proportion to the lost module health; yellow keeps one panel, red none.
	 * @param Snapshot Current state of the zone's module.
	 * @param PanelCount Panels of the zone.
	 * @return The number of panels that should be hidden.
	 */
	static int32 GetHiddenPanelTarget(const FVehicleModuleSnapshot& Snapshot, int32 PanelCount);

	/** @return False when every panel of the zone is already hidden. */
	bool HidePanelClosestToLastHit(FAddOnArmorPanelZoneState& Zone);
	void RestoreLastHiddenPanel(FAddOnArmorPanelZoneState& Zone);
	FVector GetZoneCentre(const FAddOnArmorPanelZoneState& Zone) const;
	void WriteHiddenPanelMask() const;

	/**
	 * @brief Throws the given panels off the hull in one pooled Niagara burst; nothing while fog of war hides the owner.
	 * @param Zone Zone the panels belong to; its last hit pushes the pieces away from the impact.
	 * @param PanelIndices Layout indices of the panels that were just hidden.
	 */
	void ThrowPanelDebris(const FAddOnArmorPanelZoneState& Zone, TConstArrayView<int32> PanelIndices) const;
	/**
	 * @brief Plays the weight class's armour-off clang at the zone's last hit; nothing while fog of war hides the owner.
	 * @param Zone Zone that just lost panels.
	 */
	void PlayArmorOffSound(const FAddOnArmorPanelZoneState& Zone) const;
	/** @return World Z of the ground under the location, or the owner's Z when nothing is hit. */
	float GetGroundHeightBelow(const FVector& WorldLocation) const;
};
