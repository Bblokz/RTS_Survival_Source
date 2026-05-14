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
	// The selection is intentionally built in two independent parts:
	// - Focus decides the broad unit family, such as squads, light tanks, medium tanks, or heavy tanks.
	// - Specialty decides the tactical reason, such as anti-tank pressure or another role bias.
	//
	// Keeping these separate lets later training code combine a broad family with a role preference
	// instead of forcing every strategic pressure source to name one exact trainable unit.
	FEnemyStrategicTrainingSelection Selection;

	// Pick the strongest weighted focus bucket before spending any pressure.
	// This must happen before SpendPickedFocus because spending lowers the selected bucket,
	// and lowering it first would make the returned selection no longer represent the bucket state
	// that won this decision.
	Selection.Focus = PickFocusBucket();

	// Pick the strongest weighted specialty bucket before spending any pressure.
	// This is also done before any spending so focus and specialty are both chosen from the same
	// pre-spend snapshot of strategic demand.
	Selection.Specialty = PickSpecialtyBucket();

	// After the focus has been selected, spend part of that bucket so the same focus does not
	// automatically dominate every future training decision.
	//
	// The spend is not a full reset. The selected bucket keeps a remaining fraction of its pressure,
	// which means a genuinely important long-term need can still win again later, but weaker competing
	// buckets get a chance to catch up.
	SpendPickedFocus(Selection.Focus);

	// Spend the selected specialty independently from the focus.
	// This allows, for example, the AI to reduce repeated AntiTank specialty pressure without also
	// erasing all pressure for the selected unit family.
	SpendPickedSpecialty(Selection.Specialty);

	// When training-pressure debugging is compiled in and enabled, print the post-spend bucket state.
	// Printing after spending is intentional: designers can see what pressure remains for the next
	// decision instead of only seeing the state that existed before this selection consumed part of it.
	if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
		EnemyAISettings::Debugging::TrainingPressureDebugging)
	{
		RTSFunctionLibrary::PrintString(GetDebugString(), FColor::Cyan,
			EnemyAISettings::Debugging::TrainingPressureDebugDuration);
	}

	// Return the already-picked result so the caller can use this focus/specialty pair for the next
	// tech-aware training choice.
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
