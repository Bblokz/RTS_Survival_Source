// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalPOIPCGNodes.h"

#include "Components/BoxComponent.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGPin.h"
#include "RTS_Survival/SurvivalMode/PointsOfInterest/SurvivalPointsOfInterestSubsystem.h"

#define LOCTEXT_NAMESPACE "SurvivalPOIPCG"

namespace SurvivalPOIPCGConstants
{
	const FName SitesPin = TEXT("Sites");
	const FName PlacedPin = TEXT("Placed");
	const FName WildlifePin = TEXT("Wildlife");
	const FName SiteIndexAttribute = TEXT("SiteIndex");
	const FName TypeAttribute = TEXT("POIType");
	const FName LandmarkAttribute = TEXT("Landmark");
	const FName SectorAttribute = TEXT("Sector");
	const FName OwnerStartAttribute = TEXT("OwnerStart");
	const FName SubLayoutAttribute = TEXT("SubLayout");
	const FName RadiusAttribute = TEXT("Radius");
	const FName AssetKeyAttribute = TEXT("AssetKey");
	const FName RoleAttribute = TEXT("Role");
	const FName SinkAttribute = TEXT("Sink");
	const FName EntryAttribute = TEXT("Entry");
	const FName CountAttribute = TEXT("Count");
	const FName FlightHeightAttribute = TEXT("FlightHeight");
	const FName SemiXAttribute = TEXT("SemiX");
	const FName SemiYAttribute = TEXT("SemiY");
	const FName GroupSeedAttribute = TEXT("GroupSeed");
	constexpr double PointExtent = 100.0;
}

namespace
{
	USurvivalPointsOfInterestSubsystem* SurvivalPOIPCG_GetSubsystem(const FPCGContext* Context)
	{
		const UPCGComponent* Component = Context->SourceComponent.Get();
		const ASurvivalPOIPCGVolume* Volume = IsValid(Component) ? Cast<ASurvivalPOIPCGVolume>(Component->GetOwner()) : nullptr;
		return IsValid(Volume) ? Volume->GetSubsystem() : nullptr;
	}

	UPCGPointData* SurvivalPOIPCG_AddOutput(FPCGContext* Context, const FName Pin)
	{
		UPCGPointData* PointData = NewObject<UPCGPointData>();
		FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
		Output.Data = PointData;
		Output.Pin = Pin;
		return PointData;
	}

	FPCGPoint& SurvivalPOIPCG_AddPoint(UPCGPointData& PointData, const FVector& Location, const float YawDegrees,
	                                   const float Scale, const int32 Seed)
	{
		using namespace SurvivalPOIPCGConstants;
		FPCGPoint& Point = PointData.GetMutablePoints().AddDefaulted_GetRef();
		Point.Transform = FTransform(FRotator(0.f, YawDegrees, 0.f), Location, FVector(Scale));
		Point.BoundsMin = FVector(-PointExtent);
		Point.BoundsMax = FVector(PointExtent);
		Point.Seed = Seed;
		PointData.Metadata->InitializeOnSet(Point.MetadataEntry);
		return Point;
	}

	template <typename T>
	FPCGMetadataAttribute<T>* SurvivalPOIPCG_Attribute(UPCGPointData& PointData, const FName Name, const T Default)
	{
		return PointData.Metadata->CreateAttribute<T>(Name, Default, false, false);
	}

	template <typename T>
	T SurvivalPOIPCG_Read(const UPCGPointData& PointData, const FName Name, const FPCGPoint& Point, const T Fallback)
	{
		const FPCGMetadataAttribute<T>* Attribute = PointData.Metadata->GetConstTypedAttribute<T>(Name);
		return Attribute != nullptr ? Attribute->GetValueFromItemKey(Point.MetadataEntry) : Fallback;
	}

