// Copyright (C) Bas Blokzijl - All rights reserved.

#include "LandscapeAmbientFxSubsystem.h"

#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Landscape.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraSystemInstanceController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "Misc/PackageName.h"
#include "RTS_Survival/Environment/AmbientFx/AmbientFxSandMask.h"
#include "RTS_Survival/Environment/AmbientFx/RTSAmbientFxSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSAmbientFx, Log, All);

namespace AmbientFxPrivate
{
	// 0 disabled, 1 low, 2 medium, 3 maximum; written by the graphics settings (ERTSAmbientEffectsLevel).
	TAutoConsoleVariable<int32> CVarAmbientFx(
		TEXT("rts.AmbientFx"),
		3,
		TEXT("Ambient landscape effects (dust devils): 0 disabled, 1 low, 2 medium, 3 maximum."),
		ECVF_Default);

	// For measuring: devils appear at full strength up to the maximum at once, and vanish at once when disabled, so
	// an off/on benchmark phase sees the whole cost straight away.
	TAutoConsoleVariable<int32> CVarAmbientFxInstant(
		TEXT("rts.AmbientFx.Instant"),
		0,
		TEXT("1: dust devils spawn up to the maximum at once at full strength and vanish at once (benchmarks)."),
		ECVF_Default);

	// For checking a map or a measuring run: logs the devils and the particles of every emitter every few seconds.
	TAutoConsoleVariable<int32> CVarAmbientFxLog(
		TEXT("rts.AmbientFx.Log"),
		0,
		TEXT("1: log the alive dust devils, the camera focus and the live particles per emitter every 2 seconds."),
		ECVF_Default);
	constexpr float StatusLogInterval = 2.f;

	constexpr int32 LevelLow = 1;
	constexpr int32 LevelMedium = 2;
	constexpr int32 LevelMaximum = 3;

	// User parameters of the dust devil Niagara system (see RTS.AmbientFx.PatchNiagaraSystem).
	const FName PositionAndScaleArray = TEXT("DevilPositionAndScale");
	const FName MotionArray = TEXT("DevilMotion");
	const FName AliveSlotsArray = TEXT("AliveSlots");
	const FName AliveCountParameter = TEXT("AliveCount");
	const FName DustColorParameter = TEXT("DustColor");
	const TCHAR* const SpawnRateSuffix = TEXT("_SpawnRate");
	const TCHAR* const PerDevilRateSuffix = TEXT("_PerDevilRate");

	constexpr float GroundTraceUp = 5000.f;
	constexpr float GroundTraceDown = 30000.f;
	// Above this many cells the grid gets coarser, so the start-of-mission scan stays a few milliseconds.
	constexpr int32 MaxSandGridCells = 16384;
	constexpr float FocusUpdateInterval = 0.5f;
	// How far along the camera's view the focus trace looks for the ground, in cm.
	constexpr float FocusTraceLength = 200000.f;
	constexpr float FadeInSeconds = 3.f;
	constexpr float FadeOutSeconds = 4.f;
	// A new devil keeps at least this distance from the others, in cm.
	constexpr float MinDistanceBetweenDevils = 2500.f;
	constexpr int32 SpawnPointAttempts = 8;
	// Slow sideways swing of the drift direction, in radians, and how fast it swings.
	constexpr float WanderAmplitudeRadians = 0.6f;
	constexpr float WanderRate = 0.15f;
	// How fast the devil's height eases to the traced ground, per second.
	constexpr float GroundFollowRate = 4.f;
	// Steeper ground than this (normal Z) does not start a devil.
	constexpr float MinSpawnGroundNormalZ = 0.85f;
	// Share of devils that turn clockwise.
	constexpr float ClockwiseShare = 0.35f;
}

bool ULandscapeAmbientFxSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	if (not IsValid(World))
	{
		return false;
	}
	return World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE;
}

void ULandscapeAmbientFxSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	bM_IsReady = false;
	const URTSAmbientFxSettings* Settings = URTSAmbientFxSettings::Get();
	if (Settings == nullptr || InWorld.GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (GetIsMapExcluded(InWorld, *Settings))
	{
		UE_LOG(LogRTSAmbientFx, Display, TEXT("Ambient effects: map %s is excluded in the project settings."),
		       *InWorld.GetMapName());
		return;
	}
	if (not BeginPlay_BuildSandGrid(InWorld, *Settings))
	{
		return;
	}
	if (not BeginPlay_CreateNiagaraComponent(InWorld, *Settings))
	{
		return;
	}
	M_Random.Initialize(static_cast<int32>(FPlatformTime::Cycles()));
	UpdateMaxDustDevils(*Settings);
	M_SecondsUntilSpawn = M_Random.FRandRange(0.f, M_SpawnInterval.Max);
	M_PublishedPositionAndScale.SetNumZeroed(MaxDustDevilSlots);
	M_PublishedMotion.SetNumZeroed(MaxDustDevilSlots);
	M_PublishedAliveSlots.Reserve(MaxDustDevilSlots);
	bM_IsReady = true;
}

void ULandscapeAmbientFxSubsystem::Deinitialize()
{
	if (IsValid(M_NiagaraComponent))
	{
		M_NiagaraComponent->DestroyComponent();
	}
	M_NiagaraComponent = nullptr;
	bM_IsReady = false;
	Super::Deinitialize();
}

TStatId ULandscapeAmbientFxSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULandscapeAmbientFxSubsystem, STATGROUP_Tickables);
}

int32 ULandscapeAmbientFxSubsystem::GetAliveDustDevilCount() const
{
	int32 AliveCount = 0;
	for (const FAmbientDustDevil& DustDevil : M_DustDevils)
	{
		AliveCount += DustDevil.bIsAlive ? 1 : 0;
	}
	return AliveCount;
}

void ULandscapeAmbientFxSubsystem::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	const URTSAmbientFxSettings* Settings = URTSAmbientFxSettings::Get();
	if (not bM_IsReady || Settings == nullptr)
	{
		return;
	}
	UpdateMaxDustDevils(*Settings);
	UpdateCameraFocus(DeltaTime);
	UpdateDustDevils(DeltaTime, *Settings);
	TrySpawnDustDevil(DeltaTime, *Settings);

	const bool bHasDustDevils = GetAliveDustDevilCount() > 0;
	if (bHasDustDevils || bM_WasPublishingLastTick)
	{
		PublishDustDevils();
	}
	bM_WasPublishingLastTick = bHasDustDevils;
	LogStatusWhenAsked(DeltaTime);
}

void ULandscapeAmbientFxSubsystem::LogStatusWhenAsked(const float DeltaTime)
{
	if (AmbientFxPrivate::CVarAmbientFxLog.GetValueOnGameThread() == 0)
	{
		return;
	}
	M_SecondsUntilStatusLog -= DeltaTime;
	if (M_SecondsUntilStatusLog > 0.f)
	{
		return;
	}
	M_SecondsUntilStatusLog = AmbientFxPrivate::StatusLogInterval;
	FString Devils;
	for (int32 SlotIndex = 0; SlotIndex < MaxDustDevilSlots; ++SlotIndex)
	{
		const FAmbientDustDevil& DustDevil = M_DustDevils[SlotIndex];
		if (DustDevil.bIsAlive)
		{
			Devils += FString::Printf(TEXT("[%d %s s=%.2f age=%.0f] "), SlotIndex,
			                          *DustDevil.Location.ToCompactString(), DustDevil.Strength, DustDevil.Age);
		}
	}
	UE_LOG(LogRTSAmbientFx, Display, TEXT("RTS_AMBIENT_FX status max=%d alive=%d focus=%s component=%s particles: %s devils: %s"),
	       M_MaxDustDevils, GetAliveDustDevilCount(), *M_CameraFocus.ToCompactString(),
	       IsValid(M_NiagaraComponent) && M_NiagaraComponent->IsActive() ? TEXT("active") : TEXT("inactive"),
	       *DescribeEmitterParticleCounts(), *Devils);
}

