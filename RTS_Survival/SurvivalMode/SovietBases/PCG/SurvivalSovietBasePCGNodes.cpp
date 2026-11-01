// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalSovietBasePCGNodes.h"

#include "Components/BoxComponent.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGPin.h"
#include "RTS_Survival/SurvivalMode/SovietBases/SurvivalSovietBaseComponent.h"

#define LOCTEXT_NAMESPACE "SurvivalSovietBasePCG"

namespace SovietBasePCGConstants
{
	const FName AfterPin = TEXT("After");
	const FName PlacedPin = TEXT("Placed");
	const FName SitePin = TEXT("Site");
	const FName ClassIndexAttribute = TEXT("ClassIndex");
	const FName RoleAttribute = TEXT("Role");
	const FName ThreatPointsAttribute = TEXT("ThreatPoints");
	const FName PadKindAttribute = TEXT("PadKind");
	const FName RoadAttribute = TEXT("Road");
	const FName RoadKindAttribute = TEXT("RoadKind");
	const FName RoadWidthAttribute = TEXT("RoadWidth");
	const FName SteepnessAttribute = TEXT("Steepness");
	constexpr double PointExtent = 100.0;
}

namespace
{
	/** @return The base this graph run builds, or false outside a run started by a volume. */
	bool SovietBasePCG_GetBase(FPCGContext* Context, USurvivalSovietBaseComponent*& OutSovietBases, int32& OutBaseArrayIndex)
	{
		const UPCGComponent* Component = Context->SourceComponent.Get();
		const ASurvivalSovietBasePCGVolume* Volume = IsValid(Component)
			                                             ? Cast<ASurvivalSovietBasePCGVolume>(Component->GetOwner())
			                                             : nullptr;
		OutSovietBases = IsValid(Volume) ? Volume->GetSovietBases() : nullptr;
		OutBaseArrayIndex = IsValid(Volume) ? Volume->GetBaseArrayIndex() : INDEX_NONE;
		return IsValid(OutSovietBases);
	}

	/** @return A callback that wakes this sleeping node; safe after the graph was cancelled. */
	TFunction<void()> SovietBasePCG_MakeWakeUp(FPCGContext* Context)
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

	UPCGPointData* SovietBasePCG_AddOutput(FPCGContext* Context, const FName Pin)
	{
		UPCGPointData* PointData = NewObject<UPCGPointData>();
		FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
		Output.Data = PointData;
		Output.Pin = Pin;
		return PointData;
	}

	FPCGPoint SovietBasePCG_MakePoint(const FVector& Location, const double YawRadians, const FVector& Scale, const int32 Seed)
	{
		using namespace SovietBasePCGConstants;
		FPCGPoint Point;
		Point.Transform = FTransform(FQuat(FVector::UpVector, YawRadians), Location, Scale);
		Point.BoundsMin = FVector(-PointExtent);
		Point.BoundsMax = FVector(PointExtent);
		Point.Seed = Seed;
		return Point;
	}

	void SovietBasePCG_WriteStructures(const FSurvivalSovietBasePlan& Plan, const int32 FirstStructure, const double GroundZ,
	                                   UPCGPointData& PointData)
	{
		using namespace SovietBasePCGConstants;
		FPCGMetadataAttribute<int32>* ClassAttribute = PointData.Metadata->CreateAttribute<int32>(ClassIndexAttribute, INDEX_NONE, false, false);
		FPCGMetadataAttribute<int32>* RoleData = PointData.Metadata->CreateAttribute<int32>(RoleAttribute, 0, false, false);
		FPCGMetadataAttribute<int32>* ThreatData = PointData.Metadata->CreateAttribute<int32>(ThreatPointsAttribute, 0, false, false);
		FPCGMetadataAttribute<int32>* PadData = PointData.Metadata->CreateAttribute<int32>(PadKindAttribute, 0, false, false);
		TArray<FPCGPoint>& Points = PointData.GetMutablePoints();
		for (int32 Index = FirstStructure; Index < Plan.Structures.Num(); ++Index)
		{
			const FSurvivalSovietPlannedStructure& Structure = Plan.Structures[Index];
			const FVector Scale(Structure.StretchXY.X * Structure.UniformScale, Structure.StretchXY.Y * Structure.UniformScale,
			                    Structure.UniformScale);
			FPCGPoint& Point = Points.Add_GetRef(SovietBasePCG_MakePoint(
				FVector(Structure.Position, GroundZ), Structure.YawRadians, Scale, Index));
			PointData.Metadata->InitializeOnSet(Point.MetadataEntry);
			ClassAttribute->SetValue(Point.MetadataEntry, Structure.ClassIndex);
			RoleData->SetValue(Point.MetadataEntry, static_cast<int32>(Structure.Role));
			ThreatData->SetValue(Point.MetadataEntry, Structure.ThreatPoints);
			PadData->SetValue(Point.MetadataEntry, static_cast<int32>(Structure.PadKind));
		}
	}

