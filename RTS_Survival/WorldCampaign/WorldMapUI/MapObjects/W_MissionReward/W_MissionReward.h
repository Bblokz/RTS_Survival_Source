// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/RichTextBlock.h"
#include "RewardStructs/FMissionRewardStructs.h"
#include "RTS_Survival/CardSystem/CardUI/RTSCardWidgets/W_RTSCard.h"
#include "W_MissionReward.generated.h"

/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_MissionReward : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetupReward(const FPrimaryReward& PrimaryReward,
		 const FSecondaryReward& SecondaryReward);
	
protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UW_RTSCard* BP_RTS_Card_1  = nullptr;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UW_RTSCard* BP_RTS_Card_2  = nullptr;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UW_RTSCard* BP_RTS_Card_3  = nullptr;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UW_RTSCard* BP_RTS_Card_4  = nullptr;
	
	// By default has <DisplayAmount>+200</><img id="Radixite"/><Radixite>Radixite</>
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	URichTextBlock* M_RadixiteBonusRichText;
	
	// By default has <DisplayAmount>+200</><img id="Radixite"/><Exp>Exp</>
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	 URichTextBlock* M_CommandExpBonusRichText;
	
	// by default has <DisplayAmount>+2</><img id="WeaponBlueprint"/> <DisplayAmount>+2</><img id="VehicleBlueprint"/> 
	// <DisplayAmount>+2</><img id="ConstructionBlueprint"/> <DisplayAmount>+2</><img id="EnergyBlueprint"/>  <Armor>Blueprints</>
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	 URichTextBlock* M_BlueprintBonuses;

	// By default has: <Armor>Secondary:</> <DisplayAmount>+200</><img id="Radixite"/><Radixite>Radixite</>
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	 URichTextBlock*M_SecondaryBonuses;
	
	
};