FString ULandscapeAmbientFxSubsystem::DescribeEmitterParticleCounts() const
{
	if (not IsValid(M_NiagaraComponent))
	{
		return TEXT("no component");
	}
	const FNiagaraSystemInstanceControllerConstPtr InstanceController = M_NiagaraComponent->GetSystemInstanceController();
	const FNiagaraSystemInstance* SystemInstance = InstanceController.IsValid()
		                                               ? InstanceController->GetSystemInstance_Unsafe()
		                                               : nullptr;
	if (SystemInstance == nullptr)
	{
		return TEXT("no system instance");
	}
	FString Description;
	for (const FNiagaraEmitterInstanceRef& EmitterInstance : SystemInstance->GetEmitters())
	{
		Description += FString::Printf(TEXT("%s=%d/%d "), *EmitterInstance->GetEmitterHandle().GetName().ToString(),
		                               EmitterInstance->GetNumParticles(), EmitterInstance->GetTotalSpawnedParticles());
	}
	return Description;
}

bool ULandscapeAmbientFxSubsystem::GetIsValidNiagaraComponent() const
{
	if (IsValid(M_NiagaraComponent))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_NiagaraComponent"),
		TEXT("ULandscapeAmbientFxSubsystem::GetIsValidNiagaraComponent"),
		this);
	return false;
}

bool ULandscapeAmbientFxSubsystem::GetIsMapExcluded(const UWorld& InWorld, const URTSAmbientFxSettings& Settings)
{
	const FString MapPackageName = UWorld::RemovePIEPrefix(InWorld.GetOutermost()->GetName());
	for (const TSoftObjectPtr<UWorld>& ExcludedMap : Settings.ExcludedMaps)
	{
		if (ExcludedMap.ToSoftObjectPath().GetLongPackageName() == MapPackageName)
		{
			return true;
		}
	}
	return false;
}

bool ULandscapeAmbientFxSubsystem::BeginPlay_BuildSandGrid(UWorld& InWorld, const URTSAmbientFxSettings& Settings)
{
	FBox LandscapeBounds(ForceInit);
	for (TActorIterator<ALandscapeProxy> It(&InWorld); It; ++It)
	{
		const ALandscapeProxy* Proxy = *It;
		if (IsValid(Proxy))
		{
			LandscapeBounds += Proxy->GetComponentsBoundingBox(true);
		}
	}
	if (not LandscapeBounds.IsValid)
	{
		UE_LOG(LogRTSAmbientFx, Display, TEXT("Ambient effects: map %s has no landscape."), *InWorld.GetMapName());
		return false;
	}

	if (not BeginPlay_ReadBakedSandMask(InWorld, Settings))
	{
		BeginPlay_SetupTracedGrid(LandscapeBounds, Settings);
	}
	const float CellSize = M_SandGrid.CellSize;
	M_SandGrid.SandCellIndices.Reset();
	M_SandGrid.SandCellGroundPoints.Reset();

	// Hits per physical surface type; one log line shows whether the landscape layers tell sand apart.
	TMap<int32, int32> HitsPerSurface;
	int32 MissCount = 0;
	const double ScanStartSeconds = FPlatformTime::Seconds();
	for (int32 CellY = 0; CellY < M_SandGrid.CellCount.Y; ++CellY)
	{
		for (int32 CellX = 0; CellX < M_SandGrid.CellCount.X; ++CellX)
		{
			const FVector2D CellCentre = M_SandGrid.Origin + (FVector2D(CellX, CellY) + 0.5) * CellSize;
			FHitResult Hit;
			if (not TraceLandscape(FVector(CellCentre, LandscapeBounds.Max.Z), Hit))
			{
				++MissCount;
				continue;
			}
			const UPhysicalMaterial* PhysicalMaterial = Hit.PhysMaterial.Get();
			HitsPerSurface.FindOrAdd(static_cast<int32>(UPhysicalMaterial::DetermineSurfaceType(PhysicalMaterial)))++;
			const int32 CellIndex = CellY * M_SandGrid.CellCount.X + CellX;
			if (not bM_HasBakedSandMask)
			{
				M_SandGrid.bIsSandCell[CellIndex] = GetIsSandHit(Hit) ? 1 : 0;
			}
			if (M_SandGrid.bIsSandCell[CellIndex] == 0 || Hit.ImpactNormal.Z < AmbientFxPrivate::MinSpawnGroundNormalZ)
			{
				continue;
			}
			M_SandGrid.SandCellIndices.Add(CellIndex);
			M_SandGrid.SandCellGroundPoints.Add(Hit.ImpactPoint);
		}
	}

	FString SurfaceCounts;
	for (const TPair<int32, int32>& SurfaceCount : HitsPerSurface)
	{
		SurfaceCounts += FString::Printf(TEXT("SurfaceType%d=%d "), SurfaceCount.Key, SurfaceCount.Value);
	}
	UE_LOG(LogRTSAmbientFx, Display,
	       TEXT("Ambient effects: map %s %s grid %dx%d cells of %.0f cm, flat sand cells %d, hits per surface: %s, misses %d, %.1f ms"),
	       *InWorld.GetMapName(), bM_HasBakedSandMask ? TEXT("baked sand mask") : TEXT("traced surfaces"),
	       M_SandGrid.CellCount.X, M_SandGrid.CellCount.Y, CellSize,
	       M_SandGrid.SandCellIndices.Num(), *SurfaceCounts.TrimEnd(), MissCount,
	       (FPlatformTime::Seconds() - ScanStartSeconds) * 1000.0);
	return not M_SandGrid.SandCellIndices.IsEmpty();
}

