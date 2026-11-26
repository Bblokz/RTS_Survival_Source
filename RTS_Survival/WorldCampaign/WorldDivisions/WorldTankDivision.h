// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionBase.h"
#include "WorldTankDivision.generated.h"

class UWorldDivisionTankFormation;

/**
 * @brief World division whose damage removes whole tank subtype entries from its roster. Its formation component
 * shows one small tank per roster entry.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API AWorldTankDivision : public AWorldDivisionBase
{
	GENERATED_BODY()

public:
	AWorldTankDivision();

	virtual void PostInitializeComponents() override;

	UWorldDivisionTankFormation* GetTankFormation() const { return M_TankFormation; }

	/**
	 * @brief Removes whole tank entries from the roster and updates current division strength.
	 * @param DamageEntryCount Number of tank entries to remove from the end of the array.
	 */
	virtual void ApplyWorldDivisionDamage(int32 DamageEntryCount) override;

	/**
	 * @brief Replaces tank composition and treats the new count as this division's full roster.
	 * @param TankSubtypes Tank subtype entries where duplicates represent multiple tanks.
	 */
	void SetTankSubtypes(const TArray<ETankSubtype>& TankSubtypes);
	const TArray<ETankSubtype>& GetTankSubtypes() const { return M_TankSubtypes; }
	virtual int32 GetRemainingEntryCount() const override { return M_TankSubtypes.Num(); }

protected:
	virtual void BeginPlay() override;

	/**
	 * @brief Writes tank composition into generic world division save data.
	 * @param OutDivisionSaveData Save data being assembled by the base class.
	 */
	virtual void FillSubtypeSaveData(FWorldDivisionSaveData& OutDivisionSaveData) const override;

	/**
	 * @brief Restores tank composition and estimates original count from saved strength.
	 * @param DivisionSaveData Save data loaded for this tank division.
	 */
	virtual void RestoreSubtypeSaveData(const FWorldDivisionSaveData& DivisionSaveData) override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World Campaign|World Divisions",
		meta=(AllowPrivateAccess="true"))
	TArray<ETankSubtype> M_TankSubtypes;

	int32 M_StartingTankCount = 0;

	// Draws the roster as small tanks; rebuilt whenever the roster is set or restored.
	UPROPERTY()
	TObjectPtr<UWorldDivisionTankFormation> M_TankFormation = nullptr;

	bool GetIsValidTankFormation() const;

	/**
	 * @brief Recalculates current strength from remaining tanks versus starting tank count.
	 */
	void RefreshStrengthFromTankCount();
};
