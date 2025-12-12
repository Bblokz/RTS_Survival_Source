// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTSExperienceLevels.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"

#include "ExperienceComponent.generated.h"

enum class ESquadSubtype : uint8;
enum class ETankSubtype : uint8;
class ACPPGameState;
struct FTrainingOption;
class IExperienceInterface;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API URTSExperienceComp : public UActorComponent
{
	GENERATED_BODY()

public:
	URTSExperienceComp();

	void ReceiveExperience(const float Amount);

	inline float GetExperienceWorth() const { return M_ExperienceWorth; };

	void InitExperienceComponent(const TScriptInterface<IExperienceInterface>& Owner,
	                             const FTrainingOption& UnitTypeAndSubtype, const int8 OwningPlayer);

	// Returns a lambda callback that safely calls ReceiveExperience.
	TFunction<void(float)> GetExperienceCallback();

	// Registers this component with a UI manager so that it can update the experience bar.
	void SetExperienceBarSelected(const bool bSetSelected, TObjectPtr<class UActionUIManager> ActionUIManager);

	//  Calculates and pushes the updated experience info to the UI.
	void UpdateExperienceUI() const;

private:
	UPROPERTY()
	FUnitExperience M_UnitExperience;

	TScriptInterface<IExperienceInterface> M_Owner;

	// Pointer to the action UI manager (if registered).
	UPROPERTY()
	TObjectPtr<UActionUIManager> M_ActionUIManager;

	int32 M_ExperienceWorth = 0;
	// If ever needed to update the experience settings from the game state.
	int8 M_OwningPlayer = 0;

	bool GetIsValidOwner() const;
	void OnLevelUpOwner() const;

	float GetPercentageProgressToNextLevel(int32& OutExpNeededNextLevel, int32 InCumulativeExp) const;
	float GetPercentageProgressAtMaxLevel(int32& OutExpNeededNextLevel, int32 InCumulativeExp) const;

	void SetupExperienceLevels(const FTrainingOption& UnitTypeAndSubtype, const uint8 OwningPlayer);
	void SetupVeterancyIconSet(const FTrainingOption& UnitTypeAndSubtype);

	void SetupTankExp(const ACPPGameState* GameState, ETankSubtype TankSubType, const uint8 OwningPlayer);
	void SetupSquadExp(const ACPPGameState* GameState, ESquadSubtype SquadSubType, const uint8 OwningPlayer);
	void SetupAircraftExp(const ACPPGameState* GameState, EAircraftSubtype AircraftTypeSubtype, const uint8 OwningPlayer);
	void OnUnitTypeError(const FString& TypeAsString) const;
	void OnInvalidUnitForVeterancy(const FString& TypeAsString) const;

	void Debug_Experience(const FString& Message) const;
};
