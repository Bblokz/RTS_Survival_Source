// Copyright (C) Bas Blokzijl - All rights reserved.

#include "AircraftStrafeImpactBatcher.h"

#include "Components/DecalComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraDataSet.h"
#include "NiagaraDataSetAccessor.h"
#include "RTS_Survival/Environment/ImpactDecals/RTSImpactDecalSettings.h"
#include "RTS_Survival/Environment/ImpactDecals/LandscapeImpactDecalSubsystem.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraSystemInstanceController.h"
#include "RTS_Survival/Physics/RTSSurfaceSubtypes.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/RTSWeaponVFXSettings/RTSWeaponVFXSettings.h"

namespace AircraftStrafeImpactNames
{
	// User parameters of NS_AircraftStrafeImpacts (written by RTS.StrafeImpactVfx.BuildNiagaraSystem).
	const FName PositionAndCalibreArray = TEXT("StrafePositionAndCalibre");
	const FName NormalAndSurfaceArray = TEXT("StrafeNormalAndSurface");
	const FName GroupOffset[] = {TEXT("GroundOffset"), TEXT("UnitOffset")};
	const FName GroupCount[] = {TEXT("GroundCount"), TEXT("UnitCount")};
	// Emitters are named <Group>_<Layer>; these are the group prefixes in the order of EAircraftStrafeImpactGroup.
	const TCHAR* GroupPrefix[] = {TEXT("Ground_"), TEXT("Unit_")};
	const TCHAR* PerImpactSuffix = TEXT("_PerImpact");
	const TCHAR* SpawnCountSuffix = TEXT("_SpawnCount");
	constexpr double MaxSafeWorldCoordinateCm = 10000000.0;
}

namespace AircraftStrafePockDecals
{
	// Pocks alive at once over all strafes; a few strafes' worth, recycled oldest first.
	constexpr int32 RingSize = 240;
	// Width of a 7.92 mm pock (cm); larger calibres scale with (calibre / 7.92)^0.6, as the strafe impact VFX.
	// 60 cm reads as a pock from the RTS camera; at 30 cm (the hole alone) the MG pocks were 4 pixels and vanished.
	constexpr float BaseDiameterCm = 60.f;
	constexpr float DiameterJitter = 0.25f;
	constexpr float ProjectionHalfDepthCm = 30.f;
	// Stays this long, then fades out over FadeSeconds (opacity in DecalColor.A, stepped every FadeTickSeconds).
	constexpr float HoldSeconds = 14.f;
	constexpr float FadeSeconds = 8.f;
	constexpr float FadeTickSeconds = 0.25f;
	// The crater atlas: 16 cells; the smallest shell band's cells read as a small dark hole at this size.
	constexpr float AtlasCellsTotal = 16.f;
}

UAircraftStrafeImpactBatcher::UAircraftStrafeImpactBatcher()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	for (int32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
	{
		M_PublishedOffset[GroupIndex] = 0;
		M_PublishedCount[GroupIndex] = 0;
#if RTS_WITH_MAP_TESTS
		M_TotalPublishedByGroup[GroupIndex] = 0;
#endif
	}
}

EAircraftStrafeImpactSurface UAircraftStrafeImpactBatcher::GetSurfaceForPhysicalSurface(const ERTSSurfaceType SurfaceType)
{
	switch (SurfaceType)
	{
	case ERTSSurfaceType::Stone:
		return EAircraftStrafeImpactSurface::Stone;
	case ERTSSurfaceType::Metal:
		return EAircraftStrafeImpactSurface::Metal;
	case ERTSSurfaceType::Sand:
	case ERTSSurfaceType::Flesh:
	case ERTSSurfaceType::Air:
		break;
	}
	return EAircraftStrafeImpactSurface::Dirt;
}

