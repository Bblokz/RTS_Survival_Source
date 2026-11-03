// Copyright (C) Bas Blokzijl - All rights reserved.

#include "PCGSurvivalSitePicker.h"

#include "Data/PCGPointData.h"
#include "EngineUtils.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "PCGComponent.h"
#include "PCGPin.h"
#include "RTS_Survival/SurvivalMode/StartLocations/SurvivalStartPlacementHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

#define LOCTEXT_NAMESPACE "PCGSurvivalSitePicker"

namespace SurvivalSitePickerConstants
{
	const FName ActorClassAttribute = TEXT("ActorClass");
	constexpr float GroundTraceUp = 20000.f;
	constexpr float GroundTraceDown = 50000.f;
	constexpr double PointExtent = 100.0;
	constexpr int32 AttemptsPerMember = 24;
}

namespace
{
	/** @brief The picker's own running state; one instance lives for one execution of the node. */
	struct FSurvivalSitePickRun
	{
		const UPCGSurvivalSitePickerSettings* Settings = nullptr;
		const FSurvivalGroundProjector* Projector = nullptr;
		UPCGPointData* PointData = nullptr;
		FPCGMetadataAttribute<FSoftClassPath>* ClassAttribute = nullptr;
		FVector2D MapCentre = FVector2D::ZeroVector;
		FRandomStream Random;
		TMap<FSoftObjectPath, int32> UsedPerClass;
	};

	TArray<ASurvivalMapSite*> GatherSites(UWorld& World, const UPCGSurvivalSitePickerSettings& Settings,
	                                      const FVector2D& MapCentre)
	{
		TArray<ASurvivalMapSite*> Sites;
		for (TActorIterator<ASurvivalMapSite> It(&World); It; ++It)
		{
			if (not IsValid(*It) or It->GetCategory() != Settings.SiteCategory)
			{
				continue;
			}
			const float DistanceFromCentre = FVector2D::Distance(FVector2D(It->GetActorLocation()), MapCentre);
			if (DistanceFromCentre >= Settings.MinDistanceFromCentre and DistanceFromCentre <= Settings.MaxDistanceFromCentre)
			{
				Sites.Add(*It);
			}
		}
		// Actor iteration order is not stable between runs; the seed must give the same picks every time.
		Sites.Sort([](const ASurvivalMapSite& A, const ASurvivalMapSite& B) { return A.GetName() < B.GetName(); });
		return Sites;
	}

	template <typename T>
	void ShuffleWithStream(TArray<T>& Items, const FRandomStream& Random)
	{
		for (int32 Index = Items.Num() - 1; Index > 0; --Index)
		{
			Items.Swap(Index, Random.RandRange(0, Index));
		}
	}

	int32 GetCompassSector(const FVector2D& Position, const FVector2D& MapCentre, const int32 SectorCount)
	{
		const FVector2D Offset = Position - MapCentre;
		const float Degrees = FRotator::NormalizeAxis(FMath::RadiansToDegrees(FMath::Atan2(Offset.Y, Offset.X))) + 180.f;
		return FMath::Clamp(FMath::FloorToInt(Degrees / 360.f * SectorCount), 0, SectorCount - 1);
	}

	bool GetKeepsSpacing(const FVector2D& Position, const TArray<FVector2D>& Taken, const float Spacing)
	{
		for (const FVector2D& Other : Taken)
		{
			if (FVector2D::DistSquared(Position, Other) < FMath::Square(Spacing))
			{
				return false;
			}
		}
		return true;
	}

	/** @brief Goes round the compass taking one shuffled site per wedge, so no side of the map stays empty. */
	TArray<ASurvivalMapSite*> PickSites(FSurvivalSitePickRun& Run, TArray<ASurvivalMapSite*> Candidates)
	{
		const UPCGSurvivalSitePickerSettings& Settings = *Run.Settings;
		const int32 Wanted = Run.Random.RandRange(Settings.MinSites, FMath::Max(Settings.MinSites, Settings.MaxSites));
		const int32 SectorCount = FMath::Max(1, Settings.CompassSectors);
		ShuffleWithStream(Candidates, Run.Random);
		TArray<TArray<ASurvivalMapSite*>> PerSector;
		PerSector.SetNum(SectorCount);
		for (ASurvivalMapSite* Site : Candidates)
		{
			const FVector2D Position(Site->GetActorLocation());
			PerSector[GetCompassSector(Position, Run.MapCentre, SectorCount)].Add(Site);
		}
		TArray<ASurvivalMapSite*> Picked;
		TArray<FVector2D> Taken;
		int32 Sector = Run.Random.RandRange(0, SectorCount - 1);
		int32 EmptyInARow = 0;
		while (Picked.Num() < Wanted and EmptyInARow < SectorCount)
		{
			TArray<ASurvivalMapSite*>& Bucket = PerSector[Sector];
			Sector = (Sector + 1) % SectorCount;
			const int32 FoundIndex = Bucket.IndexOfByPredicate([&Taken, &Settings](const ASurvivalMapSite* Site)
			{
				return GetKeepsSpacing(FVector2D(Site->GetActorLocation()), Taken, Settings.MinSiteSpacing);
			});
			if (FoundIndex == INDEX_NONE)
			{
				++EmptyInARow;
				continue;
			}
			EmptyInARow = 0;
			ASurvivalMapSite* Site = Bucket[FoundIndex];
			Bucket.RemoveAt(FoundIndex);
			Picked.Add(Site);
			Taken.Add(FVector2D(Site->GetActorLocation()));
		}
		return Picked;
	}