	void SurvivalPOIPCG_WriteSites(const TArray<FSurvivalPOISite>& Sites, UPCGPointData& PointData)
	{
		using namespace SurvivalPOIPCGConstants;
		FPCGMetadataAttribute<int32>* Index = SurvivalPOIPCG_Attribute<int32>(PointData, SiteIndexAttribute, INDEX_NONE);
		FPCGMetadataAttribute<int32>* Type = SurvivalPOIPCG_Attribute<int32>(PointData, TypeAttribute, 0);
		FPCGMetadataAttribute<int32>* Landmark = SurvivalPOIPCG_Attribute<int32>(PointData, LandmarkAttribute, 0);
		FPCGMetadataAttribute<int32>* Sector = SurvivalPOIPCG_Attribute<int32>(PointData, SectorAttribute, INDEX_NONE);
		FPCGMetadataAttribute<int32>* Owner = SurvivalPOIPCG_Attribute<int32>(PointData, OwnerStartAttribute, INDEX_NONE);
		FPCGMetadataAttribute<int32>* SubLayout = SurvivalPOIPCG_Attribute<int32>(PointData, SubLayoutAttribute, 0);
		FPCGMetadataAttribute<double>* Radius = SurvivalPOIPCG_Attribute<double>(PointData, RadiusAttribute, 0.0);
		for (const FSurvivalPOISite& Site : Sites)
		{
			const FPCGPoint& Point = SurvivalPOIPCG_AddPoint(PointData, Site.Center, Site.YawDegrees, 1.f, Site.Seed);
			Index->SetValue(Point.MetadataEntry, Site.SiteIndex);
			Type->SetValue(Point.MetadataEntry, static_cast<int32>(Site.Type));
			Landmark->SetValue(Point.MetadataEntry, Site.bIsLandmark ? 1 : 0);
			Sector->SetValue(Point.MetadataEntry, Site.Sector);
			Owner->SetValue(Point.MetadataEntry, Site.OwnerStart);
			SubLayout->SetValue(Point.MetadataEntry, Site.SubLayout);
			Radius->SetValue(Point.MetadataEntry, Site.Radius);
		}
	}

	void SurvivalPOIPCG_ReadSites(const FPCGContext* Context, TArray<FSurvivalPOISite>& OutSites)
	{
		using namespace SurvivalPOIPCGConstants;
		for (const FPCGTaggedData& Input : Context->InputData.GetInputsByPin(SitesPin))
		{
			const UPCGPointData* PointData = Cast<UPCGPointData>(Input.Data);
			if (PointData == nullptr)
			{
				continue;
			}
			for (const FPCGPoint& Point : PointData->GetPoints())
			{
				FSurvivalPOISite& Site = OutSites.AddDefaulted_GetRef();
				Site.SiteIndex = SurvivalPOIPCG_Read<int32>(*PointData, SiteIndexAttribute, Point, INDEX_NONE);
				Site.Type = static_cast<ESurvivalPOIType>(SurvivalPOIPCG_Read<int32>(*PointData, TypeAttribute, Point, 0));
				Site.bIsLandmark = SurvivalPOIPCG_Read<int32>(*PointData, LandmarkAttribute, Point, 0) != 0;
				Site.Sector = SurvivalPOIPCG_Read<int32>(*PointData, SectorAttribute, Point, INDEX_NONE);
				Site.OwnerStart = SurvivalPOIPCG_Read<int32>(*PointData, OwnerStartAttribute, Point, INDEX_NONE);
				Site.SubLayout = SurvivalPOIPCG_Read<int32>(*PointData, SubLayoutAttribute, Point, 0);
				Site.Radius = static_cast<float>(SurvivalPOIPCG_Read<double>(*PointData, RadiusAttribute, Point, 0.0));
				Site.Center = Point.Transform.GetLocation();
				Site.YawDegrees = static_cast<float>(Point.Transform.Rotator().Yaw);
				Site.Seed = Point.Seed;
			}
		}
	}

	TFunction<void()> SurvivalPOIPCG_MakeWakeUp(FPCGContext* Context)
	{
		const TWeakPtr<FPCGContextHandle> Handle = Context->GetOrCreateHandle();
		return [Handle]()
		{
			if (FPCGContext* SleepingContext = FPCGContext::GetContextFromHandle<FPCGContext>(Handle))
			{
				SleepingContext->bIsPaused = false;
			}
		};
	}
}

// ---------------------------------------------------------------------------
// Volume
// ---------------------------------------------------------------------------

ASurvivalPOIPCGVolume::ASurvivalPOIPCGVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	M_Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	M_Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M_Bounds->SetCanEverAffectNavigation(false);
	M_Bounds->SetHiddenInGame(true);
	RootComponent = M_Bounds;
	M_PCGComponent = CreateDefaultSubobject<UPCGComponent>(TEXT("PCG"));
	M_PCGComponent->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
}

