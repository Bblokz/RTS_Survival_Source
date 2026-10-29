// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UnitVariantSelectionStyle.h"

#include "W_UnitVariantDiscardDialog.generated.h"

class UBorder;
class UButton;
class UImage;
class UTextBlock;
class UVerticalBox;
class UW_UnitVariantSelectionMenu;

/**
 * @brief Asks before Back throws away variant choices that were not confirmed; created and shown by
 * UW_UnitVariantSelectionMenu, which receives the answer. Covers the whole screen so nothing behind it is clickable.
 */
UCLASS()
class RTS_SURVIVAL_API UW_UnitVariantDiscardDialog : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetupDialog(UW_UnitVariantSelectionMenu* Menu, const FUnitVariantSelectionStyle& Style);

	/** @param ChangeCount How many units have a choice that differs from the confirmed one. */
	void ShowDialog(const int32 ChangeCount);
	void HideDialog();
	bool GetIsOpen() const { return bM_IsOpen; }

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY()
	TWeakObjectPtr<UW_UnitVariantSelectionMenu> M_Menu;

	UPROPERTY()
	TObjectPtr<UImage> M_Dimmer;

	UPROPERTY()
	TObjectPtr<UBorder> M_Plate;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_TitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_BodyText;

	UPROPERTY()
	TObjectPtr<UButton> M_KeepEditingButton;

	UPROPERTY()
	TObjectPtr<UButton> M_DiscardButton;

	bool bM_IsOpen = false;

	void BuildWidgetTree();
	void BuildButtonRow(UVerticalBox* PlateColumn, const FUnitVariantSelectionStyle& Style);

	UFUNCTION()
	void OnKeepEditingClicked();

	UFUNCTION()
	void OnDiscardClicked();

	bool GetIsValidMenu() const;
	bool GetIsValidPlate() const;
};
