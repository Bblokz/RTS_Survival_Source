// Copyright (c) Bas Blokzijl. All rights reserved.
#include "WeaponData.h"

#include "NiagaraComponent.h"
#include "WeaponSystems.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/CPPWeaponsMaster.h"
#include "RTS_Survival/Weapons/Projectile/Projectile.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/AudioComponent.h"
#include "FRTSWeaponHelpers/FRTSWeaponHelpers.h"
#include "Sound/SoundCue.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/Physics/FRTS_PhysicsHelper.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/ArmorCalculation.h"
#include "RTS_Survival/RTSComponents/ArmorComponent/Armor.h"
#include "RTS_Survival/Weapons/RTSWeaponVFXSettings/RTSWeaponVFXSettings.h"
#include "RTS_Survival/Weapons/SmallArmsProjectileManager/SmallArmsProjectileManager.h"
#include "Trace/Trace.h"
#include "WeaponOwner/WeaponOwner.h"

// === Global shell-color cache ================================================

namespace BounceVfx
{
	// Used to scale the HE bounce VFX for larger calibres.
	FName ScaleMltName = "Scale";

	inline void ApplyScaleToHeHeatBounce(UNiagaraComponent* System, const float ScaleMlt)
	{
		System->SetFloatParameter(ScaleMltName, ScaleMlt);
	}
}

namespace LaunchVfx
{
	// Runtime cache shared by all weapons.
	TMap<EWeaponShellType, FLinearColor> G_ShellTypeColors;
	TMap<EWeaponShellType, FLinearColor> G_ShellTypeGroundSmokeColors;
	bool bG_ShellTypeColorsInitialized = false;
	float BaseLightIntensity = 2000;

	FName LifeTimeName = "LifetimeMlt";
	FName LightMltName = "LightMlt";
	FName WeaponCalibreName = "NormalizedWeaponCalibre";
	FName WeaponShellColorName = "MuzzleColor";
	FName UseMuzzleSmokeName = "UseMuzzleSmoke";
	FName UseGroundSmokeName = "UseGroundSmoke";
	FName GroundSmokeColor = "GroundSmokeColor";
	FName GroundZOffsetName = "GroundZOffset";
	FName DirectSizeMltName = "SizeMlt";

	inline void InitShellColorsFromSettings_IfNeeded()
	{
		if (bG_ShellTypeColorsInitialized)
		{
			return;
		}
		const URTSWeaponVFXSettings* const Settings = URTSWeaponVFXSettings::Get();
		if (Settings == nullptr)
		{
			RTSFunctionLibrary::ReportError(TEXT("GLOBAL shell color cache: URTSWeaponVFXSettings missing."));
			bG_ShellTypeColorsInitialized = true; // Prevent retry-spam; can be refreshed explicitly later.
			return;
		}
		G_ShellTypeColors = Settings->ShellTypeColors; // copy once
		G_ShellTypeGroundSmokeColors = Settings->ShellTypeGroundSmokeColors;
		bG_ShellTypeColorsInitialized = true;
	}
}

const TMap<EWeaponShellType, FLinearColor>& GLOBAL_GetShellColorMap()
{
	LaunchVfx::InitShellColorsFromSettings_IfNeeded();
	return LaunchVfx::G_ShellTypeColors;
}

const TMap<EWeaponShellType, FLinearColor>& GLOBAL_GetShellGroundSmokeColorMap()
{
	LaunchVfx::InitShellColorsFromSettings_IfNeeded();
	return LaunchVfx::G_ShellTypeGroundSmokeColors;
}

FLinearColor GLOBAL_GetShellColor(const EWeaponShellType ShellType)
{
	LaunchVfx::InitShellColorsFromSettings_IfNeeded();
	if (const FLinearColor* const Found = LaunchVfx::G_ShellTypeColors.Find(ShellType))
	{
		return *Found;
	}
	return FLinearColor::Red;
}

FLinearColor GLOBAL_GetShellGroundSmokeColor(const EWeaponShellType ShellType)
{
	if (const FLinearColor* const Found = LaunchVfx::G_ShellTypeGroundSmokeColors.Find(ShellType))
	{
		return *Found;
	}
	return FLinearColor::Red;
}

void GLOBAL_RefreshShellColorsFromSettings()
{
	LaunchVfx::bG_ShellTypeColorsInitialized = false;
	LaunchVfx::InitShellColorsFromSettings_IfNeeded();
}

void FWeaponData::CopyWeaponDataValues(const FWeaponData* const WeaponData)
{
	if (!WeaponData)
	{
		return;
	}

	DamageType = WeaponData->DamageType;
	WeaponCalibre = WeaponData->WeaponCalibre;
	ShellType = WeaponData->ShellType;
	ShellTypes = WeaponData->ShellTypes;
	WeaponName = WeaponData->WeaponName;
	TNTExplosiveGrams = WeaponData->TNTExplosiveGrams;
	BaseDamage = WeaponData->BaseDamage;
	DamageFlux = WeaponData->DamageFlux;
	Range = WeaponData->Range;
	ArmorPen = WeaponData->ArmorPen;
	ArmorPenMaxRange = WeaponData->ArmorPenMaxRange;
	MagCapacity = WeaponData->MagCapacity;
	ReloadSpeed = WeaponData->ReloadSpeed;
	BaseCooldown = WeaponData->BaseCooldown;
	CooldownFlux = WeaponData->CooldownFlux;
	Accuracy = WeaponData->Accuracy;
	ShrapnelRange = WeaponData->ShrapnelRange;
	ShrapnelDamage = WeaponData->ShrapnelDamage;
	ShrapnelParticles = WeaponData->ShrapnelParticles;
	ShrapnelPen = WeaponData->ShrapnelPen;
	ProjectileMovementSpeed = WeaponData->ProjectileMovementSpeed; // ← add
	BehaviourAttributes = WeaponData->BehaviourAttributes;
}

bool FLaunchEffectSettings::HasLaunchSettings() const
{
	return UseLaunchSettings != EWeaponLaunchSettingsType::None;
}


void FShellVfxOverwrites::HandleWeaponShellChange(const EWeaponShellType OldType, const EWeaponShellType NewType,
                                                  USoundCue*& OutBounceSound, UNiagaraSystem*& OutBounceVfx,
                                                  TMap<ERTSSurfaceType, FRTSSurfaceImpactData>& OutImpacts)
{
	if (OldType == NewType)
	{
		return;
	}
	const bool bIsOldAPType = UsesAPImpacts(OldType);
	const bool bIsNewAPType = UsesAPImpacts(NewType);
	if (bIsOldAPType && bIsNewAPType)
	{
		return;
	}
	if (not bIsApStored)
	{
		if (bIsOldAPType)
		{
			M_CachedAPImpacts = OutImpacts;
			M_CachedAPBounce.ImpactEffect = OutBounceVfx;
			M_CachedAPBounce.ImpactSound = OutBounceSound;
			bIsApStored = true;
		}
	}
	if (not bIsOldAPType)
	{
		if (not bIsApStored)
		{
			RTSFunctionLibrary::ReportError("A shell type was not set to AP but also the AP effects were never stored"
				"this is unexpected as we assume that weapons always start with AP and then switch to something else"
				"and that weapons that start with something else are not able to switch at all");
			return;
		}
		// Something before was not AP; restore the effects as if we are now changing from AP to something else to not
		// lose data.
		RestoreImpactsWithStoredAP(OutBounceSound, OutBounceVfx, OutImpacts);
	}
	// Before was AP or is restored to AP; everything is set to AP impacts.
	if (GetShellUsesHeImpacts(NewType) && HasAnyHeAndHeatOverwrites())
	{
		SetOutEffectsToUseHeAndHeat(OutImpacts);
		SetBounceToHeAndHeat(OutBounceSound, OutBounceVfx);
		return;
	}
}

bool FShellVfxOverwrites::UsesAPImpacts(const EWeaponShellType Type) const
{
	return Type == EWeaponShellType::Shell_AP
		|| Type == EWeaponShellType::Shell_APHE
		|| Type == EWeaponShellType::Shell_APHEBC;
}

bool FShellVfxOverwrites::GetShellUsesHeImpacts(const EWeaponShellType ShellType) const
{
	return ShellType == EWeaponShellType::Shell_HE
		|| ShellType == EWeaponShellType::Shell_HEAT;
}


bool FShellVfxOverwrites::HasAnyHeAndHeatOverwrites() const
{
	return !HeAndHeat_ImpactOverwriteData.IsEmpty();
}


void FShellVfxOverwrites::SetOutEffectsToUseHeAndHeat(
	TMap<ERTSSurfaceType, FRTSSurfaceImpactData>& OutImpacts) const
{
	if (!HasAnyHeAndHeatOverwrites())
	{
		return;
	}

	for (const TPair<ERTSSurfaceType, FRTSSurfaceImpactData>& Pair : HeAndHeat_ImpactOverwriteData)
	{
		const ERTSSurfaceType SurfaceType = Pair.Key;
		const FRTSSurfaceImpactData& OverwriteData = Pair.Value;
		OutImpacts.FindOrAdd(SurfaceType) = OverwriteData;
	}
}

void FShellVfxOverwrites::SetBounceToHeAndHeat(
	USoundCue*& BounceSound,
	UNiagaraSystem*& BounceSystem)
{
	if (!IsValid(HeAndHeat_BounceOverWriteData.ImpactEffect)
		&& !IsValid(HeAndHeat_BounceOverWriteData.ImpactSound))
	{
		return;
	}

	BounceSound = HeAndHeat_BounceOverWriteData.ImpactSound;
	BounceSystem = HeAndHeat_BounceOverWriteData.ImpactEffect;
	bIsUsingHeBounceOverwrite = true;
}

void FShellVfxOverwrites::RestoreImpactsWithStoredAP(
	USoundCue*& OutBounceSound,
	UNiagaraSystem*& OutBounceVfx,
	TMap<ERTSSurfaceType, FRTSSurfaceImpactData>& OutImpacts)
{
	OutBounceSound = M_CachedAPBounce.ImpactSound;
	OutBounceVfx = M_CachedAPBounce.ImpactEffect;
	OutImpacts = M_CachedAPImpacts;
	bIsUsingHeBounceOverwrite = false;
}


