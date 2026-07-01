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
	FRTSStrengthEstimationRichTextMessage StrengthEstimation;
	// What type of object the player clicked.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EMapItemType Type = EMapItemType::Empty;

	void ResetStrengthEstimation();
	void SetStrengthInfluenceReasons(const TArray<FRTSStrengthEstimationInfluenceReason>& InfluenceReasons);
	void AddStrengthInfluenceReason(const FRTSStrengthEstimationInfluenceReason& InfluenceReason);
	void RefreshStrengthPercentageText();
};
