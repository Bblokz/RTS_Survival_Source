// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "WorldDivisionTankFormation.generated.h"

class UBoxComponent;
class UNiagaraComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * @brief Where the tanks of a division stand around its centre, as pure functions so the layout is unit tested.
 */
namespace WorldDivisionFormationLayout
{
	/**
	 * @brief Lays the slots out in rows across the division's forward direction, front row first, each row centred.
	 * Rows are balanced: four slots with at most three per row give two rows of two.
	 * @param SlotCount Number of roster entries.
	 * @param MaxPerRow Most tanks side by side; values below 1 count as 1.
	 * @param ColumnSpacing Distance between two tanks side by side.
	 * @param RowSpacing Distance between two rows.
	 * @return One offset per slot in the division's local space: X forward, Y right.
	 */
	RTS_SURVIVAL_API TArray<FVector2D> GetSlotOffsets(int32 SlotCount, int32 MaxPerRow, float ColumnSpacing,
	                                                  float RowSpacing);

	/** @return Half the size of the box around every slot offset; zero without slots. */
	RTS_SURVIVAL_API FVector2D GetFootprintHalfSize(const TArray<FVector2D>& SlotOffsets);
}

/** What one formation slot shows. */
enum class EWorldDivisionTankSlotState : uint8
{
	Alive,
	// Destroyed in the roster, still standing on the map until the fire mission show blows it up.
	DeferredCasualty,
	Wreck
};

/**
 * @brief One tank of the formation. Slot i is roster entry i.
 */
USTRUCT()
struct FWorldDivisionTankSlot
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> PieceMesh = nullptr;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> WreckFire = nullptr;

	ETankSubtype Subtype = ETankSubtype::Tank_None;

	// Offset from the division's root: X forward, Y right.
	FVector2D LocalOffset = FVector2D::ZeroVector;

	EWorldDivisionTankSlotState State = EWorldDivisionTankSlotState::Alive;
};

/**
 * @brief Created by AWorldTankDivision. Shows one small tank per roster entry, in rows facing the division's forward
 * direction, and turns the tanks a hit removed into wrecks. Meshes and sizes come from UWorldBatteryFireSettings.
 * Without any loadable tank mesh it shows nothing and the Blueprint's own look stays.
 */
UCLASS(ClassGroup=(WorldCampaign))
class RTS_SURVIVAL_API UWorldDivisionTankFormation : public UActorComponent
{
	GENERATED_BODY()

public:
	UWorldDivisionTankFormation();

	/**
	 * @brief Throws the current tanks and wrecks away and builds one tank per roster entry. Does nothing before the
	 * owner has begun play or when it has no root to attach to.
	 * @param TankSubtypes The division's roster.
	 */
	void RebuildFromRoster(const TArray<ETankSubtype>& TankSubtypes);

	/**
	 * @brief The roster lost its last entries. Their tanks become wrecks at once, or stay standing as deferred
	 * casualties while a fire mission show is about to blow them up.
	 * @param NewEntryCount Roster entries left; every slot from this index on is a casualty.
	 */
	void HandleRosterShrunk(int32 NewEntryCount);

	/** While on, casualties keep standing until ExplodeSlot or FlushDeferredCasualties. */
	void SetDefersCasualtyVisuals(bool bDefers);

	/**
	 * @brief Blows one tank up: explosion effect, then its wreck mesh (or nothing when it has none).
	 * @param SlotIndex Slot to destroy; ignored when it is no standing tank.
	 * @return true when a tank was blown up.
	 */
	bool ExplodeSlot(int32 SlotIndex);

	/** Turns every casualty that was not blown up into a wreck without an effect, and stops deferring. */
	void FlushDeferredCasualties();

	int32 GetSlotCount() const { return M_Slots.Num(); }

	int32 GetWreckCount() const;

	/** @return true when at least one tank mesh is shown. */
	bool GetShowsTanks() const { return M_Slots.Num() > 0; }

	/**
	 * @brief World location of a slot, for aiming a shell at it.
	 * @param SlotIndex Slot to look up.
	 * @param OutLocation The slot's location on the map.
	 * @return false when the formation shows no such slot.
	 */
	bool TryGetSlotWorldLocation(int32 SlotIndex, FVector& OutLocation) const;

private:
	void DestroySlots();

	/**
	 * @brief Creates the mesh component of one slot.
	 * @param PieceMesh Mesh to show.
	 * @param LocalOffset Slot offset from the division's root.
	 * @return The registered component, or nullptr without an owner root.
	 */
	UStaticMeshComponent* CreatePieceComponent(UStaticMesh* PieceMesh, const FVector2D& LocalOffset);

	/** Scales and seats a mesh so its longest side has the configured length and its underside is on the map. */
	void FitPieceToMap(UStaticMeshComponent& PieceComponent, const UStaticMesh& PieceMesh,
	                   const FVector2D& LocalOffset) const;

	UStaticMesh* LoadPieceMesh(ETankSubtype Subtype) const;
	UStaticMesh* LoadWreckMesh(ETankSubtype Subtype) const;

	void TurnSlotIntoWreck(FWorldDivisionTankSlot& Slot, bool bWithFire);

	/** Hides the Blueprint's own meshes while tanks are shown; their collision stays, so the piece stays clickable. */
	void HideOwnerMeshes();
	void RestoreOwnerMeshes();

	/** Covers the whole formation so every tank answers the cursor, since the tanks have no collision. */
	void UpdateClickBox(const TArray<FVector2D>& SlotOffsets);

	// Loads the plate mesh once per rebuild; null when the setting is empty or the asset is missing.
	UStaticMesh* LoadPlateMesh() const;
	void CreatePlate(UStaticMesh* PlateMesh, const TArray<FVector2D>& SlotOffsets);

	UPROPERTY()
	TArray<FWorldDivisionTankSlot> M_Slots;

	// Meshes of the Blueprint that this component hid; shown again when no tank can be shown.
	UPROPERTY()
	TArray<TWeakObjectPtr<UStaticMeshComponent>> M_HiddenOwnerMeshes;

	UPROPERTY()
	TObjectPtr<UBoxComponent> M_ClickBox = nullptr;

	// The dark plate under the tanks; created after the owner's meshes are hidden, so it is not hidden with them.
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_Plate = nullptr;

	// Height the tanks are lifted by while a plate is shown; 0 without a plate.
	float M_PieceBaseHeight = 0.f;

	bool bM_DefersCasualtyVisuals = false;
};
