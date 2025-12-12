// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSLandscapeDivider.h"
#include "PCGComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "LandscapeGenerationSettings/RTS_PCG_Parameters.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "SeedSettings/RuntimeSeedManager/RTSRuntimeSeedManager.h"

// Sets default values
ARTSLandscapeDivider::ARTSLandscapeDivider()
{
	PrimaryActorTick.bCanEverTick = false;

	if (!RootComponent)
	{
		RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	}
	RootComponent->SetMobility(EComponentMobility::Static);
	// Init when generating the landscape.
	M_SeedManager = CreateDefaultSubobject<URTSRuntimeSeedManager>("RTSRuntimeSeedManager");
}

void ARTSLandscapeDivider::Regenerate()
{
	GenerateRegions();
}

int32 ARTSLandscapeDivider::GetAmountOfPlayerStartRegions() const
{
	int32 Amount = 0;
	for(auto EachRegion : RegionPresetSettings.PresetRegions)
	{
		if(EachRegion.RegionType == ERTSPresetRegion::PresetRegion_PlayerStart)
		{
			Amount++;
		}
	}
	return Amount;
}

void ARTSLandscapeDivider::BeginPlay()
{
	Super::BeginPlay();
	GenerateRegions();
}

void ARTSLandscapeDivider::GenerateRegions()
{
	if (not EnsureValidSeedManager())
	{
		RTSFunctionLibrary::ReportError(
			"The landscape generation failed due to an invalid seed manager on the landscape divider!");
		return;
	}

	// Remove any existing box components.
	for (auto& BoxComp : M_Boxes)
	{
		if (IsValid(BoxComp))
		{
			BoxComp->DestroyComponent();
		}
	}

	M_Boxes.Empty();
	// Clear preset-region storage.
	M_PresetRegions.Empty();

	// 0. Initialize RandomStream with seed.
	const FRandomStream RandomStream(Seed);
	M_SeedManager->InitializeSeed(Seed);


	// 1. Generate preset regions (which also stores their bounds in M_PresetRegions)
	GeneratePresetRegions(RandomStream);

	// 2. Compute the remaining area for biomes.
	const FBox2D FullArea(FVector2D(-LandscapeExtent, -LandscapeExtent), FVector2D(LandscapeExtent, LandscapeExtent));
	TArray<FBox2D> BiomeCandidateRegions = GenerateBiomeRegions(FullArea, M_PresetRegions,
	                                                            BiomeGenerationSettings.AmountRegions, RandomStream);

	// 3. Create box components for each biome candidate region.
	for (const FBox2D& Region : BiomeCandidateRegions)
	{
		CreateBoxFromRegion(Region);
	}

	// 4. Distribute biome tags among the biome boxes.
	DistributeBiomes(RandomStream);

	// 5. Set up the PCG component after regions have been created.
	SetupPcgComponent();
}


// Private helper struct for storing a placed preset region.
struct FPresetRegionPlacement
{
	// The calculated 2D bounds (with Min as bottom–left and Max as top–right).
	FBox2D RegionBox;
	// The type of preset region (PlayerStart, EnemyStart, etc.).
	ERTSPresetRegion RegionType;
};

void ARTSLandscapeDivider::GeneratePresetRegions(const FRandomStream& RandomStreamWithSeed)
{
	// Define the full landscape area.
	const FBox2D FullLandscapeArea(FVector2D(-LandscapeExtent, -LandscapeExtent),
	                               FVector2D(LandscapeExtent, LandscapeExtent));

	// Container for the successfully placed preset regions.
	TArray<FPresetRegionPlacement> PlacedPresetRegions;

	if (not EnsurePresetRegionsAreValid())
	{
		return;
	}

	if (RegionPresetSettings.PresetRegions.Num() == 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("No preset regions defined in RegionPresetSettings.PresetRegions."));
		return;
	}

	// Attempt to place all preset regions via backtracking.
	const bool bSuccess = BacktrackingPlacePresetRegions(0, PlacedPresetRegions, FullLandscapeArea,
	                                                     RegionPresetSettings,
	                                                     RandomStreamWithSeed);
	if (!bSuccess)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Failed to place all preset regions. The current preset configuration may not be feasible."));
		return;
	}

	M_PresetRegions.Empty();

	// Create a box component for each preset region and store its bounds.
	for (const FPresetRegionPlacement& Placement : PlacedPresetRegions)
	{
		CreateBoxFromRegion(Placement.RegionBox);
		M_PresetRegions.Add(Placement.RegionBox);
		URTSDivisionBox* LastBox = M_Boxes.Last();
		if (RegionPresetSettings.RegionTags.Contains(Placement.RegionType))
		{
			LastBox->ComponentTags.Add(RegionPresetSettings.RegionTags[Placement.RegionType]);
		}
	}
}

