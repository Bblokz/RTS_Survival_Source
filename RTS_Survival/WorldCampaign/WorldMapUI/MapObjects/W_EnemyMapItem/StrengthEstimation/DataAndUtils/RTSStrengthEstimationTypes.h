#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RTSStrengthEstimationTypes.generated.h"

USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FRTSStrengthEstimationInfluenceReason
{
	GENERATED_BODY()

	static const TCHAR PositiveInfluenceSignOpeningTag[];
	static const TCHAR PositiveInfluenceValueOpeningTag[];
	static const TCHAR NegativeInfluenceSignOpeningTag[];
	static const TCHAR NegativeInfluenceValueOpeningTag[];
	static const TCHAR PositiveInfluenceSignText[];
	static const TCHAR NegativeInfluenceSignText[];
	static const TCHAR RichTextClosingTag[];

	FRTSStrengthEstimationInfluenceReason() = default;

	FRTSStrengthEstimationInfluenceReason(
		const FText& InReasonText,
		const int32 InInfluencePercent);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strength Estimation", meta = (MultiLine = true))
	FText ReasonText;

	// Positive values increase the estimated strength. Negative values reduce it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strength Estimation")
	int32 InfluencePercent = 0;

	bool GetHasInfluence() const;
	bool GetIsNegativeInfluence() const;

	FText BuildRichTextLine() const;

private:
	static const FText& GetRichTextLineFormat();

	const TCHAR* GetSignOpeningTag() const;
	const TCHAR* GetValueOpeningTag() const;
	const TCHAR* GetSignText() const;
};

USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FRTSStrengthEstimationRichTextMessage
{
	GENERATED_BODY()

public:
	static const TCHAR InfluenceReasonLineSeparator[];
	static const TCHAR TotalTitleOpeningTag[];
	static const TCHAR TotalPositiveValueOpeningTag[];
	static const TCHAR TotalNegativeValueOpeningTag[];
	static const TCHAR RichTextClosingTag[];

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strength Estimation")
	TArray<FRTSStrengthEstimationInfluenceReason> InfluenceReasons;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Strength Estimation", meta = (MultiLine = true))
	FText EstimationsRichText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Strength Estimation", meta = (MultiLine = true))
	FText TotalRichText;

	void FormatRichTextMessages();

	FText BuildEstimationsRichText() const;
	FText BuildTotalRichText() const;

	int32 GetTotalInfluencePercent() const;

private:
	static const FText& GetTotalRichTextFormat();
	static const TCHAR* GetTotalValueOpeningTag(const int32 TotalInfluencePercent);
};
