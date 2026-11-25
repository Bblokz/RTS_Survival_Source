// Copyright (C) Bas Blokzijl - All rights reserved.

#include "TankShellTrailBatcher.h"

#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/Projectile/Projectile.h"
#include "RTS_Survival/Weapons/RTSWeaponVFXSettings/RTSWeaponVFXSettings.h"
#include "RTS_Survival/Weapons/SmallArmsProjectileManager/SmallArmsProjectileManager.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponShellType/WeaponShellType.h"

namespace TankShellTrailNames
{
	// User parameters of the tank shell trail Niagara system; the same contract as the small arms muzzle system.
	const FName PositionAndScaleArray = TEXT("ShotPositionAndScale");
	const FName DirectionArray = TEXT("ShotDirection");

	const FName GroupOffset[] = {TEXT("TracerOffset"), TEXT("GlowOffset"), TEXT("SmokeOffset")};
	const FName GroupCountName[] = {TEXT("TracerCount"), TEXT("GlowCount"), TEXT("SmokeCount")};
	const TCHAR* GroupPrefix[] = {TEXT("Tracer_"), TEXT("Glow_"), TEXT("Smoke_")};
	const TCHAR* PerShotSuffix = TEXT("_PerShot");
	const TCHAR* SpawnCountSuffix = TEXT("_SpawnCount");
}

namespace TankShellTrailTuning
{
	// Distance between smoke samples for a 75 mm shell; the puffs overlap at this spacing without stacking overdraw.
	constexpr float SmokeSpacingCm = 30.0f;
	// Distance between tracer samples for a 75 mm shell; below the sprite width so the streak has no beads.
	constexpr float TracerSpacingCm = 7.0f;
	constexpr float ReferenceCalibreMm = 75.0f;
	constexpr float MinCalibreScale = 0.35f;
	constexpr float MaxCalibreScale = 2.6f;
	// The spacing never follows a small calibre all the way down, or autocannons spawn far too many samples.
	constexpr float MinSpacingScale = 0.6f;
	// A shell that moved less than this did not fly; one that moved more was moved by the pool, not by flight.
	constexpr float MinSegmentCm = 1.0f;
	constexpr float MaxSegmentCm = 6000.0f;
	constexpr int32 MaxSamplesPerSegment = 96;
	// The HLSL reads kind = floor(w / 10) and scale = w - kind * 10.
	constexpr float KindPackingStep = 10.0f;
	constexpr double MaxSafeWorldCoordinateCm = 10000000.0;

	float GetShellKind(const EWeaponShellType ShellType)
	{
		switch (ShellType)
		{
		case EWeaponShellType::Shell_HE:
			return 1.0f;
		case EWeaponShellType::Shell_HEAT:
			return 2.0f;
		case EWeaponShellType::Shell_APCR:
			return 3.0f;
		case EWeaponShellType::Shell_Fire:
			return 4.0f;
		default:
			return 0.0f;
		}
	}
}

UTankShellTrailBatcher::UTankShellTrailBatcher()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	for (int32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
	{
		M_PublishedOffset[GroupIndex] = 0;
		M_PublishedCount[GroupIndex] = 0;
	}
}

void UTankShellTrailBatcher::BeginPlay()
{
	Super::BeginPlay();
	const URTSWeaponVFXSettings* VfxSettings = URTSWeaponVFXSettings::Get();
	if (VfxSettings == nullptr || not VfxSettings->bUseTankShellTrailVfx)
	{
		return;
	}
	UNiagaraSystem* TrailSystem = VfxSettings->TankShellTrailNiagaraSystem.LoadSynchronous();
	if (not IsValid(TrailSystem))
	{
		RTSFunctionLibrary::ReportError(
			"The tank shell trail Niagara system set in the RTS Weapon VFX project settings could not be loaded: "
			+ VfxSettings->TankShellTrailNiagaraSystem.ToString()
			+ "\n Tank shells fall back to the shell effect of each projectile.");
		return;
	}
	M_MaxSamplesPerFrame = FMath::Max(1, VfxSettings->MaxTankShellTrailSamplesPerFrame);
	BeginPlay_ReadEmitterSpawns(*TrailSystem);
	BeginPlay_CreateNiagaraComponent(TrailSystem);
	UE_LOG(LogTemp, Display, TEXT("RTS_TANK_SHELL_TRAIL ready=%d emitters=%d"),
	       bM_IsReadyToDraw ? 1 : 0, M_EmitterSpawns.Num());
}