bool ARTSLandscapeDivider::EnsurePresetRegionsAreValid()
{
	for (auto EachPreset : RegionPresetSettings.PresetRegions)
	{
		if (EachPreset.MinMaxRegionWidth.X > EachPreset.MinMaxRegionWidth.Y)
		{
			RTSFunctionLibrary::ReportError("Invalid MinMaxRegionWidth for Preset Region");
			return false;
		}
		if (EachPreset.MinMaxRegionWidth.Y > LandscapeExtent)
		{
			RTSFunctionLibrary::ReportError("Invalid MinMaxRegionWidth for Preset Region too large Width");
			return false;
		}
		if (EachPreset.MinMaxRegionHeight.X > EachPreset.MinMaxRegionHeight.Y)
		{
			RTSFunctionLibrary::ReportError("Invalid MinMaxRegionHeight for Preset Region");
			return false;
		}
		if (EachPreset.MinMaxRegionHeight.Y > LandscapeExtent)
		{
			RTSFunctionLibrary::ReportError("Invalid MinMaxRegionHeight for Preset Region too large Height");
			return false;
		}
	}
	return true;
}

// Recursive function that attempts to place all preset regions (starting at RegionIndex)
// within the FullArea by iterating over candidate grid positions and checking minimal distance constraints.
bool ARTSLandscapeDivider::BacktrackingPlacePresetRegions(const int32 RegionIndex,
                                                          TArray<FPresetRegionPlacement>& OutPlacements,
                                                          const FBox2D& FullArea,
                                                          const FRTSLandscapeRegionPresets& PresetSettings,
                                                          const FRandomStream& RandomStreamWithSeed) const
{
	// Base case: all preset regions have been placed.
	if (RegionIndex >= PresetSettings.PresetRegions.Num())
	{
		return true;
	}

	// Retrieve the current preset region settings.
	const FRTSPresetRegion& CurrentPreset = PresetSettings.PresetRegions[RegionIndex];
	const float GridStep = PresetSettings.Stepsize;

	// Try several attempts for generating a region size.
	const int32 MaxDimensionAttempts = 10;
	for (int32 DimensionAttempt = 0; DimensionAttempt < MaxDimensionAttempts; ++DimensionAttempt)
	{
		// Generate a random region width and height (rounded to a multiple of the grid step).
		const float RegionWidth = GenerateDimension(CurrentPreset.MinMaxRegionWidth, GridStep, RandomStreamWithSeed);
		const float RegionHeight = GenerateDimension(CurrentPreset.MinMaxRegionHeight, GridStep, RandomStreamWithSeed);
		const FVector2D RegionSize(RegionWidth, RegionHeight);

		// Get candidate bottom–left positions for this region.
		// For the first preset region, we randomize the order; for subsequent regions we use a deterministic order.
		const bool bRandomizeOrder = (RegionIndex == 0);
		TArray<FVector2D> CandidatePositions = GetCandidateGridPositions(
			RegionSize, FullArea, bRandomizeOrder, RandomStreamWithSeed);

		// Iterate over all candidate positions.
		for (const FVector2D& CandidateBottomLeft : CandidatePositions)
		{
			// Construct the candidate region bounds.
			const FBox2D CandidateRegionBox(CandidateBottomLeft, CandidateBottomLeft + RegionSize);

			// Verify that the candidate region fits entirely within the full landscape area.
			if (!FullArea.IsInsideOrOn(CandidateRegionBox.Min) || !FullArea.IsInsideOrOn(CandidateRegionBox.Max))
			{
				continue; // Does not fit—try the next candidate.
			}

			// Check minimal distance constraints against every region already placed.
			bool bValidPlacement = true;
			for (const FPresetRegionPlacement& PlacedRegion : OutPlacements)
			{
				// Look up the required minimal distance from the current region's settings.
				float RequiredDistance = 0.f;
				if (PresetSettings.RegionMinDistances.Contains(CurrentPreset.RegionType))
				{
					const FRTSPresetRegionMinimalDistances& CurrentMinDistances = PresetSettings.RegionMinDistances[
						CurrentPreset.RegionType];
					if (CurrentMinDistances.MinDistances.Contains(PlacedRegion.RegionType))
					{
						RequiredDistance = CurrentMinDistances.MinDistances[PlacedRegion.RegionType];
					}
				}

				// Calculate the actual distance between the candidate region and the placed region.
				const float ActualDistance = GetDistanceBetweenBoxes(CandidateRegionBox, PlacedRegion.RegionBox);
				if (ActualDistance < RequiredDistance)
				{
					bValidPlacement = false;
					break;
				}
			}

			if (!bValidPlacement)
			{
				continue; // Constraint not met; try next candidate position.
			}

			// Candidate placement is valid—add it and try to place the next preset region.
			FPresetRegionPlacement NewPlacement;
			NewPlacement.RegionBox = CandidateRegionBox;
			NewPlacement.RegionType = CurrentPreset.RegionType;
			OutPlacements.Add(NewPlacement);

			if (BacktrackingPlacePresetRegions(RegionIndex + 1, OutPlacements, FullArea, PresetSettings,
			                                   RandomStreamWithSeed))
			{
				return true; // Successfully placed all subsequent regions.
			}

			// Backtrack: remove the placement and try the next candidate.
			OutPlacements.Pop();
		}
		// If no candidate worked for this dimension attempt, try generating new dimensions.
	}

	// All attempts failed for the current preset region; signal failure.
	return false;
}

