#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldStrengthTypes.h"
#include "RTSStrengthEstimationTypes.generated.h"

/**
 * @brief One signed percentage contribution shown in the strength estimation UI.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldStrengthReason
{
	GENERATED_BODY()

	static const TCHAR PositiveStrengthSignOpeningTag[];
	static const TCHAR PositiveStrengthValueOpeningTag[];
	static const TCHAR NegativeStrengthSignOpeningTag[];
	static const TCHAR NegativeStrengthValueOpeningTag[];
	static const TCHAR PositiveStrengthSignText[];
	static const TCHAR NegativeStrengthSignText[];
	static const TCHAR RichTextClosingTag[];

	FWorldStrengthReason() = default;

	/**
	 * @brief Creates one player-facing strength reason line.
	 * @param InStrengthReason Rich text reason shown after the percentage value.
	 * @param InStrengthPercent Signed strength percentage. Positive values increase estimated strength.
	 */
	FWorldStrengthReason(
		const FText& InStrengthReason,
		const int32 InStrengthPercent);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strength Estimation", meta = (MultiLine = true))
	FText StrengthReason;

	// Positive values increase the estimated strength. Negative values reduce it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strength Estimation")
	int32 StrengthPercent = 0;

	/**
	 * @brief Checks whether this reason contributes a non-zero percentage.
	 * @return true when StrengthPercent is not zero.
	 */
	bool GetHasStrength() const;

	/**
	 * @brief Checks whether this reason reduces the estimated strength.
	 * @return true when StrengthPercent is below zero.
	 */
	bool GetIsNegativeStrength() const;

	/**
	 * @brief Builds the rich text line for this reason, including sign, percentage, and reason text.
	 * @return Empty text when this reason has no strength contribution.
	 */
	FText BuildRichTextLine() const;

private:
	static const FText& GetRichTextLineFormat();

	const TCHAR* GetSignOpeningTag() const;
	const TCHAR* GetValueOpeningTag() const;
	const TCHAR* GetSignText() const;
};

/**
 * @brief Category-aware strength estimation data and cached rich text for the hover details panel.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldStrengthReport
{
	GENERATED_BODY()

public:
	static const TCHAR StrengthReasonLineSeparator[];
	static const TCHAR TotalTitleOpeningTag[];
	static const TCHAR TotalPositiveValueOpeningTag[];
	static const TCHAR TotalNegativeValueOpeningTag[];
	static const TCHAR RichTextClosingTag[];

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strength Estimation")
	TArray<FWorldStrengthReason> FortificationStrengthReasons;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Strength Estimation", meta = (MultiLine = true))
	FText FortificationStrengthRichText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strength Estimation")
	TArray<FWorldStrengthReason> FieldDivisionReasons;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Strength Estimation", meta = (MultiLine = true))
	FText FieldDivisionsRichText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strength Estimation")
	TArray<FWorldStrengthReason> StrategicSupportReasons;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Strength Estimation", meta = (MultiLine = true))
	FText StrategicSupportRichText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Strength Estimation", meta = (MultiLine = true))
	FText TotalRichText;

	/**
	 * @brief Regenerates all cached rich text fields from the currently stored category reasons.
	 */
	void FormatRichTextMessages();

	/**
	 * @brief Clears all strength categories and refreshes rich text output.
	 */
	void Reset();

	/**
	 * @brief Replaces the reasons for one strength category and refreshes rich text output.
	 * @param StrengthType Category to replace.
	 * @param InStrengthReasons New reasons for the category.
	 */
	void SetStrengthReasons(EWorldStrengthTypes StrengthType,
	                         const TArray<FWorldStrengthReason>& InStrengthReasons);

	/**
	 * @brief Adds one non-zero reason to a category and refreshes rich text output.
	 * @param StrengthType Category receiving the reason.
	 * @param StrengthReason Reason to add.
	 */
	void AddStrengthReason(EWorldStrengthTypes StrengthType,
	                        const FWorldStrengthReason& StrengthReason);

	/**
	 * @brief Returns the stored reasons for one strength category.
	 * @param StrengthType Category to inspect.
	 * @return Category reason array, or an empty static array for None/unknown categories.
	 */
	const TArray<FWorldStrengthReason>& GetStrengthReasons(EWorldStrengthTypes StrengthType) const;

	/**
	 * @brief Builds rich text for one category from its currently stored reasons.
	 * @param StrengthType Category to format.
	 * @return Joined rich text lines, or empty text when the category has no non-zero reasons.
	 */
	FText BuildRichTextForStrengthType(EWorldStrengthTypes StrengthType) const;

	/**
	 * @brief Builds the rich text total line from all strength categories.
	 * @return Rich text total line.
	 */
	FText BuildTotalRichText() const;

	/**
	 * @brief Sums all strength categories.
	 * @return Total signed strength percentage.
	 */
	int32 GetTotalStrengthPercent() const;

	/**
	 * @brief Sums one strength category.
	 * @param StrengthType Category to sum.
	 * @return Signed strength percentage for that category.
	 */
	int32 GetStrengthPercentForStrengthType(EWorldStrengthTypes StrengthType) const;

private:
	static const FText& GetTotalRichTextFormat();
	static const TCHAR* GetTotalValueOpeningTag(const int32 TotalStrengthPercent);
	static const TArray<FWorldStrengthReason>& GetEmptyStrengthReasons();
};
