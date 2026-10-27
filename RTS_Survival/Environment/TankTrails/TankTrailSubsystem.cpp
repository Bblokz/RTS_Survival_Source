// Copyright (C) Bas Blokzijl - All rights reserved.

#include "TankTrailSubsystem.h"

#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "EngineUtils.h"
#include "GlobalRenderResources.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Landscape.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/TrackedTankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSTankTrails, Log, All);

namespace TankTrailPrivate
{
	const TCHAR* const RenderTargetPath =
		TEXT("/Game/RTS_Survival/Blueprints/Environment/TankTrails/RT_TankTrails.RT_TankTrails");
	const TCHAR* const ParameterCollectionPath =
		TEXT("/Game/RTS_Survival/Blueprints/Environment/TankTrails/MPC_TankTrails.MPC_TankTrails");

	// Landscape material scalar: above zero means the material shows trails, so the writer has work to do.
	const FName TrailStrengthParameter = TEXT("TankTrailStrength");
	const FName BoundsMinParameter = TEXT("TrailBoundsMin");
	const FName InverseSizeParameter = TEXT("TrailBoundsInvSize");
	const FName EnabledParameter = TEXT("TrailEnabled");

	// World size of one trail texel; ruts are about three texels wide.
	constexpr float TexelWorldSize = 16.f;
	constexpr int32 MaxTargetDimension = 8192;
	// The saved asset stays this small so it costs nothing outside a game with trails.
	constexpr int32 IdleTargetDimension = 4;

	// Trails are drawn a few times per second; a tank at full speed moves well under a segment length per update.
	constexpr float UpdateIntervalSeconds = 0.1f;
	constexpr float MinSegmentLength = 25.f;
	// Further than this between two updates is a teleport (spawn, transport, cheat), not driving.
	constexpr float MaxSegmentLength = 1500.f;

	// Track width as a fraction of the hull width, clamped for unusual meshes.
	constexpr float TrackWidthFraction = 0.2f;
	constexpr float MinTrackWidth = 30.f;
	constexpr float MaxTrackWidth = 90.f;
	constexpr float FallbackHalfWidth = 150.f;
	// The rut is flat in the middle and fades out over the outer part of the track on both sides.
	constexpr float RutFlatHalfFraction = 0.3f;
	// Full coverage, so overlapping segments saturate instead of leaving darker dots at their joins.
	constexpr float RutAlpha = 1.f;
	// Both ends of a segment ramp out over this many track widths. Where the trail goes on, the next segment's
	// full-depth body covers the ramp; where a tank stopped or reversed, the rut ends in a slope instead of a vertical
	// wall that the landscape shading turned into a black dot. The ramps also close the gaps of turning tanks.
	constexpr float EndCapLengthFraction = 1.5f;
	constexpr int32 TrianglesPerStrip = 6;
	constexpr int32 StripsPerSegment = 3;

	// Old trails fade away; zero half life keeps every trail for the whole match.
	constexpr float FadeIntervalSeconds = 4.f;
	constexpr float FadeHalfLifeSeconds = 300.f;

	TAutoConsoleVariable<int32> CVarTankTrails(
		TEXT("rts.TankTrails"),
		1,
		TEXT("1 draws tank track trails on landscapes whose material supports them, 0 turns them off."),
		ECVF_Default);

	FCanvasUVTri MakeTriangle(const FVector2D& A, const float AlphaA, const FVector2D& B, const float AlphaB,
	                          const FVector2D& C, const float AlphaC)
	{
		FCanvasUVTri Triangle;
		Triangle.V0_Pos = A;
		Triangle.V1_Pos = B;
		Triangle.V2_Pos = C;
		Triangle.V0_Color = FLinearColor(1.f, 1.f, 1.f, AlphaA);
		Triangle.V1_Color = FLinearColor(1.f, 1.f, 1.f, AlphaB);
		Triangle.V2_Color = FLinearColor(1.f, 1.f, 1.f, AlphaC);
		return Triangle;
	}
}

bool UTankTrailSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld() && not IsRunningDedicatedServer();
}

void UTankTrailSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (not BeginPlay_ReadLandscape(InWorld))
	{
		return;
	}
	M_RenderTarget = LoadObject<UTextureRenderTarget2D>(nullptr, TankTrailPrivate::RenderTargetPath);
	M_ParameterCollection = LoadObject<UMaterialParameterCollection>(nullptr, TankTrailPrivate::ParameterCollectionPath);
	if (not GetIsValidRenderTarget() || not GetIsValidParameterCollection())
	{
		return;
	}
	if (not BeginPlay_SetupRenderTarget())
	{
		UE_LOG(LogRTSTankTrails, Warning, TEXT("Tank trails: the render target could not be created."));
		return;
	}
	bM_IsActive = true;
	UE_LOG(LogRTSTankTrails, Display, TEXT("Tank trails active: render target %dx%d"), M_TargetSize.X,
	       M_TargetSize.Y);
	BeginPlay_RegisterTanks(InWorld);
	M_ActorSpawnedHandle = InWorld.AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateUObject(this, &UTankTrailSubsystem::OnActorSpawned));
	UpdateConsoleState();
}

void UTankTrailSubsystem::Deinitialize()
{
	if (bM_IsActive)
	{
		if (UWorld* World = GetWorld())
		{
			World->RemoveOnActorSpawnedHandler(M_ActorSpawnedHandle);
		}
		if (GetIsValidRenderTarget())
		{
			// The asset is shared by every world; hand the memory back.
			M_RenderTarget->ResizeTarget(TankTrailPrivate::IdleTargetDimension, TankTrailPrivate::IdleTargetDimension);
		}
	}
	bM_IsActive = false;
	M_Vehicles.Empty();
	Super::Deinitialize();
}

void UTankTrailSubsystem::Tick(const float DeltaTime)
{
	if (not bM_IsActive)
	{
		return;
	}
	UpdateConsoleState();
	if (not bM_IsEnabledByConsole)
	{
		return;
	}
	M_SecondsSinceUpdate += DeltaTime;
	M_SecondsSinceFade += DeltaTime;
	if (M_SecondsSinceUpdate < TankTrailPrivate::UpdateIntervalSeconds)
	{
		return;
	}
	M_SecondsSinceUpdate = 0.f;
	UpdateTrails();
}

TStatId UTankTrailSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTankTrailSubsystem, STATGROUP_Tickables);
}

bool UTankTrailSubsystem::GetIsValidRenderTarget() const
{
	if (IsValid(M_RenderTarget))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(FString::Printf(
		TEXT("Tank trails: render target %s is missing.\n at function: UTankTrailSubsystem::GetIsValidRenderTarget"),
		TankTrailPrivate::RenderTargetPath));
	return false;
}

bool UTankTrailSubsystem::GetIsValidParameterCollection() const
{
	if (IsValid(M_ParameterCollection))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(FString::Printf(
		TEXT("Tank trails: parameter collection %s is missing."
			"\n at function: UTankTrailSubsystem::GetIsValidParameterCollection"),
		TankTrailPrivate::ParameterCollectionPath));
	return false;
}

bool UTankTrailSubsystem::BeginPlay_ReadLandscape(UWorld& InWorld)
{
	FBox LandscapeBounds(ForceInit);
	int32 ProxyCount = 0;
	FString MaterialNames;
	for (TActorIterator<ALandscapeProxy> It(&InWorld); It; ++It)
	{
		const ALandscapeProxy* Proxy = *It;
		const UMaterialInterface* Material = IsValid(Proxy) ? Proxy->GetLandscapeMaterial() : nullptr;
		++ProxyCount;
		MaterialNames += IsValid(Material) ? Material->GetName() + TEXT(" ") : FString(TEXT("none "));
		float Strength = 0.f;
		const FHashedMaterialParameterInfo StrengthInfo(TankTrailPrivate::TrailStrengthParameter);
		if (not IsValid(Material) || not Material->GetScalarParameterValue(StrengthInfo, Strength))
		{
			continue;
		}
		M_TrailStrength = FMath::Max(M_TrailStrength, Strength);
		LandscapeBounds += Proxy->GetComponentsBoundingBox(true);
	}
	// One line per map: tells at a glance why a map does or does not get trails.
	UE_LOG(LogRTSTankTrails, Display, TEXT("Tank trails: %d landscape proxies (%s), TankTrailStrength %.2f, bounds %s"),
	       ProxyCount, *MaterialNames.TrimEnd(), M_TrailStrength, *LandscapeBounds.ToString());
	if (M_TrailStrength <= 0.f || not LandscapeBounds.IsValid)
	{
		return false;
	}
	M_BoundsMin = FVector2D(LandscapeBounds.Min);
	M_BoundsSize = FVector2D(LandscapeBounds.GetSize());
	return M_BoundsSize.X > 0.f && M_BoundsSize.Y > 0.f;
}

