#include "WreckEngineFireDeathComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundCue.h"
#include "TimerManager.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Subsystems/FireSubsystem/RTSFireSubsystem.h"
#include "RTS_Survival/Units/Tanks/WreckTurretToss/WreckDebrisBurstComponent.h"
#include "RTS_Survival/Units/Tanks/WreckTurretToss/WreckDebrisSettings.h"
#include "RTS_Survival/Units/Tanks/WreckTurretToss/WreckPartFlightComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSBlueprintFunctionLibrary.h"

DEFINE_LOG_CATEGORY_STATIC(LogWreckEngineFire, Log, All);

namespace WreckEngineFireDeathConstants
{
	const TCHAR* const CharOverlayMaterialPath =
		TEXT("/Game/RTS_Survival/SpecialEffects/AllExplosions/Mesh/EngineDeck/M_WreckChar_Overlay.M_WreckChar_Overlay");

	// Overlay material parameters (M_WreckChar_Overlay).
	const FName CharCentreParam = TEXT("CharCentre");
	const FName CharRadiusParam = TEXT("CharRadius");
	const FName EmberStartTimeParam = TEXT("EmberStartTime");
	const FName EmberCoolDownParam = TEXT("EmberCoolDown");

	// The deck fire sits a little into the open engine bay rather than on top of the deck plates.
	constexpr float DeckFireSinkDepth = 25.f;
	// Ground traces start this far above the deck and end this far below it.
	constexpr float GroundTraceUp = 300.f;
	constexpr float GroundTraceDown = 1500.f;
	// Scorch decals scatter this far around the fuel pool.
	constexpr float ScorchScatter = 120.f;
	// Share of random direction mixed into the deck's tumble axis so not every deck flips the same way.
	constexpr float DeckTumbleAxisRandomness = 0.35f;
	constexpr float FullCircleDegrees = 360.f;
}

FWreckEngineFireDeathSettings::FWreckEngineFireDeathSettings()
	: CharOverlayMaterial(FSoftObjectPath(WreckEngineFireDeathConstants::CharOverlayMaterialPath))
{
}

UWreckEngineFireDeathComponent::UWreckEngineFireDeathComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UWreckEngineFireDeathComponent::StartEngineFireDeath(const FWreckEngineFireDeathSettings& Settings,
                                                          const FVector& EngineWorldLocation,
                                                          const FVector& EngineDirection,
                                                          const bool bIsAmmoCookOffDeath)
{
	M_Settings = Settings;
	if (not TryFindHullMesh())
	{
		return false;
	}
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	M_FireSubsystem = World->GetSubsystem<URTSFireSubsystem>();
	M_EngineDeckLocation = GetEngineDeckLocation(EngineWorldLocation);
	M_EngineDirection = FVector(EngineDirection.X, EngineDirection.Y, 0.f).GetSafeNormal();
	UE_LOG(LogWreckEngineFire, Verbose, TEXT("Engine fire death on %s (cook-off too: %d)"), *GetNameSafe(GetOwner()),
	       bIsAmmoCookOffDeath);

	// Char first: the debris pieces spawned below must not get the overlay.
	ApplyCharOverlay();
	if (not bIsAmmoCookOffDeath)
	{
		SpawnExplosion(M_Settings.ExplosionType, M_Settings.ExplosionSound);
		StartSecondaryExplosion();
		StartDeckThrow();
	}
	StartDeckFire();
	StartFuelSpill();
	return true;
}

void UWreckEngineFireDeathComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_SecondaryExplosionTimerHandle);
	}
	StopFire(M_DeckFireHandle);
	StopFire(M_FuelFireHandle);
	Super::EndPlay(EndPlayReason);
}

bool UWreckEngineFireDeathComponent::TryFindHullMesh()
{
	const AActor* Wreck = GetOwner();
	if (not IsValid(Wreck))
	{
		return false;
	}
	TArray<UStaticMeshComponent*> MeshComponents;
	Wreck->GetComponents<UStaticMeshComponent>(MeshComponents);
	for (UStaticMeshComponent* MeshComponent : MeshComponents)
	{
		if (IsValid(MeshComponent) && MeshComponent->GetFName() == M_Settings.HullComponentName)
		{
			M_HullMesh = MeshComponent;
			return true;
		}
	}
	RTSFunctionLibrary::ReportError(FString::Printf(
		TEXT("UWreckEngineFireDeathComponent::TryFindHullMesh - wreck %s has no static mesh component named %s."),
		*Wreck->GetName(), *M_Settings.HullComponentName.ToString()));
	return false;
}

