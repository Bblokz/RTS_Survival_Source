// Copyright (C) Bas Blokzijl - All rights reserved.

#include "AircraftDeathComponent.h"

#include "AircraftDeathMath.h"
#include "AircraftWreckActor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "TimerManager.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSAircraftDeath, Log, All);

namespace AircraftDeathPrivate
{
	// The smoke the wreck left in the air drifts on after the impact before the aircraft actor goes.
	constexpr float TrailFadeSeconds = 8.f;
	// Without propeller bones the fire sits this far out on a wing, as a share of the half span.
	constexpr float FallbackWingEngineSpanShare = 0.35f;
	// A wreck that hits something steep (a wall) still explodes upright.
	constexpr float MinimumUpwardNormalZ = 0.2f;
	// The pieces start this far above the ground at their lowest point.
	constexpr float WreckGroundClearance = 15.f;
	// A landed aircraft this close above the ground (beyond its own height) is parked and breaks up where it stands.
	constexpr float ParkedHeightTolerance = 400.f;
	constexpr float ParkedScatterShare = 0.2f;
	// The jet flame streams back from the engine as a tongue of fire a few metres long at this size.
	constexpr float EngineFireScale = 1.f;
	constexpr float SmokeTrailPuffScale = 0.6f;
	// Half the wingspan of the Bf 109 mesh; bigger aircraft burn and explode bigger, up to the maximum.
	constexpr float FighterHalfSpan = 700.f;
	constexpr float MaxSizeEffectScale = 2.f;
	const FRotator FlameStreamsBack(0.f, 180.f, 0.f);
	constexpr float ExplosionLift = 180.f;

	const TCHAR* const EngineFireVfxPath = TEXT("/Game/RTS_Survival/SpecialEffects/ArmyVFX/Niagara/Jet/NS_Jet_Fire_Continuous.NS_Jet_Fire_Continuous");
	const TCHAR* const SmokeTrailVfxPath = TEXT("/Game/RTS_Survival/SpecialEffects/Smoke/SmokeVol1/Niagara/NS_SmokePuff4a.NS_SmokePuff4a");
	const TCHAR* const ImpactExplosionVfxPath = TEXT("/Game/RTS_Survival/SpecialEffects/AA_OptimExplosions/NS_RTS_Explosion_Big_9.NS_RTS_Explosion_Big_9");
	const TCHAR* const GroundFireVfxPath = TEXT("/Game/RTS_Survival/SpecialEffects/AllExplosions/TankExpl/NS_LargeTank_OnFire.NS_LargeTank_OnFire");
	const TCHAR* const AirBreakupVfxPath = TEXT("/Game/RTS_Survival/SpecialEffects/AA_OptimExplosions/NS_RTS_Explosion_Medium_1.NS_RTS_Explosion_Medium_1");
	const TCHAR* const GroundBurstVfxPath = TEXT("/Game/RTS_Survival/SpecialEffects/AllExplosions/Niagara/Sand/NS_Explosion_Sand_6.NS_Explosion_Sand_6");
	const TCHAR* const FuelFireballVfxPath = TEXT("/Game/RTS_Survival/SpecialEffects/AA_OptimExplosions/N_RTS_Explosion_Fuel_01.N_RTS_Explosion_Fuel_01");
	const TCHAR* const SecondaryExplosionVfxPath = TEXT("/Game/RTS_Survival/SpecialEffects/AA_OptimExplosions/NS_RTS_Explosion_Medium_3.NS_RTS_Explosion_Medium_3");
	const TCHAR* const BurningDebrisVfxPath = TEXT("/Game/RTS_Survival/SpecialEffects/Fire/NFire_debris_00.NFire_debris_00");

	// The smoke puff system loops forever; after its first burst it is switched off and its smoke drifts away.
	constexpr float SmokePuffBurstSeconds = 0.15f;
	constexpr float AirBreakupExplosionScale = 0.45f;
	constexpr float GroundBurstScale = 0.8f;
	constexpr float FuelFireballScale = 1.1f;
	constexpr float SecondaryExplosionScale = 0.55f;
	constexpr float BurningDebrisScale = 0.5f;

	// In the air: a nose-fire aircraft loses its tail (else a wing tip) this often.
	constexpr float NoseFireTailShare = 0.6f;
	// Tail pieces: the rear part of the aircraft by this share of its length.
	constexpr float TailLengthShare = 0.25f;
	// The outer wing panel of a burning wing starts this far beyond the engine, as a share of the half span.
	constexpr float OuterWingMarginShare = 0.08f;
	// A wing tip: the pieces beyond this share of the half span.
	constexpr float WingTipSpanShare = 0.55f;
	// A wreck with fewer pieces does not tear apart in the air; a tear takes at least this many.
	constexpr int32 MinPiecesForAirBreakup = 4;
	constexpr int32 MinTornPieces = 2;
	// A torn-off part flies off sideways, up and back by these shares of AirBreakupSeparationSpeed, randomised by the range.
	constexpr float TornPartSideKickShare = 0.7f;
	constexpr float TornPartUpKickShare = 0.5f;
	constexpr float TornPartBackKickShare = 0.3f;
	const FVector2D TornPartKickRandomRange(0.6f, 1.2f);
	// Smoke from a torn part stops once it moved slower than the minimum after this long (it landed).
	constexpr float TornPartSmokeMinSeconds = 0.3f;
	// Torn-off parts spin; their smoke trail lasts this long, puffed more sparsely than the wreck's own.
	constexpr float TornPartSpinRadians = 5.f;
	constexpr float TornPartSmokeSeconds = 3.5f;
	constexpr float TornPartSmokeSpacingMlt = 1.6f;
	constexpr float TornPartSmokeScale = 0.6f;
	constexpr float TornPartMinSmokeSpeed = 250.f;

	// On the ground. Blasts in time after the nose hits.
	constexpr float MainBlastSeconds = 0.08f;
	constexpr float FuelBlastSeconds = 0.2f;
	const FVector2D SecondaryBlastSeconds(0.45f, 1.4f);
	constexpr float BurningDebrisSeconds = 2.f;
	constexpr int32 MaxSecondaryBlasts = 3;
	constexpr int32 FighterBurningPieces = 2;
	constexpr int32 MaxBurningPieces = 4;
	// The hull slows to this share of its skid speed by the time it has come apart.
	constexpr float SkidEndSpeedShare = 0.3f;
	// A held piece whose lowest point comes this close to the ground is torn off at once.
	constexpr float GroundReleaseClearance = 30.f;
	// A held piece is let go this long before it would reach the ground; it is made dynamic a few frames later.
	constexpr float GroundReleaseLookAheadSeconds = 0.08f;
	// No piece of the hull moves faster than this while it levels on the ground.
	constexpr float MaxHullPieceSpeed = 2500.f;
	// Peak over average speed of the smoothstep the hull levels with.
	constexpr float SmoothstepPeakSpeedFactor = 1.5f;
	// At low frame rates the hull moves far per frame: look ahead at least this many frames.
	constexpr float GroundReleaseLookAheadFrames = 3.f;
	// Share of a released piece's speed into the ground the ground takes away: all of it. A piece is let go within
	// 30 cm of the ground and needs none of it; with 15 % kept, small Fw 189 pieces still fell through.
	constexpr float GroundAbsorbShare = 1.f;
	// Kick of a piece torn off on the ground, as shares of PieceScatterSpeed.
	const FVector2D PieceUpKickShare(0.2f, 0.6f);
	constexpr float PieceSideKickShare = 0.5f;
	constexpr float PieceForwardKickShare = 0.3f;
	const FVector2D PieceSpinRadians(2.f, 7.f);
	// What still holds together when the hull has come apart is let go with this share of the scatter, and some spin.
	constexpr float FinalPieceUpKickShare = 0.2f;
	constexpr float FinalPieceSpinRadians = 2.f;
	const FVector2D SecondaryBlastScaleRange(0.8f, 1.2f);
	const FVector2D BurningDebrisScaleRange(0.7f, 1.2f);
	constexpr int32 BurningDebrisAttemptsPerFire = 3;
	// The fuel fireball goes up ahead of the hull centre by this share of its half length, a little lower than the main blast.
	constexpr float FuelBlastForwardShare = 0.5f;
	constexpr float FuelBlastLiftShare = 0.6f;
	constexpr float SecondaryBlastLiftShare = 0.3f;
	const TCHAR* const EngineHitSoundPath = TEXT("/Game/Explosion_Sounds/CUE/City/Fast/Small/Explosion_City_Fast_Small_MedDist-01_Cue.Explosion_City_Fast_Small_MedDist-01_Cue");
	const TCHAR* const ImpactSoundPath = TEXT("/Game/Explosion_Sounds/CUE/City/Fast/Large/Explosion_City_Fast_Large_MedDist-01_Cue.Explosion_City_Fast_Large_MedDist-01_Cue");

	// Ground under the wreck is sampled on a grid this many points wide; uneven ground under a wing is found too.
	constexpr int32 WreckGroundSamplesPerSide = 4;
	constexpr float WreckGroundTraceMargin = 500.f;
	// Higher ground than this above the impact is a tree or a wall beside the wreck, not ground to lift it onto.
	constexpr float MaxGroundRiseUnderWreck = 300.f;

	/**
	 * @return Whether the collection has geometry; OutPlacedBox then holds its pieces placed at Placement.
	 * The aircraft's mesh bounds reach far below its wheels (skeletal bounds are padded).
	 */
	bool TryGetPlacedCollectionBox(const UGeometryCollection* Collection, const FTransform& Placement,
	                               FBox& OutPlacedBox)
	{
		if (not IsValid(Collection) || not Collection->GetGeometryCollection().IsValid())
		{
			return false;
		}
		const FGeometryCollection& Geometry = *Collection->GetGeometryCollection();
		FBox CollectionBox(ForceInit);
		for (int32 GeometryIndex = 0; GeometryIndex < Geometry.BoundingBox.Num(); ++GeometryIndex)
		{
			const int32 TransformIndex = Geometry.TransformIndex[GeometryIndex];
			const FTransform PieceTransform = Geometry.Transform.IsValidIndex(TransformIndex)
				                                  ? FTransform(Geometry.Transform[TransformIndex])
				                                  : FTransform::Identity;
			CollectionBox += Geometry.BoundingBox[GeometryIndex].TransformBy(PieceTransform);
		}
		if (not CollectionBox.IsValid)
		{
			return false;
		}
		OutPlacedBox = CollectionBox.TransformBy(Placement);
		return true;
	}

