#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldStrengthEstimationComponent.h"

namespace
{
	const FText& GetBaseFortificationStrengthReasonText()
	{
		static const FText BaseFortificationStrengthReasonText =
			FText::FromString(TEXT("Base Fortification Strength"));
		return BaseFortificationStrengthReasonText;
	}

	FWorldStrengthContribution BuildStrengthContribution(
		const EWorldStrengthTypes StrengthType,
		const FWorldStrengthReason& StrengthReason)
	{
		FWorldStrengthContribution StrengthContribution;
		StrengthContribution.StrengthType = StrengthType;
		StrengthContribution.StrengthReason = StrengthReason.StrengthReason;
		StrengthContribution.StrengthPercentage = StrengthReason.StrengthPercent;
		return StrengthContribution;
	}

	TArray<FWorldStrengthReason> BuildStrengthReasons(
		const TArray<FWorldStrengthContribution>& StrengthContributions)
	{
		TArray<FWorldStrengthReason> StrengthReasons;
		StrengthReasons.Reserve(StrengthContributions.Num());
		for (const FWorldStrengthContribution& StrengthContribution : StrengthContributions)
		{
			if (StrengthContribution.StrengthPercentage == 0)
			{
				continue;
			}

			StrengthReasons.Emplace(
				StrengthContribution.StrengthReason,
				StrengthContribution.StrengthPercentage);
		}

		return StrengthReasons;
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

TArray<FWorldStrengthContribution> UWorldStrengthEstimationComponent::GetStrengthContributions() const
{
	TArray<FWorldStrengthContribution> StrengthContributions;
	const int32 ContributionCount = M_FortificationModifierContributions.Num()
		+ M_FieldDivisionContributions.Num()
		+ M_StrategicSupportContributions.Num()
		+ 1;
	StrengthContributions.Reserve(ContributionCount);

	if (M_BaseFortificationStrengthContribution.StrengthPercentage != 0)
	{
		StrengthContributions.Add(M_BaseFortificationStrengthContribution);
	}

	StrengthContributions.Append(M_FortificationModifierContributions);
	StrengthContributions.Append(M_FieldDivisionContributions);
	StrengthContributions.Append(M_StrategicSupportContributions);
	return StrengthContributions;
}

const TArray<EWorldStrategicSupport>& UWorldStrengthEstimationComponent::GetStrategicSupportTypes() const
{
	return M_StrategicSupportTypes;
}

void UWorldStrengthEstimationComponent::SetBaseFortificationStrengthReason(
	const FWorldStrengthReason& StrengthReason)
{
	M_BaseFortificationStrengthContribution = BuildStrengthContribution(
		EWorldStrengthTypes::FortificationStrength,
		StrengthReason);
	RebuildStrengthEstimationMessage();
}

int32 UWorldStrengthEstimationComponent::GetBaseFortificationStrengthPercentage() const
{
	return M_BaseFortificationStrengthContribution.StrengthPercentage;
}

void UWorldStrengthEstimationComponent::SetBaseFortificationStrengthPercentage(const int32 StrengthPercentage)
{
	M_BaseFortificationStrengthContribution.StrengthType = EWorldStrengthTypes::FortificationStrength;
	M_BaseFortificationStrengthContribution.StrengthReason = GetBaseFortificationStrengthReasonText();
	M_BaseFortificationStrengthContribution.StrengthPercentage = StrengthPercentage;
	RebuildStrengthEstimationMessage();
}

void UWorldStrengthEstimationComponent::AddBaseFortificationStrengthPercentage(const int32 AddedStrengthPercentage)
{
	SetBaseFortificationStrengthPercentage(GetBaseFortificationStrengthPercentage() + AddedStrengthPercentage);
}

void UWorldStrengthEstimationComponent::ResetFortificationModifierReport()
{
	M_FortificationModifierContributions.Reset();
	RebuildStrengthEstimationMessage();
}

void UWorldStrengthEstimationComponent::AddFortificationModifierReason(
	const EWorldFortificationStrength FortificationStrength,
	const FWorldStrengthReason& StrengthReason)
{
	if (not StrengthReason.GetHasStrength())
	{
		return;
	}

	FWorldStrengthContribution StrengthContribution = BuildStrengthContribution(
		EWorldStrengthTypes::FortificationStrength,
		StrengthReason);
	StrengthContribution.FortificationModification = FortificationStrength;
	M_FortificationModifierContributions.Add(StrengthContribution);
	RebuildStrengthEstimationMessage();
}

void UWorldStrengthEstimationComponent::ResetStrategicSupportReport()
{
	M_StrategicSupportContributions.Reset();
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
	AddStrategicSupportReason(StrategicSupport, FGuid(), StrengthReason);
}

void UWorldStrengthEstimationComponent::AddStrategicSupportReason(
	const EWorldStrategicSupport StrategicSupport,
	const FGuid& SourceAnchorKey,
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

	FWorldStrengthContribution StrengthContribution = BuildStrengthContribution(
		EWorldStrengthTypes::StrategicSupport,
		StrengthReason);
	StrengthContribution.StrategicSupport = StrategicSupport;
	StrengthContribution.SourceKey = SourceAnchorKey;
	M_StrategicSupportContributions.Add(StrengthContribution);
	RebuildStrengthEstimationMessage();
}

void UWorldStrengthEstimationComponent::ResetFieldDivisionReport()
{
	M_FieldDivisionContributions.Reset();
	RebuildStrengthEstimationMessage();
}

void UWorldStrengthEstimationComponent::AddFieldDivisionReason(
	const EWorldFieldDivisions FieldDivision,
	const FGuid& DivisionKey,
	const int32 OwningPlayer,
	const FWorldStrengthReason& StrengthReason)
{
	if (not StrengthReason.GetHasStrength())
	{
		return;
	}

	FWorldStrengthContribution StrengthContribution = BuildStrengthContribution(
		EWorldStrengthTypes::FieldDivisions,
		StrengthReason);
	StrengthContribution.FieldDivision = FieldDivision;
	StrengthContribution.SourceKey = DivisionKey;
	StrengthContribution.OwningPlayer = OwningPlayer;
	M_FieldDivisionContributions.Add(StrengthContribution);
	RebuildStrengthEstimationMessage();
}

void UWorldStrengthEstimationComponent::RebuildStrengthEstimationMessage()
{
	/*
	 * The UI has one FortificationStrength section, but the component keeps base strength and fortification modifiers
	 * separate so saves/restores and runtime modifier refreshes can touch one without rebuilding the other by hand.
	 */
	TArray<FWorldStrengthReason> FortificationStrengthReasons;
	if (M_BaseFortificationStrengthContribution.StrengthPercentage != 0)
	{
		FortificationStrengthReasons.Emplace(
			M_BaseFortificationStrengthContribution.StrengthReason,
			M_BaseFortificationStrengthContribution.StrengthPercentage);
	}

	FortificationStrengthReasons.Append(BuildStrengthReasons(M_FortificationModifierContributions));

	M_StrengthEstimationMessage.SetStrengthReasons(
		EWorldStrengthTypes::FortificationStrength,
		FortificationStrengthReasons);
	M_StrengthEstimationMessage.SetStrengthReasons(
		EWorldStrengthTypes::FieldDivisions,
		BuildStrengthReasons(M_FieldDivisionContributions));
	M_StrengthEstimationMessage.SetStrengthReasons(
		EWorldStrengthTypes::StrategicSupport,
		BuildStrengthReasons(M_StrategicSupportContributions));
	M_StrengthEstimationMessage.FormatRichTextMessages();
}