bool ULandscapeAmbientFxSubsystem::BeginPlay_ReadBakedSandMask(const UWorld& InWorld, const URTSAmbientFxSettings& Settings)
{
	const FString MapName = UWorld::RemovePIEPrefix(FPackageName::GetShortName(InWorld.GetOutermost()));
	const UAmbientFxSandMask* SandMask = Cast<UAmbientFxSandMask>(Settings.GetSandMaskPath(MapName).TryLoad());
	bM_HasBakedSandMask = IsValid(SandMask) && SandMask->GetIsValidMask();
	if (not bM_HasBakedSandMask)
	{
		return false;
	}
	M_SandGrid.Origin = SandMask->Origin;
	M_SandGrid.CellSize = SandMask->CellSize;
	M_SandGrid.CellCount = SandMask->CellCount;
	M_SandGrid.bIsSandCell = SandMask->SandCells;
	return true;
}

void ULandscapeAmbientFxSubsystem::BeginPlay_SetupTracedGrid(const FBox& LandscapeBounds, const URTSAmbientFxSettings& Settings)
{
	const FVector2D BoundsSize(LandscapeBounds.GetSize());
	const float MinCellSizeForCap = FMath::Sqrt(BoundsSize.X * BoundsSize.Y / AmbientFxPrivate::MaxSandGridCells);
	M_SandGrid.CellSize = FMath::Max(Settings.SandCellSize, MinCellSizeForCap);
	M_SandGrid.Origin = FVector2D(LandscapeBounds.Min);
	M_SandGrid.CellCount = FIntPoint(
		FMath::Max(1, FMath::FloorToInt(BoundsSize.X / M_SandGrid.CellSize)),
		FMath::Max(1, FMath::FloorToInt(BoundsSize.Y / M_SandGrid.CellSize)));
	M_SandGrid.bIsSandCell.SetNumZeroed(M_SandGrid.CellCount.X * M_SandGrid.CellCount.Y);
}

bool ULandscapeAmbientFxSubsystem::BeginPlay_CreateNiagaraComponent(
	UWorld& InWorld,
	const URTSAmbientFxSettings& Settings)
{
	UNiagaraSystem* DustDevilSystem = Settings.DustDevilNiagaraSystem.LoadSynchronous();
	if (not IsValid(DustDevilSystem))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("The dust devil Niagara system set in the RTS Ambient Effects project settings could not be loaded: ")
			+ Settings.DustDevilNiagaraSystem.ToString());
		return false;
	}
	AWorldSettings* WorldSettings = InWorld.GetWorldSettings();
	if (not IsValid(WorldSettings))
	{
		return false;
	}
	M_NiagaraComponent = NewObject<UNiagaraComponent>(WorldSettings);
	if (not GetIsValidNiagaraComponent())
	{
		return false;
	}
	// The emitters simulate in world space and the component stays at the origin; its fixed bounds cover the map.
	M_NiagaraComponent->bAutoActivate = false;
	M_NiagaraComponent->SetAutoDestroy(false);
	M_NiagaraComponent->SetAsset(DustDevilSystem);
	M_NiagaraComponent->SetCastShadow(false);
	M_NiagaraComponent->RegisterComponentWithWorld(&InWorld);
	M_NiagaraComponent->SetVariableLinearColor(AmbientFxPrivate::DustColorParameter, Settings.DustColor);
	BeginPlay_ReadEmitterSpawnRates(*DustDevilSystem);
	UE_LOG(LogRTSAmbientFx, Display, TEXT("Ambient effects: dust devils ready, %d emitters, system %s"),
	       M_EmitterSpawnRates.Num(), *DustDevilSystem->GetPathName());
	return true;
}