	void ApplyEffectParameters(UNiagaraComponent* Effect, const FAircraftDeathEffect& Settings)
	{
		if (not IsValid(Effect))
		{
			return;
		}
		for (const TPair<FName, float>& Parameter : Settings.FloatParameters)
		{
			Effect->SetVariableFloat(Parameter.Key, Parameter.Value);
		}
		for (const TPair<FName, FVector>& Parameter : Settings.VectorParameters)
		{
			Effect->SetVariableVec3(Parameter.Key, Parameter.Value);
		}
		for (const TPair<FName, FLinearColor>& Parameter : Settings.ColorParameters)
		{
			Effect->SetVariableLinearColor(Parameter.Key, Parameter.Value);
		}
	}

	/** @brief Adds the pieces on one side of the aircraft beyond MinOutwardY, keyed outermost first. */
	void RankWingPieces(const TArray<FAircraftWreckPiece>& Pieces, const float SideSign, const float MinOutwardY,
	                    TArray<TPair<float, int32>>& OutRanked)
	{
		for (int32 PieceIndex = 0; PieceIndex < Pieces.Num(); ++PieceIndex)
		{
			const float OutwardY = static_cast<float>(Pieces[PieceIndex].LocalCentre.Y) * SideSign;
			if (OutwardY > MinOutwardY)
			{
				OutRanked.Emplace(-OutwardY, PieceIndex);
			}
		}
	}

	/** @brief Adds the pieces behind TailLimitX, keyed rearmost first. */
	void RankTailPieces(const TArray<FAircraftWreckPiece>& Pieces, const float TailLimitX,
	                    TArray<TPair<float, int32>>& OutRanked)
	{
		for (int32 PieceIndex = 0; PieceIndex < Pieces.Num(); ++PieceIndex)
		{
			const float PieceX = static_cast<float>(Pieces[PieceIndex].LocalCentre.X);
			if (PieceX < TailLimitX)
			{
				OutRanked.Emplace(PieceX, PieceIndex);
			}
		}
	}

	template <typename AssetType>
	AssetType* FindDefaultAsset(const TCHAR* const Path)
	{
		ConstructorHelpers::FObjectFinder<AssetType> Finder(Path);
		return Finder.Succeeded() ? Finder.Object : nullptr;
	}
}

UAircraftDeathComponent::UAircraftDeathComponent()
{
	using namespace AircraftDeathPrivate;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// Before physics: the wreck's pieces follow the aircraft in the same frame, under the engine fire.
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	// Defaults every aircraft shares; found here so they are loaded with the class and cooked with it.
	M_DeathSettings.EngineFire.System = FindDefaultAsset<UNiagaraSystem>(EngineFireVfxPath);
	M_DeathSettings.SmokeTrail.System = FindDefaultAsset<UNiagaraSystem>(SmokeTrailVfxPath);
	M_DeathSettings.ImpactExplosion.System = FindDefaultAsset<UNiagaraSystem>(ImpactExplosionVfxPath);
	M_DeathSettings.GroundFire.System = FindDefaultAsset<UNiagaraSystem>(GroundFireVfxPath);
	M_DeathSettings.AirBreakupExplosion.System = FindDefaultAsset<UNiagaraSystem>(AirBreakupVfxPath);
	M_DeathSettings.AirBreakupExplosion.Scale = AirBreakupExplosionScale;
	M_DeathSettings.GroundBurst.System = FindDefaultAsset<UNiagaraSystem>(GroundBurstVfxPath);
	M_DeathSettings.GroundBurst.Scale = GroundBurstScale;
	M_DeathSettings.FuelFireball.System = FindDefaultAsset<UNiagaraSystem>(FuelFireballVfxPath);
	M_DeathSettings.FuelFireball.Scale = FuelFireballScale;
	M_DeathSettings.SecondaryExplosion.System = FindDefaultAsset<UNiagaraSystem>(SecondaryExplosionVfxPath);
	M_DeathSettings.SecondaryExplosion.Scale = SecondaryExplosionScale;
	M_DeathSettings.BurningDebris.System = FindDefaultAsset<UNiagaraSystem>(BurningDebrisVfxPath);
	M_DeathSettings.BurningDebris.Scale = BurningDebrisScale;
	M_DeathSettings.EngineFire.Scale = EngineFireScale;
	M_DeathSettings.SmokeTrail.Scale = SmokeTrailPuffScale;
	M_DeathSettings.EngineHitSound = FindDefaultAsset<USoundBase>(EngineHitSoundPath);
	M_DeathSettings.ImpactSound = FindDefaultAsset<USoundBase>(ImpactSoundPath);
}

bool UAircraftDeathComponent::GetIsFireBurning() const
{
	return bM_IsFireBurning;
}

bool UAircraftDeathComponent::GetHasCrashCollection() const
{
	return IsValid(M_CrashCollection);
}

void UAircraftDeathComponent::BeginPlay()
{
	Super::BeginPlay();
	M_Aircraft = Cast<AAircraftMaster>(GetOwner());
	(void)GetIsValidAircraft();
	BeginPlay_LoadCrashCollection();
}

bool UAircraftDeathComponent::GetIsValidAircraft() const
{
	if (M_Aircraft.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Aircraft",
	                                                             "UAircraftDeathComponent::GetIsValidAircraft", GetOwner());
	return false;
}

FString UAircraftDeathComponent::GetCrashCollectionPath() const
{
	if (not M_DeathSettings.CrashCollectionOverride.IsNull())
	{
		return M_DeathSettings.CrashCollectionOverride.ToSoftObjectPath().ToString();
	}
	const AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return FString();
	}
	TArray<FString> StaticMeshNames;
	TArray<FString> SkeletalMeshNames;
	TArray<UStaticMeshComponent*> StaticMeshComponents;
	Owner->GetComponents<UStaticMeshComponent>(StaticMeshComponents);
	for (const UStaticMeshComponent* StaticMeshComponent : StaticMeshComponents)
	{
		if (IsValid(StaticMeshComponent) && StaticMeshComponent->IsVisible() && IsValid(StaticMeshComponent->GetStaticMesh()))
		{
			StaticMeshNames.Add(StaticMeshComponent->GetStaticMesh()->GetName());
		}
	}
	TArray<USkeletalMeshComponent*> SkeletalMeshComponents;
	Owner->GetComponents<USkeletalMeshComponent>(SkeletalMeshComponents);
	for (const USkeletalMeshComponent* SkeletalMeshComponent : SkeletalMeshComponents)
	{
		if (IsValid(SkeletalMeshComponent) && IsValid(SkeletalMeshComponent->GetSkeletalMeshAsset()))
		{
			SkeletalMeshNames.Add(SkeletalMeshComponent->GetSkeletalMeshAsset()->GetName());
		}
	}
	return FAircraftDeathMath::GetCrashCollectionObjectPath(
		FAircraftDeathMath::GetCrashCollectionAssetName(StaticMeshNames, SkeletalMeshNames));
}

float UAircraftDeathComponent::GetSizeEffectScale() const
{
	using namespace AircraftDeathPrivate;
	const AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return 1.f;
	}
	// The span of the parked or flying aircraft: its widest mesh across its own right axis.
	float HalfSpan = 0.f;
	TArray<UMeshComponent*> Meshes;
	Owner->GetComponents<UMeshComponent>(Meshes);
	for (const UMeshComponent* MeshComponent : Meshes)
	{
		if (IsValid(MeshComponent) && MeshComponent->IsVisible())
		{
			HalfSpan = FMath::Max(HalfSpan, static_cast<float>(MeshComponent->CalcBounds(FTransform::Identity).BoxExtent.Y));
		}
	}
	return FMath::Clamp(HalfSpan / FighterHalfSpan, 1.f, MaxSizeEffectScale);
}

float UAircraftDeathComponent::GetEffectScale() const
{
	return M_DeathSettings.EffectScale * M_SizeEffectScale;
}

void UAircraftDeathComponent::BeginPlay_LoadCrashCollection()
{
	M_AircraftMesh = IsValid(GetOwner()) ? GetOwner()->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	M_SizeEffectScale = GetSizeEffectScale();
	if (M_AircraftMesh.IsValid())
	{
		M_GroundClearance = M_AircraftMesh->Bounds.BoxExtent.Z;
	}
	const FString CollectionPath = GetCrashCollectionPath();
	if (CollectionPath.IsEmpty())
	{
		return;
	}
	const FSoftObjectPath SoftPath(CollectionPath);
	const IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	if (not AssetRegistry.GetAssetByObjectPath(SoftPath).IsValid())
	{
		// No collection built for these meshes yet: the aircraft still burns and explodes, without pieces.
		UE_LOG(LogTemp, Warning, TEXT("Aircraft %s has no crash collection at %s; run RTS.Aircraft.BuildCrashCollections."),
		       *GetOwner()->GetName(), *CollectionPath);
		return;
	}
	M_CrashCollectionPath = SoftPath;
	TWeakObjectPtr<UAircraftDeathComponent> WeakThis(this);
	UAssetManager::GetStreamableManager().RequestAsyncLoad(SoftPath, [WeakThis, SoftPath]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->M_CrashCollection = Cast<UGeometryCollection>(SoftPath.ResolveObject());
	});
}

void UAircraftDeathComponent::StartDeath(const FVector& VelocityAtDeath)
{
	if (M_DeathPhase != EAircraftDeathPhase::Alive || not GetIsValidAircraft())
	{
		return;
	}
	M_DeathPhase = EAircraftDeathPhase::Crashing;
	if (GetIsParkedOnTheGround())
	{
		// Parked: nothing to fall from, the aircraft blows apart where it stands.
		bM_WasParked = true;
		M_FlightState.Location = M_Aircraft->GetActorLocation();
		Impact(M_Aircraft->GetActorLocation(), FVector::UpVector, false);
		return;
	}
	StartEngineFire();
	StartCrashFlight(VelocityAtDeath);
	// Hashed: aircraft spawned one after the other have neighbouring ids, whose first random numbers barely differ.
	M_Random.Initialize(static_cast<int32>(HashCombine(GetTypeHash(GetOwner()->GetUniqueID()),
	                                                   GetTypeHash(GetOwner()->GetFName()))));
	// From the hit on the wreck is drawn by its crash pieces, held together until they tear off; the engine flash
	// covers the swap. Without a collection the aircraft falls whole and still explodes.
	(void)EnsureCarriedWreck();
	if (M_DeathSettings.bBreakUpInTheAir)
	{
		const FVector2D Delay = M_DeathSettings.AirBreakupDelaySeconds;
		M_AirBreakupSeconds = M_Random.FRandRange(FMath::Min(Delay.X, Delay.Y), FMath::Max(Delay.X, Delay.Y));
	}
	SetComponentTickEnabled(true);
}