	const FSurvivalSiteActorEntry* PickEntry(FSurvivalSitePickRun& Run, const TArray<FSurvivalSiteActorEntry>& Pool)
	{
		float TotalWeight = 0.f;
		TArray<const FSurvivalSiteActorEntry*> Allowed;
		for (const FSurvivalSiteActorEntry& Entry : Pool)
		{
			const int32* Used = Run.UsedPerClass.Find(Entry.ActorClass.ToSoftObjectPath());
			const bool bAtLimit = Entry.MaxPerGeneration > 0 and Used != nullptr and *Used >= Entry.MaxPerGeneration;
			if (Entry.ActorClass.IsNull() or Entry.Weight <= 0.f or bAtLimit)
			{
				continue;
			}
			Allowed.Add(&Entry);
			TotalWeight += Entry.Weight;
		}
		float Roll = Run.Random.FRandRange(0.f, TotalWeight);
		for (const FSurvivalSiteActorEntry* Entry : Allowed)
		{
			Roll -= Entry->Weight;
			if (Roll <= 0.f)
			{
				return Entry;
			}
		}
		return Allowed.IsEmpty() ? nullptr : Allowed.Last();
	}

	void AddActorPoint(FSurvivalSitePickRun& Run, const FSurvivalSiteActorEntry& Entry, const FVector2D& Position,
	                   const float YawDegrees)
	{
		FSurvivalGroundHit Ground;
		if (not Run.Projector->TryProject(Position, Ground))
		{
			return;
		}
		const FQuat Yaw(FVector::UpVector, FMath::DegreesToRadians(YawDegrees + Entry.YawOffsetDegrees));
		const FQuat Tilt = Entry.bAlignToGround ? FQuat::FindBetweenNormals(FVector::UpVector, Ground.Normal) : FQuat::Identity;
		const FVector Location = Ground.Location + FVector::UpVector * Entry.GroundOffset;
		FPCGPoint& Point = Run.PointData->GetMutablePoints().AddDefaulted_GetRef();
		Point.Transform = FTransform(Tilt * Yaw, Location, FVector::OneVector);
		Point.Density = 1.f;
		Point.Steepness = 1.f;
		Point.Seed = Run.Random.RandHelper(MAX_int32);
		Point.BoundsMin = FVector(-SurvivalSitePickerConstants::PointExtent);
		Point.BoundsMax = FVector(SurvivalSitePickerConstants::PointExtent);
		Run.PointData->Metadata->InitializeOnSet(Point.MetadataEntry);
		const FSoftObjectPath ClassPath = Entry.ActorClass.ToSoftObjectPath();
		Run.ClassAttribute->SetValue(Point.MetadataEntry, FSoftClassPath(ClassPath.ToString()));
		++Run.UsedPerClass.FindOrAdd(ClassPath);
	}

	float GetCentreYaw(FSurvivalSitePickRun& Run, const ASurvivalMapSite& Site)
	{
		switch (Run.Settings->CentreYawMode)
		{
		case ESurvivalSiteYawMode::FaceOutward:
			return FMath::RadiansToDegrees(FMath::Atan2(Site.GetActorLocation().Y - Run.MapCentre.Y,
			                                            Site.GetActorLocation().X - Run.MapCentre.X));
		case ESurvivalSiteYawMode::KeepSiteYaw:
			return Site.GetActorRotation().Yaw;
		default:
			return Run.Random.FRandRange(0.f, 360.f);
		}
	}

	/** @brief Compares the ground at Position with the straight lines across the footprint, so a tilt does not count. */
	bool GetIsGroundEvenUnder(const FSurvivalSitePickRun& Run, const FSurvivalSiteActorEntry& Entry, const FVector2D& Position)
	{
		if (Entry.EvenGroundRadius <= 0.f)
		{
			return true;
		}
		float CentreHeight = 0.f;
		if (not Run.Projector->TryGetHeight(Position, CentreHeight))
		{
			return false;
		}
		constexpr int32 DiameterCount = 4;
		for (int32 DiameterIndex = 0; DiameterIndex < DiameterCount; ++DiameterIndex)
		{
			const FVector2D Offset = FVector2D(Entry.EvenGroundRadius, 0.f).GetRotated(180.f * DiameterIndex / DiameterCount);
			float HeightA = 0.f;
			float HeightB = 0.f;
			if (not Run.Projector->TryGetHeight(Position + Offset, HeightA)
				or not Run.Projector->TryGetHeight(Position - Offset, HeightB))
			{
				return false;
			}
			if (FMath::Abs(CentreHeight - (HeightA + HeightB) * 0.5f) > Entry.MaxGroundBend)
			{
				return false;
			}
		}
		return true;
	}