	void SovietBasePCG_WriteRoads(const FSurvivalSovietBasePlan& Plan, const int32 FirstRoad, const double GroundZ, UPCGPointData& PointData)
	{
		using namespace SovietBasePCGConstants;
		FPCGMetadataAttribute<int32>* RoadData = PointData.Metadata->CreateAttribute<int32>(RoadAttribute, INDEX_NONE, false, false);
		FPCGMetadataAttribute<int32>* KindData = PointData.Metadata->CreateAttribute<int32>(RoadKindAttribute, 0, false, false);
		FPCGMetadataAttribute<double>* WidthData = PointData.Metadata->CreateAttribute<double>(RoadWidthAttribute, 0.0, false, false);
		TArray<FPCGPoint>& Points = PointData.GetMutablePoints();
		for (int32 RoadIndex = FirstRoad; RoadIndex < Plan.Roads.Num(); ++RoadIndex)
		{
			const FSurvivalSovietPlannedRoad& Road = Plan.Roads[RoadIndex];
			for (const FVector2D& RoadPoint : Road.Points)
			{
				FPCGPoint& Point = Points.Add_GetRef(SovietBasePCG_MakePoint(FVector(RoadPoint, GroundZ), 0.0, FVector::OneVector, RoadIndex));
				PointData.Metadata->InitializeOnSet(Point.MetadataEntry);
				RoadData->SetValue(Point.MetadataEntry, RoadIndex);
				KindData->SetValue(Point.MetadataEntry, static_cast<int32>(Road.Kind));
				WidthData->SetValue(Point.MetadataEntry, Road.Width);
			}
		}
	}

	void SovietBasePCG_ReadStructures(const UPCGPointData& PointData, TArray<FSurvivalSovietPlannedStructure>& OutStructures)
	{
		using namespace SovietBasePCGConstants;
		const FPCGMetadataAttribute<int32>* ClassAttribute = PointData.Metadata->GetConstTypedAttribute<int32>(ClassIndexAttribute);
		const FPCGMetadataAttribute<int32>* RoleData = PointData.Metadata->GetConstTypedAttribute<int32>(RoleAttribute);
		const FPCGMetadataAttribute<int32>* ThreatData = PointData.Metadata->GetConstTypedAttribute<int32>(ThreatPointsAttribute);
		const FPCGMetadataAttribute<int32>* PadData = PointData.Metadata->GetConstTypedAttribute<int32>(PadKindAttribute);
		if (ClassAttribute == nullptr or RoleData == nullptr or ThreatData == nullptr or PadData == nullptr)
		{
			return;
		}
		for (const FPCGPoint& Point : PointData.GetPoints())
		{
			FSurvivalSovietPlannedStructure& Structure = OutStructures.AddDefaulted_GetRef();
			Structure.ClassIndex = ClassAttribute->GetValueFromItemKey(Point.MetadataEntry);
			Structure.Role = static_cast<ESurvivalSovietStructureRole>(RoleData->GetValueFromItemKey(Point.MetadataEntry));
			Structure.ThreatPoints = ThreatData->GetValueFromItemKey(Point.MetadataEntry);
			Structure.Position = FVector2D(Point.Transform.GetLocation());
			Structure.YawRadians = FMath::DegreesToRadians(Point.Transform.Rotator().Yaw);
			const FVector Scale = Point.Transform.GetScale3D();
			Structure.UniformScale = Scale.Z;
			Structure.StretchXY = FVector2D(Scale.X, Scale.Y) / Scale.Z;
			Structure.PadKind = static_cast<ESurvivalSovietPadKind>(PadData->GetValueFromItemKey(Point.MetadataEntry));
		}
	}

