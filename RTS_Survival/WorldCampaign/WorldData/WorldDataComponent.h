// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WorldDataComponent.generated.h"

class AGeneratorWorldCampaign;
class UWorldData;
enum class EMapEnemyItem : uint8;
enum class EMapMission : uint8;
enum class ERTSGameDifficulty : uint8;
enum class EWorldFieldDivisions : uint8;
enum class EWorldFortificationStrength : uint8;
enum class EWorldStrategicSupport : uint8;
struct FWorldStrengthReason;

/**
 * @brief Component placed on the world generator to load designer campaign data into generated map objects.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UWorldDataComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWorldDataComponent();

	/**
	 * @brief Loads non-placement WorldData content, such as rewards and objective text, into generated objects.
	 * @param WorldGenerator Generator whose current placement state supplies the target objects.
	 */
	void LoadWorldDataIntoObjects(AGeneratorWorldCampaign* WorldGenerator) const;

	/**
	 * @brief Builds the base fortification strength reason for a mission object.
	 * @param MissionType Mission type used as the world data key.
	 * @param GameDifficulty Current campaign difficulty.
	 * @param OutStrengthReason Output UI-ready reason. Reset to zero when world data is unavailable.
	 * @return true when the mission base entry exists in WorldData.
	 */
	bool TryBuildMissionBaseDifficultyReason(EMapMission MissionType,
	                                         ERTSGameDifficulty GameDifficulty,
	                                         FWorldStrengthReason& OutStrengthReason) const;

	/**
	 * @brief Builds the base fortification strength reason for an enemy object.
	 * @param EnemyItemType Enemy item type used as the world data key.
	 * @param GameDifficulty Current campaign difficulty.
	 * @param OutStrengthReason Output UI-ready reason. Reset to zero when world data is unavailable.
	 * @return true when the enemy base entry exists in WorldData.
	 */
	bool TryBuildEnemyBaseDifficultyReason(EMapEnemyItem EnemyItemType,
	                                       ERTSGameDifficulty GameDifficulty,
	                                       FWorldStrengthReason& OutStrengthReason) const;

	/**
	 * @brief Builds a UI-ready reason for one fortification modifier enum.
	 * @param FortificationStrength Fortification modifier enum stored on the target object.
	 * @param GameDifficulty Current campaign difficulty.
	 * @param OutStrengthReason Output UI-ready reason from WorldData.
	 * @return true when the fortification definition exists in WorldData.
	 */
	bool TryBuildFortificationStrengthReason(EWorldFortificationStrength FortificationStrength,
	                                         ERTSGameDifficulty GameDifficulty,
	                                         FWorldStrengthReason& OutStrengthReason) const;

	/**
	 * @brief Builds a UI-ready reason for one strategic support enum.
	 * @param StrategicSupport Strategic support enum stored on UWorldStrategicSupportArea.
	 * @param GameDifficulty Current campaign difficulty.
	 * @param OutStrengthReason Output UI-ready reason from WorldData.
	 * @return true when the strategic support definition exists in WorldData.
	 */
	bool TryBuildStrategicSupportReason(EWorldStrategicSupport StrategicSupport,
	                                    ERTSGameDifficulty GameDifficulty,
	                                    FWorldStrengthReason& OutStrengthReason) const;

	/**
	 * @brief Builds a UI-ready reason for one field division enum.
	 * @param FieldDivision Field division type stored on a world division actor.
	 * @param GameDifficulty Current campaign difficulty.
	 * @param OutStrengthReason Output UI-ready reason from WorldData.
	 * @return true when the field division definition exists in WorldData.
	 */
	bool TryBuildFieldDivisionReason(EWorldFieldDivisions FieldDivision,
	                                 ERTSGameDifficulty GameDifficulty,
	                                 FWorldStrengthReason& OutStrengthReason) const;

private:
	bool GetIsValidWorldData() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|World Data",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWorldData> M_WorldData = nullptr;
};