	void AddSiteMembers(FSurvivalSitePickRun& Run, const ASurvivalMapSite& Site)
	{
		const UPCGSurvivalSitePickerSettings& Settings = *Run.Settings;
		const int32 MemberCount = Run.Random.RandRange(Settings.MinMembers, FMath::Max(Settings.MinMembers, Settings.MaxMembers));
		const FVector2D SiteCentre(Site.GetActorLocation());
		const float InnerRadius = FMath::Min(Settings.CentreClearance, Site.GetSiteRadius());
		TArray<FVector2D> Taken;
		const FSurvivalSiteActorEntry* SharedEntry = Settings.bMembersShareOneClass ? PickEntry(Run, Settings.MemberPool) : nullptr;
		for (int32 MemberIndex = 0; MemberIndex < MemberCount; ++MemberIndex)
		{
			const FSurvivalSiteActorEntry* Entry = Settings.bMembersShareOneClass ? SharedEntry : PickEntry(Run, Settings.MemberPool);
			if (Entry == nullptr)
			{
				return;
			}
			for (int32 Attempt = 0; Attempt < SurvivalSitePickerConstants::AttemptsPerMember; ++Attempt)
			{
				const float Radius = Run.Random.FRandRange(InnerRadius, Site.GetSiteRadius());
				const FVector2D Position = SiteCentre + FVector2D(Radius, 0.f).GetRotated(Run.Random.FRandRange(0.f, 360.f));
				if (GetKeepsSpacing(Position, Taken, Settings.MemberSpacing) and GetIsGroundEvenUnder(Run, *Entry, Position))
				{
					Taken.Add(Position);
					AddActorPoint(Run, *Entry, Position, Run.Random.FRandRange(0.f, 360.f));
					break;
				}
			}
		}
	}
}

#if WITH_EDITOR
FText UPCGSurvivalSitePickerSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Survival Site Picker");
}

FText UPCGSurvivalSitePickerSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Picks a seeded, evenly spread part of the level's survival map sites of one category "
		"and outputs the actors to spawn in the ActorClass attribute.");
}
#endif

TArray<FPCGPinProperties> UPCGSurvivalSitePickerSettings::InputPinProperties() const
{
	return {};
}

TArray<FPCGPinProperties> UPCGSurvivalSitePickerSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
	return Pins;
}

FPCGElementPtr UPCGSurvivalSitePickerSettings::CreateElement() const
{
	return MakeShared<FPCGSurvivalSitePickerElement>();
}

bool FPCGSurvivalSitePickerElement::ExecuteInternal(FPCGContext* Context) const
{
	const UPCGSurvivalSitePickerSettings* Settings = Context->GetInputSettings<UPCGSurvivalSitePickerSettings>();
	UPCGComponent* Component = Context->SourceComponent.Get();
	UWorld* World = IsValid(Component) ? Component->GetWorld() : nullptr;
	if (not IsValid(Settings) or not IsValid(World))
	{
		return true;
	}
	UPCGPointData* PointData = NewObject<UPCGPointData>();
	FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
	Output.Data = PointData;
	Output.Pin = PCGPinConstants::DefaultOutputLabel;

	const FSurvivalGroundProjector Projector(*World, SurvivalSitePickerConstants::GroundTraceUp,
	                                         SurvivalSitePickerConstants::GroundTraceDown);
	FSurvivalSitePickRun Run;
	Run.Settings = Settings;
	Run.Projector = &Projector;
	Run.PointData = PointData;
	Run.ClassAttribute = PointData->Metadata->CreateAttribute<FSoftClassPath>(
		SurvivalSitePickerConstants::ActorClassAttribute, FSoftClassPath(), false, false);
	Run.MapCentre = IsValid(Component->GetOwner()) ? FVector2D(Component->GetOwner()->GetActorLocation()) : FVector2D::ZeroVector;
	// The component's seed directly: the context seed ignores it on nodes without bUseSeed. Category and ring keep two
	// nodes from drawing the same numbers.
	Run.Random.Initialize(PCGHelpers::ComputeSeed(Component->Seed, static_cast<int32>(Settings->SiteCategory),
	                                              FMath::RoundToInt(Settings->MinDistanceFromCentre)));

	const TArray<ASurvivalMapSite*> Picked = PickSites(Run, GatherSites(*World, *Settings, Run.MapCentre));
	for (const ASurvivalMapSite* Site : Picked)
	{
		if (const FSurvivalSiteActorEntry* Entry = PickEntry(Run, Settings->CentrePool))
		{
			AddActorPoint(Run, *Entry, FVector2D(Site->GetActorLocation()), GetCentreYaw(Run, *Site));
		}
		AddSiteMembers(Run, *Site);
	}
	if (Picked.Num() < Settings->MinSites)
	{
		RTSFunctionLibrary::ReportWarning(FString::Printf(TEXT("Survival site picker: only %d of at least %d sites of "
			"category %d fit the spacing."), Picked.Num(), Settings->MinSites, static_cast<int32>(Settings->SiteCategory)));
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
