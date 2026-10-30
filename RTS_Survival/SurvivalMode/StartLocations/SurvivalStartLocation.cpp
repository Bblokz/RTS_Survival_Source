// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalStartLocation.h"

#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowType.h"

#include "Components/BillboardComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/DateTime.h"
#include "RTS_Survival/SurvivalMode/Director/SurvivalModeDirector.h"
#include "RTS_Survival/SurvivalMode/StartLocations/SurvivalStartContentSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace SurvivalStartLocationConstants
{
	// Defense point scans use the landscape only; generous traces cover tall maps.
	constexpr float DefensePointTraceUp = 20000.f;
	constexpr float DefensePointTraceDown = 50000.f;

	constexpr double DebugRefreshIntervalSeconds = 0.3;
	constexpr int32 DebugRingSegments = 96;
	constexpr float DebugHeightOffset = 60.f;
	constexpr float DebugMarkerHeight = 600.f;
	constexpr float DebugDefenseSphereRadius = 150.f;
	constexpr int32 DebugCircleSegments = 40;

	const FColor BuildAreaColor(60, 220, 90);
	const FColor ResourceBandColor(60, 200, 230);
	const FColor WreckBandColor(240, 150, 40);
	const FColor DesignerPointColor(255, 220, 0);
	const FColor HillTopPointColor(255, 60, 60);
	const FColor FallbackPointColor(160, 160, 160);
	const FColor RadixiteColor(120, 255, 140);
	const FColor MetalColor(190, 190, 215);
	const FColor WreckColor(240, 150, 40);
	const FColor PitColor(255, 40, 40);
	const FColor CraterColor(140, 100, 70);
}

ASurvivalStartLocation::ASurvivalStartLocation()
{
	// Ticks in the editor only to draw the debug view; BeginPlay turns it off unless bDrawDebugInGame.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	M_FowComponent = CreateDefaultSubobject<UFowComp>(TEXT("FowComponent"));

#if WITH_EDITORONLY_DATA
	M_EditorSprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (M_EditorSprite)
	{
		M_EditorSprite->SetupAttachment(RootComponent);
		M_EditorSprite->bIsScreenSizeScaled = true;
	}
#endif
}

FVector ASurvivalStartLocation::GetDeployCenter() const
{
	return GetActorLocation();
}

void ASurvivalStartLocation::BeginPlay()
{
	Super::BeginPlay();
	// Every start location resolves its points: the chosen one places pits, the others host Soviet bases.
	RefreshDefensePoints();
	M_DebugPlan.Reset();
	bM_HasDebugPlan = false;
	if (bDrawDebugInGame)
	{
		RefreshDebugRings();
	}
	SetActorTickEnabled(bDrawDebugInGame);
	SetRevealFogOfWar(true);
}

void ASurvivalStartLocation::SetRevealFogOfWar(const bool bReveal)
{
	if (not IsValid(M_FowComponent))
	{
		return;
	}
	if (bReveal)
	{
		M_FowComponent->SetVisionRadius(PickerVisionRadius);
		M_FowComponent->StartFow(EFowBehaviour::Fow_Active, true);
		return;
	}
	M_FowComponent->StopFowParticipation();
}

void ASurvivalStartLocation::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (M_ContentLoadHandle.IsValid())
	{
		M_ContentLoadHandle->CancelHandle();
		M_ContentLoadHandle.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void ASurvivalStartLocation::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickEditorDebugRefresh();
	if (GetShouldDrawDebugView())
	{
		DrawDebugView();
	}
}

bool ASurvivalStartLocation::ShouldTickIfViewportsOnly() const
{
#if WITH_EDITOR
	return true;
#else
	return false;
#endif
}

void ASurvivalStartLocation::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	bM_DebugViewDirty = true;
}

