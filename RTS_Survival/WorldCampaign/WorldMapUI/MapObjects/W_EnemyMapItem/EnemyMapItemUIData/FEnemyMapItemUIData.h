#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapItemType.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/MapObjects/W_EnemyMapItem/StrengthEstimation/DataAndUtils/RTSStrengthEstimationTypes.h"

#include "FEnemyMapItemUIData.generated.h"

USTRUCT(BlueprintType, Blueprintable)
struct FEnemyOrMissionMapItemUIData
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText TitleText = FText::FromString("<Text_LBadTitle>CRD Factory</>");
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FText StrengthPercentageText = FText::FromString("<Text_NewBad>20%</>");
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText DescriptionText = FText::FromString("<Text_Armor>This CRD Base periodically produces light vehicles which reinforce nearby CRD elements.</>");
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText PrimaryObjectiveText = FText::FromString("<Text_Armor>Destroy the factory.</>");
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText SecondaryObjectiveText = FText::FromString("<Text_Armor>Get Radixite Info.</>");
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText OnDefeatText = FText::FromString(TEXT(
		"<Text_BadTitle>-</> <Text_Bad14>Enemy strength increases by 20%</>\n"
		"<Text_BadTitle>-</> <Text_Bad14>CRD will attempt to reinforce the position with extra divisions</>"
	));
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FWorldStrengthReport StrengthEstimation;
	// What type of object the player clicked.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EMapItemType Type = EMapItemType::Empty;

	/**
	 * @brief Clears the projected strength report and refreshes the compact percentage text.
	 */
	void ResetStrengthEstimation();

	/**
	 * @brief Copies the authoritative component report into this UI data projection.
	 * @param StrengthEstimationMessage Report from UWorldStrengthEstimationComponent.
	 */
	void SetStrengthEstimation(const FWorldStrengthReport& StrengthEstimationMessage);

	/**
	 * @brief Replaces fortification strength reasons on this UI projection.
	 * @param StrengthReasons Reasons to put in the fortification category.
	 */
	void SetStrengthReasons(const TArray<FWorldStrengthReason>& StrengthReasons);

	/**
	 * @brief Replaces one category of strength reasons on this UI projection.
	 * @param StrengthType Category to replace.
	 * @param StrengthReasons Reasons to put in the category.
	 */
	void SetStrengthReasons(EWorldStrengthTypes StrengthType,
	                        const TArray<FWorldStrengthReason>& StrengthReasons);

	/**
	 * @brief Adds one fortification strength reason to this UI projection.
	 * @param StrengthReason Reason to add.
	 */
	void AddStrengthReason(const FWorldStrengthReason& StrengthReason);

	/**
	 * @brief Adds one reason to a specific strength category on this UI projection.
	 * @param StrengthType Category receiving the reason.
	 * @param StrengthReason Reason to add.
	 */
	void AddStrengthReason(EWorldStrengthTypes StrengthType,
	                       const FWorldStrengthReason& StrengthReason);

	/**
	 * @brief Regenerates the compact total percentage text shown on enemy/mission map buttons.
	 */
	void RefreshStrengthPercentageText();
};