bool UAircraftDeathComponent::GetIsParkedOnTheGround() const
{
	using namespace AircraftDeathPrivate;
	const UWorld* World = GetWorld();
	const AAircraftMaster* Aircraft = M_Aircraft.Get();
	if (not IsValid(World) || not IsValid(Aircraft) || Aircraft->GetLandedState() != EAircraftLandingState::Landed)
	{
		return false;
	}
	// An aircraft waiting for its drop order counts as landed while it hangs in the air; that one still falls.
	// From above the aircraft: a parked aircraft's origin can sit exactly on the ground, where a trace starts too low.
	const FVector Start = Aircraft->GetActorLocation() + FVector(0.f, 0.f, M_GroundClearance + ParkedHeightTolerance);
	const FVector End = Aircraft->GetActorLocation() - FVector(0.f, 0.f, ParkedHeightTolerance);
	FHitResult GroundHit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AircraftParkedCheck), false, Aircraft);
	return World->LineTraceSingleByObjectType(GroundHit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic),
	                                          QueryParams);
}

void UAircraftDeathComponent::StartEngineFire()
{
	USkeletalMeshComponent* Mesh = M_AircraftMesh.Get();
	if (not IsValid(Mesh))
	{
		return;
	}
	FVector LocalFireLocation = FVector::ZeroVector;
	ChooseFireAttachPoint(*Mesh, LocalFireLocation);
	bM_IsFireBurning = true;
	const float EffectScale = GetEffectScale();
	// On the mesh at the engine, not on the propeller bone: that bone spins, and on some rotor meshes it points
	// sideways, which would sweep the flame around. The flame streams back along the aircraft.
	if (IsValid(M_DeathSettings.EngineFire.System))
	{
		M_EngineFire = UNiagaraFunctionLibrary::SpawnSystemAttached(
			M_DeathSettings.EngineFire.System, Mesh, NAME_None, LocalFireLocation, AircraftDeathPrivate::FlameStreamsBack,
			FVector(EffectScale * M_DeathSettings.EngineFire.Scale),
			EAttachLocation::KeepRelativeOffset, false, ENCPoolMethod::None, true);
		AircraftDeathPrivate::ApplyEffectParameters(M_EngineFire, M_DeathSettings.EngineFire);
	}
	M_FireLocalOffset = LocalFireLocation;
	M_DistanceSinceSmokePuff = M_DeathSettings.SmokeTrailSpacing;
	if (IsValid(M_DeathSettings.EngineHitSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, M_DeathSettings.EngineHitSound,
		                                      Mesh->GetComponentTransform().TransformPosition(LocalFireLocation));
	}
}

void UAircraftDeathComponent::ChooseFireAttachPoint(USkeletalMeshComponent& Mesh, FVector& OutLocalLocation)
{
	using namespace AircraftDeathPrivate;
	if (not M_DeathSettings.FireSocketOverride.IsNone() && Mesh.DoesSocketExist(M_DeathSettings.FireSocketOverride))
	{
		M_FireAttachName = M_DeathSettings.FireSocketOverride;
		OutLocalLocation = Mesh.GetSocketTransform(M_FireAttachName, RTS_Component).GetLocation();
		const bool bIsOnWing = FAircraftDeathMath::GetSideOfLocalPoint(OutLocalLocation) != EAircraftDamagedSide::None;
		M_FireOrigin = M_DeathSettings.FireOrigin != EAircraftFireOrigin::Automatic
			               ? M_DeathSettings.FireOrigin
			               : (bIsOnWing ? EAircraftFireOrigin::WingEngine : EAircraftFireOrigin::NoseEngine);
	}
	else
	{
		TArray<FName> EngineBones;
		TArray<FVector> EngineLocations;
		GatherEngineBones(Mesh, EngineBones, EngineLocations);
		M_FireOrigin = FAircraftDeathMath::ResolveFireOrigin(M_DeathSettings.FireOrigin, EngineBones.Num());
		const int32 EngineIndex = M_FireOrigin == EAircraftFireOrigin::WingEngine
			                          ? FAircraftDeathMath::PickWingEngineIndex(EngineLocations, GetOwner()->GetUniqueID())
			                          : FAircraftDeathMath::PickNoseEngineIndex(EngineLocations);
		if (EngineLocations.IsValidIndex(EngineIndex))
		{
			M_FireAttachName = EngineBones[EngineIndex];
			OutLocalLocation = EngineLocations[EngineIndex];
		}
		else
		{
			// No propeller bones: the nose or a wing found from the size of the mesh.
			const FBoxSphereBounds LocalBounds = Mesh.CalcBounds(FTransform::Identity);
			const FVector Centre = LocalBounds.Origin;
			const FVector Extent = LocalBounds.BoxExtent;
			OutLocalLocation = M_FireOrigin == EAircraftFireOrigin::WingEngine
				                   ? Centre + FVector(0.f, Extent.Y * FallbackWingEngineSpanShare, 0.f)
				                   : Centre + FVector(Extent.X, 0.f, 0.f);
		}
	}
	M_FlightState.DamagedSide = M_FireOrigin == EAircraftFireOrigin::WingEngine
		                            ? FAircraftDeathMath::GetSideOfLocalPoint(OutLocalLocation)
		                            : EAircraftDamagedSide::None;
}

void UAircraftDeathComponent::GatherEngineBones(const USkeletalMeshComponent& Mesh, TArray<FName>& OutBones,
                                                TArray<FVector>& OutLocations)
{
	for (int32 BoneIndex = 0; BoneIndex < Mesh.GetNumBones(); ++BoneIndex)
	{
		const FName BoneName = Mesh.GetBoneName(BoneIndex);
		if (not BoneName.ToString().StartsWith(FAircraftDeathMath::EngineBonePrefix))
		{
			continue;
		}
		OutBones.Add(BoneName);
		OutLocations.Add(Mesh.GetSocketTransform(BoneName, RTS_Component).GetLocation());
	}
}

void UAircraftDeathComponent::StartCrashFlight(const FVector& VelocityAtDeath)
{
	const AAircraftMaster* Aircraft = M_Aircraft.Get();
	M_FlightState.Location = Aircraft->GetActorLocation();
	M_FlightState.Velocity = FAircraftDeathMath::GetInitialCrashVelocity(
		VelocityAtDeath, Aircraft->GetActorForwardVector(), M_DeathSettings.MinCrashSpeed);
	M_InitialCrashSpeed = M_FlightState.Velocity.Size();
	M_FlightState.RollDegrees = Aircraft->GetActorRotation().Roll;
	M_FlightState.SecondsFalling = 0.f;
}

void UAircraftDeathComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
                                            FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (M_DeathPhase == EAircraftDeathPhase::Crashing)
	{
		TickCrashFlight(DeltaTime);
		return;
	}
	if (bM_IsImpactStaging)
	{
		TickImpactStage(DeltaTime);
		return;
	}
	SetComponentTickEnabled(false);
}

void UAircraftDeathComponent::TickCrashFlight(const float DeltaTime)
{
	AAircraftMaster* Aircraft = M_Aircraft.Get();
	const UWorld* World = GetWorld();
	if (not IsValid(Aircraft) || not IsValid(World))
	{
		SetComponentTickEnabled(false);
		return;
	}
	const FVector OldLocation = M_FlightState.Location;
	const FQuat OldRotation = Aircraft->GetActorQuat();
	FAircraftDeathMath::StepCrashFlight(M_FlightState, M_DeathSettings, M_FireOrigin, DeltaTime, World->GetGravityZ());
	const FQuat NewRotation = FAircraftDeathMath::GetCrashRotation(M_FlightState, Aircraft->GetActorRotation()).Quaternion();
	FHitResult GroundHit;
	bool bNoseHit = false;
	if (FindImpact(OldLocation, OldRotation, M_FlightState.Location, NewRotation, GroundHit, bNoseHit))
	{
		if (bNoseHit)
		{
			// Where the aircraft was when its nose touched; the rest of it is still above the ground.
			M_FlightState.Location = FMath::Lerp(OldLocation, M_FlightState.Location, GroundHit.Time);
			Aircraft->SetActorLocationAndRotation(M_FlightState.Location,
			                                      FQuat::Slerp(OldRotation, NewRotation, GroundHit.Time),
			                                      false, nullptr, ETeleportType::TeleportPhysics);
		}
		else
		{
			M_FlightState.Location = GroundHit.Location + FVector(0.f, 0.f, M_GroundClearance);
			Aircraft->SetActorLocation(M_FlightState.Location, false, nullptr, ETeleportType::TeleportPhysics);
		}
		TickCarriedWreck(DeltaTime);
		Impact(GroundHit.ImpactPoint, GroundHit.ImpactNormal, true);
		return;
	}
	Aircraft->SetActorLocationAndRotation(M_FlightState.Location, NewRotation, false, nullptr,
	                                      ETeleportType::TeleportPhysics);
	M_DistanceSinceSmokePuff += FVector::Dist(OldLocation, M_FlightState.Location);
	DropSmokePuffs();
	TickAirBreakup();
	TickCarriedWreck(DeltaTime);
	if (M_FlightState.SecondsFalling > M_DeathSettings.MaxCrashSeconds)
	{
		// Fell off the map: no ground to plough into.
		Impact(M_FlightState.Location, FVector::UpVector, false);
	}
}

void UAircraftDeathComponent::DropSmokePuffs()
{
	const USkeletalMeshComponent* Mesh = M_AircraftMesh.Get();
	if (not IsValid(M_DeathSettings.SmokeTrail.System) || not IsValid(Mesh)
		|| M_DistanceSinceSmokePuff < M_DeathSettings.SmokeTrailSpacing)
	{
		return;
	}
	M_DistanceSinceSmokePuff = 0.f;
	const FVector FireLocation = Mesh->GetComponentTransform().TransformPosition(M_FireLocalOffset);
	// Smoke grows slower with the size of the aircraft than the flames, or a bomber's trail hides the bomber.
	const float SmokeScale = M_DeathSettings.EffectScale * FMath::Sqrt(M_SizeEffectScale) * M_DeathSettings.SmokeTrail.Scale;
	SpawnSmokePuff(FireLocation, SmokeScale);
}