	void SovietBasePCG_ReadRoads(const UPCGPointData& PointData, TArray<FSurvivalSovietPlannedRoad>& OutRoads)
	{
		using namespace SovietBasePCGConstants;
		const FPCGMetadataAttribute<int32>* RoadData = PointData.Metadata->GetConstTypedAttribute<int32>(RoadAttribute);
		const FPCGMetadataAttribute<int32>* KindData = PointData.Metadata->GetConstTypedAttribute<int32>(RoadKindAttribute);
		const FPCGMetadataAttribute<double>* WidthData = PointData.Metadata->GetConstTypedAttribute<double>(RoadWidthAttribute);
		if (RoadData == nullptr or KindData == nullptr or WidthData == nullptr)
		{
			return;
		}
		for (const FPCGPoint& Point : PointData.GetPoints())
		{
			const int32 RoadIndex = RoadData->GetValueFromItemKey(Point.MetadataEntry);
			if (RoadIndex < 0)
			{
				continue;
			}
			if (OutRoads.Num() <= RoadIndex)
			{
				OutRoads.SetNum(RoadIndex + 1);
			}
			FSurvivalSovietPlannedRoad& Road = OutRoads[RoadIndex];
			Road.Kind = static_cast<ESurvivalSovietRoadKind>(KindData->GetValueFromItemKey(Point.MetadataEntry));
			Road.Width = WidthData->GetValueFromItemKey(Point.MetadataEntry);
			Road.Points.Add(FVector2D(Point.Transform.GetLocation()));
		}
	}
}

// ---------------------------------------------------------------------------
// Volume
// ---------------------------------------------------------------------------

ASurvivalSovietBasePCGVolume::ASurvivalSovietBasePCGVolume()
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

void ASurvivalSovietBasePCGVolume::StartGeneration(USurvivalSovietBaseComponent* SovietBases, const int32 InBaseArrayIndex,
                                                   UPCGGraphInterface* Graph, const int32 Seed, const float Radius)
{
	M_SovietBases = SovietBases;
	M_BaseArrayIndex = InBaseArrayIndex;
	M_Bounds->SetBoxExtent(FVector(Radius, Radius, Radius));
	M_PCGComponent->Seed = Seed;
	M_PCGComponent->SetGraphLocal(Graph);
	constexpr bool bForce = true;
	M_PCGComponent->GenerateLocal(bForce);
}

// ---------------------------------------------------------------------------
// Site node
// ---------------------------------------------------------------------------

#if WITH_EDITOR
FText UPCGSovietBaseSiteSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("SiteTitle", "Soviet Base Site");
}
#endif

TArray<FPCGPinProperties> UPCGSovietBaseSiteSettings::InputPinProperties() const
{
	return {};
}

TArray<FPCGPinProperties> UPCGSovietBaseSiteSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(SovietBasePCGConstants::SitePin, EPCGDataType::Point);
	return Pins;
}

FPCGElementPtr UPCGSovietBaseSiteSettings::CreateElement() const
{
	return MakeShared<FPCGSovietBaseSiteElement>();
}

bool FPCGSovietBaseSiteElement::ExecuteInternal(FPCGContext* Context) const
{
	using namespace SovietBasePCGConstants;
	USurvivalSovietBaseComponent* SovietBases = nullptr;
	int32 BaseArrayIndex = INDEX_NONE;
	if (not SovietBasePCG_GetBase(Context, SovietBases, BaseArrayIndex))
	{
		return true;
	}
	if (not SovietBases->GetIsBasePrepared(BaseArrayIndex))
	{
		Context->bIsPaused = true;
		SovietBases->BeginBasePreparation(BaseArrayIndex, SovietBasePCG_MakeWakeUp(Context));
		return false;
	}

	const FSurvivalSovietSiteMap* Site = SovietBases->GetBaseSite(BaseArrayIndex);
	UPCGPointData* PointData = SovietBasePCG_AddOutput(Context, SitePin);
	if (Site == nullptr)
	{
		return true;
	}
	FPCGMetadataAttribute<double>* Steepness = PointData->Metadata->CreateAttribute<double>(SteepnessAttribute, 0.0, false, false);
	TArray<FPCGPoint>& Points = PointData->GetMutablePoints();
	Points.Reserve(Site->GetSampleCount());
	const double CenterHeight = Site->GetCenterHeight();
	for (int32 Sample = 0; Sample < Site->GetSampleCount(); ++Sample)
	{
		const FVector2D Position = Site->GetSamplePosition(Sample);
		FPCGPoint& Point = Points.Add_GetRef(SovietBasePCG_MakePoint(
			FVector(Position, CenterHeight + Site->GetRelativeHeight(Position)), 0.0, FVector::OneVector, Sample));
		Point.Density = Site->GetHasGround(Sample) ? 1.f : 0.f;
		PointData->Metadata->InitializeOnSet(Point.MetadataEntry);
		Steepness->SetValue(Point.MetadataEntry, Site->GetSampleSlope(Sample));
	}
	return true;
}

// ---------------------------------------------------------------------------
// Stage node
// ---------------------------------------------------------------------------

#if WITH_EDITOR
FText UPCGSovietBaseStageSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("StageTitle", "Soviet Base Stage");
}
#endif

