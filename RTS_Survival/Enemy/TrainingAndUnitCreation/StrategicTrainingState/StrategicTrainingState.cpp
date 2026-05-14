#include "StrategicTrainingState.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Enemy/EnemyAISettings/EnemyAISettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace
{
	const TArray<EAITrainingFocus>& GetAllTrainingFocusBuckets()
	{
		static const TArray<EAITrainingFocus> AllTrainingFocusBuckets =
		{
			EAITrainingFocus::Infantry,
			EAITrainingFocus::LightTanks,
			EAITrainingFocus::MediumTanks,
			EAITrainingFocus::HeavyTanks
		};
		return AllTrainingFocusBuckets;
	}

	FString GetTrainingFocusDebugString(const EAITrainingFocus TrainingFocus)
	{
		return UEnum::GetValueAsString(TrainingFocus);
	}

	FString GetTrainingSpecialtyDebugString(const EAITrainingFocusSpecialty TrainingSpecialty)
	{
		return UEnum::GetValueAsString(TrainingSpecialty);
	}

	template <typename TEnum>
	TEnum PickWeightedTrainingBucket(
		const TMap<TEnum, float>& Buckets,
		const TFunctionRef<float(TEnum, float)> GetBucketWeight,
		const TEnum FallbackValue)
	{
		float TotalWeight = 0.f;
		for (const TPair<TEnum, float>& Bucket : Buckets)
		{
			TotalWeight += GetBucketWeight(Bucket.Key, Bucket.Value);
		}

		if (TotalWeight <= EnemyAISettings::TrainingPressure::MinimumTrainingBucketPressure)
		{
			return FallbackValue;
		}

		const float RandomWeight = FMath::FRandRange(0.f, TotalWeight);
		float RunningWeight = 0.f;
		for (const TPair<TEnum, float>& Bucket : Buckets)
		{
			RunningWeight += GetBucketWeight(Bucket.Key, Bucket.Value);
			if (RandomWeight <= RunningWeight)
			{
				return Bucket.Key;
			}
		}

		return FallbackValue;
	}
}

void FEnemyStrategicTrainingBuckets::AddPressureContribution(
	const FEnemyStrategicTrainingPressureContribution& PressureContribution)
{
	if (PressureContribution.PressureAmount <= EnemyAISettings::TrainingPressure::MinimumTrainingBucketPressure)
	{
		return;
	}

	const bool bHasNoTrainingPressure = PressureContribution.FocusPressure == EAITrainingFocus::NoFocus
		&& PressureContribution.SpecialtyPressure == EAITrainingFocusSpecialty::NoTrainingPressure
		&& not PressureContribution.bSpreadFocusPressureAcrossAllBuckets;
	if (bHasNoTrainingPressure)
	{
		return;
	}

	AddFocusPressure(PressureContribution);
	AddSpecialtyPressure(PressureContribution);
	AddRecentPressureDebugEntry(PressureContribution);
}

FEnemyStrategicTrainingSelection FEnemyStrategicTrainingBuckets::PickAndSpendTrainingSelection()
{
	FEnemyStrategicTrainingSelection Selection;
	Selection.Focus = PickFocusBucket();
	Selection.Specialty = PickSpecialtyBucket();

	SpendPickedFocus(Selection.Focus);
	SpendPickedSpecialty(Selection.Specialty);

	if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
		EnemyAISettings::Debugging::TrainingPressureDebugging)
	{
		RTSFunctionLibrary::PrintString(GetDebugString(), FColor::Cyan,
			EnemyAISettings::Debugging::TrainingPressureDebugDuration);
	}

	return Selection;
}