FWeaponData GLOBAL_GetWeaponDataForShellType(const FWeaponData& OldWeaponData)
{
	FWeaponData NewWeaponData = OldWeaponData;

	switch (OldWeaponData.ShellType)
	{
	case EWeaponShellType::Shell_AP:
	case EWeaponShellType::Shell_APHE:
		// No change needed for AP and APHE, return the original data.
		return NewWeaponData;

	case EWeaponShellType::Shell_APHEBC:
		// Adjust ArmorPenMaxRange to be closer to ArmorPen.
		NewWeaponData.ArmorPenMaxRange = FMath::Lerp(NewWeaponData.ArmorPen,
		                                             NewWeaponData.ArmorPenMaxRange,
		                                             DeveloperSettings::GameBalance::Weapons::Projectiles::APHEBC_ArmorPenLerpFactor);
		break;

	case EWeaponShellType::Shell_HE:
		// Adjust damage and armor penetration for HE shells.
		NewWeaponData.ArmorPen *= DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ArmorPenMlt;
		NewWeaponData.ArmorPenMaxRange = NewWeaponData.ArmorPen;
		NewWeaponData.TNTExplosiveGrams *=
			DeveloperSettings::GameBalance::Weapons::Projectiles::HE_TNTExplosiveGramsMlt;
		NewWeaponData.BaseDamage *= DeveloperSettings::GameBalance::Weapons::Projectiles::HE_DamageMlt;
		NewWeaponData.ShrapnelDamage *= DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ShrapnelDamageMlt;
		NewWeaponData.ShrapnelRange *= DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ShrapnelRangeMlt;
		NewWeaponData.ShrapnelPen *= DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ShrapnelPenMlt;
		NewWeaponData.ShrapnelParticles *=
			DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ShrapnelParticlesMlt;
		NewWeaponData.ProjectileMovementSpeed *=
			DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ProjectileSpeedMlt;

		break;

	case EWeaponShellType::Shell_HEAT:
		// Adjust armor penetration for HEAT shells and equalize min and max range pen.
		NewWeaponData.ArmorPen *= DeveloperSettings::GameBalance::Weapons::Projectiles::HEAT_ArmorPenMlt;
		NewWeaponData.ArmorPenMaxRange = NewWeaponData.ArmorPen;
		NewWeaponData.ProjectileMovementSpeed *=
			DeveloperSettings::GameBalance::Weapons::Projectiles::HEAT_ProjectileSpeedMlt;
		break;

	case EWeaponShellType::Shell_Radixite:
		// Adjust damage and armor penetration at max range for Radixite shells.
		NewWeaponData.BaseDamage *= DeveloperSettings::GameBalance::Weapons::Projectiles::Radixite_DamageMlt;
		NewWeaponData.ArmorPenMaxRange *=
			DeveloperSettings::GameBalance::Weapons::Projectiles::Radixite_ArmorPenMaxRangeMlt;
		NewWeaponData.Accuracy += DeveloperSettings::GameBalance::Weapons::Projectiles::Radixite_AccuracyDelta;
		break;

	case EWeaponShellType::Shell_APCR:
		// Adjust damage and armor penetration for APCR shells.
		NewWeaponData.ArmorPen *= DeveloperSettings::GameBalance::Weapons::Projectiles::APCR_ArmorPenMlt;
		NewWeaponData.ArmorPenMaxRange *= DeveloperSettings::GameBalance::Weapons::Projectiles::APCR_ArmorPenMlt;
		NewWeaponData.BaseDamage *= DeveloperSettings::GameBalance::Weapons::Projectiles::APCR_DamageMlt;
		NewWeaponData.ShrapnelParticles = 0;
		NewWeaponData.ProjectileMovementSpeed *=
			DeveloperSettings::GameBalance::Weapons::Projectiles::APCR_ProjectileSpeedMlt;
		break;

	default:
		RTSFunctionLibrary::ReportError(
			"The provided shell type has no alteration implemented for function GLOBAL_GetWeaponDataForShellType."
			"provided shell type: " + UEnum::GetValueAsString(OldWeaponData.ShellType));
		break;
	}

	return NewWeaponData;
}

// ===== FWeaponShellCase ======================================================

void FWeaponShellCase::InitShellCase(UMeshComponent* NewAttachMeshComponent, UWorld* WorldSpawnedIn)
{
	// Defer actual component creation until first use; just store context.
	if (ShellCaseVfx && NewAttachMeshComponent && WorldSpawnedIn)
	{
		AttachMeshComponent = NewAttachMeshComponent;
		World = WorldSpawnedIn;

		// Reset cached state in case of re-init.
		M_CachedShellCaseNiagara.Reset();
		bM_StaticParamsInitialized = false;
	}
}

void FWeaponShellCase::SpawnShellCase() const
{
	UNiagaraComponent* NiagaraComp = GetOrCreateShellCaseComp();
	if (!NiagaraComp)
	{
		return;
	}

	// Static params set exactly once for this component.
	InitializeStaticParamsIfNeeded(NiagaraComp);

	// Restart for this shot.
	RestartNiagara(NiagaraComp);
}

void FWeaponShellCase::SpawnShellCaseStartRandomBurst(const int BurstAmount) const
{
	UNiagaraComponent* NiagaraComp = GetOrCreateShellCaseComp();
	if (!NiagaraComp)
	{
		return;
	}

	// Static params set exactly once for this component.
	InitializeStaticParamsIfNeeded(NiagaraComp);

	// Per-burst value (must be updated every burst).
	NiagaraComp->SetVariableInt(FName("Spawn Count"), BurstAmount);

	// Restart for this burst.
	RestartNiagara(NiagaraComp);
}

// --- helpers ----------------------------------------------------------------

UNiagaraComponent* FWeaponShellCase::GetOrCreateShellCaseComp() const
{
	if (!ShellCaseVfx || !AttachMeshComponent)
	{
		return nullptr;
	}

	// Reuse cached if valid and still configured for this system and socket.
	if (M_CachedShellCaseNiagara.IsValid())
	{
		UNiagaraComponent* const Cached = M_CachedShellCaseNiagara.Get();
		return Cached;
	}

	// Spawn a persistent, inactive component attached at the configured socket.
	UNiagaraComponent* NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
		ShellCaseVfx,
		AttachMeshComponent,
		ShellCaseSocketName,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		FVector(1.f),
		EAttachLocation::SnapToTarget,
		/*bAutoDestroy*/ false,
		ENCPoolMethod::None,
		/*bPreCullCheck*/ true,
		/*bAutoActivate*/ false);

	if (!NiagaraComp)
	{
		return nullptr;
	}

	M_CachedShellCaseNiagara = NiagaraComp;
	bM_StaticParamsInitialized = false;
	return NiagaraComp;
}

void FWeaponShellCase::InitializeStaticParamsIfNeeded(UNiagaraComponent* NiagaraComp) const
{
	if (!NiagaraComp || !AdjustVfxParams)
	{
		return;
	}

	if (bM_StaticParamsInitialized)
	{
		return; // already initialized for this component
	}

	NiagaraComp->SetVariableFloat(FName("MeshSizeMlt"), MeshSizeMlt);
	NiagaraComp->SetVariableFloat(FName("LifetimeMlt"), LifetimeMlt);
	NiagaraComp->SetVariableFloat(FName("SpriteSizeMlt"), SpriteSizeMlt);
	NiagaraComp->SetVariableFloat(FName("Velocity Speed Mlt"), SpeedMlt);

	bM_StaticParamsInitialized = true;
}

void FWeaponShellCase::RestartNiagara(UNiagaraComponent* NiagaraComp)
{
	if (!NiagaraComp)
	{
		return;
	}
	NiagaraComp->ReinitializeSystem();
	NiagaraComp->Activate(true);
}


// --- Initialize -------------------------------------------------------------

void FWeaponImpactPool::Initialize(
	UWorld* InWorld,
	USoundAttenuation* InAttenuation,
	USoundConcurrency* InConcurrency,
	int32 InCapacity, float WeaponCalibre)
{
	World = InWorld;
	Attenuation = InAttenuation;
	Concurrency = InConcurrency;
	M_WeaponCalibre = WeaponCalibre;

	if (InCapacity < 1)
	{
		InCapacity = 1;
	}

	if (InCapacity != Capacity)
	{
		Capacity = InCapacity;
		Slots.SetNum(Capacity);
		NextIndex = 0;
	}
}

// --- Public API -------------------------------------------------------------

void FWeaponImpactPool::PlayImpact(
	const FVector& Location,
	const FRotator& Rotation,
	UNiagaraSystem* NiagaraSystem,
	const FVector& NiagaraScale,
	USoundBase* Sound)
{
	FSlot& Slot = NextSlot();
	Activate(Slot, Location, Rotation, NiagaraSystem, NiagaraScale, Sound, false);
}

void FWeaponImpactPool::PlayBounce(
	const FVector& Location,
	const FRotator& Rotation,
	UNiagaraSystem* NiagaraSystem,
	const FVector& NiagaraScale,
	USoundBase* Sound, const bool bIsUsingHEBounceOverwrite)
{
	FSlot& Slot = NextSlot();
	Activate(Slot, Location, Rotation, NiagaraSystem, NiagaraScale, Sound, bIsUsingHEBounceOverwrite);
}


void FWeaponImpactPool::Cleanup()
{
	// Explicitly destroy owned components and clear slots.
	for (FSlot& Slot : Slots)
	{
		if (Slot.Audio)
		{
			// Safe guard: stop and destroy
			Slot.Audio->Stop();
			if (UWorld* W = World.Get())
			{
				Slot.Audio->UnregisterComponent();
			}
			Slot.Audio->DestroyComponent();
			Slot.Audio = nullptr;
		}
		if (Slot.Niagara)
		{
			Slot.Niagara->DeactivateImmediate();
			if (UWorld* W = World.Get())
			{
				Slot.Niagara->UnregisterComponent();
			}
			Slot.Niagara->DestroyComponent();
			Slot.Niagara = nullptr;
		}
		Slot.LastSoundAsset = nullptr;
		Slot.LastNiagaraAsset = nullptr;
	}
	Slots.Reset();
	Capacity = 0;
	NextIndex = 0;
	// Keep weak World/Attenuation/Concurrency as-is; weapon likely still holds those.
}

int32 FWeaponImpactPool::RecommendCapacity(
	const FWeaponData& Data,
	EWeaponFireMode FireMode,
	const float BurstCooldown)
{
	float HowLongOneImpactLasts = 0.8f;
	if (Data.WeaponCalibre > 14)
	{
		HowLongOneImpactLasts = 1.2f;
	}
	if (Data.WeaponCalibre > 35)
	{
		HowLongOneImpactLasts = 1.5f;
	}
	if (Data.WeaponCalibre > 75)
	{
		HowLongOneImpactLasts = 2.0f;
	}
	HowLongOneImpactLasts = FMath::Clamp(HowLongOneImpactLasts, 0.2f, 4.0f);

	const float MinInterval = 0.05f;

	if (Data.MagCapacity == 1)
	{
		float Interval = FMath::Max(MinInterval, Data.ReloadSpeed);

		float ExpectedOverlap = HowLongOneImpactLasts / Interval;

		int32 Capacity = FMath::CeilToInt(ExpectedOverlap);
		if (Capacity < 1) { Capacity = 1; }
		if (Capacity > 4) { Capacity = 4; }
		return Capacity;
	}

	float Interval = 0.1f;
	if (FireMode == EWeaponFireMode::Single)
	{
		Interval = FMath::Max(MinInterval, Data.BaseCooldown);
	}
	if (FireMode != EWeaponFireMode::Single)
	{
		float UseInterval = (BurstCooldown > 0.f) ? BurstCooldown : Data.BaseCooldown;
		Interval = FMath::Max(MinInterval, UseInterval);
	}

	float ExpectedOverlap = HowLongOneImpactLasts / Interval;

	int32 Capacity = FMath::CeilToInt(ExpectedOverlap);
	if (Capacity < 2) { Capacity = 2; }
	if (Capacity > 12) { Capacity = 12; }
	return Capacity;
}

// --- Private helpers -------------------------------------------------------

FSlot& FWeaponImpactPool::NextSlot()
{
	if (Slots.Num() < Capacity)
	{
		Slots.SetNum(Capacity);
	}

	int32 Index = NextIndex;
	NextIndex = (NextIndex + 1) % Capacity;
	return Slots[Index];
}

UNiagaraComponent* FWeaponImpactPool::GetOrCreateNiagara(FSlot& Slot)
{
	if (Slot.Niagara)
	{
		return Slot.Niagara;
	}

	UWorld* WorldPtr = World.Get();
	if (!WorldPtr)
	{
		return nullptr;
	}

	AWorldSettings* WS = WorldPtr->GetWorldSettings();
	if (!WS)
	{
		return nullptr;
	}

	UNiagaraComponent* NiagaraComp = NewObject<UNiagaraComponent>(WS);
	if (!NiagaraComp)
	{
		return nullptr;
	}

	NiagaraComp->bAutoActivate = false;
	NiagaraComp->SetAutoDestroy(false);
	NiagaraComp->RegisterComponentWithWorld(WorldPtr);

	Slot.Niagara = NiagaraComp;
	Slot.LastNiagaraAsset = nullptr;
	return NiagaraComp;
}

UAudioComponent* FWeaponImpactPool::GetOrCreateAudio(FSlot& Slot)
{
	if (Slot.Audio)
	{
		return Slot.Audio;
	}

	UWorld* WorldPtr = World.Get();
	if (!WorldPtr)
	{
		return nullptr;
	}

	AWorldSettings* WS = WorldPtr->GetWorldSettings();
	if (!WS)
	{
		return nullptr;
	}

	UAudioComponent* AudioComp = NewObject<UAudioComponent>(WS);
	if (!AudioComp)
	{
		return nullptr;
	}

	AudioComp->bAutoActivate = false;
	AudioComp->bAutoDestroy = false;
	AudioComp->RegisterComponentWithWorld(WorldPtr);

	if (Attenuation)
	{
		AudioComp->AttenuationSettings = Attenuation;
	}
	if (Concurrency)
	{
		AudioComp->ConcurrencySet.Empty();
		AudioComp->ConcurrencySet.Add(Concurrency);
	}

	Slot.Audio = AudioComp;
	Slot.LastSoundAsset = nullptr;
	return AudioComp;
}