bool UTankTrailSubsystem::BeginPlay_SetupRenderTarget()
{
	using namespace TankTrailPrivate;
	M_TargetSize.X = FMath::Clamp(FMath::CeilToInt(M_BoundsSize.X / TexelWorldSize), IdleTargetDimension,
	                              MaxTargetDimension);
	M_TargetSize.Y = FMath::Clamp(FMath::CeilToInt(M_BoundsSize.Y / TexelWorldSize), IdleTargetDimension,
	                              MaxTargetDimension);
	M_RenderTarget->RenderTargetFormat = RTF_R8;
	M_RenderTarget->ClearColor = FLinearColor::Black;
	M_RenderTarget->bAutoGenerateMips = false;
	M_RenderTarget->AddressX = TA_Clamp;
	M_RenderTarget->AddressY = TA_Clamp;
	// Ruts are coverage, not colour: keep the canvas from gamma correcting what it writes.
	M_RenderTarget->TargetGamma = 1.f;
	M_RenderTarget->SizeX = M_TargetSize.X;
	M_RenderTarget->SizeY = M_TargetSize.Y;
	M_RenderTarget->UpdateResource();
	M_RenderTarget->UpdateResourceImmediate(true);
	return M_RenderTarget->GameThread_GetRenderTargetResource() != nullptr;
}

void UTankTrailSubsystem::BeginPlay_RegisterTanks(UWorld& InWorld)
{
	for (TActorIterator<ATrackedTankMaster> It(&InWorld); It; ++It)
	{
		RegisterTank(*It);
	}
}

void UTankTrailSubsystem::OnActorSpawned(AActor* SpawnedActor)
{
	if (ATrackedTankMaster* Tank = Cast<ATrackedTankMaster>(SpawnedActor))
	{
		RegisterTank(Tank);
	}
}

void UTankTrailSubsystem::RegisterTank(ATrackedTankMaster* Tank)
{
	if (not IsValid(Tank))
	{
		return;
	}
	FTankTrailVehicle& Vehicle = M_Vehicles.AddDefaulted_GetRef();
	Vehicle.Tank = Tank;
}

void UTankTrailSubsystem::ApplyMaterialParameters(const bool bShowTrails) const
{
	UWorld* World = GetWorld();
	if (not IsValid(World) || not IsValid(M_ParameterCollection))
	{
		return;
	}
	const FLinearColor BoundsMin(M_BoundsMin.X, M_BoundsMin.Y, 0.f, 0.f);
	// Z and W carry the size of one texel in UV, the material uses it to find the rut edges.
	const FLinearColor InverseSize(1.f / M_BoundsSize.X, 1.f / M_BoundsSize.Y, 1.f / M_TargetSize.X,
	                               1.f / M_TargetSize.Y);
	UKismetMaterialLibrary::SetVectorParameterValue(World, M_ParameterCollection, TankTrailPrivate::BoundsMinParameter,
	                                                BoundsMin);
	UKismetMaterialLibrary::SetVectorParameterValue(World, M_ParameterCollection,
	                                                TankTrailPrivate::InverseSizeParameter, InverseSize);
	UKismetMaterialLibrary::SetScalarParameterValue(World, M_ParameterCollection, TankTrailPrivate::EnabledParameter,
	                                                bShowTrails ? 1.f : 0.f);
}

void UTankTrailSubsystem::UpdateConsoleState()
{
	const bool bIsEnabled = TankTrailPrivate::CVarTankTrails.GetValueOnGameThread() != 0;
	if (bIsEnabled == bM_IsEnabledByConsole)
	{
		return;
	}
	bM_IsEnabledByConsole = bIsEnabled;
	// Tanks kept driving while trails were off; start their trails where they are now.
	for (FTankTrailVehicle& Vehicle : M_Vehicles)
	{
		Vehicle.bHasLastTracks = false;
	}
	ApplyMaterialParameters(bIsEnabled);
}

