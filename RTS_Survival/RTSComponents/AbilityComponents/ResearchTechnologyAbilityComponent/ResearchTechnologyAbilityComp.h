// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Audio/RTSVoiceLines/RTSVoicelines.h"
#include "RTS_Survival/FactionSystem/FactionSelection/FactionPlayerController.h"
#include "RTS_Survival/TechTree/Technologies/Technologies.h"
#include "RTS_Survival/UnitData/UnitCost.h"
#include "ResearchTechnologyAbilityComp.generated.h"

class ASquadController;
class ICommands;
class UPlayerTechManager;
class USoundBase;
struct FUnitAbilityEntry;

/**
 * @brief Edited inline on a research component to define one step in a nested research chain.
 */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UResearchTechnologyAbilityTechnologyData : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETechnology Technology = ETechnology::Tech_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<ETechnology> RequiredTechnologies;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FUnitCost Costs = FUnitCost();

	// Designers can nest follow-up research entries as deeply as this command-card slot should progress.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced)
	TArray<TObjectPtr<UResearchTechnologyAbilityTechnologyData>> FollowUpTechnologies;

	// Optional custom announcer lines for this technology; empty means no custom line is played.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVoiceLineData CompletedAnnouncerVoiceLines;
};

USTRUCT(Blueprintable)
struct FResearchTechnologyAbilitySettings
{
	GENERATED_BODY()

	// Attempts to add the ability to this index of the Unit's Ability Array.
	// Reverts to first empty index if negative or already occupied.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PreferredAbilityIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced)
	TObjectPtr<UResearchTechnologyAbilityTechnologyData> TechnologyData = nullptr;
};

struct FResearchTechnologyAbilityTechnologyRuntimeData
{
	ETechnology Technology = ETechnology::Tech_NONE;
	TArray<ETechnology> RequiredTechnologies;
	FUnitCost Costs = FUnitCost();
	FVoiceLineData CompletedAnnouncerVoiceLines;
};

/**
 * @brief Add this to a command-capable unit to expose an instant technology research action.
 *
 * The component owns the current technology subtype and advances/removes the command-card entry
 * when research completes.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UResearchTechnologyAbilityComp : public UActorComponent
{
	GENERATED_BODY()

public:
	UResearchTechnologyAbilityComp();

	ETechnology GetTechnology() const { return M_CurrentTechnologyData.Technology; }
	const TArray<ETechnology>& GetRequiredTechnologies() const { return M_CurrentTechnologyData.RequiredTechnologies; }
	const TArray<ETechnology>& GetRequiredTechnologiesForCurrentTechnology() const
	{
		return GetRequiredTechnologies();
	}

	/**
	 * @brief Legacy entry point for completed research notifications routed through this component.
	 * @param CompletedTechnology Technology that just became unavailable to research.
	 */
	void TechResearchComplete(ETechnology CompletedTechnology);

	/**
	 * @brief Keeps this command-card slot in sync when any registered tech source completes research.
	 * @param ResearchedTechnology Technology that must be removed, swapped past, or cut out of this chain.
	 */
	void AccountForResearchedTechnology(ETechnology ResearchedTechnology);

	/**
	 * @brief Plays custom audio before this component advances away from the matching tech.
	 * @param CompletedTechnology Technology that may match this component's current entry.
	 * @return True only when this component owned and played a valid custom completion line.
	 */
	bool TryPlayCompletedAnnouncerVoiceLine(ETechnology CompletedTechnology);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<ERTSFaction, FResearchTechnologyAbilitySettings> TechAbilitySettingsPerFaction;
	
private:
	FResearchTechnologyAbilitySettings M_FactionChosenTechAbilitySettings;
	void RefreshOwnerReferences();
	// Select faction data before registering, because tech chains differ per player faction.
	void BeginPlay_SetActiveTechAbilityForFaction();
	// Flatten the nested designer data so already-researched nested techs can be cut out safely.
	void BeginPlay_InitRuntimeSettings();
	void BeginPlay_InitValidateSettings() const;
	// Register after faction/runtime setup, so manager replay can immediately prune researched techs.
	void BeginPlay_RegisterAtPlayerTechManager();
	void BeginPlay_InitAddAbility();
	void BeginPlay_InitAddAbilityToCommandsNextTick();
	void AddAbilityToSquad(ASquadController* Squad);
	void AddAbilityToCommands();
	/**
	 * @brief Advances the command-card slot after its currently offered tech has completed.
	 * @param CompletedTechnology Previous command subtype that must be swapped or removed from UI.
	 */
	void SwapToNextTechnology(ETechnology CompletedTechnology);

	/**
	 * @brief Cuts a researched nested tech out so deeper follow-ups can surface later.
	 * @param ResearchedTechnology Technology represented by the removed runtime entry.
	 * @param ResearchedTechnologyIndex Current index of that entry in the flattened chain.
	 */
	void RemoveResearchedTechnologyFromQueuedEntries(ETechnology ResearchedTechnology, int32 ResearchedTechnologyIndex);

	/**
	 * @brief Searches the mutable chain because manager notifications may remove nested entries.
	 * @param Technology Technology to locate in this component's remaining chain.
	 * @return The current chain index, or INDEX_NONE when this component does not offer it.
	 */
	int32 FindOrderedTechnologyIndex(ETechnology Technology) const;

	/**
	 * @brief Mirrors internal tech-chain changes to the owner's command card only after UI insertion.
	 * @param CompletedTechnology Previous command subtype used to find the command-card entry.
	 */
	void UpdateCommandCardForResearchedCurrentTechnology(ETechnology CompletedTechnology);

	/**
	 * @brief Flattens nested designer data so manager notifications can skip completed entries.
	 * @param TechnologyData Root or follow-up node from the configured research chain.
	 * @param OutTechnologyEntries Mutable runtime chain in command-card progression order.
	 */
	void BuildOrderedTechnologyEntries(
		const UResearchTechnologyAbilityTechnologyData* TechnologyData,
		TArray<FResearchTechnologyAbilityTechnologyRuntimeData>& OutTechnologyEntries) const;

	/**
	 * @brief Copies blueprint-owned config into runtime data that can be safely reordered.
	 * @param TechnologyData Config node to copy into the runtime chain.
	 * @return Runtime entry used by the command-card sync path.
	 */
	FResearchTechnologyAbilityTechnologyRuntimeData CreateRuntimeTechnologyData(
		const UResearchTechnologyAbilityTechnologyData& TechnologyData) const;
	FUnitAbilityEntry CreateCurrentAbilityEntry() const;

	bool GetIsValidOwnerCommandsInterface() const;
	bool GetIsValidPlayerTechManager() const;

	UPROPERTY()
	TScriptInterface<ICommands> M_OwnerCommandsInterface;

	UPROPERTY()
	TObjectPtr<UPlayerTechManager> M_PlayerTechManager = nullptr;

	FResearchTechnologyAbilityTechnologyRuntimeData M_CurrentTechnologyData;
	// Flattened mutable chain; manager notifications remove researched nested techs from this list.
	TArray<FResearchTechnologyAbilityTechnologyRuntimeData> M_OrderedTechnologyEntries;
	int32 M_CurrentTechnologyIndex = INDEX_NONE;
	bool bM_HasAddedAbilityToCommands = false;
};