FString UPCGSovietBaseStageSettings::GetAdditionalTitleInformation() const
{
	return StaticEnum<ESurvivalSovietPlanStage>()->GetNameStringByValue(static_cast<int64>(Stage));
}

TArray<FPCGPinProperties> UPCGSovietBaseStageSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(SovietBasePCGConstants::AfterPin, EPCGDataType::Any);
	return Pins;
}

TArray<FPCGPinProperties> UPCGSovietBaseStageSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(SovietBasePCGConstants::PlacedPin, EPCGDataType::Point);
	return Pins;
}

FPCGElementPtr UPCGSovietBaseStageSettings::CreateElement() const
{
	return MakeShared<FPCGSovietBaseStageElement>();
}

bool FPCGSovietBaseStageElement::ExecuteInternal(FPCGContext* Context) const
{
	using namespace SovietBasePCGConstants;
	const UPCGSovietBaseStageSettings* Settings = Context->GetInputSettings<UPCGSovietBaseStageSettings>();
	USurvivalSovietBaseComponent* SovietBases = nullptr;
	int32 BaseArrayIndex = INDEX_NONE;
	if (Settings == nullptr or not SovietBasePCG_GetBase(Context, SovietBases, BaseArrayIndex))
	{
		return true;
	}
	UPCGPointData* PointData = SovietBasePCG_AddOutput(Context, PlacedPin);
	const FSurvivalSovietBasePlan* PlanBefore = SovietBases->GetBasePlan(BaseArrayIndex);
	const int32 FirstRoad = PlanBefore != nullptr ? PlanBefore->Roads.Num() : 0;
	const int32 FirstStructure = SovietBases->RunBasePlanStage(BaseArrayIndex, Settings->Stage);
	const FSurvivalSovietBasePlan* Plan = SovietBases->GetBasePlan(BaseArrayIndex);
	if (Plan == nullptr)
	{
		return true;
	}
	const double GroundZ = Context->SourceComponent->GetOwner()->GetActorLocation().Z;
	if (Plan->Roads.Num() > FirstRoad)
	{
		// Two kinds of points on one output would mix attributes; roads (streets, and yard paths from the dressing stage) get
		// their own data on the same pin.
		SovietBasePCG_WriteRoads(*Plan, FirstRoad, GroundZ, *PointData);
		SovietBasePCG_WriteStructures(*Plan, FirstStructure, GroundZ, *SovietBasePCG_AddOutput(Context, PlacedPin));
		return true;
	}
	SovietBasePCG_WriteStructures(*Plan, FirstStructure, GroundZ, *PointData);
	return true;
}

// ---------------------------------------------------------------------------
// Spawn node
// ---------------------------------------------------------------------------

#if WITH_EDITOR
FText UPCGSovietBaseSpawnSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("SpawnTitle", "Soviet Base Spawn");
}
#endif

TArray<FPCGPinProperties> UPCGSovietBaseSpawnSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(SovietBasePCGConstants::PlacedPin, EPCGDataType::Point);
	return Pins;
}

TArray<FPCGPinProperties> UPCGSovietBaseSpawnSettings::OutputPinProperties() const
{
	return {};
}

FPCGElementPtr UPCGSovietBaseSpawnSettings::CreateElement() const
{
	return MakeShared<FPCGSovietBaseSpawnElement>();
}

bool FPCGSovietBaseSpawnElement::ExecuteInternal(FPCGContext* Context) const
{
	using namespace SovietBasePCGConstants;
	USurvivalSovietBaseComponent* SovietBases = nullptr;
	int32 BaseArrayIndex = INDEX_NONE;
	if (not SovietBasePCG_GetBase(Context, SovietBases, BaseArrayIndex) or SovietBases->GetIsBaseSpawnRequested(BaseArrayIndex))
	{
		return true;
	}

	TArray<FSurvivalSovietPlannedStructure> Structures;
	TArray<FSurvivalSovietPlannedRoad> Roads;
	for (const FPCGTaggedData& Input : Context->InputData.GetInputsByPin(PlacedPin))
	{
		const UPCGPointData* PointData = Cast<UPCGPointData>(Input.Data);
		if (PointData != nullptr)
		{
			SovietBasePCG_ReadStructures(*PointData, Structures);
			SovietBasePCG_ReadRoads(*PointData, Roads);
		}
	}
	Context->bIsPaused = true;
	SovietBases->SpawnBaseFromGraph(BaseArrayIndex, MoveTemp(Structures), MoveTemp(Roads), SovietBasePCG_MakeWakeUp(Context));
	return false;
}

#undef LOCTEXT_NAMESPACE