void UAircraftDeathComponent::SpawnSmokePuff(const FVector& Location, const float Scale)
{
	UWorld* World = GetWorld();
	if (not IsValid(M_DeathSettings.SmokeTrail.System) || not IsValid(World))
	{
		return;
	}
	UNiagaraComponent* Puff = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		this, M_DeathSettings.SmokeTrail.System, Location, FRotator::ZeroRotator, FVector(Scale), true, true,
		ENCPoolMethod::AutoRelease);
	if (not IsValid(Puff))
	{
		return;
	}
	AircraftDeathPrivate::ApplyEffectParameters(Puff, M_DeathSettings.SmokeTrail);
	++M_SmokePuffCount;
	// The puff system loops forever: left on, every puff of the trail kept smoking where the wreck had been, long after
	// the crash. Switched off after its first burst, its smoke drifts away and it returns to the pool.
	TWeakObjectPtr<UNiagaraComponent> WeakPuff(Puff);
	FTimerHandle PuffHandle;
	World->GetTimerManager().SetTimer(PuffHandle, FTimerDelegate::CreateLambda([WeakPuff]()
	{
		if (WeakPuff.IsValid())
		{
			WeakPuff->Deactivate();
		}
	}), AircraftDeathPrivate::SmokePuffBurstSeconds, false);
}

bool UAircraftDeathComponent::FindImpact(const FVector& OldLocation, const FQuat& OldRotation, const FVector& NewLocation,
                                         const FQuat& NewRotation, FHitResult& OutHit, bool& bOutNoseHit) const
{
	bOutNoseHit = false;
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AircraftCrashImpact), false, GetOwner());
	if (M_Wreck.IsValid())
	{
		QueryParams.AddIgnoredActor(M_Wreck.Get());
	}
	const FCollisionObjectQueryParams ObjectParams(ECC_WorldStatic);
	// From the centre of the wreck to where its belly will be: the belly meets the ground first when it falls flat,
	// and a trace that started at the belly could start below the ground and miss it.
	const FVector BellyOffset(0.f, 0.f, -M_GroundClearance);
	FHitResult BellyHit;
	const bool bBellyHit = World->LineTraceSingleByObjectType(BellyHit, OldLocation, NewLocation + BellyOffset,
	                                                          ObjectParams, QueryParams);
	// In a dive the nose leads: it touches the ground while the rest of the aircraft is still in the air.
	const float NoseReach = GetNoseReach();
	FHitResult NoseHit;
	const FVector OldNose = OldLocation + OldRotation.GetForwardVector() * NoseReach;
	const FVector NewNose = NewLocation + NewRotation.GetForwardVector() * NoseReach;
	const bool bNoseHit = NoseReach > 0.f
		&& World->LineTraceSingleByObjectType(NoseHit, OldNose, NewNose, ObjectParams, QueryParams)
		&& not NoseHit.bStartPenetrating;
	FHitResult PieceHit;
	const bool bPieceHit = TraceLowestHeldPiece(FTransform(OldRotation, OldLocation), FTransform(NewRotation, NewLocation),
	                                            QueryParams, PieceHit);
	if (bPieceHit && (not bNoseHit || PieceHit.Time < NoseHit.Time) && (not bBellyHit || PieceHit.Time < BellyHit.Time))
	{
		// Placed like a nose hit: where the aircraft was when that part touched.
		OutHit = PieceHit;
		bOutNoseHit = true;
		return true;
	}
	if (bNoseHit && (not bBellyHit || NoseHit.Time < BellyHit.Time))
	{
		OutHit = NoseHit;
		bOutNoseHit = true;
		return true;
	}
	if (bBellyHit)
	{
		OutHit = BellyHit;
		return true;
	}
	return false;
}

bool UAircraftDeathComponent::TraceLowestHeldPiece(const FTransform& OldTransform, const FTransform& NewTransform,
                                                   const FCollisionQueryParams& QueryParams, FHitResult& OutHit) const
{
	const AAircraftWreckActor* Wreck = M_Wreck.Get();
	const UWorld* World = GetWorld();
	if (not IsValid(Wreck) || not IsValid(World) || not Wreck->GetIsCarriedWreckShown())
	{
		return false;
	}
	// The piece whose bottom will be lowest; its bottom is traced from where it was to where it will be.
	int32 LowestPieceIndex = INDEX_NONE;
	float LowestBottomZ = TNumericLimits<float>::Max();
	FVector LowestBottomOffset = FVector::ZeroVector;
	const TArray<FAircraftWreckPiece>& Pieces = Wreck->GetPieces();
	for (int32 PieceIndex = 0; PieceIndex < Pieces.Num(); ++PieceIndex)
	{
		const FAircraftWreckPiece& Piece = Pieces[PieceIndex];
		if (Piece.bReleased)
		{
			continue;
		}
		const float HalfHeight = static_cast<float>(
			FMath::Abs(NewTransform.GetUnitAxis(EAxis::X).Z) * Piece.LocalExtent.X
			+ FMath::Abs(NewTransform.GetUnitAxis(EAxis::Y).Z) * Piece.LocalExtent.Y
			+ FMath::Abs(NewTransform.GetUnitAxis(EAxis::Z).Z) * Piece.LocalExtent.Z);
		const float BottomZ = static_cast<float>(NewTransform.TransformPosition(Piece.LocalCentre).Z) - HalfHeight;
		if (BottomZ < LowestBottomZ)
		{
			LowestBottomZ = BottomZ;
			LowestPieceIndex = PieceIndex;
			LowestBottomOffset = FVector(0.f, 0.f, -HalfHeight);
		}
	}
	if (LowestPieceIndex == INDEX_NONE)
	{
		return false;
	}
	const FVector LocalCentre = Pieces[LowestPieceIndex].LocalCentre;
	const FVector OldBottom = OldTransform.TransformPosition(LocalCentre) + LowestBottomOffset;
	const FVector NewBottom = NewTransform.TransformPosition(LocalCentre) + LowestBottomOffset;
	const FCollisionObjectQueryParams ObjectParams(ECC_WorldStatic);
	return World->LineTraceSingleByObjectType(OutHit, OldBottom, NewBottom, ObjectParams, QueryParams)
		&& not OutHit.bStartPenetrating;
}

float UAircraftDeathComponent::GetNoseReach() const
{
	if (IsValid(M_CrashCollection) && M_CrashCollection->GetGeometryCollection().IsValid())
	{
		// The collection is baked in the aircraft actor's space: its furthest point along +X is the nose.
		const FGeometryCollection& Geometry = *M_CrashCollection->GetGeometryCollection();
		float NoseX = 0.f;
		for (int32 GeometryIndex = 0; GeometryIndex < Geometry.BoundingBox.Num(); ++GeometryIndex)
		{
			const int32 TransformIndex = Geometry.TransformIndex[GeometryIndex];
			const FTransform PieceTransform = Geometry.Transform.IsValidIndex(TransformIndex)
				                                  ? FTransform(Geometry.Transform[TransformIndex])
				                                  : FTransform::Identity;
			const FBox PlacedBox = FBox(Geometry.BoundingBox[GeometryIndex]).TransformBy(PieceTransform);
			NoseX = FMath::Max(NoseX, static_cast<float>(PlacedBox.Max.X));
		}
		return NoseX;
	}
	const USkeletalMeshComponent* Mesh = M_AircraftMesh.Get();
	const AActor* Owner = GetOwner();
	if (not IsValid(Mesh) || not IsValid(Owner))
	{
		return 0.f;
	}
	const FBox MeshBox = Mesh->CalcBounds(Mesh->GetComponentTransform()).GetBox();
	return static_cast<float>(MeshBox.TransformBy(GetUnscaledAircraftTransform().Inverse()).Max.X);
}

void UAircraftDeathComponent::Impact(const FVector& ImpactLocation, const FVector& ImpactNormal,
                                     const bool bTearApartFromTheNose)
{
	M_DeathPhase = EAircraftDeathPhase::Impacted;
	M_ImpactLocation = ImpactLocation;
	FVector UpwardNormal = ImpactNormal;
	if (UpwardNormal.Z < AircraftDeathPrivate::MinimumUpwardNormalZ)
	{
		UpwardNormal = FVector::UpVector;
	}
	// Trails first: hiding the meshes would hide what is still attached to them.
	Impact_LetTrailsFade();
	// A parked aircraft blows apart where it stands; one that came down ploughs on and comes apart from the nose back.
	// That needs the carried wreck to be up; one that hit the ground before then breaks up the old way.
	const bool bCanTearApart = bTearApartFromTheNose && not bM_WasParked && M_Wreck.IsValid()
		&& M_Wreck->GetIsCarriedWreckShown();
	if (not bCanTearApart && M_Wreck.IsValid() && not M_Wreck->GetIsCarriedWreckShown())
	{
		M_Wreck->Destroy();
		M_Wreck = nullptr;
	}
	if (bCanTearApart)
	{
		Impact_HideAircraft();
		StartImpactStage(ImpactLocation, UpwardNormal);
	}
	else
	{
		SetComponentTickEnabled(false);
		if (M_Wreck.IsValid() && M_Wreck->GetHasHeldPieces())
		{
			// Fell off the map while carried: let everything go.
			for (int32 PieceIndex = 0; PieceIndex < M_Wreck->GetPieces().Num(); ++PieceIndex)
			{
				M_Wreck->ReleasePiece(PieceIndex, FVector::ZeroVector, FVector::ZeroVector);
			}
			M_BreakupPieceCount = M_Wreck->GetPieceCount();
		}
		else
		{
			Impact_SpawnWreck();
		}
		Impact_SpawnEffects(ImpactLocation, UpwardNormal);
		Impact_HideAircraft();
	}
	if (AActor* Owner = GetOwner())
	{
		Owner->SetLifeSpan(AircraftDeathPrivate::TrailFadeSeconds);
	}
}