void FWeaponImpactPool::Activate(
	FSlot& Slot,
	const FVector& Location,
	const FRotator& Rotation,
	UNiagaraSystem* NiagaraSystem,
	const FVector& NiagaraScale,
	USoundBase* Sound, const bool bIsUsingHeBounceOverwrite)
{
	// --- Niagara ---
	if (UNiagaraComponent* NiagaraComp = GetOrCreateNiagara(Slot))
	{
		const bool bAssetChanged = (Slot.LastNiagaraAsset != NiagaraSystem);
		if (bAssetChanged)
		{
			NiagaraComp->SetAsset(NiagaraSystem);
			Slot.LastNiagaraAsset = NiagaraSystem;
		}
		if (bIsUsingHeBounceOverwrite)
		{
			BounceVfx::ApplyScaleToHeHeatBounce(NiagaraComp, FMath::Max(0.25, FMath::Pow(2, M_WeaponCalibre / 76)));
		}

		NiagaraComp->SetWorldLocationAndRotation(Location, Rotation);
		NiagaraComp->SetWorldScale3D(NiagaraScale);
		NiagaraComp->ReinitializeSystem();
		NiagaraComp->Activate(true);
	}

	// --- Audio ---
	if (Sound)
	{
		if (UAudioComponent* AudioComp = GetOrCreateAudio(Slot))
		{
			const bool bSoundChanged = (Slot.LastSoundAsset != Sound);
			if (bSoundChanged)
			{
				AudioComp->SetSound(Sound);
				Slot.LastSoundAsset = Sound;
			}
			AudioComp->SetWorldLocation(Location);
			AudioComp->SetWorldRotation(Rotation);
			AudioComp->Play(0.f);
		}
	}
}

// ===== END FWeaponShellCase ======================================================

UWeaponState::UWeaponState(): WeaponIndex(0), WeaponHitType(), TraceChannel(),
                              FireModeFunc(nullptr),
                              World(nullptr),
                              OwningPlayer(0),
                              M_WeaponName(), M_WeaponFireMode()
{
}

UWeaponState::~UWeaponState()
{
}

void UWeaponState::UpgradeWeaponWithExtraShellType(const EWeaponShellType ExtraShellType)
{
	WeaponData.ShellTypes.Add(ExtraShellType);
}

void UWeaponState::UpgradeWeaponWithRangeMlt(const float RangeMlt)
{
	WeaponData.Range *= RangeMlt;
}

void UWeaponState::RegisterActorToIgnore(AActor* RTSValidActor, const bool bRegister)
{
	if (bRegister)
	{
		if (ActorsToIgnore.Contains(RTSValidActor))
		{
			const FString ActorName = RTSValidActor ? RTSValidActor->GetName() : "InvalidActor";
			RTSFunctionLibrary::ReportError("Attempted to ignore actor for weapon but actor is already being ignored:"
				+ ActorName);
			return;
		}
		ActorsToIgnore.Add(RTSValidActor);
	}
	else
	{
		if (!ActorsToIgnore.Contains(RTSValidActor))
		{
			const FString ActorName = RTSValidActor ? RTSValidActor->GetName() : "InvalidActor";
			RTSFunctionLibrary::ReportError("Attempted to un-ignore actor for weapon but actor is not being ignored:"
				+ ActorName);
			return;
		}
		ActorsToIgnore.Remove(RTSValidActor);
	}
}


void UWeaponState::Fire(const FVector& AimPointOpt /*= nullptr*/)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FireWeapon);
	if (IsValid(WeaponOwner.GetObject()) && FireModeFunc)
	{
		ExplicitAimPoint = AimPointOpt;
		// Fires the weapon based on the EWeaponFireMode.
		(this->*FireModeFunc)();
	}
}

void UWeaponState::StopFire(
	const bool bStopReload,
	const bool bStopCoolDown)
{
	if (not World)
	{
		return;
	}
	OnStopFire();
	if (bM_IsReloading)
	{
		if (bStopReload)
		{
			// Stop reload.
			World->GetTimerManager().ClearTimer(M_WeaponTimerHandle);
			bM_IsReloading = false;
		}
		return;
	}
	// On cooldown but not reloading?
	if (bM_IsOnCooldown)
	{
		if (bStopCoolDown)
		{
			// Stop cooldown.
			World->GetTimerManager().ClearTimer(M_WeaponTimerHandle);
			bM_IsOnCooldown = false;
			OnCooldownShutDown();
		}
	}
	else
	{
		// Stop weapon fire.
		World->GetTimerManager().ClearTimer(M_WeaponTimerHandle);
	}
}

void UWeaponState::DisableWeapon()
{
	StopFire(true, true);
	OnDisableWeapon();
}

float UWeaponState::GetRange() const
{
	return WeaponData.Range;
}

void UWeaponState::ForceInstantReload()
{
	OnReloadFinished();
}

bool UWeaponState::IsWeaponFullyLoaded() const
{
	return M_CurrentMagCapacity == WeaponData.MagCapacity;
}

const FWeaponData& UWeaponState::GetRawWeaponData() const
{
	return WeaponData;
}

FWeaponVFX* UWeaponState::GetWeaponVfx()
{
	return &M_WeaponVfx;
}

const FWeaponVFX* UWeaponState::GetWeaponVfx() const
{
	return &M_WeaponVfx;
}

FWeaponData* UWeaponState::GetWeaponDataToUpgrade()
{
	return &WeaponData;
}

void UWeaponState::Upgrade(const FBehaviourWeaponAttributes& BehaviourWeaponAttributes, const bool bAddUpgrade)
{
	FWeaponData* WeaponDataToUpgrade = GetWeaponDataToUpgrade();
	if (WeaponDataToUpgrade == nullptr)
	{
		return;
	}

	FBehaviourWeaponAttributes& CurrentBehaviourAttributes = WeaponDataToUpgrade->BehaviourAttributes;

	WeaponDataToUpgrade->BaseDamage -= CurrentBehaviourAttributes.Damage;
	WeaponDataToUpgrade->Range -= CurrentBehaviourAttributes.Range;
	WeaponDataToUpgrade->WeaponCalibre -= static_cast<float>(CurrentBehaviourAttributes.WeaponCalibre);
	WeaponDataToUpgrade->ArmorPen -= CurrentBehaviourAttributes.ArmorPenetration;
	WeaponDataToUpgrade->ArmorPenMaxRange -= CurrentBehaviourAttributes.ArmorPenetrationMaxRange;
	WeaponDataToUpgrade->ReloadSpeed -= CurrentBehaviourAttributes.ReloadSpeed;
	WeaponDataToUpgrade->Accuracy -= CurrentBehaviourAttributes.Accuracy;
	WeaponDataToUpgrade->MagCapacity -= CurrentBehaviourAttributes.MagSize;
	WeaponDataToUpgrade->BaseCooldown -= CurrentBehaviourAttributes.BaseCooldown;

	if (bAddUpgrade)
	{
		CurrentBehaviourAttributes.Damage += BehaviourWeaponAttributes.Damage;
		CurrentBehaviourAttributes.Range += BehaviourWeaponAttributes.Range;
		CurrentBehaviourAttributes.WeaponCalibre += BehaviourWeaponAttributes.WeaponCalibre;
		CurrentBehaviourAttributes.ArmorPenetration += BehaviourWeaponAttributes.ArmorPenetration;
		CurrentBehaviourAttributes.ArmorPenetrationMaxRange += BehaviourWeaponAttributes.ArmorPenetrationMaxRange;
		CurrentBehaviourAttributes.ReloadSpeed += BehaviourWeaponAttributes.ReloadSpeed;
		CurrentBehaviourAttributes.Accuracy += BehaviourWeaponAttributes.Accuracy;
		CurrentBehaviourAttributes.MagSize += BehaviourWeaponAttributes.MagSize;
		CurrentBehaviourAttributes.BaseCooldown += BehaviourWeaponAttributes.BaseCooldown;
	}
	else
	{
		CurrentBehaviourAttributes.Damage -= BehaviourWeaponAttributes.Damage;
		CurrentBehaviourAttributes.Range -= BehaviourWeaponAttributes.Range;
		CurrentBehaviourAttributes.WeaponCalibre -= BehaviourWeaponAttributes.WeaponCalibre;
		CurrentBehaviourAttributes.ArmorPenetration -= BehaviourWeaponAttributes.ArmorPenetration;
		CurrentBehaviourAttributes.ArmorPenetrationMaxRange -= BehaviourWeaponAttributes.ArmorPenetrationMaxRange;
		CurrentBehaviourAttributes.ReloadSpeed -= BehaviourWeaponAttributes.ReloadSpeed;
		CurrentBehaviourAttributes.Accuracy -= BehaviourWeaponAttributes.Accuracy;
		CurrentBehaviourAttributes.MagSize -= BehaviourWeaponAttributes.MagSize;
		CurrentBehaviourAttributes.BaseCooldown -= BehaviourWeaponAttributes.BaseCooldown;
	}

	WeaponDataToUpgrade->BaseDamage += CurrentBehaviourAttributes.Damage;
	WeaponDataToUpgrade->Range += CurrentBehaviourAttributes.Range;
	WeaponDataToUpgrade->WeaponCalibre += static_cast<float>(CurrentBehaviourAttributes.WeaponCalibre);
	WeaponDataToUpgrade->ArmorPen += CurrentBehaviourAttributes.ArmorPenetration;
	WeaponDataToUpgrade->ArmorPenMaxRange += CurrentBehaviourAttributes.ArmorPenetrationMaxRange;
	WeaponDataToUpgrade->ReloadSpeed += CurrentBehaviourAttributes.ReloadSpeed;
	WeaponDataToUpgrade->Accuracy += CurrentBehaviourAttributes.Accuracy;
	WeaponDataToUpgrade->MagCapacity += CurrentBehaviourAttributes.MagSize;
	WeaponDataToUpgrade->BaseCooldown += CurrentBehaviourAttributes.BaseCooldown;
}

FWeaponData UWeaponState::GetWeaponDataAdjustedForShellType() const
{
	if (WeaponData.ShellType == EWeaponShellType::Shell_APHE || WeaponData.ShellType == EWeaponShellType::Shell_AP)
	{
		return WeaponData;
	}
	return GLOBAL_GetWeaponDataForShellType(WeaponData);
}

bool UWeaponState::ChangeWeaponShellType(const EWeaponShellType NewShellType)
{
	if (WeaponData.ShellTypes.Contains(NewShellType))
	{
		if (WeaponData.ShellType != NewShellType)
		{
			M_WeaponVfx.ShellSpecificVfxOverwrites.HandleWeaponShellChange(WeaponData.ShellType, NewShellType,
			                                                               M_WeaponVfx.BounceSound,
			                                                               M_WeaponVfx.BounceEffect,
			                                                               M_WeaponVfx.SurfaceImpactEffects);
			WeaponData.ShellType = NewShellType;

			OnWeaponShellTypeChanged.Broadcast(NewShellType);
			return true;
		}
		return false;
	}
	ReportErrorForWeapon(
		"Attempted to change weapon shell type to a type not supported by the weapon."
		"New shell type: " + UEnum::GetValueAsString(NewShellType));
	return false;
}

void UWeaponState::ChangeOwningPlayer(const int32 NewOwningPlayer)
{
	OwningPlayer = NewOwningPlayer;
	if (OwningPlayer == 1)
	{
		TraceChannel = COLLISION_TRACE_ENEMY;
	}
	else
	{
		TraceChannel = COLLISION_TRACE_PLAYER;
	}
}