void ULandscapeAmbientFxSubsystem::BeginPlay_ReadEmitterSpawnRates(const UNiagaraSystem& System)
{
	M_EmitterSpawnRates.Reset();
	TArray<FNiagaraVariable> UserParameters;
	System.GetExposedParameters().GetUserParameters(UserParameters);
	for (const FNiagaraVariable& UserParameter : UserParameters)
	{
		FString EmitterName = UserParameter.GetName().ToString();
		if (UserParameter.GetType() != FNiagaraTypeDefinition::GetFloatDef()
			|| not EmitterName.RemoveFromEnd(AmbientFxPrivate::PerDevilRateSuffix))
		{
			continue;
		}
		const FNiagaraVariable StoredParameter(
			FNiagaraTypeDefinition::GetFloatDef(), FName(TEXT("User.") + UserParameter.GetName().ToString()));
		const float PerDevilRate = System.GetExposedParameters().GetParameterValue<float>(StoredParameter);
		M_EmitterSpawnRates.Add(TPair<FName, float>(FName(EmitterName + AmbientFxPrivate::SpawnRateSuffix), PerDevilRate));
	}
}

void ULandscapeAmbientFxSubsystem::UpdateMaxDustDevils(const URTSAmbientFxSettings& Settings)
{
	switch (AmbientFxPrivate::CVarAmbientFx.GetValueOnGameThread())
	{
	case AmbientFxPrivate::LevelLow:
		M_MaxDustDevils = Settings.MaxDustDevilsLow;
		M_SpawnInterval = Settings.SpawnIntervalLow;
		break;
	case AmbientFxPrivate::LevelMedium:
		M_MaxDustDevils = Settings.MaxDustDevilsMedium;
		M_SpawnInterval = Settings.SpawnIntervalMedium;
		break;
	case AmbientFxPrivate::LevelMaximum:
		M_MaxDustDevils = Settings.MaxDustDevilsMaximum;
		M_SpawnInterval = Settings.SpawnIntervalMaximum;
		break;
	default:
		M_MaxDustDevils = 0;
		break;
	}
	M_MaxDustDevils = FMath::Clamp(M_MaxDustDevils, 0, MaxDustDevilSlots);
}

void ULandscapeAmbientFxSubsystem::UpdateCameraFocus(const float DeltaTime)
{
	M_SecondsUntilFocusUpdate -= DeltaTime;
	if (M_SecondsUntilFocusUpdate > 0.f)
	{
		return;
	}
	M_SecondsUntilFocusUpdate = AmbientFxPrivate::FocusUpdateInterval;
	const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (not IsValid(PlayerController))
	{
		return;
	}
	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	// The focus is where the camera looks at the ground, so devils appear on screen and not behind the camera.
	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AmbientFxFocus), false);
	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * AmbientFxPrivate::FocusTraceLength;
	if (GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, COLLISION_TRACE_LANDSCAPE, QueryParams))
	{
		M_CameraFocus = Hit.ImpactPoint;
	}
	else
	{
		M_CameraFocus = ViewLocation;
	}
	bM_HasCameraFocus = true;
}

void ULandscapeAmbientFxSubsystem::UpdateDustDevils(const float DeltaTime, const URTSAmbientFxSettings& Settings)
{
	TraceGroundForNextDustDevil();
	const bool bIsInstant = AmbientFxPrivate::CVarAmbientFxInstant.GetValueOnGameThread() != 0;
	int32 AliveCount = 0;
	for (FAmbientDustDevil& DustDevil : M_DustDevils)
	{
		if (not DustDevil.bIsAlive)
		{
			continue;
		}
		++AliveCount;
		// More devils than allowed after the player lowered the setting: the ones past the limit fade out.
		if (AliveCount > M_MaxDustDevils)
		{
			DustDevil.bIsFadingOut = true;
			DustDevil.Strength = bIsInstant ? 0.f : DustDevil.Strength;
		}
		UpdateDustDevil(DustDevil, DeltaTime, Settings);
	}
}

