// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Resources/ResourceComponent/ResourceComponent.h"
#include "RTS_Survival/TechTree/Technologies/Technologies.h"
#include "UObject/Object.h"
#include "PlayerTechManager.generated.h"

class AAircraftMaster;
class ABuildingExpansion;
class ACPPController;
class ANomadicVehicle;
class ASquadController;
class ATankMaster;
class UTechnologyEffect;
enum class ERTSResourceType : uint8;

/**
 * @brief Owned by the player controller to track researched technologies and apply their effects.
 *
 * Gameplay systems query this manager before allowing tech-gated actions or displaying tech requirements.
 */
UCLASS()
class RTS_SURVIVAL_API UPlayerTechManager : public UActorComponent
{
	GENERATED_BODY()

public:
	inline bool HasTechResearched(const ETechnology Tech) const { return M_ResearchedTechs.Contains(Tech); }
	TArray<ETechnology> GetMissingRequiredTechnologies(const TArray<ETechnology>& RequiredTechnologies) const;
	bool HasAllRequiredTechnologies(const TArray<ETechnology>& RequiredTechnologies) const;
	void OnTechResearched(const ETechnology Tech);
	static constexpr uint8 PlayerTechOwnerIndex = 1;

	void CheckTechsToApplyToTank(ATankMaster* Tank) const;
	void CheckTechsToApplyToNomadic(ANomadicVehicle* Nomadic) const;
	void CheckTechsToApplyToSquad(ASquadController* Squad) const;
	void CheckTechsToApplyToBuildingExpansion(ABuildingExpansion* BuildingExpansion) const;
	void CheckTechsToApplyToAircraft(AAircraftMaster* Aircraft) const;

	void InitTechsInManager(ACPPController* Controller);

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TSet<ETechnology> M_ResearchedTechs;

	UPROPERTY()
	TSet<ETechnology> M_TechnologiesWaitingForEffectLoad;

	UPROPERTY()
	TSet<TObjectPtr<UTechnologyEffect>> M_ResearchedTechnologyEffects;

	UPROPERTY()
	TMap<ETechnology, TSoftClassPtr<UTechnologyEffect>> M_TechnologyEffectsMap;

	void OnTechnologyEffectLoaded(ETechnology Tech);
	void RegisterAndApplyLoadedTechnology(ETechnology Tech, UTechnologyEffect* TechEffect);
	void ApplyTechnologyToCurrentUnits(UTechnologyEffect* TechEffect) const;
};