void UWeaponState::BeginDestroy()
{
	if (World)
	{
		World->GetTimerManager().ClearTimer(M_WeaponTimerHandle);
	}
	UObject::BeginDestroy();
}

void UWeaponState::OnStopFire()
{
	// Extra functionality that needs to stop once the weapon stops firing.
}

void UWeaponState::OnDisableWeapon()
{
	// Extra functionality for when the weapon is disabled.
}


void UWeaponState::InitWeaponState(
	int32 NewOwningPlayer,
	const int32 NewWeaponIndex,
	const EWeaponName NewWeaponName,
	const EWeaponFireMode NewWeaponFireMode,
	TScriptInterface<IWeaponOwner> NewWeaponOwner,
	UMeshComponent* NewMeshComponent,
	const FName NewFireSocketName,
	UWorld* NewWorld,
	FWeaponVFX NewWeaponVFX,
	FWeaponShellCase NewWeaponShellCase,
	const float NewBurstCooldown,
	const int32 NewSingleBurstAmountMaxBurstAmount,
	const int32 NewMinBurstAmount,
	const bool bNewCreateShellCasingOnEveryRandomBurst, const bool bIsLaserOrFlame)
{
	const int32 NormalizedOwner = (NewOwningPlayer == 0) ? 1 : NewOwningPlayer;
	OwningPlayer = NormalizedOwner;
	TraceChannel = (OwningPlayer == 1) ? COLLISION_TRACE_ENEMY : COLLISION_TRACE_PLAYER;

	WeaponIndex = NewWeaponIndex;
	M_WeaponName = NewWeaponName;
	WeaponOwner = NewWeaponOwner;
	MeshComponent = NewMeshComponent;
	FireSocketName = NewFireSocketName;
	World = NewWorld;
	bM_CreateShellCaseOnEachRandomBurst = bNewCreateShellCasingOnEveryRandomBurst;
	// Init weapon data; needs world to be set!!
	UpdateWeaponDataForOwningPlayer(NewWeaponName, OwningPlayer);
	// Refresh damage type class now that we have loaded the data.
	RefreshDamageTypeClass();
	M_WeaponVfx = NewWeaponVFX;
	WeaponShellCase = NewWeaponShellCase;
	// Setup references
	WeaponShellCase.InitShellCase(NewMeshComponent, NewWorld);
	M_CurrentMagCapacity = WeaponData.MagCapacity;
	// Setup burst related logic and binds the right fire mode to the FireModeFunc.
	InitWeaponMode(NewWeaponFireMode, NewSingleBurstAmountMaxBurstAmount, NewMinBurstAmount,
	               NewBurstCooldown);


	if (not bIsLaserOrFlame)
	{
		const int32 Capacity = FWeaponImpactPool::RecommendCapacity(WeaponData, M_WeaponFireMode, M_BurstCooldown);
		// todo remove.
		// const FString WeaponName = UEnum::GetValueAsString(M_WeaponName);
		// const FString MagCapacityAndReloadSpeed = "Mag cap: " + FString::FromInt(WeaponData.MagCapacity) + " rel: " +
		// 	FString::SanitizeFloat(WeaponData.ReloadSpeed);
		// const FString MaxBurstAndBurstCooldown = "Max Burst: " + FString::FromInt(NewSingleBurstAmountMaxBurstAmount) +
		// 	" burstcool: " +
		// 	FString::SanitizeFloat(NewBurstCooldown);
		//
		// const FString TotalDebug = "Recommended Capacity = " + FString::FromInt(Capacity) + "\nfor weapon " + WeaponName
		// 	+ "\n" +
		// 	MagCapacityAndReloadSpeed + "\n" + MaxBurstAndBurstCooldown;
		// RTSFunctionLibrary::DisplayNotification(TotalDebug);

		M_ImpactPool.Initialize(World, M_WeaponVfx.ImpactAttenuation, M_WeaponVfx.ImpactConcurrency, Capacity,
		                        WeaponData.WeaponCalibre);
	}

	if (IsValid(WeaponOwner.GetObject()))
	{
		WeaponOwner->OnWeaponAdded(WeaponIndex, this);
	}

	// Bind the cooldown delegate to the cooldown function.
	M_CoolDownDel = FTimerDelegate::CreateUObject(this, &UWeaponState::OnCoolDownFinished);
	// Bind the reload delegate to the reload function.
	M_ReloadDel = FTimerDelegate::CreateUObject(this, &UWeaponState::OnReloadFinished);
}

void UWeaponState::RefreshDamageTypeClass()
{
	DamageTypeClass = FRTSWeaponHelpers::GetDamageTypeClass(WeaponData.DamageType);
}


void UWeaponState::FireSingleShot()
{
	if (M_CurrentMagCapacity > 0)
	{
		if (!bM_IsOnCooldown)
		{
			CreateLaunchVfx(
				GetLaunchAndForwardVector().Key,
				GetLaunchAndForwardVector().Value);
			if (IsValid(WeaponOwner.GetObject()))
			{
				WeaponOwner->PlayWeaponAnimation(WeaponIndex, M_WeaponFireMode);
			}
			FireWeaponSystem();
			M_CurrentMagCapacity--;
			OnMagConsumed.Broadcast(M_CurrentMagCapacity);
			CoolDown();
		}
		else
		{
			// Sets the cooldown timer to exe OnCoolDownFinished in cooldown + flux seconds.
			CoolDown();
		}
	}
	else if (!bM_IsReloading)
	{
		// Sets the reload timer to exe OnFinishedReload in reload + flux seconds.
		Reload();
	}
}

void UWeaponState::FireSingleBurst()
{
	// If the weapon was cut shooting prematurely OnCooldown will be set to false however bullets still remain so only
	// checking agains M_AmountLeftInBurst will deadlock the weapon.
	if (bM_IsOnCooldown && M_AmountLeftInBurst > 0)
	{
		return;
	}
	// Check if there are enough bullets left for the burst.
	if (M_CurrentMagCapacity >= M_MaxBurstAmount)
	{
		if (!bM_IsOnCooldown)
		{
			CreateLaunchVfx(
				GetLaunchAndForwardVector().Key,
				GetLaunchAndForwardVector().Value);
			if (IsValid(WeaponOwner.GetObject()))
			{
				WeaponOwner->PlayWeaponAnimation(WeaponIndex, M_WeaponFireMode);
			}
			InitSingleBurstMode();
		}
	}
	else
	{
		// Sets the reload timer to exe OnFinishedReload in reload + flux seconds.
		Reload();
	}
}

void UWeaponState::InitSingleBurstMode()
{
	bM_IsOnCooldown = true;
	M_AmountLeftInBurst = FMath::Min(M_CurrentMagCapacity, M_MaxBurstAmount);
	World->GetTimerManager().ClearTimer(M_WeaponTimerHandle);

	const float BurstSpeed = GetTimeWithFlux(M_BurstCooldown, WeaponData.CooldownFlux);

	// Immediate call to OnSingleBurst, del bound on Init.
	M_BurstModeDel.ExecuteIfBound();

	// Loop OnSingleBurst till timer is invalidated.
	World->GetTimerManager().SetTimer(M_WeaponTimerHandle, M_BurstModeDel, BurstSpeed, true);
}

void UWeaponState::OnSingleBurst()
{
	M_CurrentMagCapacity--;
	M_AmountLeftInBurst--;
	OnMagConsumed.Broadcast(M_CurrentMagCapacity);
	FireWeaponSystem();
	if (M_AmountLeftInBurst <= 0)
	{
		OnStopBurstMode();
	}
}


void UWeaponState::FireRandomBurst()
{
	if (M_CurrentMagCapacity > 0)
	{
		if (!bM_IsOnCooldown)
		{
			InitRandomBurst();
		}
	}
	else
	{
		// Sets the reload timer to exe OnFinishedReload in reload + flux seconds.
		Reload();
	}
}

void UWeaponState::InitRandomBurst()
{
	bM_IsOnCooldown = true;
	M_AmountLeftInBurst = FMath::RandRange(M_MinBurstAmount, M_MaxBurstAmount);
	if (M_CurrentMagCapacity < M_AmountLeftInBurst)
	{
		M_AmountLeftInBurst = M_CurrentMagCapacity;
	}
	World->GetTimerManager().ClearTimer(M_WeaponTimerHandle);

	// One time animation for the full burst.
	if (IsValid(WeaponOwner.GetObject()))
	{
		WeaponOwner->PlayWeaponAnimation(WeaponIndex, M_WeaponFireMode);
	}


	const float BurstCooldown = GetTimeWithFlux(M_BurstCooldown, WeaponData.CooldownFlux);

	if (not bM_CreateShellCaseOnEachRandomBurst)
	{
		// Create full shell effect for the entire burst
		WeaponShellCase.SpawnShellCaseStartRandomBurst(M_AmountLeftInBurst);
	}

	// Immediate call to OnRandomBurst, del bound on Init.
	M_BurstModeDel.ExecuteIfBound();

	// Loop OnSingleBurst till timer is invalidated.
	World->GetTimerManager().SetTimer(M_WeaponTimerHandle, M_BurstModeDel, BurstCooldown, true);
}

void UWeaponState::OnRandomBurst()
{
	// Create launch vfx and sfx with possible ground smokes but no shell case.
	CreateLaunchVfx(
		GetLaunchAndForwardVector().Key,
		GetLaunchAndForwardVector().Value, bM_CreateShellCaseOnEachRandomBurst);


	M_CurrentMagCapacity--;
	M_AmountLeftInBurst--;
	FireWeaponSystem();

	if (M_AmountLeftInBurst <= 0)
	{
		OnStopBurstMode();
	}
}


void UWeaponState::OnStopBurstMode()
{
	// Clear burst timer.
	if (IsValid(World))
	{
		World->GetTimerManager().ClearTimer(M_WeaponTimerHandle);
		// Notify once per completed burst (after all bullets of that burst were spent)
		OnMagConsumed.Broadcast(M_CurrentMagCapacity);
		if (M_CurrentMagCapacity > 0)
		{
			// cooldown weapon, if no bullets left the fire function will call reload.
			const float CoolDownTime = GetTimeWithFlux(WeaponData.BaseCooldown, WeaponData.CooldownFlux);
			World->GetTimerManager().SetTimer(M_WeaponTimerHandle, M_CoolDownDel, CoolDownTime, false);
		}
	}
}

void UWeaponState::FireWeaponSystem()
{
	// Implemented in childs.
}

void UWeaponState::Reload()
{
	if (IsValid(World) && IsValid(WeaponOwner.GetObject()) && !bM_IsReloading && WeaponOwner->
		AllowWeaponToReload(WeaponIndex))
	{
		bM_IsReloading = true;
		World->GetTimerManager().ClearTimer(M_WeaponTimerHandle);

		const float ReloadSpeed = GetTimeWithFlux(WeaponData.ReloadSpeed, WeaponData.CooldownFlux);
		World->GetTimerManager().SetTimer(M_WeaponTimerHandle, M_ReloadDel, ReloadSpeed, false);

		WeaponOwner->OnReloadStart(WeaponIndex, WeaponData.ReloadSpeed);
	}
}

void UWeaponState::OnReloadFinished()
{
	if (IsValid(World) && IsValid(WeaponOwner.GetObject()))
	{
		bM_IsReloading = false;
		// Single fire weapons with mag capacity of 1 will have entered cooldown but reload on the next fire tick
		// therefore we need to reset the cooldown.
		bM_IsOnCooldown = false;
		M_CurrentMagCapacity = WeaponData.MagCapacity;
		World->GetTimerManager().ClearTimer(M_WeaponTimerHandle);
		WeaponOwner->OnReloadFinished(WeaponIndex);
		// Broadcast mag consumption.
		OnMagConsumed.Broadcast(M_CurrentMagCapacity);
	}
}

void UWeaponState::CoolDown()
{
	if (IsValid(World) && !bM_IsOnCooldown)
	{
		bM_IsOnCooldown = true;
		World->GetTimerManager().ClearTimer(M_WeaponTimerHandle);
		const float CoolDownTime = GetTimeWithFlux(WeaponData.BaseCooldown, WeaponData.CooldownFlux);
		World->GetTimerManager().SetTimer(M_WeaponTimerHandle, M_CoolDownDel, CoolDownTime, false);
	}
}