void ASurvivalPOIPCGVolume::StartGeneration(USurvivalPointsOfInterestSubsystem* Subsystem, UPCGGraphInterface* Graph,
                                            const int32 Seed, const FVector& HalfExtent)
{
	M_Subsystem = Subsystem;
	M_Bounds->SetBoxExtent(HalfExtent);
	M_PCGComponent->Seed = Seed;
	M_PCGComponent->SetGraphLocal(Graph);
	constexpr bool bForce = true;
	M_PCGComponent->GenerateLocal(bForce);
}

// ---------------------------------------------------------------------------
// Sites
// ---------------------------------------------------------------------------

#if WITH_EDITOR
FText UPCGSurvivalPOISitesSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("SitesTitle", "Survival POI Sites");
}
#endif

TArray<FPCGPinProperties> UPCGSurvivalPOISitesSettings::InputPinProperties() const
{
	return {};
}

TArray<FPCGPinProperties> UPCGSurvivalPOISitesSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(SurvivalPOIPCGConstants::SitesPin, EPCGDataType::Point);
	return Pins;
}

FPCGElementPtr UPCGSurvivalPOISitesSettings::CreateElement() const
{
	return MakeShared<FPCGSurvivalPOISitesElement>();
}

bool FPCGSurvivalPOISitesElement::ExecuteInternal(FPCGContext* Context) const
{
	USurvivalPointsOfInterestSubsystem* Subsystem = SurvivalPOIPCG_GetSubsystem(Context);
	if (not IsValid(Subsystem))
	{
		return true;
	}
	// Time sliced: the height field fills a slice per call; the scheduler calls again until it is done.
	if (not Subsystem->StepSiteSearch())
	{
		return false;
	}
	SurvivalPOIPCG_WriteSites(Subsystem->GetPlannedSites(), *SurvivalPOIPCG_AddOutput(Context, SurvivalPOIPCGConstants::SitesPin));
	return true;
}

// ---------------------------------------------------------------------------
// Compose
// ---------------------------------------------------------------------------

#if WITH_EDITOR
FText UPCGSurvivalPOIComposeSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("ComposeTitle", "Survival POI Compose");
}
#endif

TArray<FPCGPinProperties> UPCGSurvivalPOIComposeSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(SurvivalPOIPCGConstants::SitesPin, EPCGDataType::Point);
	return Pins;
}

TArray<FPCGPinProperties> UPCGSurvivalPOIComposeSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(SurvivalPOIPCGConstants::PlacedPin, EPCGDataType::Point);
	return Pins;
}

FPCGElementPtr UPCGSurvivalPOIComposeSettings::CreateElement() const
{
	return MakeShared<FPCGSurvivalPOIComposeElement>();
}

bool FPCGSurvivalPOIComposeElement::ExecuteInternal(FPCGContext* Context) const
{
	using namespace SurvivalPOIPCGConstants;
	USurvivalPointsOfInterestSubsystem* Subsystem = SurvivalPOIPCG_GetSubsystem(Context);
	UPCGPointData* PointData = SurvivalPOIPCG_AddOutput(Context, PlacedPin);
	if (not IsValid(Subsystem))
	{
		return true;
	}
	TArray<FSurvivalPOISite> Sites;
	SurvivalPOIPCG_ReadSites(Context, Sites);
	TArray<FSurvivalPOIPlacedObject> Objects;
	Subsystem->ComposeSites(Sites, Objects);
	FPCGMetadataAttribute<int32>* AssetKey = SurvivalPOIPCG_Attribute<int32>(*PointData, AssetKeyAttribute, INDEX_NONE);
	FPCGMetadataAttribute<int32>* Role = SurvivalPOIPCG_Attribute<int32>(*PointData, RoleAttribute, 0);
	FPCGMetadataAttribute<int32>* SiteIndex = SurvivalPOIPCG_Attribute<int32>(*PointData, SiteIndexAttribute, INDEX_NONE);
	FPCGMetadataAttribute<double>* Sink = SurvivalPOIPCG_Attribute<double>(*PointData, SinkAttribute, 0.0);
	for (int32 Index = 0; Index < Objects.Num(); ++Index)
	{
		const FSurvivalPOIPlacedObject& Object = Objects[Index];
		const FPCGPoint& Point = SurvivalPOIPCG_AddPoint(*PointData, Object.Location, Object.YawDegrees, Object.Scale, Index);
		AssetKey->SetValue(Point.MetadataEntry, Object.AssetKey);
		Role->SetValue(Point.MetadataEntry, static_cast<int32>(Object.Role));
		SiteIndex->SetValue(Point.MetadataEntry, Object.SiteIndex);
		Sink->SetValue(Point.MetadataEntry, Object.SinkCm);
	}
	return true;
}