void UAircraftStrafeImpactBatcher::BeginPlay()
{
	Super::BeginPlay();
	const URTSWeaponVFXSettings* VfxSettings = URTSWeaponVFXSettings::Get();
	if (VfxSettings == nullptr || VfxSettings->AircraftStrafeImpactNiagaraSystem.IsNull())
	{
		return;
	}
	UNiagaraSystem* StrafeImpactSystem = VfxSettings->AircraftStrafeImpactNiagaraSystem.LoadSynchronous();
	if (not IsValid(StrafeImpactSystem))
	{
		RTSFunctionLibrary::ReportError(
			"The aircraft strafe impact Niagara system set in the RTS Weapon VFX project settings could not be loaded: "
			+ VfxSettings->AircraftStrafeImpactNiagaraSystem.ToString()
			+ "\n Aircraft strafe guns draw their ground hits as regular trace impacts.");
		return;
	}
	M_MaxImpactsPerFrame = FMath::Max(1, VfxSettings->MaxAircraftStrafeImpactsPerFrame);
	BeginPlay_ReadEmitterSpawns(*StrafeImpactSystem);
	BeginPlay_CreateNiagaraComponent(StrafeImpactSystem);
}

void UAircraftStrafeImpactBatcher::BeginPlay_ReadEmitterSpawns(const UNiagaraSystem& StrafeImpactSystem)
{
	using namespace AircraftStrafeImpactNames;
	const FNiagaraUserRedirectionParameterStore& UserParameterStore = StrafeImpactSystem.GetExposedParameters();
	TArray<FNiagaraVariable> UserParameters;
	UserParameterStore.GetUserParameters(UserParameters);
	for (const FNiagaraVariable& UserParameter : UserParameters)
	{
		const FString ParameterName = UserParameter.GetName().ToString();
		if (UserParameter.GetType() != FNiagaraTypeDefinition::GetIntDef() || not ParameterName.EndsWith(PerImpactSuffix))
		{
			continue;
		}
		const FString EmitterName = ParameterName.LeftChop(FCString::Strlen(PerImpactSuffix));
		for (int32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
		{
			if (not EmitterName.StartsWith(GroupPrefix[GroupIndex]))
			{
				continue;
			}
			const FNiagaraVariable StoredParameter(FNiagaraTypeDefinition::GetIntDef(),
			                                       FName(*(TEXT("User.") + ParameterName)));
			FAircraftStrafeImpactEmitterSpawn EmitterSpawn;
			EmitterSpawn.GroupIndex = GroupIndex;
			EmitterSpawn.SpawnCountParameter = FName(*(EmitterName + SpawnCountSuffix));
			EmitterSpawn.ParticlesPerImpact = FMath::Max(1, UserParameterStore.GetParameterValue<int32>(StoredParameter));
			M_EmitterSpawns.Add(EmitterSpawn);
			break;
		}
	}
}

void UAircraftStrafeImpactBatcher::BeginPlay_CreateNiagaraComponent(UNiagaraSystem* StrafeImpactSystem)
{
	AActor* OwningActor = GetOwner();
	if (not IsValid(OwningActor) || M_EmitterSpawns.IsEmpty())
	{
		return;
	}
	// Not attached to anything: stays at the world origin, the emitters place their particles in world space.
	M_NiagaraComponent = NewObject<UNiagaraComponent>(OwningActor);
	if (not GetIsValidNiagaraComponent())
	{
		return;
	}
	M_NiagaraComponent->bAutoActivate = false;
	M_NiagaraComponent->SetAutoDestroy(false);
	M_NiagaraComponent->SetAsset(StrafeImpactSystem);
	M_NiagaraComponent->RegisterComponent();
	M_NiagaraComponent->AddTickPrerequisiteComponent(this);
	for (int32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
	{
		M_NiagaraComponent->SetVariableInt(AircraftStrafeImpactNames::GroupOffset[GroupIndex], 0);
		M_NiagaraComponent->SetVariableInt(AircraftStrafeImpactNames::GroupCount[GroupIndex], 0);
	}
	for (const FAircraftStrafeImpactEmitterSpawn& EmitterSpawn : M_EmitterSpawns)
	{
		M_NiagaraComponent->SetVariableInt(EmitterSpawn.SpawnCountParameter, 0);
	}
	M_NiagaraComponent->Activate(true);
	bM_IsReadyToDraw = true;
}

bool UAircraftStrafeImpactBatcher::GetIsValidNiagaraComponent() const
{
	if (IsValid(M_NiagaraComponent))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_NiagaraComponent",
		"GetIsValidNiagaraComponent",
		this);
	return false;
}

void UAircraftStrafeImpactBatcher::AddImpact(
	const EAircraftStrafeImpactGroup Group,
	const FVector& Location,
	const FVector& Normal,
	const float CalibreMm,
	const EAircraftStrafeImpactSurface Surface)
{
	if (not bM_IsReadyToDraw || M_PendingImpactCount >= M_MaxImpactsPerFrame)
	{
		return;
	}
	const bool bIsSafeLocation = not Location.ContainsNaN() && not Normal.ContainsNaN()
		&& Location.GetAbsMax() < AircraftStrafeImpactNames::MaxSafeWorldCoordinateCm;
	if (not bIsSafeLocation)
	{
		return;
	}
	const int32 GroupIndex = FMath::Clamp(static_cast<int32>(Group), 0, GroupCount - 1);
	M_PendingPositionAndCalibre[GroupIndex].Emplace(
		static_cast<float>(Location.X), static_cast<float>(Location.Y), static_cast<float>(Location.Z), CalibreMm);
	M_PendingNormalAndSurface[GroupIndex].Emplace(
		static_cast<float>(Normal.X), static_cast<float>(Normal.Y), static_cast<float>(Normal.Z),
		static_cast<float>(Surface));
	if (M_PendingImpactCount == 0)
	{
		SetComponentTickEnabled(true);
	}
	++M_PendingImpactCount;
}

void UAircraftStrafeImpactBatcher::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (not GetIsValidNiagaraComponent())
	{
		bM_IsReadyToDraw = false;
		SetComponentTickEnabled(false);
		return;
	}
	if (M_PendingImpactCount > 0)
	{
		PublishPendingImpacts();
		return;
	}
	// Nothing new: the system has spawned last frame's impacts, stop it from spawning them again and go idle.
	ClearPublishedImpacts();
	SetComponentTickEnabled(false);
}