void UWeaponState::OnCoolDownFinished()
{
	if (IsValid(World))
	{
		World->GetTimerManager().ClearTimer(M_WeaponTimerHandle);
		bM_IsOnCooldown = false;
	}
}


bool UWeaponState::FluxDamageHitActor_DidActorDie(
	AActor* HitActor,
	const float BaseDamageToFlux)
{
	const float Dmg = FRTSWeaponHelpers::GetDamageWithFlux(BaseDamageToFlux, WeaponData.DamageFlux);

	DamageEvent.DamageTypeClass = DamageTypeClass;

	const float Result = HitActor->TakeDamage(Dmg, DamageEvent, /*Instigator=*/nullptr, /*Causer=*/nullptr);
	return Result == 0.f;
}


void UWeaponState::OnActorKilled(AActor* KilledActor) const
{
	if (IsValid(WeaponOwner.GetObject()))
	{
		WeaponOwner->OnWeaponKilledActor(WeaponIndex, KilledActor);
	}
}

void UWeaponState::CreateWeaponImpact(const FVector& HitLocation, const ERTSSurfaceType HitSurface,
                                      const FRotator& ImpactRotation)
{
	if (!IsValid(World))
	{
		return;
	}

	if (const FRTSSurfaceImpactData* SurfaceData = M_WeaponVfx.SurfaceImpactEffects.Find(HitSurface))
	{
		USoundCue* ImpactSound = SurfaceData->ImpactSound;
		UNiagaraSystem* ImpactFx = SurfaceData->ImpactEffect;

		// Use pooled emitters; swap assets if needed; then restart.
		M_ImpactPool.PlayImpact(HitLocation, ImpactRotation, ImpactFx, M_WeaponVfx.ImpactScale, ImpactSound);
		return;
	}

	const FString SurfaceTypeString = FRTS_PhysicsHelper::GetSurfaceTypeString(HitSurface);
	ReportErrorForWeapon(
		"Could not find impact effect for surface type: " + SurfaceTypeString + " in weapon: " +
		UEnum::GetValueAsString(M_WeaponName));
}


void UWeaponState::CreateWeaponNonPenVfx(const FVector& HitLocation, const FRotator& BounceNormal)
{
	if (!IsValid(World))
	{
		return;
	}

	// Route through the same pool. For consistency we pass the provided normal/rotator.
	M_ImpactPool.PlayBounce(
		HitLocation,
		BounceNormal,
		M_WeaponVfx.BounceEffect,
		M_WeaponVfx.BounceScale,
		M_WeaponVfx.BounceSound,
		// If we use the HE overwrite then we set a special param to scale the bounce depending on the calibre.
		M_WeaponVfx.ShellSpecificVfxOverwrites.GetIsUsingHeBounceOverwrite());
}


void UWeaponState::ReportErrorForWeapon(const FString& ErrorMessage) const
{
	const FString MyName = UEnum::GetValueAsString(M_WeaponName);
	RTSFunctionLibrary::ReportError(ErrorMessage + "\n Weapon Name: " + MyName);
}


TPair<FVector, FVector> UWeaponState::GetLaunchAndForwardVector() const
{
	FVector LaunchLocation;
	FVector ForwardVector;
	if (IsValid(MeshComponent))
	{
		LaunchLocation = MeshComponent->GetSocketLocation(FireSocketName);
		const FQuat SocketRotation = MeshComponent->GetSocketQuaternion(FireSocketName);
		ForwardVector = SocketRotation.GetForwardVector();

		// draw debug of socket forward vector 200 units.
		if constexpr (DeveloperSettings::Debugging::GTurret_Master_Compile_DebugSymbols)
		{
			if (IsValid(World))
			{
				DrawDebugLine(World, LaunchLocation, LaunchLocation + ForwardVector * 200, FColor::Red, false, 0.1f, 0,
				              5.f);
			}
		}
	}
	return TPair<FVector, FVector>(LaunchLocation, ForwardVector);
}


float UWeaponState::GetTimeWithFlux(
	float BaseTime,
	int32 FluxPercentage) const
{
	return FMath::RandRange(BaseTime * (1.0f - FluxPercentage / 100.0f),
	                        BaseTime * (1.0f + FluxPercentage / 100.0f));
}

void UWeaponState::UpdateWeaponDataForOwningPlayer(const EWeaponName WeaponNameObtainValues,
                                                   const int32 OwningPlayerOfWeapon)
{
	if (not World)
	{
		RTSFunctionLibrary::ReportError("World not valid for weapon + " + GetName());
		return;
	}
	ACPPGameState* GameState = Cast<ACPPGameState>(World->GetGameState());
	if (GameState)
	{
		const FWeaponData* NewData = GameState->GetWeaponDataOfPlayer(OwningPlayerOfWeapon, WeaponNameObtainValues);
		if (NewData)
		{
			WeaponData.CopyWeaponDataValues(NewData);
			return;
		}
		ReportErrorForWeapon(
			"Could not find weapon data in game state for player: " + FString::FromInt(OwningPlayerOfWeapon)
			+ "\n Weapon Name: " + UEnum::GetValueAsString(WeaponNameObtainValues)
			+ "\n Using default fallback weapon data.");
		NewData = GameState->GetDefaultFallBackWeapon();
		if (NewData)
		{
			WeaponData.CopyWeaponDataValues(NewData);
		}
	}
}


void UWeaponState::InitWeaponMode(
	const EWeaponFireMode FireMode,
	const uint32 MaxBurstAmount,
	const uint32 MinBurstAmount,
	const float BurstCooldown)
{
	M_WeaponFireMode = FireMode;
	switch (FireMode)
	{
	case EWeaponFireMode::Single:
		FireModeFunc = &UWeaponState::FireSingleShot;
		break;
	case EWeaponFireMode::SingleBurst:
		FireModeFunc = &UWeaponState::FireSingleBurst;
		M_WeaponFireMode = EWeaponFireMode::SingleBurst;
		M_BurstModeDel = FTimerDelegate::CreateUObject(this, &UWeaponState::OnSingleBurst);
		M_MaxBurstAmount = MaxBurstAmount;
		M_BurstCooldown = BurstCooldown;
		break;
	case EWeaponFireMode::RandomBurst:
		FireModeFunc = &UWeaponState::FireRandomBurst;
		M_MaxBurstAmount = MaxBurstAmount;
		M_MinBurstAmount = MinBurstAmount;
		M_BurstModeDel = FTimerDelegate::CreateUObject(this, &UWeaponState::OnRandomBurst);
		M_BurstCooldown = BurstCooldown;
		break;
	}
}

void UWeaponState::CreateLaunchVfx(
	const FVector& LaunchLocation,
	const FVector& ForwardVector,
	const bool bCreateShellCase)
{
	if (!World)
	{
		return;
	}
	if (M_WeaponVfx.LaunchSound)
	{
		UGameplayStatics::PlaySoundAtLocation(World, M_WeaponVfx.LaunchSound, LaunchLocation,
		                                      FRotator::ZeroRotator, 1, 1, 0,
		                                      M_WeaponVfx.LaunchAttenuation, M_WeaponVfx.LaunchConcurrency);
	}

	CreateLaunchAndSmokeVfx(FireSocketName, ForwardVector.Rotation(), LaunchLocation);

	if (bCreateShellCase)
	{
		WeaponShellCase.SpawnShellCase();
	}
}

void UWeaponState::CreateLaunchAndSmokeVfx(
	const FName SocketName,
	const FRotator& LaunchRotation,
	const FVector& LaunchLocation)
{
	if (not MeshComponent || not M_WeaponVfx.LaunchEffectSettings.LaunchEffect)
	{
		return;
	}

	// Ensure (or create) the pooled component for this socket.
	UNiagaraComponent* const NiagaraComp =
		GetOrCreateLaunchNiagaraForSocket(SocketName, LaunchRotation, LaunchLocation);

	if (not NiagaraComp)
	{
		return;
	}

	switch (M_WeaponVfx.LaunchEffectSettings.UseLaunchSettings)
	{
	case EWeaponLaunchSettingsType::None:
		break;
	case EWeaponLaunchSettingsType::ColorByShellType:
		// Lazily push colors only when ShellType changed (also covers the "first time" path).
		UpdateAllCachedLaunchColorsIfShellChanged();
		break;
	case EWeaponLaunchSettingsType::DirectLifeTimeSizeScale:
		// Settings already set directly from struct values when niagara component was created.
		break;
	}

	// Clean restart for this shot, then explicitly activate to be robust.
	NiagaraComp->ReinitializeSystem();
	NiagaraComp->Activate(true);
}

void UWeaponState::OnCooldownShutDown()
{
	if (M_WeaponFireMode == EWeaponFireMode::SingleBurst && M_CurrentMagCapacity < M_MaxBurstAmount)
	{
		// Not enough for a full burst; the weapon would start reloading at the next fire tick anyway so do that now.
		M_CurrentMagCapacity = 0;
		OnMagConsumed.Broadcast(M_CurrentMagCapacity);
		M_AmountLeftInBurst = 0;
		Reload();
	}
}

UNiagaraComponent* UWeaponState::GetOrCreateLaunchNiagaraForSocket(
	const FName& SocketName,
	const FRotator& /*LaunchRotation*/,
	const FVector& /*LaunchLocation*/)
{
	if (not MeshComponent || SocketName.IsNone())
	{
		return nullptr;
	}

	// Return cached if still valid.
	if (TWeakObjectPtr<UNiagaraComponent>* Found = M_LaunchNiagaraBySocket.Find(SocketName))
	{
		if (Found->IsValid())
		{
			return Found->Get();
		}
		// weak ptr expired -> remove and recreate
		M_LaunchNiagaraBySocket.Remove(SocketName);
		RTSFunctionLibrary::ReportError("niagara component (CACHED) got invalid! respawning one!");
	}

	// Create once and attach to the muzzle socket (keeps designer scale/attachment).
	UNiagaraComponent* NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
		M_WeaponVfx.LaunchEffectSettings.LaunchEffect,
		MeshComponent,
		SocketName,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		M_WeaponVfx.LaunchScale,
		EAttachLocation::SnapToTarget,
		/*bAutoDestroy*/ false,
		ENCPoolMethod::None,
		/*bPreCullCheck*/ true,
		/*bAutoActivate*/ false);

	if (not NiagaraComp)
	{
		return nullptr;
	}

	// One-time static parameters extracted to a dedicated helper.
	InitializeLaunchNiagaraStaticParams(NiagaraComp);


	// Cache and return.
	M_LaunchNiagaraBySocket.Add(SocketName, NiagaraComp);
	return NiagaraComp;
}

void UWeaponState::InitializeLaunchNiagaraStaticParams(UNiagaraComponent* const NiagaraComp) const
{
	if (not NiagaraComp)
	{
		return;
	}

	switch (M_WeaponVfx.LaunchEffectSettings.UseLaunchSettings)
	{
	case EWeaponLaunchSettingsType::None:
		return;
	case EWeaponLaunchSettingsType::ColorByShellType:
		SetColorByShellTypeInitParams(NiagaraComp);
		return;
	case EWeaponLaunchSettingsType::DirectLifeTimeSizeScale:
		SetDirectLifeTimeSizeScaleInitParams(NiagaraComp);
		return;
	}
}