void UAircraftDeathComponent::Impact_SpawnEffects(const FVector& ImpactLocation, const FVector& ImpactNormal) const
{
	const float EffectScale = GetEffectScale();
	const FRotator AlongGround = FRotationMatrix::MakeFromZ(ImpactNormal).Rotator();
	if (IsValid(M_DeathSettings.ImpactExplosion.System))
	{
		// Above the ground: at ground level half the fireball is cut off by it.
		const FVector ExplosionLocation = ImpactLocation + ImpactNormal * AircraftDeathPrivate::ExplosionLift * EffectScale;
		UNiagaraComponent* Explosion = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this, M_DeathSettings.ImpactExplosion.System, ExplosionLocation, AlongGround,
			FVector(EffectScale * M_DeathSettings.ImpactExplosion.Scale), true, true,
			ENCPoolMethod::None);
		AircraftDeathPrivate::ApplyEffectParameters(Explosion, M_DeathSettings.ImpactExplosion);
	}
	if (IsValid(M_DeathSettings.ImpactSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, M_DeathSettings.ImpactSound, ImpactLocation);
	}
	// The fuel burns on with the wreck and goes when the wreck is cleared away.
	AAircraftWreckActor* Wreck = M_Wreck.Get();
	if (IsValid(M_DeathSettings.GroundFire.System) && IsValid(Wreck))
	{
		UNiagaraComponent* GroundFire = UNiagaraFunctionLibrary::SpawnSystemAttached(
			M_DeathSettings.GroundFire.System, Wreck->GetRootComponent(), NAME_None, ImpactLocation, AlongGround,
			FVector(EffectScale * M_DeathSettings.GroundFire.Scale), EAttachLocation::KeepWorldPosition, false, ENCPoolMethod::None, true);
		AircraftDeathPrivate::ApplyEffectParameters(GroundFire, M_DeathSettings.GroundFire);
		Wreck->AddWreckFire(GroundFire);
		Wreck->StopFiresAfter(M_DeathSettings.WreckFireSeconds);
	}
}

void UAircraftDeathComponent::Impact_SpawnWreck()
{
	UWorld* World = GetWorld();
	const AActor* Owner = GetOwner();
	if (not IsValid(World) || not IsValid(Owner))
	{
		return;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (not IsValid(M_CrashCollection) && M_CrashCollectionPath.IsValid())
	{
		// Shot down before the load at BeginPlay finished.
		M_CrashCollection = Cast<UGeometryCollection>(M_CrashCollectionPath.TryLoad());
	}
	FTransform WreckTransform = GetUnscaledAircraftTransform();
	// The nose-down wreck reaches below the impact point, and the ground under a wing can be higher than under the
	// centre; pieces that start inside the ground are pushed down through it.
	FBox PlacedWreckBox(ForceInit);
	float LowestPointZ = GetLowestVisibleMeshPointZ();
	float GroundZ = M_ImpactLocation.Z;
	if (AircraftDeathPrivate::TryGetPlacedCollectionBox(M_CrashCollection, WreckTransform, PlacedWreckBox))
	{
		LowestPointZ = static_cast<float>(PlacedWreckBox.Min.Z);
		const float HighestGroundZ = GetHighestGroundUnder(PlacedWreckBox);
		GroundZ = FMath::Clamp(HighestGroundZ, GroundZ, GroundZ + AircraftDeathPrivate::MaxGroundRiseUnderWreck);
	}
	const float WreckLift = GroundZ + AircraftDeathPrivate::WreckGroundClearance - LowestPointZ;
	if (WreckLift > 0.f)
	{
		WreckTransform.AddToTranslation(FVector(0.f, 0.f, WreckLift));
	}
	AAircraftWreckActor* Wreck = World->SpawnActor<AAircraftWreckActor>(
		AAircraftWreckActor::StaticClass(), WreckTransform, SpawnParameters);
	if (not IsValid(Wreck))
	{
		return;
	}
	M_Wreck = Wreck;
	// A parked aircraft collapses in place; thrown pieces would fly through the walls of its bay.
	const float ScatterSpeed = M_DeathSettings.PieceScatterSpeed
		* (bM_WasParked ? AircraftDeathPrivate::ParkedScatterShare : 1.f);
	M_BreakupPieceCount = Wreck->BreakUp(M_CrashCollection, M_FlightState.Velocity,
	                                     M_DeathSettings.PieceVelocityRetained, ScatterSpeed,
	                                     M_DeathSettings.WreckLifeSeconds);
}

float UAircraftDeathComponent::GetHighestGroundUnder(const FBox& WorldBox) const
{
	using namespace AircraftDeathPrivate;
	const UWorld* World = GetWorld();
	float HighestGroundZ = TNumericLimits<float>::Lowest();
	if (not IsValid(World))
	{
		return HighestGroundZ;
	}
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AircraftWreckGround), false, GetOwner());
	const FCollisionObjectQueryParams ObjectParams(ECC_WorldStatic);
	for (int32 SampleX = 0; SampleX < WreckGroundSamplesPerSide; ++SampleX)
	{
		for (int32 SampleY = 0; SampleY < WreckGroundSamplesPerSide; ++SampleY)
		{
			const float AlphaX = static_cast<float>(SampleX) / (WreckGroundSamplesPerSide - 1);
			const float AlphaY = static_cast<float>(SampleY) / (WreckGroundSamplesPerSide - 1);
			const FVector Sample(FMath::Lerp(WorldBox.Min.X, WorldBox.Max.X, AlphaX),
			                     FMath::Lerp(WorldBox.Min.Y, WorldBox.Max.Y, AlphaY), WorldBox.Max.Z);
			FHitResult GroundHit;
			const FVector End(Sample.X, Sample.Y, WorldBox.Min.Z - WreckGroundTraceMargin);
			if (World->LineTraceSingleByObjectType(GroundHit, Sample + FVector(0.f, 0.f, WreckGroundTraceMargin), End,
			                                       ObjectParams, QueryParams))
			{
				HighestGroundZ = FMath::Max(HighestGroundZ, static_cast<float>(GroundHit.ImpactPoint.Z));
			}
		}
	}
	return HighestGroundZ;
}

float UAircraftDeathComponent::GetLowestVisibleMeshPointZ() const
{
	const AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return 0.f;
	}
	float LowestPointZ = Owner->GetActorLocation().Z;
	TArray<UMeshComponent*> Meshes;
	Owner->GetComponents<UMeshComponent>(Meshes);
	for (const UMeshComponent* MeshComponent : Meshes)
	{
		if (IsValid(MeshComponent) && MeshComponent->IsVisible())
		{
			LowestPointZ = FMath::Min(LowestPointZ, static_cast<float>(MeshComponent->Bounds.GetBox().Min.Z));
		}
	}
	return LowestPointZ;
}

void UAircraftDeathComponent::Impact_HideAircraft()
{
	AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return;
	}
	HideAircraftMeshes();
	TArray<UMeshComponent*> Meshes;
	Owner->GetComponents<UMeshComponent>(Meshes);
	for (UMeshComponent* MeshComponent : Meshes)
	{
		if (IsValid(MeshComponent))
		{
			MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	Owner->SetActorEnableCollision(false);
	// The hidden aircraft lingers while its smoke fades: no engine drone at the crash site, no vision from the wreck.
	TArray<UAudioComponent*> Sounds;
	Owner->GetComponents<UAudioComponent>(Sounds);
	for (UAudioComponent* Sound : Sounds)
	{
		if (IsValid(Sound))
		{
			Sound->Stop();
		}
	}
	if (const ASelectablePawnMaster* SelectablePawn = Cast<ASelectablePawnMaster>(Owner))
	{
		if (UFowComp* Vision = SelectablePawn->GetFowComponent())
		{
			Vision->StopFowParticipation();
		}
	}
}

void UAircraftDeathComponent::Impact_LetTrailsFade()
{
	bM_IsFireBurning = false;
	if (IsValid(M_EngineFire))
	{
		// Left where the wreck came down; no new particles, the old ones live out their time.
		M_EngineFire->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		M_EngineFire->Deactivate();
	}
	Impact_StopAircraftEffects();
}

void UAircraftDeathComponent::Impact_StopAircraftEffects()
{
	AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return;
	}
	// Exhausts, contrails and whatever else the Blueprint burns: the hidden aircraft lingers a few seconds after the
	// impact and must not keep smoking where it came down.
	TArray<UFXSystemComponent*> Effects;
	Owner->GetComponents<UFXSystemComponent>(Effects);
	for (UFXSystemComponent* Effect : Effects)
	{
		if (IsValid(Effect) && Effect->IsActive())
		{
			Effect->Deactivate();
		}
	}
}


UNiagaraComponent* UAircraftDeathComponent::SpawnDeathEffect(const FAircraftDeathEffect& Effect, const FVector& Location,
                                                             const FRotator& Rotation, const float ScaleMultiplier) const
{
	if (not IsValid(Effect.System))
	{
		return nullptr;
	}
	UNiagaraComponent* Spawned = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		this, Effect.System, Location, Rotation, FVector(GetEffectScale() * Effect.Scale * ScaleMultiplier), true, true,
		ENCPoolMethod::None);
	AircraftDeathPrivate::ApplyEffectParameters(Spawned, Effect);
	return Spawned;
}

void UAircraftDeathComponent::HideAircraftMeshes() const
{
	AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return;
	}
	// Only the meshes: the fire and smoke components of the aircraft stay visible.
	TArray<UMeshComponent*> Meshes;
	Owner->GetComponents<UMeshComponent>(Meshes);
	for (UMeshComponent* MeshComponent : Meshes)
	{
		if (not IsValid(MeshComponent))
		{
			continue;
		}
		MeshComponent->SetVisibility(false, false);
		// Blueprints let the aircraft cast a shadow while hidden (fog of war); the wreck's pieces cast the shadow now.
		MeshComponent->SetCastShadow(false);
	}
}