FVector UWreckEngineFireDeathComponent::GetEngineDeckLocation(const FVector& EngineWorldLocation) const
{
	if (not GetIsValidHullMesh())
	{
		return EngineWorldLocation;
	}
	// Wreck hulls sit lower than the live tank on its suspension; keep the engine on the wreck's deck.
	const FBoxSphereBounds HullBounds = M_HullMesh->Bounds;
	const float DeckTop = HullBounds.Origin.Z + HullBounds.BoxExtent.Z;
	return FVector(EngineWorldLocation.X, EngineWorldLocation.Y, FMath::Min(EngineWorldLocation.Z, DeckTop));
}

void UWreckEngineFireDeathComponent::SpawnExplosion(const ERTS_ExplosionType ExplosionType,
                                                    USoundCue* OverrideSound) const
{
	UWreckPartFlightComponent::SpawnWreckExplosion(this, ExplosionType, M_EngineDeckLocation, OverrideSound);
}

void UWreckEngineFireDeathComponent::StartSecondaryExplosion()
{
	UWorld* World = GetWorld();
	if (not M_Settings.bPlaySecondaryExplosion || not IsValid(World))
	{
		return;
	}
	FTimerDelegate SecondaryDelegate;
	SecondaryDelegate.BindWeakLambda(this, [this]()
	{
		SpawnExplosion(M_Settings.SecondaryExplosionType, nullptr);
	});
	World->GetTimerManager().SetTimer(M_SecondaryExplosionTimerHandle, SecondaryDelegate,
	                                  FMath::Max(M_Settings.SecondaryExplosionDelay, KINDA_SMALL_NUMBER), false);
}

void UWreckEngineFireDeathComponent::StartDeckThrow() const
{
	if (not M_Settings.bThrowDeckDebris)
	{
		return;
	}
	if (not TryThrowEngineDeck())
	{
		StartDeckDebris(M_Settings.DeckDebrisCountScale);
		return;
	}
	// A few generic bits (grilles, exhaust parts) still fly with the real deck.
	const float DebrisWithDeckScale = M_Settings.DeckDebrisCountScale * M_Settings.EngineDeckThrow.DeckDebrisCountScale;
	if (DebrisWithDeckScale > 0.f)
	{
		StartDeckDebris(DebrisWithDeckScale);
	}
}

bool UWreckEngineFireDeathComponent::TryThrowEngineDeck() const
{
	UStaticMeshComponent* DeckMesh = FindEngineDeckMesh();
	if (DeckMesh == nullptr)
	{
		return false;
	}
	UWreckPartFlightComponent* DeckFlight = UWreckPartFlightComponent::AddToWreck(GetOwner());
	if (not IsValid(DeckFlight))
	{
		return false;
	}
	DeckFlight->SetupPart(DeckMesh, GetDeckFlightSettings());
	LaunchEngineDeck(*DeckFlight);
	UE_LOG(LogWreckEngineFire, Verbose, TEXT("Engine deck %s thrown off %s"), *DeckMesh->GetName(),
	       *GetNameSafe(GetOwner()));
	return true;
}

UStaticMeshComponent* UWreckEngineFireDeathComponent::FindEngineDeckMesh() const
{
	const AActor* Wreck = GetOwner();
	if (not IsValid(Wreck))
	{
		return nullptr;
	}
	TArray<UStaticMeshComponent*> MeshComponents;
	Wreck->GetComponents<UStaticMeshComponent>(MeshComponents);
	for (UStaticMeshComponent* MeshComponent : MeshComponents)
	{
		// Only a deck with simple collision can fly; anything else keeps the generic deck debris.
		if (IsValid(MeshComponent) && MeshComponent->GetFName() == M_Settings.EngineDeckThrow.EngineDeckComponentName
			&& UWreckPartFlightComponent::GetCanSimulatePart(*MeshComponent))
		{
			return MeshComponent;
		}
	}
	return nullptr;
}

void UWreckEngineFireDeathComponent::LaunchEngineDeck(UWreckPartFlightComponent& DeckFlight) const
{
	const FWreckEngineDeckThrowSettings& DeckThrow = M_Settings.EngineDeckThrow;
	const float UpSpeed = FMath::FRandRange(DeckThrow.UpSpeedRange.X, DeckThrow.UpSpeedRange.Y);
	const float SideSpeed = FMath::FRandRange(DeckThrow.SideSpeedRange.X, DeckThrow.SideSpeedRange.Y);
	const float SpinSpeed = FMath::FRandRange(DeckThrow.SpinDegreesRange.X, DeckThrow.SpinDegreesRange.Y);
	const FVector SideDirection = GetDeckLaunchSideDirection();

	// Velocities instead of impulses keep the arc the same for a Panther deck and a Tiger II deck.
	const FVector LaunchVelocity = SideDirection * SideSpeed + FVector::UpVector * UpSpeed;
	// Tumble over the direction of travel so the deck flips away from the hull.
	const FVector TumbleAxis = (FVector::CrossProduct(FVector::UpVector, SideDirection)
		+ FMath::VRand() * WreckEngineFireDeathConstants::DeckTumbleAxisRandomness).GetSafeNormal();
	DeckFlight.Launch(LaunchVelocity, TumbleAxis * SpinSpeed);
}