void UWeaponState::SetColorByShellTypeInitParams(UNiagaraComponent* NiagaraComp) const
{
	using namespace LaunchVfx;

	// Lifetime scales with weapon cadence (multi-burst lives longer) and designer multiplier.
	float LifeTime = (WeaponData.MagCapacity > 1) ? WeaponData.BaseCooldown : 1.f;
	LifeTime *= M_WeaponVfx.LaunchEffectSettings.LifeTimeMlt;
	NiagaraComp->SetFloatParameter(LifeTimeName, LifeTime);

	NiagaraComp->SetFloatParameter(
		LightMltName,
		M_WeaponVfx.LaunchEffectSettings.LightIntensityMlt * BaseLightIntensity);

	const float WeaponCalibreNormalized = FMath::Min(
		DeveloperSettings::GamePlay::Projectile::ProjectilesVfx::MaxLaunchVfxMlt, WeaponData.WeaponCalibre / 76.f);
	NiagaraComp->SetFloatParameter(WeaponCalibreName, WeaponCalibreNormalized);

	NiagaraComp->SetBoolParameter(
		UseGroundSmokeName,
		M_WeaponVfx.LaunchEffectSettings.bUseGroundLaunchSmoke);

	NiagaraComp->SetBoolParameter(
		UseMuzzleSmokeName,
		M_WeaponVfx.LaunchEffectSettings.bUseMuzzleLaunchSmoke);

	if (M_WeaponVfx.LaunchEffectSettings.bUseGroundLaunchSmoke)
	{
		NiagaraComp->SetFloatParameter(
			GroundZOffsetName,
			M_WeaponVfx.LaunchEffectSettings.GroundZOffset);
	}
}

void UWeaponState::SetDirectLifeTimeSizeScaleInitParams(UNiagaraComponent* NiagaraComp) const
{
	const float LifeTime = M_WeaponVfx.LaunchEffectSettings.LifeTimeMlt;
	const float SizeMlt = M_WeaponVfx.LaunchEffectSettings.SizeMlt;
	NiagaraComp->SetFloatParameter(LaunchVfx::LifeTimeName, LifeTime);
	NiagaraComp->SetFloatParameter(LaunchVfx::DirectSizeMltName, SizeMlt);
}

void UWeaponState::UpdateAllCachedLaunchColorsIfShellChanged()
{
	const EWeaponShellType CurrentShell = WeaponData.ShellType;
	if (CurrentShell == M_LastShellTypeForLaunchVfx)
	{
		return; // no-op; nothing to update
	}

	using namespace LaunchVfx;

	const FLinearColor MuzzleColor = GLOBAL_GetShellColor(CurrentShell);
	const FLinearColor GroundSmokeCol = GLOBAL_GetShellGroundSmokeColor(CurrentShell);
	const bool bUseGroundSmoke = M_WeaponVfx.LaunchEffectSettings.bUseGroundLaunchSmoke;

	// Push new colors to every cached component (valid only).
	for (TPair<FName, TWeakObjectPtr<UNiagaraComponent>>& Pair : M_LaunchNiagaraBySocket)
	{
		if (not Pair.Value.IsValid())
		{
			continue;
		}
		UNiagaraComponent* const Comp = Pair.Value.Get();

		Comp->SetColorParameter(WeaponShellColorName, MuzzleColor);

		if (bUseGroundSmoke)
		{
			Comp->SetColorParameter(GroundSmokeColor, GroundSmokeCol);
		}
	}

	M_LastShellTypeForLaunchVfx = CurrentShell;
}

void UWeaponStateDirectHit::InitDirectHitWeapon(
	const int32 NewOwningPlayer,
	const int32 NewWeaponIndex,
	const EWeaponName NewWeaponName,
	const EWeaponFireMode NewWeaponBurstMode,
	TScriptInterface<IWeaponOwner> NewWeaponOwner,
	UMeshComponent* NewMeshComponent,
	const FName NewFireSocketName,
	UWorld* NewWorld,
	FWeaponVFX NewWeaponVFX,
	FWeaponShellCase NewWeaponShellCase,
	const float NewBurstCooldown,
	const int32 NewSingleBurstAmountMaxBurstAmount,
	const int32 NewMinBurstAmount,
	const bool bNewCreateShellCasingOnEveryRandomBurst)
{
	WeaponHitType = EWeaponSystemType::DirectHit;
	InitWeaponState(
		NewOwningPlayer,
		NewWeaponIndex,
		NewWeaponName,
		NewWeaponBurstMode,
		NewWeaponOwner,
		NewMeshComponent,
		NewFireSocketName,
		NewWorld,
		NewWeaponVFX,
		NewWeaponShellCase,
		NewBurstCooldown,
		NewSingleBurstAmountMaxBurstAmount,
		NewMinBurstAmount,
		bNewCreateShellCasingOnEveryRandomBurst);
}

void UWeaponStateDirectHit::FireWeaponSystem()
{
}

void UWeaponStateDirectHit::FireDirectHit(AActor* HitActor)
{
}

UWeaponStateTrace::UWeaponStateTrace()
{
}


void UWeaponStateTrace::InitTraceWeapon(
	const int32 NewOwningPlayer,
	const int32 NewWeaponIndex,
	const EWeaponName NewWeaponName,
	const EWeaponFireMode NewWeaponBurstMode,
	TScriptInterface<IWeaponOwner> NewWeaponOwner,
	UMeshComponent* NewMeshComponent,
	const FName NewFireSocketName,
	UWorld* NewWorld,
	FWeaponVFX NewWeaponVFX,
	FWeaponShellCase NewWeaponShellCase,
	const float NewBurstCooldown,
	const int32 NewSingleBurstAmountMaxBurstAmount,
	const int32 NewMinBurstAmount,
	const bool bNewCreateShellCasingOnEveryRandomBurst,
	const int32 TraceProjectileType)
{
	WeaponHitType = EWeaponSystemType::Trace;
	M_TraceProjectileType = TraceProjectileType;
	InitWeaponState(
		NewOwningPlayer,
		NewWeaponIndex,
		NewWeaponName,
		NewWeaponBurstMode,
		NewWeaponOwner,
		NewMeshComponent,
		NewFireSocketName,
		NewWorld,
		NewWeaponVFX,
		NewWeaponShellCase,
		NewBurstCooldown,
		NewSingleBurstAmountMaxBurstAmount,
		NewMinBurstAmount,
		bNewCreateShellCasingOnEveryRandomBurst);
}

void UWeaponStateTrace::SetupProjectileManager(ASmallArmsProjectileManager* ProjectileManager)
{
	if (not IsValid(ProjectileManager))
	{
		ReportErrorForWeapon("weapon is provided with an invalid projectile manager. ");
		return;
	}
	M_ProjectileManager = ProjectileManager;
}

void UWeaponStateTrace::FireWeaponSystem()
{
	if (WeaponOwner.GetObject())
	{
		FireTrace(WeaponOwner->GetFireDirection(WeaponIndex));
	}
}


void UWeaponStateTrace::FireTrace(const FVector& Direction)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FireTrace);
	if (!IsValid(World))
	{
		return;
	}

	const TPair<FVector, FVector> LaunchAndForward = GetLaunchAndForwardVector();
	const FVector LaunchLocation = LaunchAndForward.Key;

	// Use the provided direction for the trace
	FVector TraceEnd = Direction * WeaponData.Range;
	TraceEnd = FRTSWeaponHelpers::GetTraceEndWithAccuracy(LaunchLocation, Direction, WeaponData.Range,
	                                                      WeaponData.Accuracy, bIsAircraftWeapon);
	if (0)
	{
		DrawDebugLine(World, LaunchLocation, TraceEnd, FColor::Green, false, 5.f, 0, 2.f);
	}
	const float ProjectileLaunchTime = IsValid(World) ? World->GetTimeSeconds() : 0.f;

	FCollisionQueryParams TraceParams(FName(TEXT("FireTrace")), true, nullptr);
	TraceParams.bTraceComplex = false;
	TraceParams.bReturnPhysicalMaterial = true;
	TraceParams.AddIgnoredActors(ActorsToIgnore);

	// Define the delegate using a lambda
	FTraceDelegate TraceDelegate;
	TWeakObjectPtr<UWeaponStateTrace> WeakThis(this);
	TraceDelegate.BindLambda(
		[WeakThis, ProjectileLaunchTime, LaunchLocation, TraceEnd](const FTraceHandle& TraceHandle,
		                                                           FTraceDatum& TraceDatum)-> void
		{
			if (not WeakThis.IsValid())
			{
				return;
			}
			WeakThis.Get()->OnAsyncTraceComplete(
				TraceDatum, ProjectileLaunchTime, LaunchLocation, TraceEnd);
		});
	// Perform the async trace
	World->AsyncLineTraceByChannel(
		EAsyncTraceType::Single,
		LaunchLocation,
		TraceEnd,
		TraceChannel,
		TraceParams,
		FCollisionResponseParams::DefaultResponseParam,
		&TraceDelegate
	);
}

bool UWeaponStateTrace::DidTracePen(const FHitResult& TraceHit, AActor*& OutHitActor) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TRACEWEAPON::SearchArmorCalc);
	UArmorCalculation* ArmorCalculationComp = FRTSWeaponHelpers::GetArmorAndActorOrParentFromHit(
		TraceHit, OutHitActor);
	if (IsValid(ArmorCalculationComp))
	{
		return DidTracePenArmorCalcComponent(ArmorCalculationComp, TraceHit);
	}
	return true;
}

bool UWeaponStateTrace::DidTracePenArmorCalcComponent(UArmorCalculation* ArmorCalculation,
                                                      const FHitResult& HitResult) const
{
	float RawArmorValue = 0.0f;
	
	EArmorPlate PlateHit = EArmorPlate::Plate_Front;
	float AdjustedArmorPen = WeaponData.ArmorPen;
	const FVector ImpactDirection = HitResult.TraceEnd - HitResult.TraceStart;
	const float EffectiveArmor = ArmorCalculation->GetEffectiveArmorOnHit(
		HitResult.Component, HitResult.Location, ImpactDirection, HitResult.ImpactNormal, RawArmorValue,
		AdjustedArmorPen, PlateHit);
	return EffectiveArmor < AdjustedArmorPen;
}


void UWeaponStateTrace::OnActorPenArmor(const FHitResult& HitResult, AActor* HitActor)
{
	if (FluxDamageHitActor_DidActorDie(HitActor, WeaponData.BaseDamage))
	{
		OnActorKilled(HitActor);
	}
}

void UWeaponStateTrace::OnAsyncTraceHitValidActor(const FHitResult& TraceHit, FVector& OutEndLocation,
                                                  const FRotator& ImpactRotation, const ERTSSurfaceType SurfaceTypeHit)
{
	AActor* HitActor;
	if (DidTracePen(TraceHit, HitActor))
	{
		if constexpr (DeveloperSettings::Debugging::GArmorCalculation_Compile_DebugSymbols)
		{
			if (IsValid(World))
			{
				DrawDebugString(World, TraceHit.ImpactPoint, "tracePen", nullptr, FColor::Red, 2.f);
			}
		}
		OnActorPenArmor(TraceHit, HitActor);
		OutEndLocation = TraceHit.ImpactPoint;
		CreateWeaponImpact(TraceHit.ImpactPoint, SurfaceTypeHit,
		                   ImpactRotation);
		return;
	}
	// Trace bounce.
	OutEndLocation = TraceHit.ImpactPoint;
	CreateWeaponNonPenVfx(TraceHit.Location, ImpactRotation);
}

void UWeaponStateTrace::OnAsyncTraceNoHit(const FVector& TraceEnd)
{
	// If no hits, still create impact at the TraceEnd location
	CreateWeaponImpact(TraceEnd, ERTSSurfaceType::Air);
}

void UWeaponStateTrace::OnAsyncTraceComplete(
	FTraceDatum& TraceDatum,
	const float ProjectileLaunchTime,
	const FVector& LaunchLocation,
	const FVector& TraceEnd)
{
	// Use Trace end on no-hit.
	FVector EndLocation = TraceEnd;
	// Get the weapon state
	if (TraceDatum.OutHits.Num() > 0)
	{
		const FHitResult TraceHit = TraceDatum.OutHits[0];
		const ERTSSurfaceType SurfaceTypeHit = FRTS_PhysicsHelper::GetRTSSurfaceType(TraceHit.PhysMaterial);
		// Create a rotation that aligns the Z-axis with the ImpactNormal
		const FRotator ImpactRotation = FRotationMatrix::MakeFromZ(TraceHit.ImpactNormal).Rotator();
		if constexpr (DeveloperSettings::Debugging::GPhysicalMaterialSurfaces_Compile_DebugSymbols)
		{
			FRTS_PhysicsHelper::DrawTextSurfaceType(SurfaceTypeHit, TraceHit.ImpactPoint, World,
			                                        2.f);
		}
		if (IsValid(TraceHit.GetActor()))
		{
			OnAsyncTraceHitValidActor(TraceHit, EndLocation, ImpactRotation, SurfaceTypeHit);
		}
		else
		{
			CreateWeaponImpact(TraceHit.ImpactPoint, SurfaceTypeHit,
			                   ImpactRotation);
			EndLocation = TraceHit.ImpactPoint;
		}
	}
	else
	{
		// If no hits, still create impact at the TraceEnd location
		OnAsyncTraceNoHit(TraceEnd);
	}
	if (M_ProjectileManager.IsValid())
	{
		M_ProjectileManager->FireProjectile(
			LaunchLocation,
			EndLocation,
			M_TraceProjectileType,
			ProjectileLaunchTime);
	}
}


