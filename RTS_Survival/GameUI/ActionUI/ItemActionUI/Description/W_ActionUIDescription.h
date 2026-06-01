// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"

#include "W_ActionUIDescription.generated.h"

enum class EAbilityID : uint8;
class UImage;
class URichTextBlock;
class ICommands;
class UW_CostDisplay;
/**
 * @brief Used by action-button hover UI to display metadata shared by every action description blueprint.
 */
UCLASS()
class RTS_SURVIVAL_API UW_ActionUIDescription : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void SetCostsForAbility(const TMap<ERTSResourceType, int32>& ResourceCosts) const;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UW_CostDisplay> M_CostDisplay = nullptr;
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> M_ActionItemImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URichTextBlock> M_AbilityDescription;
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URichTextBlock> M_AbilityTitle;

	// Called from blueprint implementation after getting the right text, title and icon from the data table
	// using the ability and custom type to access a row.
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetupDescription(
		const EAbilityID Ability,
		const int32 CustomType,
		const FText& DataTableText_Title,
		const FText& DataTableText_Description,
		UTexture* DataTable_Icon);

	bool EnsureIsValidCostDisplay() const;

private:
	void SetDataToTextAndImage(
		const FText& DataTableText_Title,
		const FText& DataTableText_Description,
		UTexture* DataTable_Icon) const;
	void OnOverrideDescriptionForTechnology(
		const EAbilityID Ability,
		const int32 CustomType,
	                      const FText& DataTableText_Title,
	                      const FText& DataTableText_Description,
	                      TObjectPtr<UTexture2D> DataTable_Icon);
};