// ---------------------------------------------------------------------------
// Wildlife
// ---------------------------------------------------------------------------

#if WITH_EDITOR
FText UPCGSurvivalWildlifeSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("WildlifeTitle", "Survival Wildlife");
}
#endif

TArray<FPCGPinProperties> UPCGSurvivalWildlifeSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(SurvivalPOIPCGConstants::SitesPin, EPCGDataType::Point);
	return Pins;
}

TArray<FPCGPinProperties> UPCGSurvivalWildlifeSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(SurvivalPOIPCGConstants::WildlifePin, EPCGDataType::Point);
	return Pins;
}

FPCGElementPtr UPCGSurvivalWildlifeSettings::CreateElement() const
{
	return MakeShared<FPCGSurvivalWildlifeElement>();
}

bool FPCGSurvivalWildlifeElement::ExecuteInternal(FPCGContext* Context) const
{
	using namespace SurvivalPOIPCGConstants;
	USurvivalPointsOfInterestSubsystem* Subsystem = SurvivalPOIPCG_GetSubsystem(Context);
	UPCGPointData* PointData = SurvivalPOIPCG_AddOutput(Context, WildlifePin);
	if (not IsValid(Subsystem))
	{
		return true;
	}
	TArray<FSurvivalPOISite> Sites;
	SurvivalPOIPCG_ReadSites(Context, Sites);
	TArray<FSurvivalWildlifeGroup> Groups;
	Subsystem->PlanWildlife(Sites, Groups);
	FPCGMetadataAttribute<int32>* Entry = SurvivalPOIPCG_Attribute<int32>(*PointData, EntryAttribute, INDEX_NONE);
	FPCGMetadataAttribute<int32>* Count = SurvivalPOIPCG_Attribute<int32>(*PointData, CountAttribute, 0);
	FPCGMetadataAttribute<int32>* SiteIndex = SurvivalPOIPCG_Attribute<int32>(*PointData, SiteIndexAttribute, INDEX_NONE);
	FPCGMetadataAttribute<double>* Height = SurvivalPOIPCG_Attribute<double>(*PointData, FlightHeightAttribute, 0.0);
	FPCGMetadataAttribute<double>* SemiX = SurvivalPOIPCG_Attribute<double>(*PointData, SemiXAttribute, 0.0);
	FPCGMetadataAttribute<double>* SemiY = SurvivalPOIPCG_Attribute<double>(*PointData, SemiYAttribute, 0.0);
	FPCGMetadataAttribute<int32>* GroupSeed = SurvivalPOIPCG_Attribute<int32>(*PointData, GroupSeedAttribute, 0);
	for (int32 Index = 0; Index < Groups.Num(); ++Index)
	{
		const FSurvivalWildlifeGroup& Group = Groups[Index];
		const FPCGPoint& Point = SurvivalPOIPCG_AddPoint(*PointData, Group.Center, Group.YawDegrees, 1.f, Index);
		Entry->SetValue(Point.MetadataEntry, Group.EntryIndex);
		Count->SetValue(Point.MetadataEntry, Group.Count);
		SiteIndex->SetValue(Point.MetadataEntry, Group.SiteIndex);
		Height->SetValue(Point.MetadataEntry, Group.FlightHeight);
		SemiX->SetValue(Point.MetadataEntry, Group.SemiAxes.X);
		SemiY->SetValue(Point.MetadataEntry, Group.SemiAxes.Y);
		GroupSeed->SetValue(Point.MetadataEntry, Group.Seed);
	}
	return true;
}

// ---------------------------------------------------------------------------
// Spawn
// ---------------------------------------------------------------------------

