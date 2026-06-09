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

USTRUCT(Blueprintable)
struct FResearchTechnologyAbilitySettings
{
	GENERATED_BODY()

	// Attempts to add the ability to this index of the Unit's Ability Array.
	// Reverts to first empty index if negative or already occupied.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PreferredAbilityIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETechnology Technology = ETechnology::Tech_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<ETechnology> RequiredTechnologies;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FUnitCost Costs = FUnitCost();

	// Ordered chain of technology abilities this component exposes after the current tech completes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<ETechnology> FollowUpTechnologies;

	// Optional custom announcer lines for this technology; empty means use only the generic completion line.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
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

	ETechnology GetTechnology() const { return M_CurrentTechnology; }
	const TArray<ETechnology>& GetRequiredTechnologies() const { return M_CurrentRequiredTechnologies; }

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
	void AddAbilityToSquad(ASquadController* Squad);
	void AddAbilityToCommands();
	void PlayCompletedAnnouncerVoiceLine();
	void SwapToNextTechnology(ETechnology CompletedTechnology);
	FUnitAbilityEntry CreateCurrentAbilityEntry() const;

	bool GetIsValidOwnerCommandsInterface() const;

	UPROPERTY()
	TScriptInterface<ICommands> M_OwnerCommandsInterface;

	ETechnology M_CurrentTechnology = ETechnology::Tech_NONE;
	TArray<ETechnology> M_CurrentRequiredTechnologies;
	int32 M_NextFollowUpTechnologyIndex = 0;
};