void UAircraftStrafeImpactBatcher::PublishPendingImpacts()
{
	M_PublishedPositionAndCalibre.Reset();
	M_PublishedNormalAndSurface.Reset();
	for (int32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
	{
		const int32 Offset = M_PublishedPositionAndCalibre.Num();
		const int32 Count = M_PendingPositionAndCalibre[GroupIndex].Num();
#if RTS_WITH_MAP_TESTS
		M_TotalPublishedByGroup[GroupIndex] += Count;
#endif
		M_PublishedPositionAndCalibre.Append(M_PendingPositionAndCalibre[GroupIndex]);
		M_PublishedNormalAndSurface.Append(M_PendingNormalAndSurface[GroupIndex]);
		M_PendingPositionAndCalibre[GroupIndex].Reset();
		M_PendingNormalAndSurface[GroupIndex].Reset();
		PublishGroupRange(GroupIndex, Offset, Count);
	}
	M_PendingImpactCount = 0;
	PublishEmitterSpawnCounts();
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector4(
		M_NiagaraComponent, AircraftStrafeImpactNames::PositionAndCalibreArray,
		TConstArrayView<FVector4f>(M_PublishedPositionAndCalibre));
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector4(
		M_NiagaraComponent, AircraftStrafeImpactNames::NormalAndSurfaceArray,
		TConstArrayView<FVector4f>(M_PublishedNormalAndSurface));
}

void UAircraftStrafeImpactBatcher::ClearPublishedImpacts()
{
	for (int32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
	{
		PublishGroupRange(GroupIndex, 0, 0);
	}
	PublishEmitterSpawnCounts();
}

void UAircraftStrafeImpactBatcher::PublishEmitterSpawnCounts()
{
	for (FAircraftStrafeImpactEmitterSpawn& EmitterSpawn : M_EmitterSpawns)
	{
		const int32 SpawnCount = M_PublishedCount[EmitterSpawn.GroupIndex] * EmitterSpawn.ParticlesPerImpact;
		if (EmitterSpawn.PublishedSpawnCount == SpawnCount)
		{
			continue;
		}
		EmitterSpawn.PublishedSpawnCount = SpawnCount;
		M_NiagaraComponent->SetVariableInt(EmitterSpawn.SpawnCountParameter, SpawnCount);
	}
}

void UAircraftStrafeImpactBatcher::PublishGroupRange(const int32 GroupIndex, const int32 Offset, const int32 Count)
{
	if (Count > 0 && M_PublishedOffset[GroupIndex] != Offset)
	{
		M_PublishedOffset[GroupIndex] = Offset;
		M_NiagaraComponent->SetVariableInt(AircraftStrafeImpactNames::GroupOffset[GroupIndex], Offset);
	}
	if (M_PublishedCount[GroupIndex] != Count)
	{
		M_PublishedCount[GroupIndex] = Count;
		M_NiagaraComponent->SetVariableInt(AircraftStrafeImpactNames::GroupCount[GroupIndex], Count);
	}
}

void UAircraftStrafeImpactBatcher::AddBulletPock(const FVector& Location, const FVector& Normal, const float CalibreMm)
{
	using namespace AircraftStrafePockDecals;
	// The player's Shell Craters setting (rts.ImpactDecals) Off turns the pocks off too.
	const bool bAreImpactDecalsOff = ULandscapeImpactDecalSubsystem::GetMaxDecalsSetting() <= 0;
	const URTSImpactDecalSettings* DecalSettings = URTSImpactDecalSettings::Get();
	if (bAreImpactDecalsOff || DecalSettings == nullptr || DecalSettings->Bands.IsEmpty() || Location.ContainsNaN()
		|| Location.GetAbsMax() >= AircraftStrafeImpactNames::MaxSafeWorldCoordinateCm)
	{
		return;
	}
	const int32 Slot = GetNextPockSlot();
	if (not M_PockDecals.IsValidIndex(Slot))
	{
		return;
	}
	UDecalComponent* Decal = M_PockDecals[Slot];
	const FRTSImpactDecalBand& SmallestBand = DecalSettings->Bands[0];
	const int32 Cell = SmallestBand.FirstAtlasCell + FMath::RandRange(0, FMath::Max(1, SmallestBand.AtlasCellCount) - 1);
	// R picks the atlas cell, G varies the look, A is the opacity (see URTSImpactDecalSettings).
	Decal->DecalColor = FLinearColor((Cell + 0.5f) / AtlasCellsTotal, FMath::FRand(), 0.f, 1.f);
	const float Radius = 0.5f * BaseDiameterCm * FMath::Pow(FMath::Max(CalibreMm, 4.f) / 7.92f, 0.6f)
		* FMath::FRandRange(1.f - DiameterJitter, 1.f + DiameterJitter);
	const FVector ProjectionAxis = -(Normal.IsNearlyZero() ? FVector::UpVector : Normal.GetSafeNormal());
	const FQuat Facing = FRotationMatrix::MakeFromX(ProjectionAxis).ToQuat();
	const FQuat Spin(ProjectionAxis, FMath::FRandRange(0.f, UE_TWO_PI));
	Decal->SetWorldTransform(FTransform(Spin * Facing, Location, FVector(ProjectionHalfDepthCm, Radius, Radius)));
	Decal->SetVisibility(true);
	if (not Decal->IsRegistered())
	{
		Decal->RegisterComponent();
	}
	Decal->MarkRenderStateDirty();
	M_PockPlacedTimes[Slot] = GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.f;
	++M_TotalBulletPocks;
#if RTS_WITH_MAP_TESTS
	constexpr int64 PockLogEvery = 40;
	if (M_TotalBulletPocks % PockLogEvery == 1)
	{
		UE_LOG(LogTemp, Display, TEXT("RTS_STRAFE_POCK n=%lld slot=%d at=%s normal=%s radius=%.0f registered=%d visible=%d t=%.1f"),
		       M_TotalBulletPocks, Slot, *Location.ToCompactString(), *Normal.ToCompactString(), Radius,
		       Decal->IsRegistered() ? 1 : 0, Decal->IsVisible() ? 1 : 0, M_PockPlacedTimes[Slot]);
	}
#endif
	StartPockFadeTimerIfNeeded();
}

int32 UAircraftStrafeImpactBatcher::GetNextPockSlot()
{
	using namespace AircraftStrafePockDecals;
	AActor* OwningActor = GetOwner();
	if (not IsValid(OwningActor))
	{
		return INDEX_NONE;
	}
	if (not bM_HasTriedLoadingPockMaterial)
	{
		bM_HasTriedLoadingPockMaterial = true;
		const URTSImpactDecalSettings* DecalSettings = URTSImpactDecalSettings::Get();
		M_PockMaterial = DecalSettings != nullptr ? DecalSettings->DecalMaterial.LoadSynchronous() : nullptr;
	}
	if (not IsValid(M_PockMaterial))
	{
		return INDEX_NONE;
	}
	if (M_PockDecals.Num() < RingSize)
	{
		M_PockPlacedTimes.Add(0.f);
		return M_PockDecals.Add(CreatePockDecal(*OwningActor));
	}
	const int32 Slot = M_NextPockSlot;
	M_NextPockSlot = (M_NextPockSlot + 1) % RingSize;
	if (not IsValid(M_PockDecals[Slot]))
	{
		// Destroyed by something else (level streaming, a cleanup): replace it instead of losing the slot.
		M_PockDecals[Slot] = CreatePockDecal(*OwningActor);
	}
	return Slot;
}

UDecalComponent* UAircraftStrafeImpactBatcher::CreatePockDecal(AActor& OwningActor)
{
	UDecalComponent* Decal = NewObject<UDecalComponent>(&OwningActor, NAME_None, RF_Transient);
	Decal->SetDecalMaterial(M_PockMaterial);
	Decal->DecalSize = FVector::OneVector;
	// Small decals vanish at RTS zoom with the default fade screen size.
	Decal->FadeScreenSize = 0.f;
	Decal->SetUsingAbsoluteLocation(true);
	Decal->SetUsingAbsoluteRotation(true);
	Decal->SetUsingAbsoluteScale(true);
	Decal->SetMobility(EComponentMobility::Movable);
	Decal->SetupAttachment(OwningActor.GetRootComponent());
	return Decal;
}

void UAircraftStrafeImpactBatcher::StartPockFadeTimerIfNeeded()
{
	UWorld* World = GetWorld();
	if (World == nullptr || World->GetTimerManager().IsTimerActive(M_PockFadeTimerHandle))
	{
		return;
	}
	World->GetTimerManager().SetTimer(M_PockFadeTimerHandle,
	                                  FTimerDelegate::CreateUObject(this, &UAircraftStrafeImpactBatcher::TickPockFades),
	                                  AircraftStrafePockDecals::FadeTickSeconds, true);
}

void UAircraftStrafeImpactBatcher::TickPockFades()
{
	using namespace AircraftStrafePockDecals;
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	bool bAnyVisible = false;
	for (int32 Slot = 0; Slot < M_PockDecals.Num(); ++Slot)
	{
		UDecalComponent* Decal = M_PockDecals[Slot];
		if (not IsValid(Decal) || not Decal->IsVisible())
		{
			continue;
		}
		const float Age = Now - M_PockPlacedTimes[Slot];
		if (Age >= HoldSeconds + FadeSeconds)
		{
			Decal->SetVisibility(false);
			continue;
		}
		bAnyVisible = true;
		if (Age <= HoldSeconds)
		{
			continue;
		}
		FLinearColor Color = Decal->DecalColor;
		Color.A = 1.f - (Age - HoldSeconds) / FadeSeconds;
		Decal->SetDecalColor(Color);
	}
	if (not bAnyVisible)
	{
		World->GetTimerManager().ClearTimer(M_PockFadeTimerHandle);
	}
}

#if RTS_WITH_MAP_TESTS
int64 UAircraftStrafeImpactBatcher::GetTotalPublishedImpacts(const EAircraftStrafeImpactGroup Group) const
{
	const int32 GroupIndex = FMath::Clamp(static_cast<int32>(Group), 0, GroupCount - 1);
	return M_TotalPublishedByGroup[GroupIndex] + M_PendingPositionAndCalibre[GroupIndex].Num();
}

int32 UAircraftStrafeImpactBatcher::GetEmitterCountWithoutSpawns() const
{
	if (not IsValid(M_NiagaraComponent))
	{
		return -1;
	}
	const FNiagaraSystemInstanceControllerConstPtr InstanceController = M_NiagaraComponent->
		GetSystemInstanceController();
	const FNiagaraSystemInstance* SystemInstance = InstanceController.IsValid()
		                                               ? InstanceController->GetSystemInstance_Unsafe()
		                                               : nullptr;
	if (SystemInstance == nullptr)
	{
		return -1;
	}
	int32 EmittersWithoutSpawns = 0;
	for (const FNiagaraEmitterInstanceRef& EmitterInstance : SystemInstance->GetEmitters())
	{
		EmittersWithoutSpawns += EmitterInstance->GetTotalSpawnedParticles() == 0 ? 1 : 0;
	}
	return EmittersWithoutSpawns;
}

int32 UAircraftStrafeImpactBatcher::GetLiveParticleCount() const
{
	if (not IsValid(M_NiagaraComponent))
	{
		return 0;
	}
	const FNiagaraSystemInstanceControllerConstPtr InstanceController = M_NiagaraComponent->
		GetSystemInstanceController();
	const FNiagaraSystemInstance* SystemInstance = InstanceController.IsValid()
		                                               ? InstanceController->GetSystemInstance_Unsafe()
		                                               : nullptr;
	if (SystemInstance == nullptr)
	{
		return 0;
	}
	int32 LiveParticles = 0;
	for (const FNiagaraEmitterInstanceRef& EmitterInstance : SystemInstance->GetEmitters())
	{
		LiveParticles += EmitterInstance->GetNumParticles();
	}
	return LiveParticles;
}

FString UAircraftStrafeImpactBatcher::DescribeParticleSamples() const
{
	constexpr int32 SamplesPerEmitter = 2;
	if (not IsValid(M_NiagaraComponent))
	{
		return TEXT("no component");
	}
	const FNiagaraSystemInstanceControllerConstPtr InstanceController = M_NiagaraComponent->
		GetSystemInstanceController();
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
		const FNiagaraDataSet& ParticleData = EmitterInstance->GetParticleData();
		const FNiagaraDataBuffer* Buffer = ParticleData.GetCurrentData();
		const int32 Count = Buffer != nullptr ? static_cast<int32>(Buffer->GetNumInstances()) : 0;
		Description += FString::Printf(TEXT("[%s n=%d"), *EmitterInstance->GetEmitterHandle().GetName().ToString(), Count);
		FNiagaraDataSetAccessor<FNiagaraPosition> PositionAccessor;
		PositionAccessor.Init(ParticleData.GetCompiledData(), TEXT("Position"));
		FNiagaraDataSetAccessor<FVector2f> SizeAccessor;
		SizeAccessor.Init(ParticleData.GetCompiledData(), TEXT("SpriteSize"));
		FNiagaraDataSetAccessor<FLinearColor> ColorAccessor;
		ColorAccessor.Init(ParticleData.GetCompiledData(), TEXT("Color"));
		const auto PositionReader = PositionAccessor.GetReader(ParticleData);
		const auto SizeReader = SizeAccessor.GetReader(ParticleData);
		const auto ColorReader = ColorAccessor.GetReader(ParticleData);
		for (int32 Index = 0; Index < FMath::Min(Count, SamplesPerEmitter); ++Index)
		{
			const FVector3f Position = PositionReader.IsValid() ? FVector3f(PositionReader.Get(Index)) : FVector3f::ZeroVector;
			const FVector2f Size = SizeReader.IsValid() ? SizeReader.Get(Index) : FVector2f::ZeroVector;
			const float Alpha = ColorReader.IsValid() ? ColorReader.Get(Index).A : -1.f;
			Description += FString::Printf(TEXT(" pos=(%.0f,%.0f,%.0f) size=(%.0f,%.0f) a=%.2f"),
			                               Position.X, Position.Y, Position.Z, Size.X, Size.Y, Alpha);
		}
		Description += TEXT("] ");
	}
	return Description;
}

FString UAircraftStrafeImpactBatcher::DescribeEmitterParticleCounts() const
{
	if (not IsValid(M_NiagaraComponent))
	{
		return TEXT("no component");
	}
	const FNiagaraSystemInstanceControllerConstPtr InstanceController = M_NiagaraComponent->
		GetSystemInstanceController();
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
#endif
