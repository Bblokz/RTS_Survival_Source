// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Audio/RTSVoiceLines/RTSVoicelines.h"
#include "RTS_Survival/TechTree/Technologies/Technologies.h"
#include "RTS_Survival/UnitData/UnitCost.h"
#include "ResearchTechnologyAbilityComp.generated.h"

class ASquadController;
class ICommands;
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

	// Optional custom announcer lines for this technology; empty means use only the generic completion line.
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

	void TechResearchComplete(ETechnology CompletedTechnology);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FResearchTechnologyAbilitySettings ResearchTechnologyAbilitySettings;

private:
	void RefreshOwnerReferences();
	void BeginPlay_InitRuntimeSettings();
	void BeginPlay_InitValidateSettings() const;
	void BeginPlay_InitAddAbility();
	void BeginPlay_InitAddAbilityToCommandsNextTick();
	void AddAbilityToSquad(ASquadController* Squad);
	void AddAbilityToCommands();
	void PlayCompletedAnnouncerVoiceLine();
	void SwapToNextTechnology(ETechnology CompletedTechnology);
	void BuildOrderedTechnologyEntries(
		const UResearchTechnologyAbilityTechnologyData* TechnologyData,
		TArray<FResearchTechnologyAbilityTechnologyRuntimeData>& OutTechnologyEntries) const;
	FResearchTechnologyAbilityTechnologyRuntimeData CreateRuntimeTechnologyData(
		const UResearchTechnologyAbilityTechnologyData& TechnologyData) const;
	FUnitAbilityEntry CreateCurrentAbilityEntry() const;

	bool GetIsValidOwnerCommandsInterface() const;

	UPROPERTY()
	TScriptInterface<ICommands> M_OwnerCommandsInterface;

	FResearchTechnologyAbilityTechnologyRuntimeData M_CurrentTechnologyData;
	TArray<FResearchTechnologyAbilityTechnologyRuntimeData> M_OrderedTechnologyEntries;
	int32 M_CurrentTechnologyIndex = INDEX_NONE;
};