void ASurvivalStartLocation::GeneratePlayerStartContent(const USurvivalStartContentSettings* ContentSettings)
{
	if (not IsValid(ContentSettings))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("ASurvivalStartLocation::GeneratePlayerStartContent: no start content settings; set "
				"USurvivalModeSettings::StartContent. Location: ") + GetName());
		return;
	}
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	if (bM_HasGeneratedPlayerContent)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("ASurvivalStartLocation::GeneratePlayerStartContent called twice for ") + GetName());
		return;
	}
	bM_HasGeneratedPlayerContent = true;
	RefreshDefensePoints();

	FSurvivalStartContentPlanInput PlanInput;
	BuildPlanInput(*ContentSettings, GetContentDifficulty(), MakeContentSeed(*ContentSettings), PlanInput);
	FSurvivalStartContentPlanner Planner(*World, *ContentSettings, PlanInput);
	M_PendingContentPlan = Planner.BuildPlan();
	ReportPlanShortfalls(M_PendingContentPlan);
	M_DebugPlan = M_PendingContentPlan;
	bM_HasDebugPlan = true;
	RequestStartContentLoad();
}

void ASurvivalStartLocation::RefreshDefensePoints()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	const FSurvivalGroundProjector Projector(*World, SurvivalStartLocationConstants::DefensePointTraceUp,
	                                         SurvivalStartLocationConstants::DefensePointTraceDown);
	TArray<FVector> DesignerPointsWorld;
	GetDesignerDefensePointsWorld(DesignerPointsWorld);
	FSurvivalDefensePointFinder::ResolveDefensePoints(Projector, GetDeployCenter(), BuildAreaRadius,
	                                                 DesignerPointsWorld, DefensePointSource, DefensePointSearch,
	                                                 M_DefensePointDetails);
	M_DefensePoints.Reset(M_DefensePointDetails.Num());
	for (const FSurvivalDefensePoint& DefensePoint : M_DefensePointDetails)
	{
		M_DefensePoints.Add(DefensePoint.Location);
	}
}

float ASurvivalStartLocation::GetContentOuterRadius(const USurvivalStartContentSettings* ContentSettings) const
{
	if (not IsValid(ContentSettings))
	{
		return BuildAreaRadius;
	}
	return BuildAreaRadius + ContentSettings->GetOuterContentDistanceFromBuildEdge();
}

void ASurvivalStartLocation::RefreshEditorPreview()
{
	RefreshDefensePoints();
	RefreshDebugRings();
	if (bM_PreviewRequested)
	{
		BuildPreviewPlan();
	}
	bM_DebugViewDirty = false;
	M_LastDebugRefreshSeconds = FPlatformTime::Seconds();
}

void ASurvivalStartLocation::PreviewStartContent()
{
	if (not GetIsValidPreviewContentSettings())
	{
		return;
	}
	M_PreviewSeed = PreviewContentSettings->Seed != 0
		                ? PreviewContentSettings->Seed
		                : static_cast<int32>(GetTypeHash(FDateTime::Now().GetTicks()));
	bM_PreviewRequested = true;
	RefreshEditorPreview();
}

void ASurvivalStartLocation::ClearStartContentPreview()
{
	bM_PreviewRequested = false;
	M_DebugPlan.Reset();
	bM_HasDebugPlan = false;
}

void ASurvivalStartLocation::BakeDefensePointsIntoDesignerPoints()
{
	RefreshDefensePoints();
	Modify();
	DesignerDefensePoints.Reset(M_DefensePointDetails.Num());
	const FTransform& ActorTransform = GetActorTransform();
	for (const FSurvivalDefensePoint& DefensePoint : M_DefensePointDetails)
	{
		DesignerDefensePoints.Add(ActorTransform.InverseTransformPosition(DefensePoint.Location));
	}
	RefreshEditorPreview();
}

ERTSGameDifficulty ASurvivalStartLocation::GetContentDifficulty() const
{
	const UWorld* World = GetWorld();
	if (not IsValid(World) or not World->IsGameWorld())
	{
		return PreviewDifficulty;
	}
	const ASurvivalModeDirector* Director = ASurvivalModeDirector::Get(this);
	if (not IsValid(Director))
	{
		return ERTSGameDifficulty::Normal;
	}
	return Director->GetDifficulty().DifficultyLevel;
}

int32 ASurvivalStartLocation::MakeContentSeed(const USurvivalStartContentSettings& ContentSettings) const
{
	const uint32 LocationHash = GetTypeHash(GetFName());
	if (ContentSettings.Seed != 0)
	{
		return static_cast<int32>(HashCombine(static_cast<uint32>(ContentSettings.Seed), LocationHash));
	}
	return static_cast<int32>(HashCombine(GetTypeHash(FDateTime::Now().GetTicks()), LocationHash));
}