FString FEnemyStrategicTrainingBuckets::GetDebugString() const
{
	FString DebugString = TEXT("Enemy Training Pressure Buckets");
	DebugString += FString::Printf(
		TEXT("\nPicked Focus: %s | Picked Specialty: %s"),
		*GetTrainingFocusDebugString(LastPickedFocus),
		*GetTrainingSpecialtyDebugString(LastPickedSpecialty));

	DebugString += TEXT("\nFocus Buckets:");
	for (const TPair<EAITrainingFocus, float>& FocusBucket : FocusBuckets)
	{
		DebugString += FString::Printf(
			TEXT("\n- %s: %.2f weighted %.2f"),
			*GetTrainingFocusDebugString(FocusBucket.Key),
			FocusBucket.Value,
			GetFocusBucketWeight(FocusBucket.Key, FocusBucket.Value));
	}

	DebugString += TEXT("\nSpecialty Buckets:");
	for (const TPair<EAITrainingFocusSpecialty, float>& SpecialtyBucket : SpecialtyBuckets)
	{
		DebugString += FString::Printf(
			TEXT("\n- %s: %.2f weighted %.2f"),
			*GetTrainingSpecialtyDebugString(SpecialtyBucket.Key),
			SpecialtyBucket.Value,
			GetSpecialtyBucketWeight(SpecialtyBucket.Key, SpecialtyBucket.Value));
	}

	DebugString += TEXT("\nRecent Pressure Sources:");
	for (const FEnemyStrategicTrainingPressureDebugEntry& DebugEntry : RecentPressureDebugEntries)
	{
		DebugString += FString::Printf(
			TEXT("\n- %s: %.2f Focus=%s Specialty=%s Spread=%s"),
			*DebugEntry.SourceDebugName,
			DebugEntry.PressureAmount,
			*GetTrainingFocusDebugString(DebugEntry.FocusPressure),
			*GetTrainingSpecialtyDebugString(DebugEntry.SpecialtyPressure),
			DebugEntry.bSpreadFocusPressureAcrossAllBuckets ? TEXT("true") : TEXT("false"));
	}

	return DebugString;
}

void FEnemyStrategicTrainingBuckets::AddFocusPressure(
	const FEnemyStrategicTrainingPressureContribution& PressureContribution)
{
	if (PressureContribution.bSpreadFocusPressureAcrossAllBuckets)
	{
		const float SplitPressure = PressureContribution.PressureAmount / GetAllTrainingFocusBuckets().Num();
		for (const EAITrainingFocus TrainingFocus : GetAllTrainingFocusBuckets())
		{
			float& FocusPressure = FocusBuckets.FindOrAdd(TrainingFocus);
			FocusPressure += SplitPressure;
		}
		return;
	}

	if (PressureContribution.FocusPressure == EAITrainingFocus::NoFocus)
	{
		return;
	}

	float& FocusPressure = FocusBuckets.FindOrAdd(PressureContribution.FocusPressure);
	FocusPressure += PressureContribution.PressureAmount;
}

void FEnemyStrategicTrainingBuckets::AddSpecialtyPressure(
	const FEnemyStrategicTrainingPressureContribution& PressureContribution)
{
	if (PressureContribution.SpecialtyPressure == EAITrainingFocusSpecialty::NoTrainingPressure)
	{
		return;
	}

	float& SpecialtyPressure = SpecialtyBuckets.FindOrAdd(PressureContribution.SpecialtyPressure);
	SpecialtyPressure += PressureContribution.PressureAmount;
}

void FEnemyStrategicTrainingBuckets::AddRecentPressureDebugEntry(
	const FEnemyStrategicTrainingPressureContribution& PressureContribution)
{
	FEnemyStrategicTrainingPressureDebugEntry DebugEntry;
	DebugEntry.FocusPressure = PressureContribution.FocusPressure;
	DebugEntry.SpecialtyPressure = PressureContribution.SpecialtyPressure;
	DebugEntry.PressureAmount = PressureContribution.PressureAmount;
	DebugEntry.bSpreadFocusPressureAcrossAllBuckets = PressureContribution.bSpreadFocusPressureAcrossAllBuckets;
	DebugEntry.SourceDebugName = PressureContribution.SourceDebugName;
	RecentPressureDebugEntries.Add(DebugEntry);

	while (RecentPressureDebugEntries.Num() > EnemyAISettings::TrainingPressure::MaxRecentDebugEntries)
	{
		RecentPressureDebugEntries.RemoveAt(0);
	}
}

EAITrainingFocus FEnemyStrategicTrainingBuckets::PickFocusBucket() const
{
	return PickWeightedTrainingBucket<EAITrainingFocus>(
		FocusBuckets,
		[this](const EAITrainingFocus Focus, const float Pressure)
		{
			return GetFocusBucketWeight(Focus, Pressure);
		},
		EAITrainingFocus::NoFocus);
}

EAITrainingFocusSpecialty FEnemyStrategicTrainingBuckets::PickSpecialtyBucket() const
{
	return PickWeightedTrainingBucket<EAITrainingFocusSpecialty>(
		SpecialtyBuckets,
		[this](const EAITrainingFocusSpecialty Specialty, const float Pressure)
		{
			return GetSpecialtyBucketWeight(Specialty, Pressure);
		},
		EAITrainingFocusSpecialty::NoTrainingPressure);
}