void ULandscapeAmbientFxSubsystem::UpdateDustDevil(
	FAmbientDustDevil& DustDevil,
	const float DeltaTime,
	const URTSAmbientFxSettings& Settings) const
{
	DustDevil.Age += DeltaTime;
	const float Wander = FMath::Sin(DustDevil.WanderPhase + DustDevil.Age * AmbientFxPrivate::WanderRate * UE_TWO_PI);
	const float Heading = DustDevil.HeadingRadians + Wander * AmbientFxPrivate::WanderAmplitudeRadians;
	DustDevil.Location += FVector(FMath::Cos(Heading), FMath::Sin(Heading), 0.f) * DustDevil.DriftSpeed * DeltaTime;
	const float GroundBlend = FMath::Min(1.f, AmbientFxPrivate::GroundFollowRate * DeltaTime);
	DustDevil.Location.Z = FMath::Lerp(DustDevil.Location.Z, DustDevil.TargetGroundZ, GroundBlend);

	const bool bIsPastLifetime = DustDevil.Age > DustDevil.Lifetime;
	const bool bIsFarFromCamera = bM_HasCameraFocus
		&& FVector::Dist2D(DustDevil.Location, M_CameraFocus) > Settings.DespawnRadius;
	if (bIsPastLifetime || bIsFarFromCamera || not GetIsSandCellAt(DustDevil.Location))
	{
		DustDevil.bIsFadingOut = true;
	}
	if (DustDevil.bIsFadingOut)
	{
		DustDevil.Strength -= DeltaTime / AmbientFxPrivate::FadeOutSeconds;
		if (DustDevil.Strength <= 0.f)
		{
			DustDevil = FAmbientDustDevil();
		}
		return;
	}
	DustDevil.Strength = FMath::Min(1.f, DustDevil.Strength + DeltaTime / AmbientFxPrivate::FadeInSeconds);
}

void ULandscapeAmbientFxSubsystem::TraceGroundForNextDustDevil()
{
	// One trace per tick, round robin over the slots: the ground under a devil changes slowly.
	for (int32 Attempt = 0; Attempt < MaxDustDevilSlots; ++Attempt)
	{
		M_NextGroundTraceSlot = (M_NextGroundTraceSlot + 1) % MaxDustDevilSlots;
		FAmbientDustDevil& DustDevil = M_DustDevils[M_NextGroundTraceSlot];
		if (not DustDevil.bIsAlive)
		{
			continue;
		}
		FHitResult Hit;
		if (TraceLandscape(DustDevil.Location, Hit))
		{
			DustDevil.TargetGroundZ = Hit.ImpactPoint.Z;
		}
		return;
	}
}

void ULandscapeAmbientFxSubsystem::TrySpawnDustDevil(const float DeltaTime, const URTSAmbientFxSettings& Settings)
{
	M_SecondsUntilSpawn -= DeltaTime;
	if (M_SecondsUntilSpawn > 0.f || not bM_HasCameraFocus)
	{
		return;
	}
	const bool bIsInstant = AmbientFxPrivate::CVarAmbientFxInstant.GetValueOnGameThread() != 0;
	M_SecondsUntilSpawn = bIsInstant ? 0.f : M_Random.FRandRange(M_SpawnInterval.Min, M_SpawnInterval.Max);
	int32 FreeSlot = INDEX_NONE;
	int32 AliveCount = 0;
	for (int32 SlotIndex = 0; SlotIndex < MaxDustDevilSlots; ++SlotIndex)
	{
		const bool bIsAlive = M_DustDevils[SlotIndex].bIsAlive;
		AliveCount += bIsAlive ? 1 : 0;
		FreeSlot = (FreeSlot == INDEX_NONE && not bIsAlive) ? SlotIndex : FreeSlot;
	}
	if (AliveCount >= M_MaxDustDevils || FreeSlot == INDEX_NONE)
	{
		return;
	}
	FVector GroundPoint;
	if (not FindSpawnPoint(Settings, GroundPoint))
	{
		return;
	}
	StartDustDevil(M_DustDevils[FreeSlot], GroundPoint, Settings);
	M_DustDevils[FreeSlot].Strength = bIsInstant ? 1.f : 0.f;
	SetComponentActive(true);
}

