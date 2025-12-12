// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/Player/PlayerTechManager/PlayerTechManager.h"
#include "W_TechItemDescription.generated.h"

class UW_CostDisplay;
enum class ERTSResourceType : uint8;
class UButton;
enum class ETechnology : uint8;
class UImage;
class URichTextBlock;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_TechItemDescription : public UUserWidget
{
	GENERATED_BODY()

public:
	
	void InitTechItemDescription(const ETechnology Technology, const TMap<ERTSResourceType, int32>& ResourceCosts, const float TimeToResearch, const bool
	                             bHasBeenResearched);
	
protected:
	UFUNCTION(BlueprintImplementableEvent)
	void OnTechItemDescriptionUpdate(const ETechnology Technology, const bool bHasBeenResearched);
	UPROPERTY(meta = (BindWidget), BlueprintReadWrite)
	URichTextBlock* RichContextBlock;

	UPROPERTY(meta = (BindWidget), BlueprintReadWrite)
	URichTextBlock* RichTitle;

	UPROPERTY(meta = (BindWidget), BlueprintReadWrite)
	URichTextBlock* AffectedUnits_1;

	UPROPERTY(meta = (BindWidget), BlueprintReadWrite)
	URichTextBlock* AffectedUnits_2;

	UPROPERTY(meta = (BindWidget), BlueprintReadWrite)
	URichTextBlock* AffectedUnits_3;

	UPROPERTY(meta = (BindWidget), BlueprintReadWrite)
	UButton* TechIcon;

	UPROPERTY(meta = (BindWidget), BlueprintReadWrite)
	UW_CostDisplay* CostDisplay;

	UPROPERTY(meta = (BindWidget), BlueprintReadWrite)
	URichTextBlock* RichResearchTime;

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetupAffectedUnitBlocks(const TArray<FText> AffectedUnitNames);
private:
	void InitResearchTime(const float TimeToResearch);
};