void FEnemyStrategicTrainingBuckets::SpendPickedFocus(const EAITrainingFocus PickedFocus)
{
	if (PickedFocus == EAITrainingFocus::NoFocus)
	{
		LastPickedFocus = PickedFocus;
		ConsecutiveFocusPickCount = 0;
		return;
	}

	if (float* Pressure = FocusBuckets.Find(PickedFocus))
	{
		*Pressure *= EnemyAISettings::TrainingPressure::PickedBucketRemainingPressureMultiplier;
	}

	ConsecutiveFocusPickCount = LastPickedFocus == PickedFocus ? ConsecutiveFocusPickCount + 1 : 1;
	LastPickedFocus = PickedFocus;
	AddRecentPickedFocus(PickedFocus);
}

void FEnemyStrategicTrainingBuckets::SpendPickedSpecialty(const EAITrainingFocusSpecialty PickedSpecialty)
{
	if (PickedSpecialty == EAITrainingFocusSpecialty::NoTrainingPressure)
	{
		LastPickedSpecialty = PickedSpecialty;
		ConsecutiveSpecialtyPickCount = 0;
		return;
	}

	if (float* Pressure = SpecialtyBuckets.Find(PickedSpecialty))
	{
		*Pressure *= EnemyAISettings::TrainingPressure::PickedBucketRemainingPressureMultiplier;
	}

	ConsecutiveSpecialtyPickCount = LastPickedSpecialty == PickedSpecialty ? ConsecutiveSpecialtyPickCount + 1 : 1;
	LastPickedSpecialty = PickedSpecialty;
	AddRecentPickedSpecialty(PickedSpecialty);
}

float FEnemyStrategicTrainingBuckets::GetFocusBucketWeight(
	const EAITrainingFocus Focus,
	const float Pressure) const
{
	if (Focus == EAITrainingFocus::NoFocus || Pressure <= EnemyAISettings::TrainingPressure::MinimumTrainingBucketPressure)
	{
		return 0.f;
	}

	const int32 RecentPickCount = GetRecentFocusPickCount(Focus);
	return Pressure / (1.f + RecentPickCount * EnemyAISettings::TrainingPressure::RecentPickPenaltyMultiplier);
}

float FEnemyStrategicTrainingBuckets::GetSpecialtyBucketWeight(
	const EAITrainingFocusSpecialty Specialty,
	const float Pressure) const
{
	if (Specialty == EAITrainingFocusSpecialty::NoTrainingPressure
		|| Pressure <= EnemyAISettings::TrainingPressure::MinimumTrainingBucketPressure)
	{
		return 0.f;
	}

	const int32 RecentPickCount = GetRecentSpecialtyPickCount(Specialty);
	return Pressure / (1.f + RecentPickCount * EnemyAISettings::TrainingPressure::RecentPickPenaltyMultiplier);
}

int32 FEnemyStrategicTrainingBuckets::GetRecentFocusPickCount(const EAITrainingFocus Focus) const
{
	int32 RecentPickCount = 0;
	for (const EAITrainingFocus RecentFocus : RecentPickedFocusHistory)
	{
		if (RecentFocus == Focus)
		{
			++RecentPickCount;
		}
	}
	return RecentPickCount;
}

int32 FEnemyStrategicTrainingBuckets::GetRecentSpecialtyPickCount(const EAITrainingFocusSpecialty Specialty) const
{
	int32 RecentPickCount = 0;
	for (const EAITrainingFocusSpecialty RecentSpecialty : RecentPickedSpecialtyHistory)
	{
		if (RecentSpecialty == Specialty)
		{
			++RecentPickCount;
		}
	}
	return RecentPickCount;
}

void FEnemyStrategicTrainingBuckets::AddRecentPickedFocus(const EAITrainingFocus PickedFocus)
{
	RecentPickedFocusHistory.Add(PickedFocus);
	while (RecentPickedFocusHistory.Num() > EnemyAISettings::TrainingPressure::RecentTrainingSelectionHistorySize)
	{
		RecentPickedFocusHistory.RemoveAt(0);
	}
}

void FEnemyStrategicTrainingBuckets::AddRecentPickedSpecialty(const EAITrainingFocusSpecialty PickedSpecialty)
{
	RecentPickedSpecialtyHistory.Add(PickedSpecialty);
	while (RecentPickedSpecialtyHistory.Num() > EnemyAISettings::TrainingPressure::RecentTrainingSelectionHistorySize)
	{
		RecentPickedSpecialtyHistory.RemoveAt(0);
	}
}

void FEnemyStrategicTrainingState::AddTrainingPressureContribution(
	const FEnemyStrategicTrainingPressureContribution& PressureContribution)
{
	TrainingBuckets.AddPressureContribution(PressureContribution);
}

FEnemyStrategicTrainingSelection FEnemyStrategicTrainingState::PickAndSpendTrainingSelection()
{
	return TrainingBuckets.PickAndSpendTrainingSelection();
}