bool ULandscapeAmbientFxSubsystem::FindSpawnPoint(const URTSAmbientFxSettings& Settings, FVector& OutGroundPoint)
{
	TArray<int32> NearbySandCells;
	const float SpawnRadiusSquared = FMath::Square(Settings.SpawnRadius);
	for (int32 SandIndex = 0; SandIndex < M_SandGrid.SandCellGroundPoints.Num(); ++SandIndex)
	{
		if (FVector::DistSquared2D(M_SandGrid.SandCellGroundPoints[SandIndex], M_CameraFocus) <= SpawnRadiusSquared)
		{
			NearbySandCells.Add(SandIndex);
		}
	}
	if (NearbySandCells.IsEmpty())
	{
		return false;
	}
	const float HalfCell = M_SandGrid.CellSize * 0.5f;
	for (int32 Attempt = 0; Attempt < AmbientFxPrivate::SpawnPointAttempts; ++Attempt)
	{
		const int32 SandIndex = NearbySandCells[M_Random.RandHelper(NearbySandCells.Num())];
		const FVector CellPoint = M_SandGrid.SandCellGroundPoints[SandIndex]
			+ FVector(M_Random.FRandRange(-HalfCell, HalfCell), M_Random.FRandRange(-HalfCell, HalfCell), 0.f);
		if (GetDistanceToNearestDustDevil(CellPoint) < AmbientFxPrivate::MinDistanceBetweenDevils)
		{
			continue;
		}
		FHitResult Hit;
		const bool bIsFlatSand = TraceLandscape(CellPoint, Hit) && GetIsSandAt(Hit)
			&& Hit.ImpactNormal.Z >= AmbientFxPrivate::MinSpawnGroundNormalZ;
		if (bIsFlatSand)
		{
			OutGroundPoint = Hit.ImpactPoint;
			return true;
		}
	}
	return false;
}

void ULandscapeAmbientFxSubsystem::StartDustDevil(
	FAmbientDustDevil& DustDevil,
	const FVector& GroundPoint,
	const URTSAmbientFxSettings& Settings)
{
	DustDevil = FAmbientDustDevil();
	DustDevil.bIsAlive = true;
	DustDevil.Location = GroundPoint;
	DustDevil.TargetGroundZ = GroundPoint.Z;
	DustDevil.HeadingRadians = FMath::DegreesToRadians(Settings.WindYawDegrees)
		+ M_Random.FRandRange(-AmbientFxPrivate::WanderAmplitudeRadians, AmbientFxPrivate::WanderAmplitudeRadians);
	DustDevil.DriftSpeed = M_Random.FRandRange(Settings.DriftSpeed.Min, Settings.DriftSpeed.Max);
	DustDevil.WanderPhase = M_Random.FRandRange(0.f, UE_TWO_PI);
	DustDevil.Lifetime = M_Random.FRandRange(Settings.Lifetime.Min, Settings.Lifetime.Max);
	DustDevil.Scale = M_Random.FRandRange(Settings.Scale.Min, Settings.Scale.Max);
	DustDevil.Seed = M_Random.FRand();
	DustDevil.Spin = M_Random.FRand() < AmbientFxPrivate::ClockwiseShare ? -1.f : 1.f;
}