void UWeaponStateProjectile::InitProjectileWeapon(
	const int32 NewOwningPlayer,
	const int32 NewWeaponIndex,
	const EWeaponName NewWeaponName,
	const EWeaponFireMode NewWeaponBurstMode,
	TScriptInterface<IWeaponOwner> NewWeaponOwner,
	UMeshComponent* NewMeshComponent,
	const FName NewFireSocketName,
	UWorld* NewWorld,
	EProjectileNiagaraSystem ProjectileNiagaraSystem,
	FWeaponVFX NewWeaponVFX,
	FWeaponShellCase NewWeaponShellCase,
	const float NewBurstCooldown,
	const int32 NewSingleBurstAmountMaxBurstAmount,
	const int32 NewMinBurstAmount,
	const bool bNewCreateShellCasingOnEveryRandomBurst)
{
	M_ProjectileNiagaraSystem = ProjectileNiagaraSystem;
	WeaponHitType = EWeaponSystemType::Projectile;
	InitWeaponState(
		NewOwningPlayer,
		NewWeaponIndex,
		NewWeaponName,
		NewWeaponBurstMode,
		NewWeaponOwner,
		NewMeshComponent,
		NewFireSocketName,
		NewWorld,
		NewWeaponVFX,
		NewWeaponShellCase,
		NewBurstCooldown,
		NewSingleBurstAmountMaxBurstAmount,
		NewMinBurstAmount,
		bNewCreateShellCasingOnEveryRandomBurst);
}

void UWeaponStateProjectile::OnProjectileKilledActor(AActor* KilledActor) const
{
	if (WeaponOwner)
	{
		OnActorKilled(KilledActor);
	}
}

void UWeaponStateProjectile::OnProjectileHit(const bool bBounced) const
{
	if (not WeaponOwner)
	{
		return;
	}
	WeaponOwner->OnProjectileHit(bBounced);
}

void UWeaponStateProjectile::SetupProjectileManager(ASmallArmsProjectileManager* ProjectileManager)
{
	M_ProjectileManager = ProjectileManager;
}

void UWeaponStateProjectile::CopyStateFrom(const UWeaponStateProjectile* Other)
{
}

void UWeaponStateProjectile::FireWeaponSystem()
{
	if (WeaponOwner)
	{
		FireProjectile(WeaponOwner->GetFireDirection(WeaponIndex));
	}
}

bool UWeaponStateProjectile::GetIsValidProjectileManager() const
{
	if (M_ProjectileManager.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(
		"PROJECTILE Weapon is provided with an invalid projectile manager. " + GetName());
	return false;
}

ASmallArmsProjectileManager* UWeaponStateProjectile::GetProjectileManager() const
{
	if (not GetIsValidProjectileManager())
	{
		return nullptr;
	}
	return M_ProjectileManager.Get();
}

EProjectileNiagaraSystem UWeaponStateProjectile::GetProjectileNiagaraSystem() const
{
	return M_ProjectileNiagaraSystem;
}

void UWeaponStateProjectile::FireProjectile(const FVector& TargetDirection)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(PrjWp_FireProjectile);
	ASmallArmsProjectileManager* ProjectileManager = GetProjectileManager();
	if (not ProjectileManager)
	{
		return;
	}

	const TPair<FVector, FVector> LaunchAndForward = GetLaunchAndForwardVector();
	const FVector LaunchLocation = LaunchAndForward.Key;

	// Create a rotation from the target direction.
	FRotator LaunchRotation = TargetDirection.Rotation();

	// Apply accuracy deviation to the launch rotation.
	LaunchRotation = FRTSWeaponHelpers::ApplyAccuracyDeviationForProjectile(LaunchRotation, WeaponData.Accuracy);

	AProjectile* SpawnedProjectile = ProjectileManager->GetDormantTankProjectile();

	if (not IsValid(SpawnedProjectile))
	{
		ReportErrorForWeapon("PROJECTILE weapon failed to get dormant projectile from pool manager.");
		return;
	}

	// Apply shell type-specific data if necessary.
	const bool bIsAPShell = WeaponData.ShellType == EWeaponShellType::Shell_AP || WeaponData.ShellType ==
		EWeaponShellType::Shell_APHE;
	const bool bIsFireShell = WeaponData.ShellType == EWeaponShellType::Shell_Fire;
	if (not bIsAPShell && not bIsFireShell)
	{
		const FWeaponData ShellAdjustedData = GLOBAL_GetWeaponDataForShellType(WeaponData);
		FireProjectileWithShellAdjustedStats(ShellAdjustedData, SpawnedProjectile, LaunchLocation, LaunchRotation);
	}
	else
	{
		FireProjectileWithShellAdjustedStats(WeaponData, SpawnedProjectile, LaunchLocation, LaunchRotation);
	}
}

void UWeaponStateProjectile::FireProjectileWithShellAdjustedStats(const FWeaponData& ShellAdjustedData,
                                                                  AProjectile* Projectile,
                                                                  const FVector& LaunchLocation,
                                                                  const FRotator& LaunchRotation)
{
	constexpr float PenFluxFactorHigh = 1 + DeveloperSettings::GameBalance::Weapons::ArmorPenFluxPercentage / 100;
	constexpr float PenFluxFactorLow = 1 - DeveloperSettings::GameBalance::Weapons::ArmorPenFluxPercentage / 100;
	const float PenAdjustedWithGameFlux = FMath::RandRange(ShellAdjustedData.ArmorPen * PenFluxFactorLow,
	                                                       ShellAdjustedData.ArmorPen * PenFluxFactorHigh);
	const float PenMaxRangeAdjustedWithGameFlux = FMath::RandRange(
		ShellAdjustedData.ArmorPenMaxRange * PenFluxFactorLow,
		ShellAdjustedData.ArmorPenMaxRange * PenFluxFactorHigh);

	FProjectileVfxSettings ProjectileVfxSettings;
	ProjectileVfxSettings.ShellType = ShellAdjustedData.ShellType;
	ProjectileVfxSettings.WeaponCaliber = ShellAdjustedData.WeaponCalibre ;
	ProjectileVfxSettings.ProjectileNiagaraSystem = M_ProjectileNiagaraSystem;

	Projectile->SetupProjectileForNewLaunch(this, WeaponData.DamageType, ShellAdjustedData.Range,
	                                        ShellAdjustedData.BaseDamage,
	                                        PenAdjustedWithGameFlux,
	                                        PenMaxRangeAdjustedWithGameFlux,
	                                        ShellAdjustedData.ShrapnelParticles, ShellAdjustedData.ShrapnelRange,
	                                        ShellAdjustedData.ShrapnelDamage,
	                                        ShellAdjustedData.ShrapnelPen, OwningPlayer,
	                                        M_WeaponVfx.SurfaceImpactEffects, M_WeaponVfx.BounceEffect,
	                                        M_WeaponVfx.BounceSound,
	                                        M_WeaponVfx.ImpactScale,
	                                        M_WeaponVfx.BounceScale,
	                                        ShellAdjustedData.ProjectileMovementSpeed,
	                                        LaunchLocation, LaunchRotation,
	                                        M_WeaponVfx.ImpactAttenuation,
	                                        M_WeaponVfx.ImpactConcurrency, ProjectileVfxSettings, WeaponData.ShellType,
	                                        ActorsToIgnore,
	                                        ShellAdjustedData.WeaponCalibre);
}

void UWeaponStateArchProjectile::InitArchProjectileWeapon(
	const int32 NewOwningPlayer,
	const int32 NewWeaponIndex,
	const EWeaponName NewWeaponName,
	const EWeaponFireMode NewWeaponBurstMode,
	TScriptInterface<IWeaponOwner> NewWeaponOwner,
	UMeshComponent* NewMeshComponent,
	const FName NewFireSocketName,
	UWorld* NewWorld,
	const EProjectileNiagaraSystem ProjectileNiagaraSystem,
	FWeaponVFX NewWeaponVFX,
	FWeaponShellCase NewWeaponShellCase,
	const float NewBurstCooldown,
	const int32 NewSingleBurstAmountMaxBurstAmount,
	const int32 NewMinBurstAmount,
	const bool bNewCreateShellCasingOnEveryRandomBurst,
	const FArchProjectileSettings& NewArchSettings)
{
	M_ArchSettings = NewArchSettings;
	InitProjectileWeapon(
		NewOwningPlayer,
		NewWeaponIndex,
		NewWeaponName,
		NewWeaponBurstMode,
		NewWeaponOwner,
		NewMeshComponent,
		NewFireSocketName,
		NewWorld,
		ProjectileNiagaraSystem,
		NewWeaponVFX,
		NewWeaponShellCase,
		NewBurstCooldown,
		NewSingleBurstAmountMaxBurstAmount,
		NewMinBurstAmount,
		bNewCreateShellCasingOnEveryRandomBurst);
}

void UWeaponStateArchProjectile::FireWeaponSystem()
{
	if (WeaponOwner)
	{
		// Fire the projectile towards the target direction
		FireProjectile(WeaponOwner->GetTargetLocation(WeaponIndex));
	}
}

void UWeaponStateArchProjectile::FireProjectile(const FVector& TargetLocationRaw)
{
	ASmallArmsProjectileManager* ProjectileManager = GetProjectileManager();
	if (not ProjectileManager)
	{
		return;
	}

	const TPair<FVector, FVector> LaunchAndForward = GetLaunchAndForwardVector();
	const FVector LaunchLocation = LaunchAndForward.Key;

	// Apply accuracy deviation to the *location* (unchanged logic).
	FVector TargetLocation = FRTSWeaponHelpers::ApplyAccuracyDeviationForArchWeapon(
		TargetLocationRaw, WeaponData.Accuracy);

	FVector LaunchDirection = (TargetLocation - LaunchLocation).GetSafeNormal();
	if (LaunchDirection.IsNearlyZero())
	{
		LaunchDirection = LaunchAndForward.Value.GetSafeNormal();
	}
	const FRotator LaunchRotation = LaunchDirection.Rotation();

	AProjectile* SpawnedProjectile = ProjectileManager->GetDormantTankProjectile();

	if (not IsValid(SpawnedProjectile))
	{
		ReportErrorForWeapon("ARCH PROJECTILE weapon failed to get dormant projectile from pool manager.");
		return;
	}

	// Apply shell type-specific data if necessary.
	const bool bIsAPShell = (WeaponData.ShellType == EWeaponShellType::Shell_AP) || (WeaponData.ShellType ==
		EWeaponShellType::Shell_APHE);
	const bool bIsFireShell = (WeaponData.ShellType == EWeaponShellType::Shell_Fire);
	if (not bIsAPShell && not bIsFireShell)
	{
		FWeaponData ShellAdjustedData = GLOBAL_GetWeaponDataForShellType(WeaponData);
		FireProjectileWithShellAdjustedStats(
			ShellAdjustedData,
			SpawnedProjectile,
			LaunchLocation,
			LaunchRotation,
			TargetLocation);
	}
	else
	{
		// no data alteration needed.
		FireProjectileWithShellAdjustedStats(WeaponData, SpawnedProjectile, LaunchLocation, LaunchRotation,
		                                     TargetLocation);
	}
}