void ASurvivalStartLocation::GetDesignerDefensePointsWorld(TArray<FVector>& OutPoints) const
{
	const FTransform& ActorTransform = GetActorTransform();
	OutPoints.Reset(DesignerDefensePoints.Num());
	for (const FVector& RelativePoint : DesignerDefensePoints)
	{
		OutPoints.Add(ActorTransform.TransformPosition(RelativePoint));
	}
}

void ASurvivalStartLocation::GatherOtherStartAreas(
	const USurvivalStartContentSettings& ContentSettings,
	TArray<FSurvivalReservedCircle>& OutAreas) const
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	for (TActorIterator<ASurvivalStartLocation> StartIterator(World); StartIterator; ++StartIterator)
	{
		const ASurvivalStartLocation* OtherStart = *StartIterator;
		if (not IsValid(OtherStart) or OtherStart == this)
		{
			continue;
		}
		FSurvivalReservedCircle& Area = OutAreas.AddDefaulted_GetRef();
		Area.Center = FVector2D(OtherStart->GetDeployCenter());
		Area.Radius = OtherStart->GetBuildAreaRadius() + ContentSettings.OtherStartLocationClearance;
	}
}

void ASurvivalStartLocation::BuildPlanInput(
	const USurvivalStartContentSettings& ContentSettings,
	const ERTSGameDifficulty Difficulty,
	const int32 Seed,
	FSurvivalStartContentPlanInput& OutInput) const
{
	OutInput.DeployCenter = GetDeployCenter();
	OutInput.BuildAreaRadius = BuildAreaRadius;
	OutInput.DefensePoints = M_DefensePointDetails;
	OutInput.Difficulty = Difficulty;
	OutInput.Seed = Seed;
	OutInput.OtherStartAreas.Reset();
	GatherOtherStartAreas(ContentSettings, OutInput.OtherStartAreas);
}

void ASurvivalStartLocation::RequestStartContentLoad()
{
	TArray<FSoftObjectPath> SoftPaths;
	M_PendingContentPlan.CollectSoftObjectPaths(SoftPaths);
	SoftPaths.RemoveAll([](const FSoftObjectPath& Path) { return Path.IsNull(); });
	if (SoftPaths.IsEmpty())
	{
		SpawnPendingContent();
		return;
	}
	// The delegate is bound to this UObject, so it is skipped safely if the location is destroyed while loading.
	M_ContentLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		SoftPaths,
		FStreamableDelegate::CreateUObject(this, &ASurvivalStartLocation::OnStartContentLoaded),
		FStreamableManager::AsyncLoadHighPriority);
	if (not M_ContentLoadHandle.IsValid())
	{
		// Nothing could be requested; spawn whatever is already in memory (missing classes are reported).
		SpawnPendingContent();
	}
}

void ASurvivalStartLocation::OnStartContentLoaded()
{
	SpawnPendingContent();
}

void ASurvivalStartLocation::SpawnPendingContent()
{
	// Moved out first: spawning runs BeginPlay on the new actors and nothing may touch the pending plan meanwhile.
	const FSurvivalStartContentPlan Plan = MoveTemp(M_PendingContentPlan);
	M_PendingContentPlan.Reset();
	if (Plan.Actors.IsEmpty() and Plan.MeshInstances.IsEmpty())
	{
		return;
	}
	TSet<FSoftObjectPath> MissingClasses;
	for (const FSurvivalPlannedActor& PlannedActor : Plan.Actors)
	{
		if (AActor* SpawnedActor = SpawnPlannedActor(PlannedActor, MissingClasses))
		{
			M_SpawnedStartContent.Add(SpawnedActor);
		}
	}
	SpawnFoliageInstances(Plan.MeshInstances, Plan.FoliageCullDistance);
	if (MissingClasses.IsEmpty())
	{
		return;
	}
	FString MissingList;
	for (const FSoftObjectPath& MissingClass : MissingClasses)
	{
		MissingList += TEXT("\n") + MissingClass.ToString();
	}
	RTSFunctionLibrary::ReportError(
		TEXT("ASurvivalStartLocation: start content classes failed to load (check the start content data asset):")
		+ MissingList);
}