// Returns an array of candidate bottom–left positions (grid cells) where a region of RegionSize might be placed.
// The grid covers the FullArea using the provided stepsize. If bRandomizeOrder is true, the candidate list is shuffled.
TArray<FVector2D> ARTSLandscapeDivider::GetCandidateGridPositions(const FVector2D& RegionSize, const FBox2D& FullArea,
                                                                  const bool bRandomizeOrder,
                                                                  const FRandomStream& RandomStreamWithSeed) const
{
	TArray<FVector2D> CandidatePositions;

	// The bottom–left corner must lie within these bounds so that the region fits fully inside FullArea.
	const float MinX = FullArea.Min.X;
	const float MaxX = FullArea.Max.X - RegionSize.X;
	const float MinY = FullArea.Min.Y;
	const float MaxY = FullArea.Max.Y - RegionSize.Y;

	// Iterate over the grid.
	for (float X = MinX; X <= MaxX; X += RegionPresetSettings.Stepsize)
	{
		for (float Y = MinY; Y <= MaxY; Y += RegionPresetSettings.Stepsize)
		{
			CandidatePositions.Add(FVector2D(X, Y));
		}
	}

	// For the very first region, randomize the candidate order.
	if (bRandomizeOrder)
	{
		for (int32 i = CandidatePositions.Num() - 1; i > 0; --i)
		{
			const int32 j = RandomStreamWithSeed.RandRange(0, i);
			CandidatePositions.Swap(i, j);
		}
	}

	return CandidatePositions;
}

// ---------------------------------------------------------------------------
// Generates a random dimension (width or height) within the given range and rounds it down to the nearest multiple of Stepsize.
float ARTSLandscapeDivider::GenerateDimension(const FVector2D& MinMaxRange, const float Stepsize,
                                              const FRandomStream& RandomStreamWithSeed) const
{
	const float RandomDimension = RandomStreamWithSeed.RandRange(MinMaxRange.X, MinMaxRange.Y);
	// Round to the nearest lower multiple of Stepsize.
	const float RoundedDimension = FMath::FloorToFloat(RandomDimension / Stepsize) * Stepsize;
	// Ensure that the dimension is at least one grid cell in size.
	return FMath::Max(RoundedDimension, Stepsize);
}

// ---------------------------------------------------------------------------
// Computes the minimum distance between two axis–aligned 2D boxes.
// If the boxes overlap along an axis, the gap for that axis is 0.
float ARTSLandscapeDivider::GetDistanceBetweenBoxes(const FBox2D& BoxA, const FBox2D& BoxB) const
{
	float dx = 0.f;
	if (BoxA.Max.X < BoxB.Min.X)
	{
		dx = BoxB.Min.X - BoxA.Max.X;
	}
	else if (BoxB.Max.X < BoxA.Min.X)
	{
		dx = BoxA.Min.X - BoxB.Max.X;
	}

	float dy = 0.f;
	if (BoxA.Max.Y < BoxB.Min.Y)
	{
		dy = BoxB.Min.Y - BoxA.Max.Y;
	}
	else if (BoxB.Max.Y < BoxA.Min.Y)
	{
		dy = BoxA.Min.Y - BoxB.Max.Y;
	}

	return FMath::Sqrt(dx * dx + dy * dy);
}


