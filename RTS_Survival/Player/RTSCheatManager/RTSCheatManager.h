// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "RTSCheatManager.generated.h"

/**
 * @brief Custom Cheat Manager for adding custom console commands.
 *
 * This class extends UCheatManager and allows us to define custom console commands
 * that can be executed from the in-game console.
 */

UCLASS()
class RTS_SURVIVAL_API URTSCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	/**
 * @brief Console command to debug energy components.
 *
 * When executed, this command prints an overview of all registered energy components,
 * indicating whether they supply or demand energy.
 */
	UFUNCTION(exec)
	void RTSDebugEnergy();
	

	UFUNCTION(exec)
	void RTSCheatEnergy(const int32 Amount);
	UFUNCTION(Exec)
	void DebugResource();

	UFUNCTION(Exec)
	void RTSCheatRadixite(const int32 Amount);

	UFUNCTION(Exec)
	void RTSCheatMetal(const int32 Amount);

	UFUNCTION(Exec)
	void RTSCheatVehicleParts(const int32 Amount);

	UFUNCTION(Exec)
	void RTSCheatFuel(const int32 Amount);

	UFUNCTION(Exec)
	void RTSCheatAmmo(const int32 Amount);

	UFUNCTION(Exec)
	void RTSCheatResource(const int32 Amount);
	UFUNCTION(Exec)
	void RTSCheatResources(const int32 Amount);
	
	UFUNCTION(Exec)
	void RTSCheatStorage(const int32 Amount);

	UFUNCTION(Exec)
	void RTSSubtractResource(const int32 Amount);

	UFUNCTION(Exec)
	void RTSCheatBlueprints(const int32 Amount);

	UFUNCTION(Exec)
	void RTSCheatTraining(const int32 Time);

	UFUNCTION(Exec)
	void RTSEnableErrors(const bool bEnable);

	UFUNCTION(Exec)
	void RTSHideUI(const bool bEnable);
	
	UFUNCTION(Exec)
	void RTSDebugEnemy() const;

	UFUNCTION(Exec)
	void RTSDebugFormation() const;
	UFUNCTION(Exec)
	void RTSDestroyBxps() const;

	UFUNCTION(Exec)
	void RTSOptimizeUnits(const bool bEnable) const;

	/** RTSCheatSetUnitVariant <UnitType> <Subtype> <Variant>: numeric enum values; affects units that spawn afterwards. */
	UFUNCTION(Exec)
	void RTSCheatSetUnitVariant(const int32 UnitType, const int32 Subtype, const int32 Variant);

	/** Prints every stored selection and the implemented count per unit with a choice. */
	UFUNCTION(Exec)
	void RTSCheatPrintUnitVariants();

	
};