#if WITH_EDITOR
FText UPCGSurvivalPOISpawnSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("SpawnTitle", "Survival POI Spawn");
}
#endif

TArray<FPCGPinProperties> UPCGSurvivalPOISpawnSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(SurvivalPOIPCGConstants::PlacedPin, EPCGDataType::Point);
	Pins.Emplace(SurvivalPOIPCGConstants::WildlifePin, EPCGDataType::Point);
	return Pins;
}

TArray<FPCGPinProperties> UPCGSurvivalPOISpawnSettings::OutputPinProperties() const
{
	return {};
}

FPCGElementPtr UPCGSurvivalPOISpawnSettings::CreateElement() const
{
	return MakeShared<FPCGSurvivalPOISpawnElement>();
}

bool FPCGSurvivalPOISpawnElement::ExecuteInternal(FPCGContext* Context) const
{
	using namespace SurvivalPOIPCGConstants;
	USurvivalPointsOfInterestSubsystem* Subsystem = SurvivalPOIPCG_GetSubsystem(Context);
	if (not IsValid(Subsystem) or Subsystem->GetIsSpawnRequested())
	{
		return true;
	}
	TArray<FSurvivalPOIPlacedObject> Objects;
	for (const FPCGTaggedData& Input : Context->InputData.GetInputsByPin(PlacedPin))
	{
		const UPCGPointData* PointData = Cast<UPCGPointData>(Input.Data);
		if (PointData == nullptr)
		{
			continue;
		}
		for (const FPCGPoint& Point : PointData->GetPoints())
		{
			FSurvivalPOIPlacedObject& Object = Objects.AddDefaulted_GetRef();
			Object.AssetKey = SurvivalPOIPCG_Read<int32>(*PointData, AssetKeyAttribute, Point, INDEX_NONE);
			Object.Role = static_cast<ESurvivalPOIRole>(SurvivalPOIPCG_Read<int32>(*PointData, RoleAttribute, Point, 0));
			Object.SiteIndex = SurvivalPOIPCG_Read<int32>(*PointData, SiteIndexAttribute, Point, INDEX_NONE);
			Object.SinkCm = static_cast<float>(SurvivalPOIPCG_Read<double>(*PointData, SinkAttribute, Point, 0.0));
			Object.Location = Point.Transform.GetLocation();
			Object.YawDegrees = static_cast<float>(Point.Transform.Rotator().Yaw);
			Object.Scale = static_cast<float>(Point.Transform.GetScale3D().Z);
		}
	}
	TArray<FSurvivalWildlifeGroup> Groups;
	for (const FPCGTaggedData& Input : Context->InputData.GetInputsByPin(WildlifePin))
	{
		const UPCGPointData* PointData = Cast<UPCGPointData>(Input.Data);
		if (PointData == nullptr)
		{
			continue;
		}
		for (const FPCGPoint& Point : PointData->GetPoints())
		{
			FSurvivalWildlifeGroup& Group = Groups.AddDefaulted_GetRef();
			Group.EntryIndex = SurvivalPOIPCG_Read<int32>(*PointData, EntryAttribute, Point, INDEX_NONE);
			Group.Count = SurvivalPOIPCG_Read<int32>(*PointData, CountAttribute, Point, 0);
			Group.SiteIndex = SurvivalPOIPCG_Read<int32>(*PointData, SiteIndexAttribute, Point, INDEX_NONE);
			Group.FlightHeight = static_cast<float>(SurvivalPOIPCG_Read<double>(*PointData, FlightHeightAttribute, Point, 0.0));
			Group.SemiAxes = FVector2D(SurvivalPOIPCG_Read<double>(*PointData, SemiXAttribute, Point, 0.0),
			                           SurvivalPOIPCG_Read<double>(*PointData, SemiYAttribute, Point, 0.0));
			Group.Seed = SurvivalPOIPCG_Read<int32>(*PointData, GroupSeedAttribute, Point, 0);
			Group.Center = Point.Transform.GetLocation();
			Group.YawDegrees = static_cast<float>(Point.Transform.Rotator().Yaw);
		}
	}
	Context->bIsPaused = true;
	Subsystem->SpawnFromGraph(MoveTemp(Objects), MoveTemp(Groups), SurvivalPOIPCG_MakeWakeUp(Context));
	return false;
}

#undef LOCTEXT_NAMESPACE