TArray<FBox2D> ARTSLandscapeDivider::GenerateBiomeRegions(const FBox2D& FullArea, const TArray<FBox2D>& PresetBoxes,
                                                          int32 TargetRegions,
                                                          const FRandomStream& RandomStreamWithSeed) const
{
	// Subtract all preset boxes from the full area.
	TArray<FBox2D> UncoveredRegions = SubtractPresetRegions(FullArea, PresetBoxes);

	// Start with the uncovered regions as our candidate regions.
	TArray<FBox2D> CandidateRegions = UncoveredRegions;

	// Subdivide the largest candidate region until we have at least TargetRegions.
	while (CandidateRegions.Num() < TargetRegions)
	{
		// Find the candidate region with the largest area.
		int32 LargestIndex = 0;
		float LargestArea = 0.f;
		for (int32 i = 0; i < CandidateRegions.Num(); ++i)
		{
			float Area = CandidateRegions[i].GetArea();
			if (Area > LargestArea)
			{
				LargestArea = Area;
				LargestIndex = i;
			}
		}

		FBox2D RegionToSplit = CandidateRegions[LargestIndex];
		FVector2D RegionSize = RegionToSplit.GetSize();
		bool bSplitVertically = (RegionSize.Y >= RegionSize.X);
		FBox2D FirstSplit, SecondSplit;

		if (bSplitVertically)
		{
			const float TotalWidth = RegionSize.Y;
			const float MinWidth = BiomeGenerationSettings.MinMaxRegionWidth.X;
			float MaxWidth = BiomeGenerationSettings.MinMaxRegionWidth.Y;
			MaxWidth = FMath::Min(MaxWidth, TotalWidth - MinWidth);

			if (TotalWidth < 2 * MinWidth)
			{
				const float LeftWidth = TotalWidth * 0.5f;
				const float SplitY = RegionToSplit.Min.Y + LeftWidth;
				FirstSplit = FBox2D(RegionToSplit.Min, FVector2D(RegionToSplit.Max.X, SplitY));
				SecondSplit = FBox2D(FVector2D(RegionToSplit.Min.X, SplitY), RegionToSplit.Max);
			}
			else
			{
				const float LeftWidth = RandomStreamWithSeed.RandRange(MinWidth, MaxWidth);
				const float SplitY = RegionToSplit.Min.Y + LeftWidth;
				FirstSplit = FBox2D(RegionToSplit.Min, FVector2D(RegionToSplit.Max.X, SplitY));
				SecondSplit = FBox2D(FVector2D(RegionToSplit.Min.X, SplitY), RegionToSplit.Max);
			}
		}
		else // split horizontally
		{
			const float TotalHeight = RegionSize.X;
			const float MinHeight = BiomeGenerationSettings.MinMaxRegionHeight.X;
			float MaxHeight = BiomeGenerationSettings.MinMaxRegionHeight.Y;
			MaxHeight = FMath::Min(MaxHeight, TotalHeight - MinHeight);

			if (TotalHeight < 2 * MinHeight)
			{
				const float TopHeight = TotalHeight * 0.5f;
				const float SplitX = RegionToSplit.Max.X - TopHeight;
				FirstSplit = FBox2D(FVector2D(SplitX, RegionToSplit.Min.Y), RegionToSplit.Max);
				SecondSplit = FBox2D(RegionToSplit.Min, FVector2D(SplitX, RegionToSplit.Max.Y));
			}
			else
			{
				const float TopHeight = RandomStreamWithSeed.RandRange(MinHeight, MaxHeight);
				const float SplitX = RegionToSplit.Max.X - TopHeight;
				FirstSplit = FBox2D(FVector2D(SplitX, RegionToSplit.Min.Y), RegionToSplit.Max);
				SecondSplit = FBox2D(RegionToSplit.Min, FVector2D(SplitX, RegionToSplit.Max.Y));
			}
		}

		// Replace the region that was split with the two new regions.
		CandidateRegions.RemoveAt(LargestIndex);
		CandidateRegions.Add(FirstSplit);
		CandidateRegions.Add(SecondSplit);
	}

	return CandidateRegions;
}


TArray<FBox2D> ARTSLandscapeDivider::SubtractPresetRegions(const FBox2D& FullArea,
                                                           const TArray<FBox2D>& PresetBoxes) const
{
	TArray<FBox2D> Result;
	Result.Add(FullArea);

	for (const FBox2D& Preset : PresetBoxes)
	{
		TArray<FBox2D> NewResult;
		for (const FBox2D& Region : Result)
		{
			TArray<FBox2D> Subtracted = SubtractRect(Region, Preset);
			NewResult.Append(Subtracted);
		}
		Result = NewResult;
	}

	return Result;
}