AActor* ASurvivalStartLocation::SpawnPlannedActor(
	const FSurvivalPlannedActor& PlannedActor,
	TSet<FSoftObjectPath>& OutMissingClasses)
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return nullptr;
	}
	UClass* ActorClass = PlannedActor.ActorClass.Get();
	if (not IsValid(ActorClass))
	{
		OutMissingClasses.Add(PlannedActor.ActorClass.ToSoftObjectPath());
		return nullptr;
	}
	FActorSpawnParameters SpawnParameters;
	// Placement already spaced the footprints; never drop a resource or pit because of a collision overlap.
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<AActor>(ActorClass, PlannedActor.Transform, SpawnParameters);
}

void ASurvivalStartLocation::SpawnFoliageInstances(
	const TArray<FSurvivalPlannedMeshInstance>& Instances,
	const float CullDistance)
{
	TMap<UStaticMesh*, UInstancedStaticMeshComponent*> ComponentPerMesh;
	for (const FSurvivalPlannedMeshInstance& Instance : Instances)
	{
		UStaticMesh* Mesh = Instance.Mesh.Get();
		if (not IsValid(Mesh))
		{
			continue;
		}
		UInstancedStaticMeshComponent** ExistingComponent = ComponentPerMesh.Find(Mesh);
		UInstancedStaticMeshComponent* FoliageComponent = ExistingComponent
			                                                  ? *ExistingComponent
			                                                  : CreateFoliageComponent(Mesh, CullDistance);
		if (not IsValid(FoliageComponent))
		{
			continue;
		}
		ComponentPerMesh.Add(Mesh, FoliageComponent);
		FoliageComponent->AddInstance(Instance.Transform, /*bWorldSpace=*/true);
	}
}

UInstancedStaticMeshComponent* ASurvivalStartLocation::CreateFoliageComponent(
	UStaticMesh* Mesh,
	const float CullDistance)
{
	UInstancedStaticMeshComponent* FoliageComponent = NewObject<UInstancedStaticMeshComponent>(this);
	if (not IsValid(FoliageComponent))
	{
		return nullptr;
	}
	FoliageComponent->SetStaticMesh(Mesh);
	FoliageComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FoliageComponent->SetCanEverAffectNavigation(false);
	FoliageComponent->SetCastShadow(false);
	if (CullDistance > 0.f)
	{
		FoliageComponent->SetCullDistances(0, FMath::RoundToInt(CullDistance));
	}
	FoliageComponent->SetupAttachment(RootComponent);
	FoliageComponent->RegisterComponent();
	AddInstanceComponent(FoliageComponent);
	M_FoliageComponents.Add(FoliageComponent);
	return FoliageComponent;
}

void ASurvivalStartLocation::ReportPlanShortfalls(const FSurvivalStartContentPlan& Plan) const
{
	if (Plan.Actors.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(
			TEXT("ASurvivalStartLocation: planned no start content around ") + GetName() +
			TEXT(" (no landscape under the location, or every spot was blocked)."));
		return;
	}
	const int32 RadixiteFields = Plan.GetPlannedFieldCount(ESurvivalStartContentCategory::Radixite);
	const int32 MetalFields = Plan.GetPlannedFieldCount(ESurvivalStartContentCategory::Metal);
	if (RadixiteFields >= Plan.RequestedRadixiteFields and MetalFields >= Plan.RequestedMetalFields)
	{
		return;
	}
	RTSFunctionLibrary::ReportWarning(FString::Printf(
		TEXT("ASurvivalStartLocation %s: placed %d/%d radixite and %d/%d metal fields; the ring outside the build "
			"area is too steep, too close to another start location or off the landscape."),
		*GetName(), RadixiteFields, Plan.RequestedRadixiteFields, MetalFields, Plan.RequestedMetalFields));
}

