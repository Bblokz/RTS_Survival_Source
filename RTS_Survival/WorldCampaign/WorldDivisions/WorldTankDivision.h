// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionBase.h"
#include "WorldTankDivision.generated.h"

/**
 * @brief World division whose damage removes whole tank subtype entries from its roster.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API AWorldTankDivision : public AWorldDivisionBase
{
	GENERATED_BODY()

public:
	virtual void PostInitializeComponents() override;
	virtual void ApplyWorldDivisionDamage(int32 DamageEntryCount) override;

	void SetTankSubtypes(const TArray<ETankSubtype>& TankSubtypes);
	const TArray<ETankSubtype>& GetTankSubtypes() const { return M_TankSubtypes; }

protected:
	virtual void FillSubtypeSaveData(FWorldDivisionSaveData& OutDivisionSaveData) const override;
	virtual void RestoreSubtypeSaveData(const FWorldDivisionSaveData& DivisionSaveData) override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World Campaign|World Divisions",
		meta=(AllowPrivateAccess="true"))
	TArray<ETankSubtype> M_TankSubtypes;

	int32 M_StartingTankCount = 0;

	void RefreshStrengthFromTankCount();
};