TArray<FBox2D> ARTSLandscapeDivider::SubtractRect(const FBox2D& A, const FBox2D& B) const
{
	TArray<FBox2D> Result;
	if (!A.Intersect(B))
	{
		Result.Add(A);
		return Result;
	}

	// Calculate the intersection rectangle.
	FBox2D Intersection = A.Overlap(B);

	// Top region.
	if (A.Max.Y > Intersection.Max.Y)
	{
		FBox2D TopRect(FVector2D(A.Min.X, Intersection.Max.Y), FVector2D(A.Max.X, A.Max.Y));
		if (TopRect.GetArea() > 0.f)
		{
			Result.Add(TopRect);
		}
	}
	// Bottom region.
	if (A.Min.Y < Intersection.Min.Y)
	{
		FBox2D BottomRect(FVector2D(A.Min.X, A.Min.Y), FVector2D(A.Max.X, Intersection.Min.Y));
		if (BottomRect.GetArea() > 0.f)
		{
			Result.Add(BottomRect);
		}
	}
	// Left region.
	if (A.Min.X < Intersection.Min.X)
	{
		FBox2D LeftRect(FVector2D(A.Min.X, Intersection.Min.Y), FVector2D(Intersection.Min.X, Intersection.Max.Y));
		if (LeftRect.GetArea() > 0.f)
		{
			Result.Add(LeftRect);
		}
	}
	// Right region.
	if (A.Max.X > Intersection.Max.X)
	{
		FBox2D RightRect(FVector2D(Intersection.Max.X, Intersection.Min.Y), FVector2D(A.Max.X, Intersection.Max.Y));
		if (RightRect.GetArea() > 0.f)
		{
			Result.Add(RightRect);
		}
	}

	return Result;
}

bool ARTSLandscapeDivider::EnsureValidSeedManager()
{
	if (IsValid(M_SeedManager))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Seed Manager is not valid, At divider: " + GetName() +
		"\n Attempting repair.");
	M_SeedManager = nullptr;
	M_SeedManager = NewObject<URTSRuntimeSeedManager>(this, TEXT("RTSRuntimeSeedManager"));
	return IsValid(M_SeedManager);
}

void ARTSLandscapeDivider::SetupPcgGraphParameters(const FRTSPCGParameters& ParametersToSet, UPCGGraph* Graph) const
{
	if (not IsValid(Graph))
	{
		return;
	}
	FName ParamName = "PlayerStartSmallResourcePoints";
	CheckPcgParameterResult(Graph->SetGraphParameter<int32>(ParamName, ParametersToSet.PlayerStartSmallResourcePoints),
	                        ParamName);
	ParamName = "PlayerStartSmallResourceRadixitePoints";
	CheckPcgParameterResult(Graph->SetGraphParameter<int32>(ParamName, ParametersToSet.PlayerStartSmallResourceRadixitePoints),
	                        ParamName);
	ParamName = "PlayerStartSmallResourceVehiclePartsPoints";
	CheckPcgParameterResult(Graph->SetGraphParameter<int32>(ParamName, ParametersToSet.PlayerStartSmallResourceVehiclePartsPoints),
	                        ParamName);
	ParamName = "PlayerStartMediumResourcePoints";
	CheckPcgParameterResult(Graph->SetGraphParameter<int32>(ParamName, ParametersToSet.PlayerStartMediumResourcePoints),
	                        ParamName);
	ParamName = "PlayerStartMediumResourceRadixitePoints";
	CheckPcgParameterResult(Graph->SetGraphParameter<int32>(ParamName, ParametersToSet.PlayerStartMediumResourceRadixitePoints),
	                        ParamName);
	ParamName = "PlayerStartMediumScavengeObjects";
	CheckPcgParameterResult(Graph->SetGraphParameter<int32>(ParamName, ParametersToSet.PlayerStartMediumScavengeObjects),
	                        ParamName);
}