void UTankShellTrailBatcher::BeginPlay_ReadEmitterSpawns(const UNiagaraSystem& TrailSystem)
{
	using namespace TankShellTrailNames;
	const FNiagaraUserRedirectionParameterStore& UserParameterStore = TrailSystem.GetExposedParameters();
	TArray<FNiagaraVariable> UserParameters;
	UserParameterStore.GetUserParameters(UserParameters);
	for (const FNiagaraVariable& UserParameter : UserParameters)
	{
		const FString ParameterName = UserParameter.GetName().ToString();
		if (UserParameter.GetType() != FNiagaraTypeDefinition::GetIntDef() || not ParameterName.EndsWith(PerShotSuffix))
		{
			continue;
		}
		const FString EmitterName = ParameterName.LeftChop(FCString::Strlen(PerShotSuffix));
		for (int32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
		{
			if (not EmitterName.StartsWith(GroupPrefix[GroupIndex]))
			{
				continue;
			}
			const FNiagaraVariable StoredParameter(
				FNiagaraTypeDefinition::GetIntDef(), FName(*(TEXT("User.") + ParameterName)));
			FTankShellTrailEmitterSpawn EmitterSpawn;
			EmitterSpawn.GroupIndex = GroupIndex;
			EmitterSpawn.SpawnCountParameter = FName(*(EmitterName + SpawnCountSuffix));
			EmitterSpawn.ParticlesPerSample = FMath::Max(1, UserParameterStore.GetParameterValue<int32>(StoredParameter));
			M_EmitterSpawns.Add(EmitterSpawn);
			break;
		}
	}
}

void UTankShellTrailBatcher::BeginPlay_CreateNiagaraComponent(UNiagaraSystem* TrailSystem)
{
	AActor* OwningActor = GetOwner();
	if (not IsValid(OwningActor))
	{
		return;
	}
	if (M_EmitterSpawns.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(
			"The tank shell trail Niagara system has no Tracer_/Glow_/Smoke_ emitters; build it with the console"
			" command RTS.TankShellTrail.BuildNiagaraSystem. Tank shells fall back to the shell effect of each projectile.");
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
	M_NiagaraComponent->SetCastShadow(false);
	M_NiagaraComponent->SetAsset(TrailSystem);
	M_NiagaraComponent->RegisterComponent();
	M_NiagaraComponent->AddTickPrerequisiteComponent(this);
	for (int32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
	{
		M_NiagaraComponent->SetVariableInt(TankShellTrailNames::GroupOffset[GroupIndex], 0);
		M_NiagaraComponent->SetVariableInt(TankShellTrailNames::GroupCountName[GroupIndex], 0);
	}
	for (const FTankShellTrailEmitterSpawn& EmitterSpawn : M_EmitterSpawns)
	{
		M_NiagaraComponent->SetVariableInt(EmitterSpawn.SpawnCountParameter, 0);
	}
	M_NiagaraComponent->Activate(true);
	bM_IsReadyToDraw = true;
	SetComponentTickEnabled(true);
}

bool UTankShellTrailBatcher::GetIsValidNiagaraComponent() const
{
	if (IsValid(M_NiagaraComponent))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_NiagaraComponent",
		"GetIsValidNiagaraComponent",
		this
	);

	return false;
}

void UTankShellTrailBatcher::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const ASmallArmsProjectileManager* Manager = Cast<ASmallArmsProjectileManager>(GetOwner());
	if (Manager == nullptr || not GetIsValidNiagaraComponent())
	{
		bM_IsReadyToDraw = false;
		SetComponentTickEnabled(false);
		return;
	}
	if (GatherSamples(*Manager, DeltaTime) > 0)
	{
		PublishPendingSamples();
		return;
	}
	if (bM_HasPublishedSamples)
	{
		// Nothing in flight: stop the system from spawning last frame's samples again.
		ClearPublishedSamples();
	}
}

int32 UTankShellTrailBatcher::GatherSamples(const ASmallArmsProjectileManager& Manager, const float DeltaTime)
{
	const FSoA_TankProjectilesPool& Pool = Manager.GetTankProjectilesPool();
	const int32 SlotCount = FMath::Min(Pool.Projectiles.Num(), Pool.bIsActive.Num());
	if (M_Slots.Num() < SlotCount)
	{
		M_Slots.SetNum(SlotCount);
	}
	int32 SampleCount = 0;
	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		FTankShellTrailSlot& Slot = M_Slots[SlotIndex];
		const AProjectile* Projectile = Pool.Projectiles[SlotIndex];
		const bool bIsFlyingTankShell = Pool.bIsActive[SlotIndex] && IsValid(Projectile)
			&& Projectile->GetIsDrawnByTankShellTrailBatcher();
		if (not bIsFlyingTankShell || SampleCount >= M_MaxSamplesPerFrame)
		{
			Slot.bIsTracked = false;
			continue;
		}
		SampleCount += GatherShellSamples(*Projectile, Slot, DeltaTime);
	}
	return SampleCount;
}

