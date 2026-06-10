// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansionEnums.h"
#include "RTS_Survival/TechTree/Technologies/Technologies.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "TechnologyEffect.generated.h"

class AAircraftMaster;
class ABuildingExpansion;
class ANomadicVehicle;
class ASquadController;
class ATankMaster;
class UBehaviour;

/**
 * @brief Data asset used by the player tech manager to apply researched upgrades to eligible live units.
 */
UCLASS()
class RTS_SURVIVAL_API UTechnologyEffect : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	ETechnology GetTechnology() const { return Technology; }
	void SetTechnology(const ETechnology NewTechnology) { Technology = NewTechnology; }

	TArray<ENomadicSubtype> GetNomadicsToApplyTo() const;
	TArray<ETankSubtype> GetTanksToApplyTo() const;
	TArray<ESquadSubtype> GetSquadsToApplyTo() const;
	TArray<EBuildingExpansionType> GetBuildingExpansionsToApplyTo() const;
	TArray<EAircraftSubtype> GetAircraftToApplyTo() const;

	void ApplyOnTank(ATankMaster* Tank);
	void ApplyOnNomadic(ANomadicVehicle* Nomadic);
	void ApplyOnSquad(ASquadController* Squad);
	void ApplyOnBuildingExpansion(ABuildingExpansion* BuildingExpansion);
	void ApplyOnAircraft(AAircraftMaster* Aircraft);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Technology")
	ETechnology Technology = ETechnology::Tech_NONE;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Technology|Affected Units")
	TArray<ENomadicSubtype> NomadicsToApplyTo;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Technology|Affected Units")
	TArray<ETankSubtype> TanksToApplyTo;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Technology|Affected Units")
	TArray<ESquadSubtype> SquadsToApplyTo;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Technology|Affected Units")
	TArray<EBuildingExpansionType> BuildingExpansionsToApplyTo;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Technology|Affected Units")
	TArray<EAircraftSubtype> AircraftToApplyTo;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Technology|Behaviour")
	TSubclassOf<UBehaviour> BehaviourClassToApply;

	virtual TArray<ENomadicSubtype> GetNomadicsToApplyTo_Internal() const;
	virtual TArray<ETankSubtype> GetTanksToApplyTo_Internal() const;
	virtual TArray<ESquadSubtype> GetSquadsToApplyTo_Internal() const;
	virtual TArray<EBuildingExpansionType> GetBuildingExpansionsToApplyTo_Internal() const;
	virtual TArray<EAircraftSubtype> GetAircraftToApplyTo_Internal() const;

	virtual void ApplyOnTank_Internal(ATankMaster* Tank);
	virtual void ApplyOnNomadic_Internal(ANomadicVehicle* Nomadic);
	virtual void ApplyOnSquad_Internal(ASquadController* Squad);
	virtual void ApplyOnBuildingExpansion_Internal(ABuildingExpansion* BuildingExpansion);
	virtual void ApplyOnAircraft_Internal(AAircraftMaster* Aircraft);

	virtual void OnTechAppliedToActor(AActor* Actor);

private:
	bool ShouldApplyToActor(AActor* Actor) const;
	void TryAddBehaviourToActor(AActor* Actor) const;
};
