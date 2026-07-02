// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Subsystems/RadiusSubsystem/ERTSRadiusType.h"
#include "RTS_Survival/WorldCampaign/WorldDifficulty/WorldDifficultyTypes.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/MapObjects/W_EnemyMapItem/StrengthEstimation/DataAndUtils/RTSStrengthEstimationTypes.h"
#include "WorldDifficultyInfluence.generated.h"

class AActor;
class URTSRadiusPoolSubsystem;

/**
 * @brief Blueprint-friendly settings for one world-map difficulty influence radius.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldDifficultyInfluenceSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Difficulty Influence",
		meta = (ClampMin = "0"))
	float XYRadius = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Difficulty Influence")
	int32 BasePercentage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Difficulty Influence")
	FRTSPerDifficultyMlt PerDifficultyMlt;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Difficulty Influence")
	ERTSRadiusType RadiusType = ERTSRadiusType::FullCircle_DifficultyRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Difficulty Influence",
		meta = (MultiLine = true))
	FText ReasonText = FText::FromString(TEXT("<Text_NewBad>Nearby Factory</>"));

	int32 GetDifficultyPercent(ERTSGameDifficulty GameDifficulty) const;
	FRTSStrengthEstimationInfluenceReason BuildInfluenceReason(ERTSGameDifficulty GameDifficulty) const;
};

/**
 * @brief Added to mission or enemy map objects to raise nearby world-map difficulty percentages.
 */
UCLASS(ClassGroup=(WorldCampaign), meta=(BlueprintSpawnableComponent), BlueprintType)
class RTS_SURVIVAL_API UWorldDifficultyInfluence : public UActorComponent
{
	GENERATED_BODY()

public:
	UWorldDifficultyInfluence();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	int32 GetDifficultyInfluencePercent(ERTSGameDifficulty GameDifficulty) const;
	FRTSStrengthEstimationInfluenceReason BuildInfluenceReason(ERTSGameDifficulty GameDifficulty) const;
	float GetXYRadius() const { return M_Settings.XYRadius; }
	ERTSRadiusType GetRadiusType() const { return M_Settings.RadiusType; }
	void ShowHoverRadius();
	void HideHoverRadius();
	void ShowSelectedRadius();
	void HideSelectedRadius();
	void HideAllRadii();

	static void ShowHoverRadiiOnActor(AActor* Actor);
	static void HideHoverRadiiOnActor(AActor* Actor);
	static void ShowSelectedRadiiOnActor(AActor* Actor);
	static void HideSelectedRadiiOnActor(AActor* Actor);

private:
	static constexpr int32 InvalidRadiusId = -1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Difficulty Influence",
		meta = (AllowPrivateAccess = "true", ShowOnlyInnerProperties))
	FWorldDifficultyInfluenceSettings M_Settings;

	UPROPERTY()
	TWeakObjectPtr<class URTSRadiusPoolSubsystem> M_RadiusPoolSubsystem;

	int32 M_HoverRadiusId = InvalidRadiusId;
	int32 M_SelectedRadiusId = InvalidRadiusId;

	bool GetIsValidRadiusPoolSubsystem();
	URTSRadiusPoolSubsystem* GetRadiusPoolSubsystem();
	int32 CreateRadius();
	bool GetIsRadiusActive(int32 RadiusId);
	void HideRadiusById(int32& RadiusId);
};
