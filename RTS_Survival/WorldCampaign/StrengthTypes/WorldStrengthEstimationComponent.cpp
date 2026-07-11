#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldStrengthEstimationComponent.h"

namespace
{
	const FText& GetBaseFortificationStrengthReasonText()
	{
		static const FText BaseFortificationStrengthReasonText =
			FText::FromString(TEXT("Base Fortification Strength"));
		return BaseFortificationStrengthReasonText;
	}
}

UWorldStrengthEstimationComponent::UWorldStrengthEstimationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

const FWorldStrengthReport& UWorldStrengthEstimationComponent::GetStrengthEstimationMessage() const
{
	return M_StrengthEstimationMessage;
}

int32 UWorldStrengthEstimationComponent::GetTotalStrengthPercentage() const
{
	return M_StrengthEstimationMessage.GetTotalStrengthPercent();
}

const TArray<EWorldStrategicSupport>& UWorldStrengthEstimationComponent::GetStrategicSupportTypes() const
{
	return M_StrategicSupportTypes;
}

void UWorldStrengthEstimationComponent::SetBaseFortificationStrengthReason(
	const FWorldStrengthReason& StrengthReason)
{
	M_BaseFortificationStrengthReason = StrengthReason;
	RebuildStrengthEstimationMessage();
}

int32 UWorldStrengthEstimationComponent::GetBaseFortificationStrengthPercentage() const
{
	return M_BaseFortificationStrengthReason.StrengthPercent;
}

void UWorldStrengthEstimationComponent::SetBaseFortificationStrengthPercentage(const int32 StrengthPercentage)
{
	M_BaseFortificationStrengthReason.StrengthReason = GetBaseFortificationStrengthReasonText();
	M_BaseFortificationStrengthReason.StrengthPercent = StrengthPercentage;
	RebuildStrengthEstimationMessage();
}

void UWorldStrengthEstimationComponent::AddBaseFortificationStrengthPercentage(const int32 AddedStrengthPercentage)
{
	SetBaseFortificationStrengthPercentage(GetBaseFortificationStrengthPercentage() + AddedStrengthPercentage);
}

void UWorldStrengthEstimationComponent::ResetFortificationModifierReport()
{
	M_FortificationModifierReasons.Reset();
	RebuildStrengthEstimationMessage();
}

void UWorldStrengthEstimationComponent::AddFortificationModifierReason(
	const FWorldStrengthReason& StrengthReason)
{
	if (not StrengthReason.GetHasStrength())
	{
		return;
	}

	M_FortificationModifierReasons.Add(StrengthReason);
	RebuildStrengthEstimationMessage();
}

void UWorldStrengthEstimationComponent::ResetStrategicSupportReport()
{
	M_StrategicSupportReasons.Reset();
	M_StrategicSupportTypes.Reset();
	RebuildStrengthEstimationMessage();
}

void UWorldStrengthEstimationComponent::AddStrategicSupportReason(
	const FWorldStrengthReason& StrengthReason)
{
	AddStrategicSupportReason(EWorldStrategicSupport::None, StrengthReason);
}

void UWorldStrengthEstimationComponent::AddStrategicSupportReason(
	const EWorldStrategicSupport StrategicSupport,
	const FWorldStrengthReason& StrengthReason)
{
	if (not StrengthReason.GetHasStrength())
	{
		return;
	}

	if (StrategicSupport != EWorldStrategicSupport::None)
	{
		M_StrategicSupportTypes.Add(StrategicSupport);
	}

	M_StrategicSupportReasons.Add(StrengthReason);
	RebuildStrengthEstimationMessage();
}

void UWorldStrengthEstimationComponent::ResetFieldDivisionReport()
{
	M_FieldDivisionReasons.Reset();
	RebuildStrengthEstimationMessage();
}

void UWorldStrengthEstimationComponent::AddFieldDivisionReason(
	const FWorldStrengthReason& StrengthReason)
{
	if (not StrengthReason.GetHasStrength())
	{
		return;
	}

	M_FieldDivisionReasons.Add(StrengthReason);
	RebuildStrengthEstimationMessage();
}

void UWorldStrengthEstimationComponent::RebuildStrengthEstimationMessage()
{
	/*
	 * The UI has one FortificationStrength section, but the component keeps base strength and fortification modifiers
	 * separate so saves/restores and runtime modifier refreshes can touch one without rebuilding the other by hand.
	 */
	TArray<FWorldStrengthReason> FortificationStrengthReasons;
	if (M_BaseFortificationStrengthReason.GetHasStrength())
	{
		FortificationStrengthReasons.Add(M_BaseFortificationStrengthReason);
	}

	for (const FWorldStrengthReason& FortificationModifierReason : M_FortificationModifierReasons)
	{
		if (FortificationModifierReason.GetHasStrength())
		{
			FortificationStrengthReasons.Add(FortificationModifierReason);
		}
	}

	M_StrengthEstimationMessage.SetStrengthReasons(
		EWorldStrengthTypes::FortificationStrength,
		FortificationStrengthReasons);
	M_StrengthEstimationMessage.SetStrengthReasons(
		EWorldStrengthTypes::FieldDivisions,
		M_FieldDivisionReasons);
	M_StrengthEstimationMessage.SetStrengthReasons(
		EWorldStrengthTypes::StrategicSupport,
		M_StrategicSupportReasons);
	M_StrengthEstimationMessage.FormatRichTextMessages();
}