FVector UWreckEngineFireDeathComponent::GetDeckLaunchSideDirection() const
{
	// Without a known engine side the deck may go any way.
	if (M_EngineDirection.IsNearlyZero())
	{
		return FRotator(0.f, FMath::FRandRange(0.f, WreckEngineFireDeathConstants::FullCircleDegrees), 0.f).Vector();
	}
	const float Spread = M_Settings.EngineDeckThrow.DirectionSpreadDegrees;
	return M_EngineDirection.RotateAngleAxis(FMath::FRandRange(-Spread, Spread), FVector::UpVector);
}

FWreckPartFlightSettings UWreckEngineFireDeathComponent::GetDeckFlightSettings() const
{
	const FWreckEngineDeckThrowSettings& DeckThrow = M_Settings.EngineDeckThrow;
	FWreckPartFlightSettings FlightSettings;
	FlightSettings.bPlayLandingImpact = DeckThrow.bPlayLandingImpact;
	FlightSettings.LandingExplosionType = DeckThrow.LandingExplosionType;
	FlightSettings.LandingSound = DeckThrow.LandingSound;
	FlightSettings.SettleLinearSpeed = DeckThrow.SettleLinearSpeed;
	FlightSettings.SettleAngularSpeed = DeckThrow.SettleAngularSpeed;
	FlightSettings.SettleDuration = DeckThrow.SettleDuration;
	FlightSettings.MaxFlightTime = DeckThrow.MaxFlightTime;
	return FlightSettings;
}

void UWreckEngineFireDeathComponent::StartDeckDebris(const float CountScale) const
{
	AActor* Wreck = GetOwner();
	if (not IsValid(Wreck) || not GetIsValidHullMesh())
	{
		return;
	}
	UWreckDebrisBurstComponent* DebrisBurst = NewObject<UWreckDebrisBurstComponent>(Wreck);
	Wreck->AddInstanceComponent(DebrisBurst);
	DebrisBurst->RegisterComponent();
	DebrisBurst->StartBurst(M_EngineDeckLocation, M_HullMesh.Get(), CountScale,
	                        GetDefault<UWreckDebrisSettings>()->EngineFireBurst, M_EngineDirection);
}

void UWreckEngineFireDeathComponent::StartDeckFire()
{
	if (M_Settings.DeckFireType == ERTSFireType::None || not GetIsValidFireSubsystem() || not GetIsValidHullMesh())
	{
		return;
	}
	const FVector DeckFireLocation = M_EngineDeckLocation
		- FVector(0.f, 0.f, WreckEngineFireDeathConstants::DeckFireSinkDepth);
	const FVector DeckOffset = M_HullMesh->GetComponentTransform().InverseTransformPosition(DeckFireLocation);
	M_DeckFireHandle = M_FireSubsystem->SpawnFireAttachedToComponent(
		GetOwner(),
		M_HullMesh.Get(),
		NAME_None,
		DeckOffset,
		M_Settings.DeckFireType,
		M_Settings.DeckFireLifeTime,
		M_Settings.DeckFireScale,
		M_Settings.DeckFireEffectParams);
}

void UWreckEngineFireDeathComponent::StartFuelSpill()
{
	if (not M_Settings.bSpillBurningFuel || not GetIsValidHullMesh())
	{
		return;
	}
	const FVector GroundLocation = GetFuelSpillGroundLocation();
	if (M_Settings.FuelFireType != ERTSFireType::None && GetIsValidFireSubsystem())
	{
		M_FuelFireHandle = M_FireSubsystem->SpawnFireAtLocation(M_Settings.FuelFireType, M_Settings.FuelFireLifeTime,
		                                                        GroundLocation, M_Settings.FuelFireScale,
		                                                        FRTSFireEffectParams());
	}
	for (int32 DecalIndex = 0; DecalIndex < M_Settings.ScorchDecalCount; ++DecalIndex)
	{
		URTSBlueprintFunctionLibrary::RTSSpawnDecal(
			this,
			ERTS_DecalType::Scorch,
			GroundLocation,
			M_Settings.ScorchDecalSize,
			M_Settings.ScorchDecalLifeTime,
			FVector2D(-WreckEngineFireDeathConstants::ScorchScatter, WreckEngineFireDeathConstants::ScorchScatter));
	}
}