int32 UTankShellTrailBatcher::GatherShellSamples(
	const AProjectile& Projectile,
	FTankShellTrailSlot& Slot,
	const float DeltaTime)
{
	using namespace TankShellTrailTuning;
	const FVector CurrentLocation = Projectile.GetActorLocation();
	const uint32 LaunchSerial = Projectile.GetLaunchSerial();
	if (not Slot.bIsTracked || Slot.LaunchSerial != LaunchSerial)
	{
		// A new launch (also of a projectile reused within one frame): the trail starts at the muzzle.
		Slot.bIsTracked = true;
		Slot.LaunchSerial = LaunchSerial;
		Slot.PreviousLocation = Projectile.GetLaunchLocation();
		Slot.TracerCarry = 0.0f;
		Slot.SmokeCarry = 0.0f;
	}
	const FVector Segment = CurrentLocation - Slot.PreviousLocation;
	const float Length = static_cast<float>(Segment.Size());
	const bool bIsSafeLocation = not CurrentLocation.ContainsNaN() && CurrentLocation.GetAbsMax() < MaxSafeWorldCoordinateCm;
	if (Length < MinSegmentCm || not bIsSafeLocation)
	{
		return 0;
	}
	const FVector StartLocation = Slot.PreviousLocation;
	Slot.PreviousLocation = CurrentLocation;
	if (Length > MaxSegmentCm)
	{
		return 0;
	}
	const FProjectileVfxSettings& VfxSettings = Projectile.GetProjectileVfxSettings();
	const float CalibreScale = FMath::Clamp(VfxSettings.WeaponCaliber / ReferenceCalibreMm, MinCalibreScale, MaxCalibreScale);
	const float PackedKindAndScale = GetShellKind(VfxSettings.ShellType) * KindPackingStep + CalibreScale;
	const float SpacingScale = FMath::Max(CalibreScale, MinSpacingScale);
	const FVector Direction = Segment / Length;

	int32 SampleCount = AddSegmentSamples(ETankShellTrailGroup::Tracer, StartLocation, Direction, Length,
	                                      TracerSpacingCm * SpacingScale, PackedKindAndScale, DeltaTime, Slot.TracerCarry);
	SampleCount += AddSegmentSamples(ETankShellTrailGroup::Smoke, StartLocation, Direction, Length,
	                                 SmokeSpacingCm * SpacingScale, PackedKindAndScale, DeltaTime, Slot.SmokeCarry);
	AddSample(ETankShellTrailGroup::Glow, CurrentLocation, Direction, PackedKindAndScale, 0.0f);
	return SampleCount + 1;
}

