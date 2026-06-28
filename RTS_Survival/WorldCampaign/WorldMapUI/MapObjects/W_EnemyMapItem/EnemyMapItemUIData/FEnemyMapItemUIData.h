#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/MapObjects/W_EnemyMapItem/StrengthEstimation/DataAndUtils/RTSStrengthEstimationTypes.h"

#include "FEnemyMapItemUIData.generated.h"

USTRUCT(blueprintType, Blueprintable)
struct FEnemyOrMissionMapItemUIData
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText TitleText = FText::FromString("<Text_LBadTitle>CRD Factory</>");
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText StrengthPercentageText = FText::FromString("<Text_NewBad>20%</>");
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText PrimaryObjectiveText = FText::FromString("<Text_Armor>Destroy the factory.</>");
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText SecondaryObjectiveText = FText::FromString("<Text_Armor>Get Radixite Info.</>");
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText OnDefeatText = FText::FromString(TEXT(
		"<Text_BadTitle>-</> <Text_Bad14>Enemy strength increases by 20%</>\n"
		"<Text_BadTitle>-</> <Text_Bad14>CRD will attempt to reinforce the position with extra divisions</>"
	));
	FRTSStrengthEstimationRichTextMessage StrengthEstimation;
};