void ASurvivalStartLocation::BuildPreviewPlan()
{
	UWorld* World = GetWorld();
	if (not IsValid(World) or not GetIsValidPreviewContentSettings())
	{
		return;
	}
	FSurvivalStartContentPlanInput PlanInput;
	BuildPlanInput(*PreviewContentSettings, PreviewDifficulty, M_PreviewSeed, PlanInput);
	FSurvivalStartContentPlanner Planner(*World, *PreviewContentSettings, PlanInput);
	M_DebugPlan = Planner.BuildPlan();
	bM_HasDebugPlan = true;
}

bool ASurvivalStartLocation::GetIsValidPreviewContentSettings() const
{
	if (IsValid(PreviewContentSettings))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("PreviewContentSettings"),
		TEXT("GetIsValidPreviewContentSettings"),
		this);
	return false;
}

void ASurvivalStartLocation::TickEditorDebugRefresh()
{
#if WITH_EDITOR
	const UWorld* World = GetWorld();
	if (not bM_DebugViewDirty or not IsValid(World) or World->IsGameWorld() or not GetShouldDrawDebugView())
	{
		return;
	}
	if (FPlatformTime::Seconds() - M_LastDebugRefreshSeconds < SurvivalStartLocationConstants::DebugRefreshIntervalSeconds)
	{
		return;
	}
	RefreshEditorPreview();
#endif
}

void ASurvivalStartLocation::RefreshDebugRings()
{
	BuildGroundRing(BuildAreaRadius, M_DebugBuildRing);
	M_DebugResourceInnerRing.Reset();
	M_DebugResourceOuterRing.Reset();
	M_DebugWreckOuterRing.Reset();
	// Optional designer aid: no settings means only the build area is drawn.
	if (not IsValid(PreviewContentSettings))
	{
		return;
	}
	const FSurvivalResourceFieldSettings& Radixite = PreviewContentSettings->RadixiteFields;
	const FSurvivalResourceFieldSettings& Metal = PreviewContentSettings->MetalFields;
	const float InnerGap = FMath::Min(Radixite.GapFromBuildEdge, Metal.GapFromBuildEdge);
	const float OuterGap = FMath::Max(
		Radixite.GapFromBuildEdge + Radixite.MaxExtraOutwardOffset + 2.f * Radixite.FieldRadius,
		Metal.GapFromBuildEdge + Metal.MaxExtraOutwardOffset + 2.f * Metal.FieldRadius);
	BuildGroundRing(BuildAreaRadius + InnerGap, M_DebugResourceInnerRing);
	BuildGroundRing(BuildAreaRadius + OuterGap, M_DebugResourceOuterRing);
	BuildGroundRing(GetContentOuterRadius(PreviewContentSettings), M_DebugWreckOuterRing);
}

void ASurvivalStartLocation::BuildGroundRing(const float Radius, TArray<FVector>& OutRing) const
{
	using namespace SurvivalStartLocationConstants;
	OutRing.Reset(DebugRingSegments);
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	const FSurvivalGroundProjector Projector(*World, DefensePointTraceUp, DefensePointTraceDown);
	const FVector Center = GetDeployCenter();
	for (int32 SegmentIndex = 0; SegmentIndex < DebugRingSegments; ++SegmentIndex)
	{
		const float Angle = 2.f * PI * SegmentIndex / DebugRingSegments;
		const FVector2D Position = FVector2D(Center) + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
		float Height = static_cast<float>(Center.Z);
		Projector.TryGetHeight(Position, Height);
		OutRing.Add(FVector(Position, Height + DebugHeightOffset));
	}
}

bool ASurvivalStartLocation::GetShouldDrawDebugView() const
{
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	if (World->IsGameWorld())
	{
		return bDrawDebugInGame;
	}
#if WITH_EDITOR
	return bAlwaysDrawDebug or IsSelectedInEditor();
#else
	return false;
#endif
}

void ASurvivalStartLocation::DrawDebugView() const
{
#if ENABLE_DRAW_DEBUG
	using namespace SurvivalStartLocationConstants;
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	const FVector Center = GetDeployCenter();
	DrawDebugLine(World, Center, Center + FVector::UpVector * DebugMarkerHeight * 2.f, BuildAreaColor, false, -1.f, 0, 8.f);
	DrawDebugRing(M_DebugBuildRing, BuildAreaColor, 12.f);
	DrawDebugRing(M_DebugResourceInnerRing, ResourceBandColor, 4.f);
	DrawDebugRing(M_DebugResourceOuterRing, ResourceBandColor, 4.f);
	DrawDebugRing(M_DebugWreckOuterRing, WreckBandColor, 4.f);
	DrawDebugDefensePoints();
	if (bM_HasDebugPlan)
	{
		DrawDebugPlan();
	}
#endif
}

