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
struct FRTSStrengthEstimationInfluenceReason;

/**
 * @brief Component placed on the world generator to load designer campaign data into generated map objects.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UWorldDataComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWorldDataComponent();

	void LoadWorldDataIntoObjects(AGeneratorWorldCampaign* WorldGenerator) const;
	bool TryBuildMissionBaseDifficultyReason(EMapMission MissionType,
	                                         ERTSGameDifficulty GameDifficulty,
	                                         FRTSStrengthEstimationInfluenceReason& OutInfluenceReason) const;
	bool TryBuildEnemyBaseDifficultyReason(EMapEnemyItem EnemyItemType,
	                                       ERTSGameDifficulty GameDifficulty,
	                                       FRTSStrengthEstimationInfluenceReason& OutInfluenceReason) const;

private:
	bool GetIsValidWorldData() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|World Data",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWorldData> M_WorldData = nullptr;
};