bool UAircraftDeathComponent::EnsureCarriedWreck()
{
	if (M_Wreck.IsValid())
	{
		return M_Wreck->GetHasHeldPieces();
	}
	UWorld* World = GetWorld();
	const AActor* Owner = GetOwner();
	if (not IsValid(World) || not IsValid(Owner))
	{
		return false;
	}
	if (not IsValid(M_CrashCollection) && M_CrashCollectionPath.IsValid())
	{
		// Shot down before the load at BeginPlay finished.
		M_CrashCollection = Cast<UGeometryCollection>(M_CrashCollectionPath.TryLoad());
	}
	if (not IsValid(M_CrashCollection))
	{
		return false;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AAircraftWreckActor* Wreck = World->SpawnActor<AAircraftWreckActor>(
		AAircraftWreckActor::StaticClass(), GetUnscaledAircraftTransform(), SpawnParameters);
	if (not IsValid(Wreck))
	{
		return false;
	}
	// Lives through the longest fall; set again when it lands.
	const int32 PieceCount = Wreck->StartCarried(M_CrashCollection,
	                                             M_DeathSettings.MaxCrashSeconds + M_DeathSettings.WreckLifeSeconds,
	                                             M_DeathSettings.PieceLinearDamping);
	if (PieceCount <= 0)
	{
		Wreck->Destroy();
		return false;
	}
	M_Wreck = Wreck;
	M_BreakupPieceCount = PieceCount;
	// The aircraft stays visible until the wreck is ready to take its place (TickCarriedWreck).
	return true;
}

void UAircraftDeathComponent::TickCarriedWreck(const float DeltaTime)
{
	AAircraftWreckActor* Wreck = M_Wreck.Get();
	const AActor* Owner = GetOwner();
	if (not IsValid(Wreck) || not IsValid(Owner) || bM_IsImpactStaging)
	{
		return;
	}
	UpdatePiecePhysicsVelocities(DeltaTime);
	// The held pieces are kinematic: moving the actor moves them, and gives them the speed they fall on with.
	Wreck->FollowTransform(GetUnscaledAircraftTransform());
	if (Wreck->GetIsCarriedWreckShown() && not bM_AircraftMeshesHidden)
	{
		// The wreck now draws the aircraft.
		bM_AircraftMeshesHidden = true;
		M_CarriedWreckShownAtSeconds = M_FlightState.SecondsFalling;
		HideAircraftMeshes();
	}
	TickTornPartSmoke();
}

void UAircraftDeathComponent::UpdatePiecePhysicsVelocities(const float DeltaTime)
{
	const AAircraftWreckActor* Wreck = M_Wreck.Get();
	if (not IsValid(Wreck) || not Wreck->GetIsCarriedWreckShown())
	{
		return;
	}
	const int32 PieceCount = Wreck->GetPieces().Num();
	const bool bHasLastFrame = M_PiecePhysicsCentres.Num() == PieceCount && DeltaTime > KINDA_SMALL_NUMBER;
	M_PiecePhysicsCentres.SetNum(PieceCount);
	M_PiecePhysicsVelocities.SetNumZeroed(PieceCount);
	for (int32 PieceIndex = 0; PieceIndex < PieceCount; ++PieceIndex)
	{
		const FVector Centre = Wreck->GetPiecePhysicsCentre(PieceIndex);
		if (bHasLastFrame)
		{
			M_PiecePhysicsVelocities[PieceIndex] = (Centre - M_PiecePhysicsCentres[PieceIndex]) / DeltaTime;
		}
		M_PiecePhysicsCentres[PieceIndex] = Centre;
	}
}

void UAircraftDeathComponent::BuildAirBreakupOrder()
{
	using namespace AircraftDeathPrivate;
	M_AirBreakupOrder.Reset();
	const AAircraftWreckActor* Wreck = M_Wreck.Get();
	if (not IsValid(Wreck) || Wreck->GetPieces().Num() < MinPiecesForAirBreakup)
	{
		return;
	}
	const TArray<FAircraftWreckPiece>& Pieces = Wreck->GetPieces();
	FBox PieceBox(ForceInit);
	for (const FAircraftWreckPiece& Piece : Pieces)
	{
		PieceBox += Piece.LocalCentre;
	}
	const float HalfSpan = FMath::Max(FMath::Abs(PieceBox.Min.Y), FMath::Abs(PieceBox.Max.Y));
	const FVector EngineLocal = GetFireLocationInActorSpace();
	// Pieces ranked by a key, lowest first.
	TArray<TPair<float, int32>> Ranked;
	if (M_FireOrigin == EAircraftFireOrigin::WingEngine && not FMath::IsNearlyZero(EngineLocal.Y))
	{
		// The burning wing loses its outer panel, beyond the engine.
		const float Side = FMath::Sign(EngineLocal.Y);
		RankWingPieces(Pieces, Side, FMath::Abs(EngineLocal.Y) + OuterWingMarginShare * HalfSpan, Ranked);
		if (Ranked.Num() < MinTornPieces)
		{
			Ranked.Reset();
			RankWingPieces(Pieces, Side, 0.f, Ranked);
		}
	}
	else if (M_Random.FRand() < NoseFireTailShare)
	{
		// A single-engine aircraft burning at the nose loses its tail.
		RankTailPieces(Pieces, PieceBox.Min.X + TailLengthShare * (PieceBox.Max.X - PieceBox.Min.X), Ranked);
	}
	if (Ranked.Num() < MinTornPieces)
	{
		// Or a wing tip.
		Ranked.Reset();
		const float Side = M_Random.FRandRange(-1.f, 1.f) < 0.f ? -1.f : 1.f;
		RankWingPieces(Pieces, Side, WingTipSpanShare * HalfSpan, Ranked);
		if (Ranked.Num() < MinTornPieces)
		{
			Ranked.Reset();
			RankWingPieces(Pieces, Side, 0.f, Ranked);
		}
	}
	Ranked.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
	for (const TPair<float, int32>& Entry : Ranked)
	{
		M_AirBreakupOrder.Add(Entry.Value);
	}
}

FVector UAircraftDeathComponent::GetFireLocationInActorSpace() const
{
	const AActor* Owner = GetOwner();
	const USkeletalMeshComponent* Mesh = M_AircraftMesh.Get();
	if (not IsValid(Owner) || not IsValid(Mesh))
	{
		return FVector::ZeroVector;
	}
	return GetUnscaledAircraftTransform().InverseTransformPosition(
		Mesh->GetComponentTransform().TransformPosition(M_FireLocalOffset));
}

FTransform UAircraftDeathComponent::GetUnscaledAircraftTransform() const
{
	const AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return FTransform::Identity;
	}
	// The crash collections carry the aircraft's scale in their geometry; the wreck itself is never scaled.
	FTransform Transform = Owner->GetActorTransform();
	Transform.SetScale3D(FVector::OneVector);
	return Transform;
}

void UAircraftDeathComponent::TickAirBreakup()
{
	if (M_AirBreakupSeconds < 0.f || bM_DidSecondAirBreakup || not M_Wreck.IsValid()
		|| not M_Wreck->GetIsCarriedWreckShown())
	{
		// Parts tear off once the wreck draws the aircraft.
		return;
	}
	const float SecondsFalling = M_FlightState.SecondsFalling;
	const int32 FirstTearCount = FMath::Max(
		2, FMath::RoundToInt(M_DeathSettings.AirBreakupPieceShare * M_Wreck->GetPieceCount()));
	if (not bM_DidFirstAirBreakup)
	{
		if (SecondsFalling < M_AirBreakupSeconds)
		{
			return;
		}
		bM_DidFirstAirBreakup = true;
		BuildAirBreakupOrder();
		TearOffPieces(FirstTearCount);
		return;
	}
	const float SecondDelay = M_DeathSettings.AirSecondBreakupDelaySeconds;
	if (SecondDelay <= 0.f)
	{
		bM_DidSecondAirBreakup = true;
		return;
	}
	if (SecondsFalling >= M_AirBreakupSeconds + SecondDelay)
	{
		bM_DidSecondAirBreakup = true;
		TearOffPieces(FMath::Max(1, FirstTearCount / 2));
	}
}

void UAircraftDeathComponent::TearOffPieces(const int32 Count)
{
	using namespace AircraftDeathPrivate;
	AAircraftWreckActor* Wreck = M_Wreck.Get();
	if (not IsValid(Wreck))
	{
		return;
	}
	const FVector Forward = Wreck->GetActorForwardVector();
	const FVector Right = Wreck->GetActorRightVector();
	FVector TearCentre = FVector::ZeroVector;
	int32 TornCount = 0;
	for (; M_NextAirBreakupIndex < M_AirBreakupOrder.Num() && TornCount < Count; ++M_NextAirBreakupIndex)
	{
		const int32 PieceIndex = M_AirBreakupOrder[M_NextAirBreakupIndex];
		if (not Wreck->GetPieces().IsValidIndex(PieceIndex) || Wreck->GetPieces()[PieceIndex].bReleased)
		{
			continue;
		}
		const FVector PieceCentre = Wreck->GetPieceWorldCentre(PieceIndex);
		const float SideSign = Wreck->GetPieces()[PieceIndex].LocalCentre.Y >= 0.f ? 1.f : -1.f;
		const float Separation = M_DeathSettings.AirBreakupSeparationSpeed
			* M_Random.FRandRange(TornPartKickRandomRange.X, TornPartKickRandomRange.Y);
		// Away from the aircraft and up a little; the air drag then holds it back behind the falling wreck.
		const FVector Kick = Right * SideSign * Separation * TornPartSideKickShare
			+ FVector::UpVector * Separation * TornPartUpKickShare - Forward * Separation * TornPartBackKickShare;
		Wreck->ReleasePiece(PieceIndex, Kick, M_Random.GetUnitVector() * TornPartSpinRadians);
		TearCentre += PieceCentre;
		++TornCount;
		if (M_SmokingTornPiece == INDEX_NONE)
		{
			M_SmokingTornPiece = PieceIndex;
			M_SmokingTornPieceLastLocation = PieceCentre;
			M_TornPartSmokeSeconds = 0.f;
		}
	}
	if (TornCount == 0)
	{
		return;
	}
	M_AirBreakupPieceCount += TornCount;
	TearCentre /= TornCount;
	SpawnDeathEffect(M_DeathSettings.AirBreakupExplosion, TearCentre, Wreck->GetActorRotation());
	if (IsValid(M_DeathSettings.EngineHitSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, M_DeathSettings.EngineHitSound, TearCentre);
	}
	UE_LOG(LogRTSAircraftDeath, Log, TEXT("RTS_AIRCRAFT_DEATH TEAR %s pieces=%d of %d at=%s after=%.2fs"),
	       *GetNameSafe(GetOwner()), TornCount, Wreck->GetPieceCount(), *TearCentre.ToCompactString(),
	       M_FlightState.SecondsFalling);
}

void UAircraftDeathComponent::TickTornPartSmoke()
{
	using namespace AircraftDeathPrivate;
	const UWorld* World = GetWorld();
	if (M_SmokingTornPiece == INDEX_NONE || not M_Wreck.IsValid() || not IsValid(World))
	{
		return;
	}
	const float DeltaSeconds = World->GetDeltaSeconds();
	M_TornPartSmokeSeconds += DeltaSeconds;
	const FVector Location = M_Wreck->GetPieceWorldCentre(M_SmokingTornPiece);
	const float Moved = FVector::Dist(Location, M_SmokingTornPieceLastLocation);
	M_SmokingTornPieceLastLocation = Location;
	const bool bCameToRest = DeltaSeconds > 0.f && M_TornPartSmokeSeconds > TornPartSmokeMinSeconds
		&& Moved / DeltaSeconds < TornPartMinSmokeSpeed;
	if (M_TornPartSmokeSeconds > TornPartSmokeSeconds || bCameToRest)
	{
		M_SmokingTornPiece = INDEX_NONE;
		return;
	}
	M_DistanceSinceTornPartPuff += Moved;
	if (M_DistanceSinceTornPartPuff < M_DeathSettings.SmokeTrailSpacing * TornPartSmokeSpacingMlt)
	{
		return;
	}
	M_DistanceSinceTornPartPuff = 0.f;
	SpawnSmokePuff(Location, M_DeathSettings.EffectScale * FMath::Sqrt(M_SizeEffectScale)
	               * M_DeathSettings.SmokeTrail.Scale * TornPartSmokeScale);
}

