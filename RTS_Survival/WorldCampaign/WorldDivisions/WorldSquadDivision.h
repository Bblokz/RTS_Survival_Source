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
	virtual void ApplyWorldDivisionDamage(int32 DamageEntryCount) override;

	void SetSquadSubtypes(const TArray<ESquadSubtype>& SquadSubtypes);
	const TArray<ESquadSubtype>& GetSquadSubtypes() const { return M_SquadSubtypes; }

protected:
	virtual void FillSubtypeSaveData(FWorldDivisionSaveData& OutDivisionSaveData) const override;
	virtual void RestoreSubtypeSaveData(const FWorldDivisionSaveData& DivisionSaveData) override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World Campaign|World Divisions",
		meta=(AllowPrivateAccess="true"))
	TArray<ESquadSubtype> M_SquadSubtypes;

	int32 M_StartingSquadCount = 0;

	void RefreshStrengthFromSquadCount();
};
