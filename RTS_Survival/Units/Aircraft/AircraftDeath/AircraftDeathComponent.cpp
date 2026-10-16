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
#include "GeometryCollection/GeometryCollectionObject.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

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
	// Defaults every aircraft shares; found here so they are loaded with the class and cooked with it.
	M_DeathSettings.EngineFire.System = FindDefaultAsset<UNiagaraSystem>(EngineFireVfxPath);
	M_DeathSettings.SmokeTrail.System = FindDefaultAsset<UNiagaraSystem>(SmokeTrailVfxPath);
	M_DeathSettings.ImpactExplosion.System = FindDefaultAsset<UNiagaraSystem>(ImpactExplosionVfxPath);
	M_DeathSettings.GroundFire.System = FindDefaultAsset<UNiagaraSystem>(GroundFireVfxPath);
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
		Impact(M_Aircraft->GetActorLocation(), FVector::UpVector);
		return;
	}
	StartEngineFire();
	StartCrashFlight(VelocityAtDeath);
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
	if (M_DeathPhase != EAircraftDeathPhase::Crashing)
	{
		SetComponentTickEnabled(false);
		return;
	}
	TickCrashFlight(DeltaTime);
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
	FAircraftDeathMath::StepCrashFlight(M_FlightState, M_DeathSettings, M_FireOrigin, DeltaTime, World->GetGravityZ());
	FHitResult GroundHit;
	if (FindImpact(OldLocation, M_FlightState.Location, GroundHit))
	{
		M_FlightState.Location = GroundHit.Location + FVector(0.f, 0.f, M_GroundClearance);
		Aircraft->SetActorLocation(M_FlightState.Location, false, nullptr, ETeleportType::TeleportPhysics);
		Impact(GroundHit.ImpactPoint, GroundHit.ImpactNormal);
		return;
	}
	Aircraft->SetActorLocationAndRotation(M_FlightState.Location,
	                                      FAircraftDeathMath::GetCrashRotation(M_FlightState, Aircraft->GetActorRotation()),
	                                      false, nullptr, ETeleportType::TeleportPhysics);
	M_DistanceSinceSmokePuff += FVector::Dist(OldLocation, M_FlightState.Location);
	DropSmokePuffs();
	if (M_FlightState.SecondsFalling > M_DeathSettings.MaxCrashSeconds)
	{
		Impact(M_FlightState.Location, FVector::UpVector);
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
	UNiagaraComponent* Puff = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		this, M_DeathSettings.SmokeTrail.System, FireLocation, FRotator::ZeroRotator, FVector(SmokeScale), true, true,
		ENCPoolMethod::AutoRelease);
	AircraftDeathPrivate::ApplyEffectParameters(Puff, M_DeathSettings.SmokeTrail);
	++M_SmokePuffCount;
}

bool UAircraftDeathComponent::FindImpact(const FVector& OldLocation, const FVector& NewLocation, FHitResult& OutHit) const
{
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	// From the centre of the wreck to where its belly will be: the belly meets the ground first, and a trace that
	// started at the belly could start below the ground and miss it.
	const FVector BellyOffset(0.f, 0.f, -M_GroundClearance);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AircraftCrashImpact), false, GetOwner());
	const FCollisionObjectQueryParams ObjectParams(ECC_WorldStatic);
	return World->LineTraceSingleByObjectType(OutHit, OldLocation, NewLocation + BellyOffset, ObjectParams, QueryParams);
}

void UAircraftDeathComponent::Impact(const FVector& ImpactLocation, const FVector& ImpactNormal)
{
	M_DeathPhase = EAircraftDeathPhase::Impacted;
	M_ImpactLocation = ImpactLocation;
	SetComponentTickEnabled(false);
	FVector UpwardNormal = ImpactNormal;
	if (UpwardNormal.Z < AircraftDeathPrivate::MinimumUpwardNormalZ)
	{
		UpwardNormal = FVector::UpVector;
	}
	Impact_SpawnWreck();
	Impact_SpawnEffects(ImpactLocation, UpwardNormal);
	// Trails first: hiding the meshes would hide what is still attached to them.
	Impact_LetTrailsFade();
	Impact_HideAircraft();
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
	FTransform WreckTransform = Owner->GetActorTransform();
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
	// Only the meshes: the smoke components of the aircraft must stay visible while they fade.
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
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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
}