void UAircraftDeathComponent::StartImpactStage(const FVector& ContactPoint, const FVector& ContactNormal)
{
	using namespace AircraftDeathPrivate;
	AAircraftWreckActor* Wreck = M_Wreck.Get();
	if (not IsValid(Wreck))
	{
		return;
	}
	bM_IsImpactStaging = true;
	bM_HullFinished = false;
	M_ImpactStageSeconds = 0.f;
	M_ImpactStageDuration = M_DeathSettings.ImpactBreakupSeconds * FMath::Sqrt(M_SizeEffectScale);
	M_ContactPoint = ContactPoint;
	M_ContactNormal = ContactNormal;
	StartImpactStage_SetupHullMotion(*Wreck);
	Wreck->SetLifeSpan(M_DeathSettings.WreckLifeSeconds);
	StartImpactStage_ScheduleBlasts();
	// The nose throws up the dirt where it hits; the fireball follows a moment later over the hull.
	SpawnDeathEffect(M_DeathSettings.GroundBurst, ContactPoint, FRotationMatrix::MakeFromZ(ContactNormal).Rotator());
	UE_LOG(LogRTSAircraftDeath, Log,
	       TEXT("RTS_AIRCRAFT_DEATH IMPACT_STAGE %s contact=%s skid_speed=%.0f duration=%.2fs pieces=%d held=%d belly=%.0f"),
	       *GetNameSafe(GetOwner()), *ContactPoint.ToCompactString(), M_SkidStartSpeed, M_ImpactStageDuration,
	       Wreck->GetPieceCount(), Wreck->GetPieceCount() - Wreck->GetReleasedPieceCount(), M_BellyDepth);
	ImpactStage_ReleasePieces(0.f);
	SetComponentTickEnabled(true);
}

void UAircraftDeathComponent::StartImpactStage_SetupHullMotion(const AAircraftWreckActor& Wreck)
{
	const FVector& ContactPoint = M_ContactPoint;
	const FVector& ContactNormal = M_ContactNormal;
	const FTransform HullStart = Wreck.GetActorTransform();
	M_HullStartRotation = HullStart.GetRotation();
	// Last frame the hull was a step back along its flight: the nose pieces released right away come in at that speed.
	M_PrevHullTransform = HullStart;
	if (const UWorld* World = GetWorld())
	{
		M_PrevHullTransform.AddToTranslation(-M_FlightState.Velocity * World->GetDeltaSeconds());
	}
	M_HullScale = HullStart.GetScale3D();
	// The hull skids on along the ground with part of its speed; the speed into the ground is spent on the impact.
	const FVector AlongGround = FVector::VectorPlaneProject(M_FlightState.Velocity, ContactNormal);
	M_SkidStartSpeed = static_cast<float>(AlongGround.Size()) * M_DeathSettings.ImpactSkidSpeedShare;
	M_SkidDistance = 0.f;
	FVector LevelForward = FVector::VectorPlaneProject(M_HullStartRotation.GetForwardVector(), ContactNormal).GetSafeNormal();
	if (LevelForward.IsNearlyZero())
	{
		LevelForward = AlongGround.GetSafeNormal();
	}
	if (LevelForward.IsNearlyZero())
	{
		LevelForward = FVector::VectorPlaneProject(FVector::ForwardVector, ContactNormal).GetSafeNormal();
	}
	M_SkidDirection = AlongGround.IsNearlyZero() ? LevelForward : AlongGround.GetSafeNormal();
	// It comes to lie flat on the ground, its nose where it hit, as the tail slams down.
	M_HullLevelRotation = FRotationMatrix::MakeFromXZ(LevelForward, ContactNormal).ToQuat();
	M_PivotLocal = HullStart.InverseTransformPosition(ContactPoint);
	M_NoseLocalX = TNumericLimits<float>::Lowest();
	M_TailLocalX = TNumericLimits<float>::Max();
	float BottomLocalZ = M_PivotLocal.Z;
	for (const FAircraftWreckPiece& Piece : Wreck.GetPieces())
	{
		M_NoseLocalX = FMath::Max(M_NoseLocalX, static_cast<float>(Piece.LocalCentre.X));
		M_TailLocalX = FMath::Min(M_TailLocalX, static_cast<float>(Piece.LocalCentre.X));
		BottomLocalZ = FMath::Min(BottomLocalZ, static_cast<float>(Piece.LocalCentre.Z - Piece.LocalExtent.Z));
	}
	// Lying flat on the nose pivot would sink the belly into the ground: the hull rises by that much as it levels.
	M_BellyDepth = FMath::Max(0.f, (M_PivotLocal.Z - BottomLocalZ) * static_cast<float>(M_HullScale.Z));
	StartImpactStage_LimitHullSpeed(Wreck);
}

void UAircraftDeathComponent::StartImpactStage_LimitHullSpeed(const AAircraftWreckActor& Wreck)
{
	using namespace AircraftDeathPrivate;
	// The piece furthest from the nose pivot moves fastest as the hull levels: it gets at least enough time that no
	// piece goes faster than the maximum (the smoothstep peaks at 1.5 times its average speed).
	float FurthestFromPivot = 0.f;
	for (const FAircraftWreckPiece& Piece : Wreck.GetPieces())
	{
		FurthestFromPivot = FMath::Max(FurthestFromPivot, static_cast<float>(FVector::Dist(Piece.LocalCentre, M_PivotLocal)));
	}
	const float LevelingRadians = M_HullStartRotation.AngularDistance(M_HullLevelRotation);
	const float SlowestStage = SmoothstepPeakSpeedFactor * FurthestFromPivot * LevelingRadians / MaxHullPieceSpeed;
	M_ImpactStageDuration = FMath::Max(M_ImpactStageDuration, SlowestStage);
}

void UAircraftDeathComponent::StartImpactStage_ScheduleBlasts()
{
	using namespace AircraftDeathPrivate;
	bM_MainBlastDone = false;
	bM_FuelBlastDone = false;
	bM_DebrisFiresDone = false;
	M_NextSecondaryBlast = 0;
	M_SecondaryBlastSeconds.Reset();
	const int32 SecondaryCount = FMath::Clamp(1 + FMath::RoundToInt((M_SizeEffectScale - 1.f) * 2.f), 1,
	                                          MaxSecondaryBlasts);
	for (int32 Index = 0; Index < SecondaryCount; ++Index)
	{
		M_SecondaryBlastSeconds.Add(M_Random.FRandRange(SecondaryBlastSeconds.X, SecondaryBlastSeconds.Y));
	}
	M_SecondaryBlastSeconds.Sort();
}

void UAircraftDeathComponent::TickImpactStage(const float DeltaTime)
{
	using namespace AircraftDeathPrivate;
	if (not M_Wreck.IsValid())
	{
		bM_IsImpactStaging = false;
		SetComponentTickEnabled(false);
		return;
	}
	M_ImpactStageSeconds += DeltaTime;
	if (not bM_HullFinished)
	{
		UpdatePiecePhysicsVelocities(DeltaTime);
		const float StageAlpha = FMath::Clamp(M_ImpactStageSeconds / FMath::Max(M_ImpactStageDuration, KINDA_SMALL_NUMBER),
		                                      0.f, 1.f);
		M_SkidDistance += M_SkidStartSpeed * FMath::Lerp(1.f, SkidEndSpeedShare, StageAlpha) * DeltaTime;
		ImpactStage_MoveHull(StageAlpha);
		ImpactStage_ReleasePieces(StageAlpha);
		if (StageAlpha >= 1.f)
		{
			ImpactStage_FinishHull();
		}
	}
	ImpactStage_SpawnBlasts();
	TickTornPartSmoke();
	if (not bM_DebrisFiresDone && M_ImpactStageSeconds >= BurningDebrisSeconds)
	{
		bM_DebrisFiresDone = true;
		ImpactStage_SpawnBurningDebris();
	}
	const bool bBlastsDone = bM_MainBlastDone && bM_FuelBlastDone
		&& M_NextSecondaryBlast >= M_SecondaryBlastSeconds.Num();
	if (bM_HullFinished && bBlastsDone && bM_DebrisFiresDone)
	{
		bM_IsImpactStaging = false;
		SetComponentTickEnabled(false);
	}
}

void UAircraftDeathComponent::ImpactStage_MoveHull(const float StageAlpha)
{
	AAircraftWreckActor* Wreck = M_Wreck.Get();
	if (not IsValid(Wreck))
	{
		return;
	}
	M_PrevHullTransform = Wreck->GetActorTransform();
	// Starts and ends gently: easing out from the start brought a long bomber's tail down at 45 m/s, and the pieces
	// it let go were thrown tens of metres up.
	const float Ease = StageAlpha * StageAlpha * (3.f - 2.f * StageAlpha);
	const FQuat Rotation = FQuat::Slerp(M_HullStartRotation, M_HullLevelRotation, Ease);
	const FVector Pivot = M_ContactPoint + M_SkidDirection * M_SkidDistance + M_ContactNormal * (M_BellyDepth * Ease);
	const FVector Centre = Pivot - FTransform(Rotation, FVector::ZeroVector, M_HullScale).TransformVector(M_PivotLocal);
	Wreck->SetActorTransform(FTransform(Rotation, Centre, M_HullScale), false, nullptr, ETeleportType::None);
}

void UAircraftDeathComponent::ImpactStage_ReleasePieces(const float StageAlpha)
{
	AAircraftWreckActor* Wreck = M_Wreck.Get();
	if (not IsValid(Wreck))
	{
		return;
	}
	const float DeltaSeconds = IsValid(GetWorld()) ? GetWorld()->GetDeltaSeconds() : 0.f;
	for (int32 PieceIndex = 0; PieceIndex < Wreck->GetPieces().Num(); ++PieceIndex)
	{
		if (not Wreck->GetPieces()[PieceIndex].bReleased)
		{
			ImpactStage_TryReleasePiece(*Wreck, PieceIndex, StageAlpha, DeltaSeconds);
		}
	}
}

