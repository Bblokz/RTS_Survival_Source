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
class UResearchTechnologyAbilityComp;
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

	/**
	 * @brief Blocks duplicate commands while a researched tech is still loading its effect asset.
	 * @param Tech Technology to check against completed and pending registrations.
	 * @return True when the tech should no longer be offered or queued.
	 */
	bool GetIsTechnologyResearchedOrPending(ETechnology Tech) const;

	TArray<ETechnology> GetMissingRequiredTechnologies(const TArray<ETechnology>& RequiredTechnologies) const;
	bool HasAllRequiredTechnologies(const TArray<ETechnology>& RequiredTechnologies) const;

	/**
	 * @brief Commits a newly researched tech and fans out command-card updates from one authority.
	 * @param Tech Technology accepted from command execution or profile loading.
	 */
	void OnTechResearched(const ETechnology Tech);

	/**
	 * @brief Registers a command-card tech source after it resolves its faction-specific tech chain.
	 * @param ResearchTechnologyAbilityComp Component that must be replayed against existing tech state.
	 */
	void RegisterResearchTechnologyAbilityComp(UResearchTechnologyAbilityComp* ResearchTechnologyAbilityComp);
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

	// Weak registrations let destroyed units disappear without keeping their research components alive.
	UPROPERTY()
	TArray<TWeakObjectPtr<UResearchTechnologyAbilityComp>> M_ResearchTechnologyAbilityComps;

	UPROPERTY()
	TMap<ETechnology, TSoftClassPtr<UTechnologyEffect>> M_TechnologyEffectsMap;

	/**
	 * @brief Finishes async effect registration before registered command cards are notified.
	 * @param Tech Technology whose soft effect class has completed loading.
	 */
	void OnTechnologyEffectLoaded(ETechnology Tech);

	/**
	 * @brief Broadcasts researched techs so every registered command-card slot prunes itself.
	 * @param Tech Technology that is no longer available to research.
	 */
	void NotifyResearchTechnologyAbilityComps(ETechnology Tech);

	// Purge dead weak registrations before broadcast/replay to avoid stale UObject access.
	void RemoveInvalidResearchTechnologyAbilityComps();

	/**
	 * @brief Replays current manager state into late-spawned research components.
	 * @return Researched and pending techs that should already be absent from command cards.
	 */
	TArray<ETechnology> GetResearchedAndPendingTechnologies() const;

	/**
	 * @brief Stores the loaded effect before units and research components are told the tech exists.
	 * @param Tech Technology represented by the loaded effect object.
	 * @param TechEffect Runtime effect instance owned by this manager.
	 */
	void RegisterAndApplyLoadedTechnology(ETechnology Tech, UTechnologyEffect* TechEffect);
	void ApplyTechnologyToCurrentUnits(UTechnologyEffect* TechEffect) const;
};