void UTankTrailSubsystem::UpdateTrails()
{
	using namespace TankTrailPrivate;
	TRACE_CPUPROFILER_EVENT_SCOPE(UTankTrailSubsystem::UpdateTrails);
	const double StartSeconds = FPlatformTime::Seconds();
	TArray<FCanvasUVTri> Triangles;
	Triangles.Reserve(M_Vehicles.Num() * TrianglesPerStrip * StripsPerSegment * 2);
	int32 SegmentCount = 0;
	for (int32 VehicleIndex = M_Vehicles.Num() - 1; VehicleIndex >= 0; --VehicleIndex)
	{
		FTankTrailVehicle& Vehicle = M_Vehicles[VehicleIndex];
		const ATrackedTankMaster* Tank = Vehicle.Tank.Get();
		if (not IsValid(Tank))
		{
			M_Vehicles.RemoveAtSwap(VehicleIndex);
			continue;
		}
		if (Tank->IsHidden())
		{
			Vehicle.bHasLastTracks = false;
			continue;
		}
		if (Vehicle.HalfTrackSpacing < 0.f)
		{
			MeasureTracks(Vehicle, *Tank);
		}
		const FVector2D Right = FVector2D(Tank->GetActorRightVector()).GetSafeNormal();
		const FVector2D Centre = FVector2D(Tank->GetActorLocation()) + Right * Vehicle.TrackCentreOffset;
		const FVector2D LeftTrack = Centre - Right * Vehicle.HalfTrackSpacing;
		const FVector2D RightTrack = Centre + Right * Vehicle.HalfTrackSpacing;
		if (not Vehicle.bHasLastTracks)
		{
			Vehicle.LastLeftTrack = LeftTrack;
			Vehicle.LastRightTrack = RightTrack;
			Vehicle.bHasLastTracks = true;
			continue;
		}
		// Either track counts: a tank turning on the spot moves its tracks but not its centre.
		const float Moved = FMath::Max(FVector2D::Distance(LeftTrack, Vehicle.LastLeftTrack),
		                               FVector2D::Distance(RightTrack, Vehicle.LastRightTrack));
		if (Moved < MinSegmentLength)
		{
			continue;
		}
		if (Moved <= MaxSegmentLength)
		{
			AddTrackSegment(Vehicle.LastLeftTrack, LeftTrack, Vehicle.TrackWidth, Triangles);
			AddTrackSegment(Vehicle.LastRightTrack, RightTrack, Vehicle.TrackWidth, Triangles);
			SegmentCount += 2;
		}
		Vehicle.LastLeftTrack = LeftTrack;
		Vehicle.LastRightTrack = RightTrack;
	}
	if (Triangles.Num() > 0)
	{
		DrawTriangles(Triangles);
	}
	if (FadeHalfLifeSeconds > 0.f && M_SecondsSinceFade >= FadeIntervalSeconds)
	{
		DrawFade(1.f - FMath::Exp2(-M_SecondsSinceFade / FadeHalfLifeSeconds));
		M_SecondsSinceFade = 0.f;
		++M_Stats.FadePasses;
	}
	++M_Stats.Updates;
	M_Stats.SegmentsDrawn += SegmentCount;
	M_Stats.UpdateSeconds += FPlatformTime::Seconds() - StartSeconds;
}

void UTankTrailSubsystem::MeasureTracks(FTankTrailVehicle& Vehicle, const ATrackedTankMaster& Tank)
{
	using namespace TankTrailPrivate;
	float HalfWidth = FallbackHalfWidth;
	float CentreOffset = 0.f;
	if (const UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Tank.GetRootComponent()))
	{
		const FBox LocalBox = Root->CalcBounds(
			FTransform(FQuat::Identity, FVector::ZeroVector, Root->GetComponentScale())).GetBox();
		if (LocalBox.IsValid && LocalBox.GetExtent().Y > KINDA_SMALL_NUMBER)
		{
			HalfWidth = LocalBox.GetExtent().Y;
			CentreOffset = LocalBox.GetCenter().Y;
		}
	}
	Vehicle.TrackWidth = FMath::Clamp(2.f * HalfWidth * TrackWidthFraction, MinTrackWidth, MaxTrackWidth);
	Vehicle.HalfTrackSpacing = FMath::Max(HalfWidth - Vehicle.TrackWidth * 0.5f, Vehicle.TrackWidth * 0.5f);
	Vehicle.TrackCentreOffset = CentreOffset;
}