void UAircraftDeathComponent::ImpactStage_TryReleasePiece(AAircraftWreckActor& Wreck, const int32 PieceIndex,
                                                          const float StageAlpha, const float DeltaSeconds)
{
	using namespace AircraftDeathPrivate;
	const FAircraftWreckPiece& Piece = Wreck.GetPieces()[PieceIndex];
	// How fast the held piece moves into the ground with the hull: from where it was last frame.
	const FVector PieceCentre = Wreck.GetPieceWorldCentre(PieceIndex);
	const FVector LastFrameCentre = M_PrevHullTransform.TransformPosition(Piece.LocalCentre);
	// Capped: a frame-time glitch must not turn into a huge kick.
	const float IntoGroundSpeed = DeltaSeconds > KINDA_SMALL_NUMBER
		? FMath::Clamp(static_cast<float>(FVector::DotProduct(PieceCentre - LastFrameCentre, M_ContactNormal)) / DeltaSeconds,
		               -MaxHullPieceSpeed, 0.f)
		: 0.f;
	// A piece whose lowest point meets the ground (or will by the time it is let go) tears off; the rest of the hull
	// comes apart from the nose back. By its lowest point, not its centre: a big bomber piece held until its centre
	// came down was buried metres deep and fell through the ground once loose.
	const FTransform& HullTransform = Wreck.GetActorTransform();
	const FVector ScaledExtent = Piece.LocalExtent * HullTransform.GetScale3D();
	const float HalfDepthAlongNormal = static_cast<float>(
		FMath::Abs(FVector::DotProduct(HullTransform.GetUnitAxis(EAxis::X), M_ContactNormal)) * ScaledExtent.X
		+ FMath::Abs(FVector::DotProduct(HullTransform.GetUnitAxis(EAxis::Y), M_ContactNormal)) * ScaledExtent.Y
		+ FMath::Abs(FVector::DotProduct(HullTransform.GetUnitAxis(EAxis::Z), M_ContactNormal)) * ScaledExtent.Z);
	const float PredictedClearance = static_cast<float>(FVector::DotProduct(PieceCentre - M_ContactPoint, M_ContactNormal))
		- HalfDepthAlongNormal + IntoGroundSpeed * FMath::Max(GroundReleaseLookAheadSeconds,
		                                                      GroundReleaseLookAheadFrames * DeltaSeconds);
	const float Length = M_NoseLocalX - M_TailLocalX;
	const float BackShare = Length > KINDA_SMALL_NUMBER ? (M_NoseLocalX - static_cast<float>(Piece.LocalCentre.X)) / Length : 0.f;
	if (PredictedClearance > GroundReleaseClearance && BackShare > StageAlpha)
	{
		return;
	}
	// The ground takes most of its speed into it: a thin panel driven into the ground at speed falls through. Only
	// the speed the piece really has in the physics: one that lagged the hull would be thrown up by the difference.
	float AbsorbedSpeed = IntoGroundSpeed;
	if (M_PiecePhysicsVelocities.IsValidIndex(PieceIndex))
	{
		const float PhysicsIntoGroundSpeed = FMath::Clamp(
			static_cast<float>(FVector::DotProduct(M_PiecePhysicsVelocities[PieceIndex], M_ContactNormal)),
			-MaxHullPieceSpeed, 0.f);
		AbsorbedSpeed = FMath::Max(AbsorbedSpeed, PhysicsIntoGroundSpeed);
	}
	const float Scatter = M_DeathSettings.PieceScatterSpeed;
	const FVector Kick = M_ContactNormal * (Scatter * M_Random.FRandRange(PieceUpKickShare.X, PieceUpKickShare.Y)
			- AbsorbedSpeed * GroundAbsorbShare)
		+ Wreck.GetActorRightVector() * Scatter * M_Random.FRandRange(-PieceSideKickShare, PieceSideKickShare)
		+ M_SkidDirection * Scatter * M_Random.FRandRange(0.f, PieceForwardKickShare);
	Wreck.ReleasePiece(PieceIndex, Kick, M_Random.GetUnitVector() * M_Random.FRandRange(PieceSpinRadians.X, PieceSpinRadians.Y));
}

void UAircraftDeathComponent::ImpactStage_FinishHull()
{
	using namespace AircraftDeathPrivate;
	bM_HullFinished = true;
	AAircraftWreckActor* Wreck = M_Wreck.Get();
	if (not IsValid(Wreck))
	{
		return;
	}
	// Whatever still holds together breaks off now.
	for (int32 PieceIndex = 0; PieceIndex < Wreck->GetPieces().Num(); ++PieceIndex)
	{
		Wreck->ReleasePiece(PieceIndex, M_ContactNormal * M_DeathSettings.PieceScatterSpeed * FinalPieceUpKickShare,
		                    M_Random.GetUnitVector() * FinalPieceSpinRadians);
	}
	// The fuel burns where the hull came to lie, and dies down with the wreck's other fires.
	const FVector FireLocation = M_ContactPoint + M_SkidDirection * M_SkidDistance;
	if (IsValid(M_DeathSettings.GroundFire.System))
	{
		UNiagaraComponent* GroundFire = UNiagaraFunctionLibrary::SpawnSystemAttached(
			M_DeathSettings.GroundFire.System, Wreck->GetRootComponent(), NAME_None, FireLocation,
			FRotationMatrix::MakeFromZ(M_ContactNormal).Rotator(),
			FVector(GetEffectScale() * M_DeathSettings.GroundFire.Scale), EAttachLocation::KeepWorldPosition, false,
			ENCPoolMethod::None, true);
		AircraftDeathPrivate::ApplyEffectParameters(GroundFire, M_DeathSettings.GroundFire);
		Wreck->AddWreckFire(GroundFire);
	}
	Wreck->StopFiresAfter(M_DeathSettings.WreckFireSeconds);
	UE_LOG(LogRTSAircraftDeath, Log, TEXT("RTS_AIRCRAFT_DEATH HULL_DONE %s skid=%.0f at=%s"),
	       *GetNameSafe(GetOwner()), M_SkidDistance, *Wreck->GetActorLocation().ToCompactString());
}

void UAircraftDeathComponent::ImpactStage_SpawnBlasts()
{
	using namespace AircraftDeathPrivate;
	const AAircraftWreckActor* Wreck = M_Wreck.Get();
	if (not IsValid(Wreck))
	{
		return;
	}
	const FRotator AlongGround = FRotationMatrix::MakeFromZ(M_ContactNormal).Rotator();
	const float Lift = ExplosionLift * GetEffectScale();
	if (not bM_MainBlastDone && M_ImpactStageSeconds >= MainBlastSeconds)
	{
		bM_MainBlastDone = true;
		SpawnDeathEffect(M_DeathSettings.ImpactExplosion, Wreck->GetActorLocation() + M_ContactNormal * Lift, AlongGround);
		if (IsValid(M_DeathSettings.ImpactSound))
		{
			UGameplayStatics::PlaySoundAtLocation(this, M_DeathSettings.ImpactSound, Wreck->GetActorLocation());
		}
	}
	if (not bM_FuelBlastDone && M_ImpactStageSeconds >= FuelBlastSeconds)
	{
		bM_FuelBlastDone = true;
		// The tanks sit in the wings and the fuselage behind the engine: over the hull, ahead along the skid.
		const float HalfLength = 0.5f * (M_NoseLocalX - M_TailLocalX);
		SpawnDeathEffect(M_DeathSettings.FuelFireball,
		                 Wreck->GetActorLocation() + M_SkidDirection * HalfLength * FuelBlastForwardShare
		                 + M_ContactNormal * Lift * FuelBlastLiftShare, AlongGround);
	}
	while (M_SecondaryBlastSeconds.IsValidIndex(M_NextSecondaryBlast)
		&& M_ImpactStageSeconds >= M_SecondaryBlastSeconds[M_NextSecondaryBlast])
	{
		++M_NextSecondaryBlast;
		const int32 PieceIndex = GetRandomReleasedPiece();
		const FVector BlastLocation = (PieceIndex != INDEX_NONE ? Wreck->GetPieceWorldCentre(PieceIndex) : Wreck->GetActorLocation())
			+ M_ContactNormal * Lift * SecondaryBlastLiftShare;
		SpawnDeathEffect(M_DeathSettings.SecondaryExplosion, BlastLocation, AlongGround,
		                 M_Random.FRandRange(SecondaryBlastScaleRange.X, SecondaryBlastScaleRange.Y));
		if (IsValid(M_DeathSettings.EngineHitSound))
		{
			UGameplayStatics::PlaySoundAtLocation(this, M_DeathSettings.EngineHitSound, BlastLocation);
		}
	}
}

void UAircraftDeathComponent::ImpactStage_SpawnBurningDebris()
{
	using namespace AircraftDeathPrivate;
	AAircraftWreckActor* Wreck = M_Wreck.Get();
	if (not IsValid(Wreck) || not IsValid(M_DeathSettings.BurningDebris.System))
	{
		return;
	}
	const int32 FireCount = FMath::Clamp(FMath::RoundToInt(FighterBurningPieces * M_SizeEffectScale),
	                                     FighterBurningPieces, MaxBurningPieces);
	TArray<int32> Burning;
	for (int32 Attempt = 0; Attempt < FireCount * BurningDebrisAttemptsPerFire && Burning.Num() < FireCount; ++Attempt)
	{
		const int32 PieceIndex = GetRandomReleasedPiece();
		if (PieceIndex == INDEX_NONE || Burning.Contains(PieceIndex))
		{
			continue;
		}
		Burning.Add(PieceIndex);
		UNiagaraComponent* Fire = UNiagaraFunctionLibrary::SpawnSystemAttached(
			M_DeathSettings.BurningDebris.System, Wreck->GetRootComponent(), NAME_None,
			Wreck->GetPieceWorldCentre(PieceIndex), FRotator::ZeroRotator,
			FVector(GetEffectScale() * M_DeathSettings.BurningDebris.Scale
				* M_Random.FRandRange(BurningDebrisScaleRange.X, BurningDebrisScaleRange.Y)),
			EAttachLocation::KeepWorldPosition, false, ENCPoolMethod::None, true);
		AircraftDeathPrivate::ApplyEffectParameters(Fire, M_DeathSettings.BurningDebris);
		Wreck->AddWreckFire(Fire);
	}
}

int32 UAircraftDeathComponent::GetRandomReleasedPiece()
{
	const AAircraftWreckActor* Wreck = M_Wreck.Get();
	if (not IsValid(Wreck))
	{
		return INDEX_NONE;
	}
	TArray<int32> Released;
	for (int32 PieceIndex = 0; PieceIndex < Wreck->GetPieces().Num(); ++PieceIndex)
	{
		if (Wreck->GetPieces()[PieceIndex].bReleased)
		{
			Released.Add(PieceIndex);
		}
	}
	return Released.IsEmpty() ? INDEX_NONE : Released[M_Random.RandHelper(Released.Num())];
}