void UWeaponStateArchProjectile::FireProjectileWithShellAdjustedStats(const FWeaponData& ShellAdjustedData,
                                                                      AProjectile* Projectile,
                                                                      const FVector& LaunchLocation,
                                                                      const FRotator& LaunchRotation,
                                                                      const FVector& TargetLocation)
{
	constexpr float PenFluxFactorHigh = 1 + DeveloperSettings::GameBalance::Weapons::ArmorPenFluxPercentage / 100;
	constexpr float PenFluxFactorLow = 1 - DeveloperSettings::GameBalance::Weapons::ArmorPenFluxPercentage / 100;
	const float PenAdjustedWithGameFlux = FMath::RandRange(ShellAdjustedData.ArmorPen * PenFluxFactorLow,
	                                                       ShellAdjustedData.ArmorPen * PenFluxFactorHigh);
	const float PenMaxRangeAdjustedWithGameFlux = FMath::RandRange(
		ShellAdjustedData.ArmorPenMaxRange * PenFluxFactorLow,
		ShellAdjustedData.ArmorPenMaxRange * PenFluxFactorHigh);

	FProjectileVfxSettings ProjectileVfxSettings;
	ProjectileVfxSettings.ShellType = ShellAdjustedData.ShellType;
	ProjectileVfxSettings.WeaponCaliber = ShellAdjustedData.WeaponCalibre;
	ProjectileVfxSettings.ProjectileNiagaraSystem = GetProjectileNiagaraSystem();

	Projectile->SetupProjectileForNewLaunch(this, WeaponData.DamageType, ShellAdjustedData.Range,
	                                        ShellAdjustedData.BaseDamage, PenAdjustedWithGameFlux,
	                                        PenMaxRangeAdjustedWithGameFlux,
	                                        ShellAdjustedData.ShrapnelParticles, ShellAdjustedData.ShrapnelRange,
	                                        ShellAdjustedData.ShrapnelDamage,
	                                        ShellAdjustedData.ShrapnelPen, OwningPlayer,
	                                        M_WeaponVfx.SurfaceImpactEffects, M_WeaponVfx.BounceEffect,
	                                        M_WeaponVfx.BounceSound,
	                                        M_WeaponVfx.ImpactScale,
	                                        M_WeaponVfx.BounceScale,
	                                        ShellAdjustedData.ProjectileMovementSpeed,
	                                        LaunchLocation, LaunchRotation,
	                                        M_WeaponVfx.ImpactAttenuation,
	                                        M_WeaponVfx.ImpactConcurrency, ProjectileVfxSettings, WeaponData.ShellType,
	                                        ActorsToIgnore,
	                                        ShellAdjustedData.WeaponCalibre);

	Projectile->SetupArcedLaunch(LaunchLocation, TargetLocation, ShellAdjustedData.ProjectileMovementSpeed,
	                             ShellAdjustedData.Range, M_ArchSettings, M_WeaponVfx.LaunchAttenuation,
	                             M_WeaponVfx.LaunchConcurrency);
}

void UWeaponStateRocketProjectile::InitRocketProjectileWeapon(
	const int32 NewOwningPlayer,
	const int32 NewWeaponIndex,
	const EWeaponName NewWeaponName,
	const EWeaponFireMode NewWeaponBurstMode,
	TScriptInterface<IWeaponOwner> NewWeaponOwner,
	UMeshComponent* NewMeshComponent,
	const FName NewFireSocketName,
	UWorld* NewWorld,
	const EProjectileNiagaraSystem ProjectileNiagaraSystem,
	FWeaponVFX NewWeaponVFX,
	FWeaponShellCase NewWeaponShellCase,
	const FRocketWeaponSettings& NewRocketSettings,
	const float NewBurstCooldown,
	const int32 NewSingleBurstAmountMaxBurstAmount,
	const int32 NewMinBurstAmount,
	const bool bNewCreateShellCasingOnEveryRandomBurst)
{
	constexpr int32 InitialRocketSocketIndex = 0;

	M_RocketSettings = NewRocketSettings;
	M_NextRocketSocketIndex = InitialRocketSocketIndex;
	InitProjectileWeapon(
		NewOwningPlayer,
		NewWeaponIndex,
		NewWeaponName,
		NewWeaponBurstMode,
		NewWeaponOwner,
		NewMeshComponent,
		NewFireSocketName,
		NewWorld,
		ProjectileNiagaraSystem,
		NewWeaponVFX,
		NewWeaponShellCase,
		NewBurstCooldown,
		NewSingleBurstAmountMaxBurstAmount,
		NewMinBurstAmount,
		bNewCreateShellCasingOnEveryRandomBurst);
}

void UWeaponStateRocketProjectile::FireWeaponSystem()
{
	if (WeaponOwner)
	{
		FireProjectile(WeaponOwner->GetTargetLocation(WeaponIndex));
	}
}

TPair<FVector, FVector> UWeaponStateRocketProjectile::GetLaunchAndForwardVectorForRocketSocket()
{
	if (not IsValid(MeshComponent))
	{
		return TPair<FVector, FVector>(FVector::ZeroVector, FVector::ZeroVector);
	}

	constexpr int32 FirstSocketIndex = 0;
	constexpr int32 SocketIndexIncrement = 1;
	const int32 SocketCount = M_RocketSettings.FireSocketNames.Num();
	const bool bHasSocketOverrides = SocketCount > FirstSocketIndex;
	const int32 MaxSocketIndex = SocketCount - SocketIndexIncrement;
	const int32 SelectedSocketIndex = bHasSocketOverrides
		? FMath::Clamp(M_NextRocketSocketIndex, FirstSocketIndex, MaxSocketIndex)
		: FirstSocketIndex;
	const FName SelectedSocketName = bHasSocketOverrides
		? M_RocketSettings.FireSocketNames[SelectedSocketIndex]
		: FireSocketName;

	if (bHasSocketOverrides)
	{
		M_NextRocketSocketIndex = (SelectedSocketIndex + SocketIndexIncrement) % SocketCount;
	}

	const FName SocketNameToUse = SelectedSocketName.IsNone() ? FireSocketName : SelectedSocketName;
	const FVector LaunchLocation = MeshComponent->GetSocketLocation(SocketNameToUse);
	const FQuat SocketRotation = MeshComponent->GetSocketQuaternion(SocketNameToUse);
	const FVector ForwardVector = SocketRotation.GetForwardVector();

	// draw debug of socket forward vector 200 units.
	if constexpr (DeveloperSettings::Debugging::GTurret_Master_Compile_DebugSymbols)
	{
		if (IsValid(World))
		{
			constexpr float DebugLineLength = 200.0f;
			constexpr float DebugLineLifetime = 0.1f;
			constexpr int32 DebugLineDepthPriority = 0;
			constexpr float DebugLineThickness = 5.0f;

			DrawDebugLine(World,
			              LaunchLocation,
			              LaunchLocation + ForwardVector * DebugLineLength,
			              FColor::Red,
			              false,
			              DebugLineLifetime,
			              DebugLineDepthPriority,
			              DebugLineThickness);
		}
	}

	return TPair<FVector, FVector>(LaunchLocation, ForwardVector);
}

void UWeaponStateRocketProjectile::FireProjectile(const FVector& TargetLocationRaw)
{
	ASmallArmsProjectileManager* ProjectileManager = GetProjectileManager();
	if (not ProjectileManager)
	{
		return;
	}

	const TPair<FVector, FVector> LaunchAndForward = GetLaunchAndForwardVectorForRocketSocket();
	const FVector LaunchLocation = LaunchAndForward.Key;

	FVector TargetLocation = FRTSWeaponHelpers::ApplyAccuracyDeviationForArchWeapon(
		TargetLocationRaw, WeaponData.Accuracy);

	FVector LaunchDirection = (TargetLocation - LaunchLocation).GetSafeNormal();
	if (LaunchDirection.IsNearlyZero())
	{
		LaunchDirection = LaunchAndForward.Value.GetSafeNormal();
	}
	const FRotator LaunchRotation = LaunchDirection.Rotation();

	AProjectile* SpawnedProjectile = ProjectileManager->GetDormantTankProjectile();

	if (not IsValid(SpawnedProjectile))
	{
		ReportErrorForWeapon("ROCKET PROJECTILE weapon failed to get dormant projectile from pool manager.");
		return;
	}

	const bool bIsAPShell = (WeaponData.ShellType == EWeaponShellType::Shell_AP) || (WeaponData.ShellType ==
		EWeaponShellType::Shell_APHE);
	const bool bIsFireShell = (WeaponData.ShellType == EWeaponShellType::Shell_Fire);
	if (not bIsAPShell && not bIsFireShell)
	{
		FWeaponData ShellAdjustedData = GLOBAL_GetWeaponDataForShellType(WeaponData);
		FireProjectileWithShellAdjustedStats(
			ShellAdjustedData,
			SpawnedProjectile,
			LaunchLocation,
			LaunchRotation,
			TargetLocation);
	}
	else
	{
		FireProjectileWithShellAdjustedStats(
			WeaponData,
			SpawnedProjectile,
			LaunchLocation,
			LaunchRotation,
			TargetLocation);
	}
}

void UWeaponStateRocketProjectile::FireProjectileWithShellAdjustedStats(const FWeaponData& ShellAdjustedData,
                                                                        AProjectile* Projectile,
                                                                        const FVector& LaunchLocation,
                                                                        const FRotator& LaunchRotation,
                                                                        const FVector& TargetLocation)
{
	constexpr float PenFluxFactorHigh = 1 + DeveloperSettings::GameBalance::Weapons::ArmorPenFluxPercentage / 100;
	constexpr float PenFluxFactorLow = 1 - DeveloperSettings::GameBalance::Weapons::ArmorPenFluxPercentage / 100;
	const float PenAdjustedWithGameFlux = FMath::RandRange(ShellAdjustedData.ArmorPen * PenFluxFactorLow,
	                                                       ShellAdjustedData.ArmorPen * PenFluxFactorHigh);
	const float PenMaxRangeAdjustedWithGameFlux = FMath::RandRange(
		ShellAdjustedData.ArmorPenMaxRange * PenFluxFactorLow,
		ShellAdjustedData.ArmorPenMaxRange * PenFluxFactorHigh);

	FProjectileVfxSettings ProjectileVfxSettings;
	ProjectileVfxSettings.ShellType = ShellAdjustedData.ShellType;
	ProjectileVfxSettings.WeaponCaliber = ShellAdjustedData.WeaponCalibre;
	ProjectileVfxSettings.ProjectileNiagaraSystem = GetProjectileNiagaraSystem();

	Projectile->SetupProjectileForNewLaunch(this, WeaponData.DamageType, ShellAdjustedData.Range,
	                                        ShellAdjustedData.BaseDamage, PenAdjustedWithGameFlux,
	                                        PenMaxRangeAdjustedWithGameFlux,
	                                        ShellAdjustedData.ShrapnelParticles, ShellAdjustedData.ShrapnelRange,
	                                        ShellAdjustedData.ShrapnelDamage,
	                                        ShellAdjustedData.ShrapnelPen, OwningPlayer,
	                                        M_WeaponVfx.SurfaceImpactEffects, M_WeaponVfx.BounceEffect,
	                                        M_WeaponVfx.BounceSound,
	                                        M_WeaponVfx.ImpactScale,
	                                        M_WeaponVfx.BounceScale,
	                                        ShellAdjustedData.ProjectileMovementSpeed,
	                                        LaunchLocation, LaunchRotation,
	                                        M_WeaponVfx.ImpactAttenuation,
	                                        M_WeaponVfx.ImpactConcurrency, ProjectileVfxSettings, WeaponData.ShellType,
	                                        ActorsToIgnore,
	                                        ShellAdjustedData.WeaponCalibre);

	Projectile->SetupRocketSwingLaunch(LaunchLocation, TargetLocation, ShellAdjustedData.ProjectileMovementSpeed,
	                                   M_RocketSettings);

	if (M_RocketSettings.RocketMesh)
	{
		Projectile->SetupAttachedRocketMesh(M_RocketSettings.RocketMesh);
	}
}
