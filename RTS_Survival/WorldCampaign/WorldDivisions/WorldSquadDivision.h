// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionBase.h"
#include "WorldSquadDivision.generated.h"

/**
 * @brief World division whose damage removes whole squad subtype entries from its roster.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API AWorldSquadDivision : public AWorldDivisionBase
{
	GENERATED_BODY()

public:
	virtual void PostInitializeComponents() override;

	/**
	 * @brief Removes whole squad entries from the roster and updates current division strength.
	 * @param DamageEntryCount Number of squad entries to remove from the end of the array.
	 */
	virtual void ApplyWorldDivisionDamage(int32 DamageEntryCount) override;

	/**
	 * @brief Replaces squad composition and treats the new count as this division's full roster.
	 * @param SquadSubtypes Squad subtype entries where duplicates represent multiple squads.
	 */
	void SetSquadSubtypes(const TArray<ESquadSubtype>& SquadSubtypes);
	const TArray<ESquadSubtype>& GetSquadSubtypes() const { return M_SquadSubtypes; }

protected:
	/**
	 * @brief Writes squad composition into generic world division save data.
	 * @param OutDivisionSaveData Save data being assembled by the base class.
	 */
	virtual void FillSubtypeSaveData(FWorldDivisionSaveData& OutDivisionSaveData) const override;

	/**
	 * @brief Restores squad composition and estimates original count from saved strength.
	 * @param DivisionSaveData Save data loaded for this squad division.
	 */
	virtual void RestoreSubtypeSaveData(const FWorldDivisionSaveData& DivisionSaveData) override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World Campaign|World Divisions",
		meta=(AllowPrivateAccess="true"))
	TArray<ESquadSubtype> M_SquadSubtypes;

	int32 M_StartingSquadCount = 0;

	/**
	 * @brief Recalculates current strength from remaining squads versus starting squad count.
	 */
	void RefreshStrengthFromSquadCount();
};