void ASurvivalStartLocation::DrawDebugRing(const TArray<FVector>& Ring, const FColor& Color, const float Thickness) const
{
#if ENABLE_DRAW_DEBUG
	const UWorld* World = GetWorld();
	if (not IsValid(World) or Ring.Num() < 2)
	{
		return;
	}
	for (int32 PointIndex = 0; PointIndex < Ring.Num(); ++PointIndex)
	{
		DrawDebugLine(World, Ring[PointIndex], Ring[(PointIndex + 1) % Ring.Num()], Color, false, -1.f, 0, Thickness);
	}
#endif
}

void ASurvivalStartLocation::DrawDebugDefensePoints() const
{
#if ENABLE_DRAW_DEBUG
	using namespace SurvivalStartLocationConstants;
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	for (const FSurvivalDefensePoint& DefensePoint : M_DefensePointDetails)
	{
		FColor Color = HillTopPointColor;
		if (DefensePoint.Origin == ESurvivalDefensePointOrigin::Designer)
		{
			Color = DesignerPointColor;
		}
		else if (DefensePoint.Origin == ESurvivalDefensePointOrigin::BuildEdgeFallback)
		{
			Color = FallbackPointColor;
		}
		const FVector Top = DefensePoint.Location + FVector::UpVector * DebugMarkerHeight;
		DrawDebugLine(World, DefensePoint.Location, Top, Color, false, -1.f, 0, 6.f);
		DrawDebugSphere(World, Top, DebugDefenseSphereRadius, 12, Color, false, -1.f, 0, 3.f);
	}
#endif
}

void ASurvivalStartLocation::DrawDebugPlan() const
{
#if ENABLE_DRAW_DEBUG
	using namespace SurvivalStartLocationConstants;
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	const auto GetCategoryColor = [](const ESurvivalStartContentCategory Category) -> FColor
	{
		switch (Category)
		{
		case ESurvivalStartContentCategory::Radixite:
			return RadixiteColor;
		case ESurvivalStartContentCategory::Metal:
			return MetalColor;
		case ESurvivalStartContentCategory::Wreck:
			return WreckColor;
		case ESurvivalStartContentCategory::Pit:
			return PitColor;
		case ESurvivalStartContentCategory::Crater:
			return CraterColor;
		}
		return FColor::White;
	};
	const FVector HorizontalY(1.f, 0.f, 0.f);
	const FVector HorizontalZ(0.f, 1.f, 0.f);
	for (const FSurvivalPlannedField& Field : M_DebugPlan.Fields)
	{
		DrawDebugCircle(World, Field.Center + FVector::UpVector * DebugHeightOffset, Field.Radius, DebugCircleSegments,
		                GetCategoryColor(Field.Category), false, -1.f, 0, 8.f, HorizontalY, HorizontalZ, false);
	}
	for (const FSurvivalPlannedActor& PlannedActor : M_DebugPlan.Actors)
	{
		const FColor Color = GetCategoryColor(PlannedActor.Category);
		const FVector Location = PlannedActor.Transform.GetLocation();
		DrawDebugCircle(World, Location + FVector::UpVector * DebugHeightOffset, PlannedActor.FootprintRadius,
		                DebugCircleSegments / 2, Color, false, -1.f, 0, 3.f, HorizontalY, HorizontalZ, false);
		DrawDebugLine(World, Location, Location + FVector::UpVector * DebugMarkerHeight * 0.5f, Color, false, -1.f, 0, 4.f);
		// Forward arrow so pit facing and wreck yaw are visible.
		const FVector Forward = PlannedActor.Transform.GetRotation().GetForwardVector() * PlannedActor.FootprintRadius;
		DrawDebugLine(World, Location + FVector::UpVector * DebugHeightOffset,
		              Location + FVector::UpVector * DebugHeightOffset + Forward, Color, false, -1.f, 0, 3.f);
	}
#endif
}