int32 UTankShellTrailBatcher::AddSegmentSamples(
	const ETankShellTrailGroup Group,
	const FVector& Start,
	const FVector& Direction,
	const float Length,
	const float Spacing,
	const float PackedKindAndScale,
	const float DeltaTime,
	float& InOutCarry)
{
	// Never more than the cap on one segment: a very fast shell gets wider gaps instead of more samples.
	const float SafeSpacing = FMath::Max(Spacing, Length / TankShellTrailTuning::MaxSamplesPerSegment);
	int32 SampleCount = 0;
	float DistanceAlongSegment = SafeSpacing - InOutCarry;
	while (DistanceAlongSegment <= Length)
	{
		// The shell passed this point a part of a frame ago; the HLSL ages the particle by it so the streak fades evenly.
		const float AgeOffsetSeconds = (1.0f - DistanceAlongSegment / Length) * DeltaTime;
		AddSample(Group, Start + Direction * DistanceAlongSegment, Direction, PackedKindAndScale, AgeOffsetSeconds);
		++SampleCount;
		DistanceAlongSegment += SafeSpacing;
	}
	InOutCarry = FMath::Clamp(Length - (DistanceAlongSegment - SafeSpacing), 0.0f, SafeSpacing);
	return SampleCount;
}

void UTankShellTrailBatcher::AddSample(
	const ETankShellTrailGroup Group,
	const FVector& Location,
	const FVector& Direction,
	const float PackedKindAndScale,
	const float AgeOffsetSeconds)
{
	const int32 GroupIndex = static_cast<int32>(Group);
	M_PendingPositionAndScale[GroupIndex].Emplace(
		static_cast<float>(Location.X), static_cast<float>(Location.Y), static_cast<float>(Location.Z), PackedKindAndScale);
	M_PendingDirection[GroupIndex].Emplace(
		static_cast<float>(Direction.X), static_cast<float>(Direction.Y), static_cast<float>(Direction.Z), AgeOffsetSeconds);
}

void UTankShellTrailBatcher::PublishPendingSamples()
{
	M_PublishedPositionAndScale.Reset();
	M_PublishedDirection.Reset();
	for (int32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
	{
		const int32 Offset = M_PublishedPositionAndScale.Num();
		const int32 Count = M_PendingPositionAndScale[GroupIndex].Num();
		M_PublishedPositionAndScale.Append(M_PendingPositionAndScale[GroupIndex]);
		M_PublishedDirection.Append(M_PendingDirection[GroupIndex]);
		// Reset keeps the memory, so a fight does not allocate every frame.
		M_PendingPositionAndScale[GroupIndex].Reset();
		M_PendingDirection[GroupIndex].Reset();
		PublishGroupRange(GroupIndex, Offset, Count);
	}
	PublishEmitterSpawnCounts();
	bM_HasPublishedSamples = true;

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector4(
		M_NiagaraComponent,
		TankShellTrailNames::PositionAndScaleArray,
		TConstArrayView<FVector4f>(M_PublishedPositionAndScale));
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector4(
		M_NiagaraComponent,
		TankShellTrailNames::DirectionArray,
		TConstArrayView<FVector4f>(M_PublishedDirection));
}

void UTankShellTrailBatcher::ClearPublishedSamples()
{
	for (int32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
	{
		PublishGroupRange(GroupIndex, 0, 0);
	}
	PublishEmitterSpawnCounts();
	bM_HasPublishedSamples = false;
}

void UTankShellTrailBatcher::PublishEmitterSpawnCounts()
{
	for (FTankShellTrailEmitterSpawn& EmitterSpawn : M_EmitterSpawns)
	{
		const int32 SpawnCount = M_PublishedCount[EmitterSpawn.GroupIndex] * EmitterSpawn.ParticlesPerSample;
		if (EmitterSpawn.PublishedSpawnCount == SpawnCount)
		{
			continue;
		}
		EmitterSpawn.PublishedSpawnCount = SpawnCount;
		M_NiagaraComponent->SetVariableInt(EmitterSpawn.SpawnCountParameter, SpawnCount);
	}
}

void UTankShellTrailBatcher::PublishGroupRange(const int32 GroupIndex, const int32 Offset, const int32 Count)
{
	if (Count > 0 && M_PublishedOffset[GroupIndex] != Offset)
	{
		M_PublishedOffset[GroupIndex] = Offset;
		M_NiagaraComponent->SetVariableInt(TankShellTrailNames::GroupOffset[GroupIndex], Offset);
	}
	if (M_PublishedCount[GroupIndex] != Count)
	{
		M_PublishedCount[GroupIndex] = Count;
		M_NiagaraComponent->SetVariableInt(TankShellTrailNames::GroupCountName[GroupIndex], Count);
	}
}