void UTankTrailSubsystem::AddTrackSegment(const FVector2D& From, const FVector2D& To, const float TrackWidth,
                                          TArray<FCanvasUVTri>& OutTriangles) const
{
	using namespace TankTrailPrivate;
	const FVector2D Direction = (To - From).GetSafeNormal();
	const FVector2D Side(-Direction.Y, Direction.X);
	const FVector2D EndCap = Direction * TrackWidth * EndCapLengthFraction;
	AddRutStrip(From - EndCap, From, Side, TrackWidth, 0.f, RutAlpha, OutTriangles);
	AddRutStrip(From, To, Side, TrackWidth, RutAlpha, RutAlpha, OutTriangles);
	AddRutStrip(To, To + EndCap, Side, TrackWidth, RutAlpha, 0.f, OutTriangles);
}

void UTankTrailSubsystem::AddRutStrip(const FVector2D& Start, const FVector2D& End, const FVector2D& Side,
                                      const float TrackWidth, const float StartDepth, const float EndDepth,
                                      TArray<FCanvasUVTri>& OutTriangles) const
{
	using namespace TankTrailPrivate;
	const FVector2D EdgeOffset = Side * TrackWidth * 0.5f;
	const FVector2D FlatOffset = Side * TrackWidth * RutFlatHalfFraction;
	// Four lines along the strip: outer edge, flat bottom, flat bottom, outer edge.
	const FVector2D OuterA0 = WorldToPixel(Start - EdgeOffset);
	const FVector2D FlatA0 = WorldToPixel(Start - FlatOffset);
	const FVector2D FlatB0 = WorldToPixel(Start + FlatOffset);
	const FVector2D OuterB0 = WorldToPixel(Start + EdgeOffset);
	const FVector2D OuterA1 = WorldToPixel(End - EdgeOffset);
	const FVector2D FlatA1 = WorldToPixel(End - FlatOffset);
	const FVector2D FlatB1 = WorldToPixel(End + FlatOffset);
	const FVector2D OuterB1 = WorldToPixel(End + EdgeOffset);
	OutTriangles.Add(MakeTriangle(OuterA0, 0.f, FlatA0, StartDepth, OuterA1, 0.f));
	OutTriangles.Add(MakeTriangle(FlatA0, StartDepth, FlatA1, EndDepth, OuterA1, 0.f));
	OutTriangles.Add(MakeTriangle(FlatA0, StartDepth, FlatB0, StartDepth, FlatA1, EndDepth));
	OutTriangles.Add(MakeTriangle(FlatB0, StartDepth, FlatB1, EndDepth, FlatA1, EndDepth));
	OutTriangles.Add(MakeTriangle(FlatB0, StartDepth, OuterB0, 0.f, FlatB1, EndDepth));
	OutTriangles.Add(MakeTriangle(OuterB0, 0.f, OuterB1, 0.f, FlatB1, EndDepth));
}

FVector2D UTankTrailSubsystem::WorldToPixel(const FVector2D& WorldLocation) const
{
	// Same mapping as the landscape material: U follows world X, V follows world Y.
	return (WorldLocation - M_BoundsMin) / M_BoundsSize * FVector2D(M_TargetSize);
}

void UTankTrailSubsystem::DrawTriangles(const TArray<FCanvasUVTri>& Triangles) const
{
	UWorld* World = GetWorld();
	FTextureRenderTargetResource* Resource = M_RenderTarget->GameThread_GetRenderTargetResource();
	if (not IsValid(World) || Resource == nullptr)
	{
		return;
	}
	// One canvas, one batched element list: every rut of this update is a single draw call.
	FCanvas Canvas(Resource, nullptr, World, World->GetFeatureLevel());
	FCanvasTriangleItem TriangleItem(Triangles, GWhiteTexture);
	TriangleItem.BlendMode = SE_BLEND_Translucent;
	Canvas.DrawItem(TriangleItem);
	Canvas.Flush_GameThread();
}

void UTankTrailSubsystem::DrawFade(const float FadeAlpha) const
{
	UWorld* World = GetWorld();
	FTextureRenderTargetResource* Resource = M_RenderTarget->GameThread_GetRenderTargetResource();
	if (not IsValid(World) || Resource == nullptr)
	{
		return;
	}
	FCanvas Canvas(Resource, nullptr, World, World->GetFeatureLevel());
	FCanvasTileItem FadeItem(FVector2D::ZeroVector, FVector2D(M_TargetSize), FLinearColor(0.f, 0.f, 0.f, FadeAlpha));
	FadeItem.BlendMode = SE_BLEND_Translucent;
	Canvas.DrawItem(FadeItem);
	Canvas.Flush_GameThread();
}