void ULandscapeAmbientFxSubsystem::PublishDustDevils()
{
	if (not GetIsValidNiagaraComponent())
	{
		return;
	}
	M_PublishedAliveSlots.Reset();
	for (int32 SlotIndex = 0; SlotIndex < MaxDustDevilSlots; ++SlotIndex)
	{
		const FAmbientDustDevil& DustDevil = M_DustDevils[SlotIndex];
		M_PublishedPositionAndScale[SlotIndex] = FVector4f(
			FVector3f(DustDevil.Location), DustDevil.bIsAlive ? DustDevil.Scale : 0.f);
		M_PublishedMotion[SlotIndex] = FVector4f(
			DustDevil.Strength, DustDevil.Seed, DustDevil.Spin, DustDevil.HeadingRadians);
		// Fading devils get no new particles; the ones they have finish their lives and fade with Strength.
		if (DustDevil.bIsAlive && not DustDevil.bIsFadingOut)
		{
			M_PublishedAliveSlots.Add(SlotIndex);
		}
	}
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector4(
		M_NiagaraComponent, AmbientFxPrivate::PositionAndScaleArray, M_PublishedPositionAndScale);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector4(
		M_NiagaraComponent, AmbientFxPrivate::MotionArray, M_PublishedMotion);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayInt32(
		M_NiagaraComponent, AmbientFxPrivate::AliveSlotsArray, M_PublishedAliveSlots);
	const int32 SpawningCount = M_PublishedAliveSlots.Num();
	M_NiagaraComponent->SetVariableInt(AmbientFxPrivate::AliveCountParameter, SpawningCount);
	for (const TPair<FName, float>& EmitterSpawnRate : M_EmitterSpawnRates)
	{
		M_NiagaraComponent->SetVariableFloat(EmitterSpawnRate.Key, EmitterSpawnRate.Value * SpawningCount);
	}
	if (GetAliveDustDevilCount() == 0)
	{
		// Lets the last particles finish, then the component stops costing anything until the next devil.
		SetComponentActive(false);
	}
}

void ULandscapeAmbientFxSubsystem::SetComponentActive(const bool bShouldBeActive) const
{
	if (not GetIsValidNiagaraComponent() || M_NiagaraComponent->IsActive() == bShouldBeActive)
	{
		return;
	}
	if (bShouldBeActive)
	{
		M_NiagaraComponent->Activate(true);
		return;
	}
	M_NiagaraComponent->Deactivate();
}

bool ULandscapeAmbientFxSubsystem::TraceLandscape(const FVector& Location, FHitResult& OutHit) const
{
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AmbientFxGround), false);
	QueryParams.bReturnPhysicalMaterial = true;
	const FVector TraceStart(Location.X, Location.Y, Location.Z + AmbientFxPrivate::GroundTraceUp);
	const FVector TraceEnd(Location.X, Location.Y, Location.Z - AmbientFxPrivate::GroundTraceDown);
	return GetWorld()->LineTraceSingleByChannel(OutHit, TraceStart, TraceEnd, COLLISION_TRACE_LANDSCAPE, QueryParams);
}

bool ULandscapeAmbientFxSubsystem::GetIsSandAt(const FHitResult& Hit) const
{
	return bM_HasBakedSandMask ? GetIsSandCellAt(Hit.ImpactPoint) : GetIsSandHit(Hit);
}

bool ULandscapeAmbientFxSubsystem::GetIsSandHit(const FHitResult& Hit) const
{
	const URTSAmbientFxSettings* Settings = URTSAmbientFxSettings::Get();
	if (Settings == nullptr)
	{
		return false;
	}
	const EPhysicalSurface SurfaceType = UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get());
	return Settings->SandSurfaceTypes.Contains(SurfaceType);
}

bool ULandscapeAmbientFxSubsystem::GetIsSandCellAt(const FVector& Location) const
{
	const FVector2D GridPosition = (FVector2D(Location) - M_SandGrid.Origin) / M_SandGrid.CellSize;
	const int32 CellX = FMath::FloorToInt(GridPosition.X);
	const int32 CellY = FMath::FloorToInt(GridPosition.Y);
	if (CellX < 0 || CellY < 0 || CellX >= M_SandGrid.CellCount.X || CellY >= M_SandGrid.CellCount.Y)
	{
		return false;
	}
	return M_SandGrid.bIsSandCell[CellY * M_SandGrid.CellCount.X + CellX] != 0;
}

float ULandscapeAmbientFxSubsystem::GetDistanceToNearestDustDevil(const FVector& Location) const
{
	float NearestDistance = TNumericLimits<float>::Max();
	for (const FAmbientDustDevil& DustDevil : M_DustDevils)
	{
		if (DustDevil.bIsAlive)
		{
			NearestDistance = FMath::Min(NearestDistance, static_cast<float>(FVector::Dist2D(DustDevil.Location, Location)));
		}
	}
	return NearestDistance;
}