void ARTSLandscapeDivider::EnsurePcgParametersAreConsistent(FRTSPCGParameters& OutParametersToCheck) const
{
	if(OutParametersToCheck.PlayerStartSmallResourceRadixitePoints >= OutParametersToCheck.PlayerStartSmallResourcePoints)
	{
		RTSFunctionLibrary::ReportError("PlayerStartSmallResourceRadixitePoints must be less than PlayerStartSmallResourcePoints,"
								  "\n Set value: " + FString::FromInt(OutParametersToCheck.PlayerStartSmallResourceRadixitePoints) +
								  "\n Small Resource Points : " + FString::FromInt(OutParametersToCheck.PlayerStartSmallResourcePoints) +
								  "\n Will reset to half of the small resource points instead.");
		OutParametersToCheck.PlayerStartMediumResourceRadixitePoints = FMath::CeilToInt(static_cast<float>(OutParametersToCheck.PlayerStartSmallResourcePoints / 2));
	}
	if(OutParametersToCheck.PlayerStartMediumResourceRadixitePoints >= OutParametersToCheck.PlayerStartMediumResourcePoints)
	{
		RTSFunctionLibrary::ReportError("PlayerStartMediumResourceRadixitePoints must be less than PlayerStartMediumResourcePoints,"
								  "\n Set value: " + FString::FromInt(OutParametersToCheck.PlayerStartMediumResourceRadixitePoints) +
								  "\n Medium Resource Points : " + FString::FromInt(OutParametersToCheck.PlayerStartMediumResourcePoints) +
								  "\n Will reset to half of the medium resource points instead.");
		OutParametersToCheck.PlayerStartMediumResourceRadixitePoints = FMath::CeilToInt(static_cast<float>(OutParametersToCheck.PlayerStartMediumResourcePoints / 2));
	}
	if(OutParametersToCheck.PlayerStartSmallResourcePoints < 3 )
	{
		RTSFunctionLibrary::ReportError("Not enough small resource points provided for pcg graph to fill player starting area with all resources!"
								  "\n Set value: " + FString::FromInt(OutParametersToCheck.PlayerStartSmallResourcePoints) +
								  "\n Will reset to 3 instead with resource evenly distributed.");
		OutParametersToCheck.PlayerStartMediumResourceRadixitePoints = 1;
		OutParametersToCheck.PlayerStartSmallResourceVehiclePartsPoints = 1;
	}
	if(OutParametersToCheck.PlayerStartSmallResourceVehiclePartsPoints <= 0)
	{
		RTSFunctionLibrary::ReportError("Not enough vehicle parts provided for pcg graph to fill player starting area with all resources!"
								  "\n Set value: " + FString::FromInt(OutParametersToCheck.PlayerStartSmallResourceVehiclePartsPoints) +
								  "\n Will reset to 1 instead.");
		OutParametersToCheck.PlayerStartSmallResourceVehiclePartsPoints = 1;
	}
	EnsureResourceRegionsNonZero(OutParametersToCheck);
}

void ARTSLandscapeDivider::EnsureResourceRegionsNonZero(FRTSPCGParameters& OutParametersToCheck) const
{
	const int32 AmountSmallRadixiteStart = OutParametersToCheck.PlayerStartSmallResourceRadixitePoints;
	const int32 AmountSmallVehiclePartsStart = OutParametersToCheck.PlayerStartSmallResourceVehiclePartsPoints;
	const int32 AmountSmallMetalStart = OutParametersToCheck.PlayerStartSmallResourcePoints - AmountSmallRadixiteStart;
	if(AmountSmallRadixiteStart <=0 || AmountSmallVehiclePartsStart <=0 || AmountSmallMetalStart <= 0)
	{
		RTSFunctionLibrary::ReportError("The PCG parameters were setup to lead to zero set(s) of resource points!"
								  "\n Small Radixite: " + FString::FromInt(AmountSmallRadixiteStart) +
								  "\n Small Vehicle Parts: " + FString::FromInt(AmountSmallVehiclePartsStart) +
								  "\n Small Metal: " + FString::FromInt(AmountSmallMetalStart) +
								  "\n will reset to 3, 1, 2 respectively.");
		OutParametersToCheck.PlayerStartSmallResourcePoints = 5;
		OutParametersToCheck.PlayerStartSmallResourceRadixitePoints = 3;
		OutParametersToCheck.PlayerStartSmallResourceVehiclePartsPoints = 1;
	}
	const int32 AmountMediumRadixite = OutParametersToCheck.PlayerStartMediumResourceRadixitePoints;
	const int32 AmountMediumMetal = OutParametersToCheck.PlayerStartMediumResourcePoints - AmountMediumRadixite;
	if(AmountMediumMetal <= 0 || AmountMediumRadixite <= 0)
	{
		RTSFunctionLibrary::ReportError("The PCG parameters were setup to lead to zero set(s) of medium resource points!"
								  "\n Medium Radixite: " + FString::FromInt(AmountMediumRadixite) +
								  "\n Medium Metal: " + FString::FromInt(AmountMediumMetal) +
								  "\n will reset to 2, 1 respectively.");
		OutParametersToCheck.PlayerStartMediumResourcePoints = 3;
		OutParametersToCheck.PlayerStartMediumResourceRadixitePoints = 2;
	}
	const int32 AmountMediumScavengeObjectsPlayerStart = OutParametersToCheck.PlayerStartMediumScavengeObjects;
	if(AmountMediumScavengeObjectsPlayerStart <= 0)
	{
		RTSFunctionLibrary::ReportError("The PCG parameters were setup to lead to zero set(s) of medium scavenge objects!"
								  "\n Medium Scavenge Objects: " + FString::FromInt(AmountMediumScavengeObjectsPlayerStart) +
								  "\n will reset to 3.");
		OutParametersToCheck.PlayerStartMediumScavengeObjects = 3;
	}
}

