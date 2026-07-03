#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DataAndUtils/RTSStrengthEstimationTypes.h"
#include "W_StrengthEstimation.generated.h"

class URichTextBlock;

UCLASS()
class RTS_SURVIVAL_API UW_StrengthEstimation : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Writes a category-aware strength report into the hover-details rich text blocks.
	 * @param StrengthEstimationMessage Report copied from the hovered map item's strength component.
	 */
	UFUNCTION(BlueprintCallable, Category = "RTS|Strength Estimation")
	void SetupStrengthEstimation(const FWorldStrengthReport& StrengthEstimationMessage);

	/**
	 * @brief Plays the Blueprint-authored visible animation for the details panel.
	 */
	void ShowVisibleAnimation();

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "World Campaign|Map Item")
	void BP_ShowVisibleAnimation();

	/**
	 * @brief Rich text for base fortification strength and fortification modifier reasons.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	URichTextBlock* M_FortificationsRichText = nullptr;

	/**
	 * @brief Rich text for nearby field division reasons.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	URichTextBlock* M_FieldDivisionsRichText = nullptr;

	/**
	 * @brief Rich text for strategic support reasons such as airfields, radars, or supply depots.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	URichTextBlock* M_StrategicSupportRichText = nullptr;

	/**
	 * @brief Rich text total for all strength categories.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	URichTextBlock* M_TotalRichText = nullptr;

private:
	/**
	 * @brief Validates that the fortification rich text widget is bound.
	 * @return true when the widget pointer is valid.
	 */
	bool GetIsValidFortificationsRichText() const;

	/**
	 * @brief Validates that the field divisions rich text widget is bound.
	 * @return true when the widget pointer is valid.
	 */
	bool GetIsValidFieldDivisionsRichText() const;

	/**
	 * @brief Validates that the strategic support rich text widget is bound.
	 * @return true when the widget pointer is valid.
	 */
	bool GetIsValidStrategicSupportRichText() const;

	/**
	 * @brief Validates that the total rich text widget is bound.
	 * @return true when the widget pointer is valid.
	 */
	bool GetIsValidTotalRichText() const;
};
