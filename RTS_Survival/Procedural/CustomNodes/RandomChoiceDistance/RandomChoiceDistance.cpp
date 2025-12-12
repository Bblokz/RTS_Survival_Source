// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RandomChoiceDistance.h"

#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "PCGParamData.h"
#include "Algo/Sort.h"
#include "Algo/Transform.h"
#include "Math/UnrealMathUtility.h"

#define LOCTEXT_NAMESPACE "PCGRandomChoiceDistanceElement"

#if WITH_EDITOR
FName UPCGRandomChoiceDistanceSettings::GetDefaultNodeName() const
{
	return FName(TEXT("RandomChoiceDistance"));
}

FText UPCGRandomChoiceDistanceSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Random Choice Distance");
}

FText UPCGRandomChoiceDistanceSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
	               "Randomly selects a subset of points from the input, ensuring that each chosen point is at least PreferredMinimalDistance away from all previously chosen points.\n\n"
	               "Supports fixed mode (select a fixed number of points) or ratio mode (select a ratio of the total points).");
}
#endif

TArray<FPCGPinProperties> UPCGRandomChoiceDistanceSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	// Require point data on the default input.
	PinProperties.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point).SetRequiredPin();
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGRandomChoiceDistanceSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGRandomChoiceDistanceConstants::ChosenEntriesLabel, EPCGDataType::Point);
	if (bOutputDiscardedEntries)
	{
		PinProperties.Emplace(PCGRandomChoiceDistanceConstants::DiscardedEntriesLabel, EPCGDataType::Point);
	}
	return PinProperties;
}

FPCGElementPtr UPCGRandomChoiceDistanceSettings::CreateElement() const
{
	return MakeShared<FPCGRandomChoiceDistanceElement>();
}

// Helper function: creates a new point data from the input data, selecting only the points with the provided indexes.
namespace PCGRandomChoiceDistance
{
	UPCGData* ChoosePointData(const UPCGData* InData, TArrayView<int32> InIndexes, FPCGContext* InContext)
	{
		const UPCGPointData* InPointData = CastChecked<const UPCGPointData>(InData);
		UPCGPointData* OutPointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(InContext);
		OutPointData->InitializeFromData(InPointData);
		const TArray<FPCGPoint>& InPoints = InPointData->GetPoints();
		TArray<FPCGPoint>& OutPoints = OutPointData->GetMutablePoints();
		OutPoints.Reserve(InIndexes.Num());
		// Ensure stable ordering.
		Algo::Sort(InIndexes);
		for (int32 Index : InIndexes)
		{
			OutPoints.Add(InPoints[Index]);
		}
		return OutPointData;
	}
}

bool FPCGRandomChoiceDistanceElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);

	const UPCGRandomChoiceDistanceSettings* Settings = Context->GetInputSettings<UPCGRandomChoiceDistanceSettings>();
	check(Settings);

	// Initialize a random stream.
	const FRandomStream RandStream(Context->GetSeed());

	// Get input data from the default input pin.
	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	for (const FPCGTaggedData& CurrentInput : Inputs)
	{
		int32 NumElements = 0;
		int32 Seed = Context->GetSeed();
		UPCGData* (*ChooseFunc)(const UPCGData*, TArrayView<int32>, FPCGContext*) = nullptr;

		const UPCGPointData* InputPointData = Cast<UPCGPointData>(CurrentInput.Data);
		if (!InputPointData)
		{
			PCGE_LOG(Verbose, GraphAndLog, LOCTEXT("InvalidData", "Input data is not point data"));
			continue;
		}
		const TArray<FPCGPoint>& InPoints = InputPointData->GetPoints();
		NumElements = InPoints.Num();
		ChooseFunc = &PCGRandomChoiceDistance::ChoosePointData;

		// Determine how many points to choose.
		int32 NumOfElementsToKeep = 0;
		if (Settings->bFixedMode)
		{
			NumOfElementsToKeep = FMath::Clamp(Settings->FixedNumber, 0, NumElements);
		}
		else
		{
			NumOfElementsToKeep = FMath::CeilToInt(NumElements * FMath::Clamp(Settings->Ratio, 0.0f, 1.0f));
		}

		// If no points should be chosen, output empty chosen and (if enabled) forward input to discarded.
		if (NumOfElementsToKeep <= 0)
		{
			if (Settings->bOutputDiscardedEntries)
			{
				FPCGTaggedData& DiscardedOutput = Outputs.Add_GetRef(CurrentInput);
				DiscardedOutput.Pin = PCGRandomChoiceDistanceConstants::DiscardedEntriesLabel;
			}
			FPCGTaggedData& ChosenOutput = Outputs.Add_GetRef(CurrentInput);
			ChosenOutput.Data = ChooseFunc(CurrentInput.Data, {}, Context);
			ChosenOutput.Pin = PCGRandomChoiceDistanceConstants::ChosenEntriesLabel;
			continue;
		}
		else if (NumOfElementsToKeep >= NumElements)
		{
			// If all points are chosen, forward input to chosen and create an empty output for discarded.
			FPCGTaggedData& ChosenOutput = Outputs.Add_GetRef(CurrentInput);
			ChosenOutput.Pin = PCGRandomChoiceDistanceConstants::ChosenEntriesLabel;
			if (Settings->bOutputDiscardedEntries)
			{
				FPCGTaggedData& DiscardedOutput = Outputs.Add_GetRef(CurrentInput);
				DiscardedOutput.Data = ChooseFunc(CurrentInput.Data, {}, Context);
				DiscardedOutput.Pin = PCGRandomChoiceDistanceConstants::DiscardedEntriesLabel;
			}
			continue;
		}

		// Create a candidate list of indexes [0, NumElements-1].
		TArray<int32> CandidateIndexes;
		CandidateIndexes.Reserve(NumElements);
		for (int32 j = 0; j < NumElements; ++j)
		{
			CandidateIndexes.Add(j);
		}

		TArray<int32> ChosenIndexes;
		ChosenIndexes.Reserve(NumOfElementsToKeep);

		// Randomly pick an initial candidate.
		const int32 InitialPos = RandStream.RandRange(0, CandidateIndexes.Num() - 1);
		int32 InitialIndex = CandidateIndexes[InitialPos];
		ChosenIndexes.Add(InitialIndex);
		CandidateIndexes.RemoveAt(InitialPos);

		// Helper lambda: returns true if the candidate point is at least PreferredMinimalDistance
		// away from every already chosen point.
		auto IsFarEnough = [&InPoints, &ChosenIndexes, Settings](int32 CandidateIdx) -> bool
		{
			const FVector& CandidatePos = InPoints[CandidateIdx].Transform.GetLocation();
			for (int32 ChosenIdx : ChosenIndexes)
			{
				const FVector& ChosenPos = InPoints[ChosenIdx].Transform.GetLocation();
				if (FVector::Dist(CandidatePos, ChosenPos) < Settings->PreferredMinimalDistance)
				{
					return false;
				}
			}
			return true;
		};

		// Iteratively choose additional points.
		bool bFoundCandidate = true;
		while (ChosenIndexes.Num() < NumOfElementsToKeep && CandidateIndexes.Num() > 0 && bFoundCandidate)
		{
			bFoundCandidate = false;
			// Pick a random starting position for the candidate iteration.
			int32 StartPos = RandStream.RandRange(0, CandidateIndexes.Num() - 1);
			int32 NumCandidates = CandidateIndexes.Num();
			for (int32 i = 0; i < NumCandidates; ++i)
			{
				int32 CurPos = (StartPos + i) % NumCandidates;
				int32 CandidateIdx = CandidateIndexes[CurPos];
				if (IsFarEnough(CandidateIdx))
				{
					ChosenIndexes.Add(CandidateIdx);
					CandidateIndexes.RemoveAt(CurPos);
					bFoundCandidate = true;
					break; // Restart search after adding one candidate.
				}
			}
		}
		if(Settings->bIfNotSatisfiedPickAny && ChosenIndexes.Num() < NumOfElementsToKeep)
		{
			// If we cannot find enough points with the current constrains; pick any other point?
			while (ChosenIndexes.Num() < NumOfElementsToKeep)
			{
				int32 RandomIndex = RandStream.RandRange(0, NumElements - 1);
				if (!ChosenIndexes.Contains(RandomIndex))
				{
					ChosenIndexes.Add(RandomIndex);
				}
				// Check if there are any points left to choose.
				if (ChosenIndexes.Num() == NumElements)
				{
					break;
				}
			}
		}

		// At this point, ChosenIndexes holds our selected candidate indexes.
		// Sort them to preserve the original order.
		Algo::Sort(ChosenIndexes);
		FPCGTaggedData& ChosenOutput = Outputs.Add_GetRef(CurrentInput);
		ChosenOutput.Data = ChooseFunc(CurrentInput.Data, MakeArrayView(ChosenIndexes.GetData(), ChosenIndexes.Num()),
		                               Context);
		ChosenOutput.Pin = PCGRandomChoiceDistanceConstants::ChosenEntriesLabel;

		if (Settings->bOutputDiscardedEntries)
		{
			// Discarded indexes are those not chosen.
			TArray<int32> AllIndexes;
			AllIndexes.Reserve(NumElements);
			for (int32 j = 0; j < NumElements; ++j)
			{
				AllIndexes.Add(j);
			}
			for (int32 idx : ChosenIndexes)
			{
				AllIndexes.Remove(idx);
			}
			FPCGTaggedData& DiscardedOutput = Outputs.Add_GetRef(CurrentInput);
			DiscardedOutput.Data = ChooseFunc(CurrentInput.Data, MakeArrayView(AllIndexes.GetData(), AllIndexes.Num()),
			                                  Context);
			DiscardedOutput.Pin = PCGRandomChoiceDistanceConstants::DiscardedEntriesLabel;
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