void ARTSLandscapeDivider::CheckPcgParameterResult(const EPropertyBagResult Result, const FName& ParameterName) const
{
	switch (Result)
	{
	case EPropertyBagResult::Success:
		break;
	case EPropertyBagResult::TypeMismatch:
		RTSFunctionLibrary::ReportError("Type mismatch when setting parameter: " + ParameterName.ToString() +
			"\n on the pcg graph of the landscape divider.");
		break;
	case EPropertyBagResult::OutOfBounds:
		RTSFunctionLibrary::ReportError("Out of bounds when setting parameter: " + ParameterName.ToString() +
			"\n on the pcg graph of the landscape divider.");
		break;
	case EPropertyBagResult::PropertyNotFound:
		RTSFunctionLibrary::ReportError("Property not found when setting parameter: " + ParameterName.ToString() +
			"\n on the pcg graph of the landscape divider.");
		break;
	case EPropertyBagResult::DuplicatedValue:
		RTSFunctionLibrary::ReportError("Duplicated value when setting parameter: " + ParameterName.ToString() +
			"\n on the pcg graph of the landscape divider.");
		break;
	}
}


void ARTSLandscapeDivider::CreateBoxFromRegion(const FBox2D& RegionBox)
{
	// Create a new box component.
	URTSDivisionBox* BoxComp = NewObject<URTSDivisionBox>(this);
	if (!IsValid(BoxComp))
	{
		OnInvalidBoxGeneration();
		return;
	}
	BoxComp->RegisterComponent();
	BoxComp->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	M_Boxes.Add(BoxComp);

	// Calculate the 2D center and size of the region.
	const FVector2D RegionSize2D = RegionBox.GetSize();
	const FVector2D RegionCenter2D = RegionBox.GetCenter();

	// Set the center (with Z = 0) and the box extents.
	const FVector RegionCenter(RegionCenter2D.X, RegionCenter2D.Y, 0.f);
	const FVector BoxExtent(RegionSize2D.X * 0.5f, RegionSize2D.Y * 0.5f, 50.f);

	BoxComp->SetBoxExtent(BoxExtent);
	BoxComp->SetRelativeLocation(RegionCenter);
	BoxComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

//------------------------------------------------------------------------------
void ARTSLandscapeDivider::SetupPcgComponent()
{
	// Check if PcGraphInstance is valid and its graph is not null.
	if (!PcGraphInstance || !PcGraphInstance->GetGraph())
	{
		FName GraphName = IsValid(PcGraphInstance) ? PcGraphInstance->GetFName() : NAME_None;
		RTSFunctionLibrary::ReportError(
			"Invalid Pcgraph instance or instance does not have a graph set!\nGraph Name: " + GraphName.ToString() +
			"\n");
		return;
	}

	// If the PCG component does not exist, create it.
	if (!PcgComponent)
	{
		PcgComponent = NewObject<UPCGComponent>(this);
		PcgComponent->RegisterComponent();
	}
	// Set the seed on the PCG component.
	PcgComponent->Seed = Seed;

	// Make sure that the parameters are self-consistent.
	EnsurePcgParametersAreConsistent(PcgParameters);
	SetupPcgGraphParameters(PcgParameters, PcGraphInstance->GetGraph());
	
	// Set the graph on the PCG component using the graph from PcGraphInstance.
	PcgComponent->SetGraph(PcGraphInstance->GetGraph());
	PcgComponent->ClearPerPinGeneratedOutput();
	PcgComponent->Generate(true);
}

void ARTSLandscapeDivider::OnInvalidBoxGeneration()
{
	RTSFunctionLibrary::ReportError(
		"Could not generate a valid RTS Division box component for the ARTSLandscapeDivider actor.");
}

void ARTSLandscapeDivider::DistributeBiomes(const FRandomStream& RandomStreamWithSeed)
{
	// Gather all vacant boxes (those with no biome tag assigned).
	TArray<URTSDivisionBox*> VacantBoxes;
	for (auto& BoxComp : M_Boxes)
	{
		if (IsValid(BoxComp))
		{
			// A box is considered vacant if it has no component tags.
			if (BoxComp->ComponentTags.Num() == 0)
			{
				VacantBoxes.Add(BoxComp);
			}
		}
	}

	// First, assign minimal biome amounts.
	AssignMinimalBiomes(VacantBoxes);

	// Then, assign any remaining boxes using weighted random selection.
	AssignWeightedBiomes(VacantBoxes, RandomStreamWithSeed);
}

void ARTSLandscapeDivider::AssignMinimalBiomes(TArray<URTSDivisionBox*>& VacantBoxes)
{
	bool bAssigned = true;
	// Continue looping as long as an assignment was made and there are still vacant boxes.
	while (bAssigned && VacantBoxes.Num() > 0)
	{
		bAssigned = false;
		for (FBiomeDistribution& Biome : BiomeGenerationSettings.BiomeDistributions)
		{
			if (Biome.MinimalAmount > 0 && VacantBoxes.Num() > 0)
			{
				URTSDivisionBox* SelectedBox = nullptr;
				if (Biome.PreferLargestVacantBiome)
				{
					SelectedBox = GetLargestBox(VacantBoxes);
				}
				else if (Biome.PreferSmallestVacantBiome)
				{
					SelectedBox = GetSmallestBox(VacantBoxes);
				}
				else
				{
					// Simply choose the first available box.
					SelectedBox = VacantBoxes[0];
				}

				if (SelectedBox)
				{
					// Assign the biome tag.
					SelectedBox->ComponentTags.Add(Biome.BiomeName);
					// Decrement the minimal amount.
					Biome.MinimalAmount--;
					// Remove the box from the vacant list.
					VacantBoxes.Remove(SelectedBox);
					bAssigned = true;
				}
			}
		}
	}
}

void ARTSLandscapeDivider::AssignWeightedBiomes(TArray<URTSDivisionBox*>& VacantBoxes,
                                                const FRandomStream& RandomStreamWithSeed)
{
	while (VacantBoxes.Num() > 0)
	{
		if (BiomeGenerationSettings.BiomeDistributions.IsEmpty())
		{
			RTSFunctionLibrary::ReportError("No biome settings found in BiomeGenerationSettings.BiomeDistributions.");
			return;
		}
		// Compute total weight from all biome distributions.
		float TotalWeight = 0.f;
		for (const FBiomeDistribution& Biome : BiomeGenerationSettings.BiomeDistributions)
		{
			TotalWeight += Biome.Weight;
		}

		// Generate a random value between 0 and TotalWeight.
		float RandVal = RandomStreamWithSeed.RandRange(0.f, TotalWeight);
		float Accum = 0.f;
		FBiomeDistribution* SelectedBiome = nullptr;
		for (FBiomeDistribution& Biome : BiomeGenerationSettings.BiomeDistributions)
		{
			Accum += Biome.Weight;
			if (RandVal <= Accum)
			{
				SelectedBiome = &Biome;
				break;
			}
		}
		if (!SelectedBiome)
		{
			SelectedBiome = &BiomeGenerationSettings.BiomeDistributions[0];
		}

		URTSDivisionBox* SelectedBox = nullptr;
		if (SelectedBiome->PreferLargestVacantBiome)
		{
			SelectedBox = GetLargestBox(VacantBoxes);
		}
		else if (SelectedBiome->PreferSmallestVacantBiome)
		{
			SelectedBox = GetSmallestBox(VacantBoxes);
		}
		else
		{
			// Choose a random vacant box.
			int32 RandIndex = RandomStreamWithSeed.RandRange(0, VacantBoxes.Num() - 1);
			SelectedBox = VacantBoxes[RandIndex];
		}

		if (SelectedBox)
		{
			SelectedBox->ComponentTags.Add(SelectedBiome->BiomeName);
			VacantBoxes.Remove(SelectedBox);
		}
	}
}

URTSDivisionBox* ARTSLandscapeDivider::GetLargestBox(const TArray<URTSDivisionBox*>& Boxes)
{
	URTSDivisionBox* Largest = nullptr;
	float LargestArea = -1.f;
	for (URTSDivisionBox* Box : Boxes)
	{
		if (IsValid(Box))
		{
			// Calculate area using the unscaled box extent.
			FVector Extent = Box->GetUnscaledBoxExtent();
			float Area = 4.f * Extent.X * Extent.Y; // (2 * X) * (2 * Y)
			if (Area > LargestArea)
			{
				LargestArea = Area;
				Largest = Box;
			}
		}
	}
	return Largest;
}

URTSDivisionBox* ARTSLandscapeDivider::GetSmallestBox(const TArray<URTSDivisionBox*>& Boxes)
{
	URTSDivisionBox* Smallest = nullptr;
	float SmallestArea = FLT_MAX;
	for (URTSDivisionBox* Box : Boxes)
	{
		if (IsValid(Box))
		{
			FVector Extent = Box->GetUnscaledBoxExtent();
			float Area = 4.f * Extent.X * Extent.Y;
			if (Area < SmallestArea)
			{
				SmallestArea = Area;
				Smallest = Box;
			}
		}
	}
	return Smallest;
}