FVector UWreckEngineFireDeathComponent::GetFuelSpillGroundLocation() const
{
	using namespace WreckEngineFireDeathConstants;
	const FBoxSphereBounds HullBounds = M_HullMesh->Bounds;
	// Without a direction the fuel pools under the engine itself.
	FVector SpillXY = M_EngineDeckLocation;
	if (not M_EngineDirection.IsNearlyZero())
	{
		const float HullReach = FMath::Abs(M_EngineDirection.X) * HullBounds.BoxExtent.X
			+ FMath::Abs(M_EngineDirection.Y) * HullBounds.BoxExtent.Y;
		SpillXY = HullBounds.Origin + M_EngineDirection * (HullReach + M_Settings.FuelSpillDistancePastHull);
	}

	const FVector TraceStart(SpillXY.X, SpillXY.Y, M_EngineDeckLocation.Z + GroundTraceUp);
	const FVector TraceEnd(SpillXY.X, SpillXY.Y, M_EngineDeckLocation.Z - GroundTraceDown);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WreckFuelSpill), false, GetOwner());
	FHitResult Hit;
	const UWorld* World = GetWorld();
	if (IsValid(World) && World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, COLLISION_TRACE_LANDSCAPE,
	                                                       QueryParams))
	{
		return Hit.ImpactPoint;
	}
	// No ground found: burn level with the bottom of the hull.
	return FVector(SpillXY.X, SpillXY.Y, HullBounds.Origin.Z - HullBounds.BoxExtent.Z);
}

void UWreckEngineFireDeathComponent::ApplyCharOverlay() const
{
	using namespace WreckEngineFireDeathConstants;
	AActor* Wreck = GetOwner();
	const UWorld* World = GetWorld();
	if (not IsValid(Wreck) || not IsValid(World) || M_Settings.CharOverlayMaterial.IsNull()
		|| not GetIsValidHullMesh())
	{
		return;
	}
	// One small material, loaded once and then cached by the engine.
	UMaterialInterface* CharMaterial = M_Settings.CharOverlayMaterial.LoadSynchronous();
	if (not IsValid(CharMaterial))
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("UWreckEngineFireDeathComponent::ApplyCharOverlay - could not load %s."),
			*M_Settings.CharOverlayMaterial.ToString()));
		return;
	}

	UMaterialInstanceDynamic* CharInstance = UMaterialInstanceDynamic::Create(CharMaterial, Wreck);
	CharInstance->SetVectorParameterValue(CharCentreParam, FLinearColor(M_EngineDeckLocation.X, M_EngineDeckLocation.Y, M_EngineDeckLocation.Z));
	CharInstance->SetScalarParameterValue(CharRadiusParam,
	                                      M_HullMesh->Bounds.SphereRadius * M_Settings.CharRadiusHullScale);
	// The material cools the embers with its own Time node, so nothing ticks here.
	CharInstance->SetScalarParameterValue(EmberStartTimeParam, World->GetTimeSeconds());
	CharInstance->SetScalarParameterValue(EmberCoolDownParam, M_Settings.EmberCoolDownSeconds);

	TArray<UStaticMeshComponent*> MeshComponents;
	Wreck->GetComponents<UStaticMeshComponent>(MeshComponents);
	for (UStaticMeshComponent* MeshComponent : MeshComponents)
	{
		// Thrown debris (added at runtime as instance components) keeps its own look.
		if (IsValid(MeshComponent) && not Wreck->GetInstanceComponents().Contains(MeshComponent))
		{
			MeshComponent->SetOverlayMaterial(CharInstance);
		}
	}
}

void UWreckEngineFireDeathComponent::StopFire(int32& InOutFireHandle) const
{
	if (InOutFireHandle == INDEX_NONE)
	{
		return;
	}
	if (M_FireSubsystem.IsValid())
	{
		M_FireSubsystem->StopFireByHandle(InOutFireHandle);
	}
	InOutFireHandle = INDEX_NONE;
}

bool UWreckEngineFireDeathComponent::GetIsValidHullMesh() const
{
	if (M_HullMesh.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, "M_HullMesh", "GetIsValidHullMesh", this);
	return false;
}

bool UWreckEngineFireDeathComponent::GetIsValidFireSubsystem() const
{
	if (M_FireSubsystem.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, "M_FireSubsystem", "GetIsValidFireSubsystem", this);
	return false;
}
