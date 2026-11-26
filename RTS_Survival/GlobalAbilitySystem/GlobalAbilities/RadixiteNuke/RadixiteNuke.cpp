// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RadixiteNuke.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/DecalComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "ProceduralMeshComponent.h"
#include "RadixiteNukeCraterRim.h"
#include "RadixiteNukeDebrisRain.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/RTSComponents/ShieldComponent/ShieldTypes.h"
#include "RTS_Survival/Subsystems/CameraShakeSubsystem/RTSCameraShakeSubsystem.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/TrackedTankMaster.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/TrackPhysicsMovementComp/TrackPhysicsMovement.h"
#include "RTS_Survival/Utils/AOE/BlastThrow/RTSBlastThrowSubsystem.h"
#include "RTS_Survival/Utils/AOE/FRTS_AOE.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/WeaponData/RTSDamageTypes/RTSDamageTypes.h"
#include "UObject/ConstructorHelpers.h"

namespace RadixiteNukeConstants
{
	// /Engine/BasicShapes/Plane is 100 x 100 cm.
	constexpr float PlaneSizeCm = 100.f;
	constexpr float PlanePaddingCm = 300.f;
	constexpr float PlaneHeightCm = 40.f;
	constexpr float ScarDepthCm = 2000.f;
	constexpr float ScarSizeFraction = 1.15f;

	constexpr float FlashRadiusFraction = 3.f;
	constexpr float ShockwaveSeconds = 2.f;
	// The crater pit decal projects this far up and down: over the rim and over hills inside the pit.
	constexpr float CraterDecalDepthCm = 2000.f;
	// The pit draws over the scar.
	constexpr int32 CraterDecalSortOrder = 1;
	// Over every other post process volume of the map while it flashes.
	constexpr float ScreenFlashPriority = 1000.f;
	// Victims of the async blast damage still find its throw this long after it (game seconds).
	constexpr float BlastThrowLifetimeSeconds = 3.f;
	// The push waits for the blast damage to land, so a tank about to die is not thrown first.
	constexpr float VehiclePushDelaySeconds = 0.2f;
	// How the push falls off from ground zero to the blast edge.
	constexpr float VehiclePushFalloffExponent = 0.7f;
	// Share of the viewport around it that still counts as on screen for the hit-stop.
	constexpr float InViewScreenMargin = 0.15f;
	// Steep but not vertical, so the warhead reads as arriving from somewhere.
	constexpr float FallSidewaysFraction = 0.35f;
	// Distance of the arc's control point from the target, as part of the approach: sets the dive angle (~35 deg).
	constexpr float ArcControlDistanceFraction = 0.3f;
	// How much of the arc's speed-up comes from easing in time; the rest of the flight runs at about even speed.
	constexpr float ArcEaseIn = 0.5f;
	// The arc comes in across the screen, a little from its far side, so the whole flight is seen.
	constexpr float ArcFromFarSideFraction = 0.5f;
	constexpr int32 ShakeCalibreMm = 800;

	const FName PlaneLengthParameter(TEXT("PlaneLength"));
	const FName PlaneWidthParameter(TEXT("PlaneWidth"));
	const FName StripLengthParameter(TEXT("StripLength"));
	const FName AreaRadiusParameter(TEXT("AreaRadius"));
	const FName InnerRadiusParameter(TEXT("InnerRadius"));
	const FName IsDrawingParameter(TEXT("IsDrawing"));
	const FName ProgressParameter(TEXT("Progress"));
	const FName GlowParameter(TEXT("Glow"));
}

ARadixiteNuke::ARadixiteNuke()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetCanBeDamaged(false);

	M_Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(M_Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	const auto SetupInertMesh = [this](UStaticMeshComponent* Mesh)
	{
		Mesh->SetupAttachment(M_Root);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->SetReceivesDecals(false);
		Mesh->SetCastShadow(false);
		Mesh->SetVisibility(false);
	};

	M_WarningPlane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WarningPlane"));
	SetupInertMesh(M_WarningPlane);
	M_WarningPlane->SetStaticMesh(PlaneMesh.Object);

	M_ShockwavePlane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShockwavePlane"));
	SetupInertMesh(M_ShockwavePlane);
	M_ShockwavePlane->SetStaticMesh(PlaneMesh.Object);

	M_WarheadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WarheadMesh"));
	SetupInertMesh(M_WarheadMesh);

	M_TrailEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("TrailEffect"));
	M_TrailEffect->SetupAttachment(M_WarheadMesh);
	M_TrailEffect->SetAutoActivate(false);

	M_SmokeTrailEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("SmokeTrailEffect"));
	M_SmokeTrailEffect->SetupAttachment(M_WarheadMesh);
	M_SmokeTrailEffect->SetAutoActivate(false);

	M_FlashLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FlashLight"));
	M_FlashLight->SetupAttachment(M_Root);
	M_FlashLight->SetCastShadows(false);
	M_FlashLight->SetIntensity(0.f);
	M_FlashLight->SetVisibility(false);

	M_ScarDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("ScarDecal"));
	M_ScarDecal->SetupAttachment(M_Root);
	// Decals project along their X axis: this one looks straight down.
	M_ScarDecal->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	M_ScarDecal->SetVisibility(false);

	M_CraterDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("CraterDecal"));
	M_CraterDecal->SetupAttachment(M_Root);
	M_CraterDecal->SetVisibility(false);
	M_CraterDecal->SortOrder = RadixiteNukeConstants::CraterDecalSortOrder;
	// A blast crater stays in view at any zoom.
	M_CraterDecal->SetFadeScreenSize(0.f);

	M_CraterRim = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("CraterRim"));
	M_CraterRim->SetupAttachment(M_Root);
	M_CraterRim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M_CraterRim->SetGenerateOverlapEvents(false);
	M_CraterRim->SetCanEverAffectNavigation(false);
	M_CraterRim->bUseAsyncCooking = true;
	M_CraterRim->SetVisibility(false);

	M_ScreenFlash = CreateDefaultSubobject<UPostProcessComponent>(TEXT("ScreenFlash"));
	M_ScreenFlash->SetupAttachment(M_Root);
	M_ScreenFlash->bUnbound = true;
	M_ScreenFlash->Priority = RadixiteNukeConstants::ScreenFlashPriority;
	M_ScreenFlash->BlendWeight = 0.f;
	M_ScreenFlash->bEnabled = false;
}

void ARadixiteNuke::StartNuke(const FVector& GroundZero, const float BlastRadius, const float CoreRadius,
                              const FRadixiteNukeSettings& Settings, const FRadixiteNukeVisuals& Visuals,
                              const uint8 OwningPlayer)
{
	M_GroundZero = GroundZero;
	M_BlastRadius = BlastRadius;
	M_CoreRadius = CoreRadius;
	M_Settings = Settings;
	M_Visuals = Visuals;
	M_OwningPlayer = OwningPlayer;
	SetActorLocation(GroundZero);

	SetupWarningArea();
	SetupShockwave();
	SetupScar();
	if (USoundBase* WarningSound = Visuals.WarningSound.Get())
	{
		if (Visuals.bWarningSound2D)
		{
			UGameplayStatics::PlaySound2D(this, WarningSound);
		}
		else
		{
			UGameplayStatics::SpawnSoundAtLocation(this, WarningSound, GroundZero);
		}
	}
	M_Phase = ERadixiteNukePhase::Warning;
	M_PhaseSeconds = 0.f;
	SetActorTickEnabled(true);
}

FVector ARadixiteNuke::GetWarheadLocation() const
{
	return M_WarheadMesh->GetComponentLocation();
}

bool ARadixiteNuke::GetHasCrater() const
{
	return M_CraterDecal->IsVisible() && M_CraterRim->IsVisible() && M_CraterRim->GetNumSections() > 0;
}

void ARadixiteNuke::SetupWarningArea()
{
	using namespace RadixiteNukeConstants;
	UMaterialInterface* WarningMaterial = M_Visuals.WarningAreaMaterial.Get();
	if (not IsValid(WarningMaterial))
	{
		return;
	}
	const float PlaneSize = 2.f * (M_BlastRadius + PlanePaddingCm);
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(WarningMaterial, this);
	Material->SetScalarParameterValue(PlaneLengthParameter, PlaneSize);
	Material->SetScalarParameterValue(PlaneWidthParameter, PlaneSize);
	Material->SetScalarParameterValue(StripLengthParameter, 0.f);
	Material->SetScalarParameterValue(AreaRadiusParameter, M_BlastRadius);
	Material->SetScalarParameterValue(InnerRadiusParameter, M_CoreRadius);
	Material->SetScalarParameterValue(IsDrawingParameter, 1.f);
	M_WarningPlane->SetMaterial(0, Material);
	M_WarningPlane->SetRelativeLocation(FVector(0.f, 0.f, PlaneHeightCm));
	M_WarningPlane->SetRelativeScale3D(FVector(PlaneSize / PlaneSizeCm, PlaneSize / PlaneSizeCm, 1.f));
	M_WarningPlane->SetVisibility(true);
}

void ARadixiteNuke::SetupShockwave()
{
	using namespace RadixiteNukeConstants;
	UMaterialInterface* ShockwaveMaterial = M_Visuals.ShockwaveMaterial.Get();
	if (not IsValid(ShockwaveMaterial))
	{
		return;
	}
	const float PlaneSize = 2.f * (M_BlastRadius + PlanePaddingCm);
	M_ShockwaveMaterial = UMaterialInstanceDynamic::Create(ShockwaveMaterial, this);
	M_ShockwaveMaterial->SetScalarParameterValue(PlaneLengthParameter, PlaneSize);
	M_ShockwaveMaterial->SetScalarParameterValue(AreaRadiusParameter, M_BlastRadius);
	M_ShockwavePlane->SetMaterial(0, M_ShockwaveMaterial);
	M_ShockwavePlane->SetRelativeLocation(FVector(0.f, 0.f, PlaneHeightCm));
	M_ShockwavePlane->SetRelativeScale3D(FVector(PlaneSize / PlaneSizeCm, PlaneSize / PlaneSizeCm, 1.f));
}

void ARadixiteNuke::SetupScar()
{
	using namespace RadixiteNukeConstants;
	UMaterialInterface* ScarMaterial = M_Visuals.ScarMaterial.Get();
	// A crater replaces the scar: the near-black burnt disc is twice its size and painted over both the pit and
	// the rim.
	if (not IsValid(ScarMaterial) || M_Visuals.Crater.RimRadius > 0.f)
	{
		return;
	}
	const float ScarHalfSize = M_BlastRadius * ScarSizeFraction;
	M_ScarMaterial = UMaterialInstanceDynamic::Create(ScarMaterial, this);
	M_ScarMaterial->SetScalarParameterValue(GlowParameter, 1.f);
	M_ScarDecal->SetDecalMaterial(M_ScarMaterial);
	M_ScarDecal->DecalSize = FVector(ScarDepthCm, ScarHalfSize, ScarHalfSize);
}

void ARadixiteNuke::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EndHitStop();
	Super::EndPlay(EndPlayReason);
}

void ARadixiteNuke::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	M_PhaseSeconds += DeltaSeconds;
	switch (M_Phase)
	{
	case ERadixiteNukePhase::Warning:
		if (M_PhaseSeconds >= M_Settings.WarningSeconds - M_Settings.FallSeconds)
		{
			StartFalling();
		}
		return;
	case ERadixiteNukePhase::Falling:
		if (M_PhaseSeconds >= M_Settings.FallSeconds)
		{
			Detonate();
			return;
		}
		TickFalling();
		return;
	case ERadixiteNukePhase::Radiating:
		TickBlastVisuals();
		TickScreenFlash();
		TickRadiating(DeltaSeconds);
		return;
	case ERadixiteNukePhase::Inactive:
	case ERadixiteNukePhase::Spent:
		return;
	}
}

void ARadixiteNuke::StartFalling()
{
	M_Phase = ERadixiteNukePhase::Falling;
	M_PhaseSeconds = 0.f;
	if (UStaticMesh* WarheadMesh = M_Visuals.WarheadMesh.Get())
	{
		M_WarheadMesh->SetStaticMesh(WarheadMesh);
		M_WarheadMesh->SetRelativeScale3D(FVector(M_Visuals.WarheadScale));
		M_WarheadMesh->SetVisibility(true);
	}
	const auto StartTrail = [this](UNiagaraComponent* Trail, UNiagaraSystem* Effect)
	{
		if (not IsValid(Effect))
		{
			return;
		}
		Trail->SetRelativeLocationAndRotation(M_Visuals.WarheadTrailOffset, M_Visuals.WarheadTrailRotation);
		Trail->SetRelativeScale3D(FVector(M_Visuals.WarheadTrailScale));
		Trail->SetAsset(Effect);
		Trail->Activate(true);
	};
	StartTrail(M_TrailEffect, M_Visuals.WarheadTrailEffect.Get());
	StartTrail(M_SmokeTrailEffect, M_Visuals.WarheadSmokeTrailEffect.Get());
	SetupFlightPath();
	TickFalling();
}

void ARadixiteNuke::SetupFlightPath()
{
	using namespace RadixiteNukeConstants;
	if (M_Settings.ArcApproachDistance <= 0.f)
	{
		M_FlightStart = M_GroundZero + FVector(M_Settings.FallStartHeight * FallSidewaysFraction, 0.f,
		                                       M_Settings.FallStartHeight);
		M_FlightControl = FMath::Lerp(M_FlightStart, M_GroundZero, 0.5f);
		return;
	}
	const FVector Direction = GetArcApproachDirection();
	const float StartHeight = M_Settings.ArcStartHeight;
	const float ApexHeight = FMath::Max(M_Settings.ArcApexHeight, StartHeight);
	// The highest point of a quadratic Bezier from StartHeight down to 0 through a control point at height C is
	// C^2 / (2C - StartHeight); solved for C.
	const float ControlHeight = ApexHeight + FMath::Sqrt(ApexHeight * (ApexHeight - StartHeight));
	M_FlightStart = M_GroundZero + Direction * M_Settings.ArcApproachDistance + FVector(0.f, 0.f, StartHeight);
	M_FlightControl = M_GroundZero + Direction * (M_Settings.ArcApproachDistance * ArcControlDistanceFraction)
		+ FVector(0.f, 0.f, ControlHeight);
}

FVector ARadixiteNuke::GetArcApproachDirection() const
{
	using namespace RadixiteNukeConstants;
	const APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	const FRotator CameraYaw(0.f, IsValid(CameraManager) ? CameraManager->GetCameraRotation().Yaw : 0.f, 0.f);
	const FVector Side = FRotationMatrix(CameraYaw).GetUnitAxis(EAxis::Y) * (FMath::RandBool() ? 1.f : -1.f);
	const FVector FarSide = FRotationMatrix(CameraYaw).GetUnitAxis(EAxis::X) * ArcFromFarSideFraction;
	return (Side + FarSide).GetSafeNormal();
}

void ARadixiteNuke::TickFalling()
{
	using namespace RadixiteNukeConstants;
	const float Alpha = FMath::Clamp(M_PhaseSeconds / M_Settings.FallSeconds, 0.f, 1.f);
	// The falling warhead keeps accelerating down to the ground; the arc only speeds up a little in its dive.
	const float T = M_Settings.ArcApproachDistance > 0.f
		                ? FMath::Lerp(Alpha, Alpha * Alpha, ArcEaseIn)
		                : Alpha * Alpha;
	const float U = 1.f - T;
	const FVector Location = U * U * M_FlightStart + 2.f * U * T * M_FlightControl + T * T * M_GroundZero;
	const FVector Tangent = U * (M_FlightControl - M_FlightStart) + T * (M_GroundZero - M_FlightControl);
	M_WarheadMesh->SetWorldLocationAndRotation(
		Location, (Tangent.Rotation().Quaternion() * M_Visuals.WarheadRotation.Quaternion()).Rotator());
}

void ARadixiteNuke::Detonate()
{
	using namespace RadixiteNukeConstants;
	M_Phase = ERadixiteNukePhase::Radiating;
	M_PhaseSeconds = 0.f;
	M_NextRadiationPulseSeconds = M_Settings.RadiationPulseSeconds;
	M_WarningPlane->SetVisibility(false);
	M_WarheadMesh->SetVisibility(false);
	M_TrailEffect->Deactivate();
	M_SmokeTrailEffect->Deactivate();

	M_FlashLight->SetLightColor(M_Visuals.FlashColor);
	M_FlashLight->SetAttenuationRadius(M_BlastRadius * FlashRadiusFraction);
	M_FlashLight->SetRelativeLocation(FVector(0.f, 0.f, M_BlastRadius * 0.5f));
	M_FlashLight->SetVisibility(true);
	M_ShockwavePlane->SetVisibility(IsValid(M_ShockwaveMaterial));
	M_ScarDecal->SetVisibility(IsValid(M_ScarMaterial));

	ShowCrater();
	if (UNiagaraSystem* DetonationEffect = M_Visuals.DetonationEffect.Get())
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, DetonationEffect, M_GroundZero, FRotator::ZeroRotator,
		                                               FVector(M_Visuals.DetonationEffectScale));
	}
	SpawnDetonationLayers();
	if (UNiagaraSystem* RadiationEffect = M_Visuals.RadiationEffect.Get())
	{
		M_RadiationEffect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, RadiationEffect, M_GroundZero);
	}
	if (USoundBase* DetonationSound = M_Visuals.DetonationSound.Get())
	{
		UGameplayStatics::PlaySound2D(this, DetonationSound);
	}
	ScheduleAfterBlastSounds();
	StartDebrisRain();
	StartImpactFeel();
	RequestBlastCameraShake();
	ScheduleBlastShakePulses();
	RegisterBlastThrow();
	DealBlastDamage();
	SchedulePushSurvivingVehicles();
	TickBlastVisuals();
}

void ARadixiteNuke::DealBlastDamage() const
{
	AActor* DamageCauser = GetDamageCauser();
	const FAoeInstigator AffectedUnits = GetAffectedUnits();
	FRTS_AOE::DealDamageInRadiusAsync(
		DamageCauser,
		M_GroundZero,
		M_CoreRadius,
		M_Settings.CoreDamage,
		M_Settings.BlastFalloffExponent,
		ERTSDamageType::Kinetic,
		AffectedUnits,
		EShieldDamageSource::AreaOfEffect);
	FRTS_AOE::DealDamageVsRearArmorInRadiusAsync(
		DamageCauser,
		M_GroundZero,
		M_BlastRadius,
		M_Settings.BlastDamage,
		M_Settings.BlastFalloffExponent,
		M_Settings.BlastFullDamageArmor,
		M_Settings.BlastArmorFalloff,
		M_Settings.BlastMaxArmor,
		ERTSDamageType::Kinetic,
		AffectedUnits,
		EShieldDamageSource::AreaOfEffect,
		TArray<TWeakObjectPtr<AActor>>(),
		EVehicleModuleSplashPolicy::DamageExternalModules,
		static_cast<EWeaponShellType>(0),
		EAoeFalloffDistance::ClosestCollision);
}

void ARadixiteNuke::RegisterBlastThrow() const
{
	const FRadixiteNukeBlastThrow& BlastThrow = M_Visuals.BlastThrow;
	const bool bThrowsHarder = not FMath::IsNearlyEqual(BlastThrow.InfantryThrowScale, 1.f) || BlastThrow.bTossTurrets;
	UWorld* World = GetWorld();
	URTSBlastThrowSubsystem* ThrowSubsystem = IsValid(World) ? World->GetSubsystem<URTSBlastThrowSubsystem>() : nullptr;
	if (not bThrowsHarder || not IsValid(ThrowSubsystem))
	{
		return;
	}
	FRTSBlastThrow Throw;
	Throw.Epicenter = M_GroundZero;
	Throw.InfantryThrowScale = BlastThrow.InfantryThrowScale;
	Throw.bTossTurrets = BlastThrow.bTossTurrets;
	Throw.TurretTossSpeedScale = BlastThrow.TurretTossSpeedScale;
	ThrowSubsystem->RegisterBlastThrow(Throw, RadixiteNukeConstants::BlastThrowLifetimeSeconds);
}

void ARadixiteNuke::SchedulePushSurvivingVehicles()
{
	const FRadixiteNukeBlastThrow& BlastThrow = M_Visuals.BlastThrow;
	UWorld* World = GetWorld();
	const bool bPushes = BlastThrow.VehiclePushSpeed > 0.f || BlastThrow.VehicleLiftSpeed > 0.f;
	if (not bPushes || not IsValid(World))
	{
		return;
	}
	FTimerHandle PushHandle;
	World->GetTimerManager().SetTimer(PushHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		PushSurvivingVehicles();
	}), RadixiteNukeConstants::VehiclePushDelaySeconds, false);
}

void ARadixiteNuke::PushSurvivingVehicles() const
{
	// The sweep reports a vehicle once per component; push each vehicle once.
	TSharedPtr<TSet<TWeakObjectPtr<AActor>>> PushedVehicles = MakeShared<TSet<TWeakObjectPtr<AActor>>>();
	TWeakObjectPtr<const ARadixiteNuke> WeakThis(this);
	FRTS_AOE::ApplyToActorsAsync(GetDamageCauser(), M_GroundZero, M_BlastRadius, GetAffectedUnits(),
	                             [WeakThis, PushedVehicles](AActor& FoundActor)
	                             {
		                             if (not WeakThis.IsValid() || PushedVehicles->Contains(&FoundActor))
		                             {
			                             return;
		                             }
		                             PushedVehicles->Add(&FoundActor);
		                             WeakThis->PushVehicle(FoundActor);
	                             });
}

void ARadixiteNuke::PushVehicle(AActor& Vehicle) const
{
	using namespace RadixiteNukeConstants;
	ATankMaster* Tank = Cast<ATankMaster>(&Vehicle);
	if (not IsValid(Tank) || not Tank->IsUnitAlive())
	{
		return;
	}
	// Deployed nomadic vehicles and trains on their rails do not simulate; they stand firm.
	USkeletalMeshComponent* TankMesh = Tank->GetTankMesh();
	if (not IsValid(TankMesh) || not TankMesh->IsSimulatingPhysics())
	{
		return;
	}
	const FRadixiteNukeBlastThrow& BlastThrow = M_Visuals.BlastThrow;
	const FVector FromGroundZero = Tank->GetActorLocation() - M_GroundZero;
	const float DistanceFraction = FMath::Clamp(FromGroundZero.Size2D() / FMath::Max(M_BlastRadius, 1.f), 0.f, 1.f);
	const float Strength = FMath::Pow(1.f - DistanceFraction, VehiclePushFalloffExponent);
	if (Strength <= 0.f)
	{
		return;
	}
	const FVector Outward = FromGroundZero.GetSafeNormal2D();
	const FVector VelocityChange = (Outward * BlastThrow.VehiclePushSpeed
		+ FVector::UpVector * BlastThrow.VehicleLiftSpeed) * Strength;
	if (const ATrackedTankMaster* TrackedTank = Cast<ATrackedTankMaster>(Tank))
	{
		if (UTrackPhysicsMovement* TrackMovement = TrackedTank->GetTrackPhysicsMovement())
		{
			TrackMovement->StartKnockback(BlastThrow.VehicleKnockbackSeconds);
		}
	}
	TankMesh->AddImpulse(VelocityChange, NAME_None, true);
	if (BlastThrow.VehicleSpinDegrees > 0.f)
	{
		// Mostly a slew about the vertical, with a little rock.
		const FVector SpinAxis = (FVector::UpVector + FMath::VRand() * 0.3f).GetSafeNormal();
		const float SpinSign = FMath::RandBool() ? 1.f : -1.f;
		TankMesh->AddAngularImpulseInDegrees(SpinAxis * BlastThrow.VehicleSpinDegrees * Strength * SpinSign, NAME_None,
		                                     true);
	}
}

void ARadixiteNuke::RequestBlastCameraShake() const
{
	const UWorld* World = GetWorld();
	URTSCameraShakeSubsystem* ShakeSubsystem = IsValid(World)
		                                           ? World->GetSubsystem<URTSCameraShakeSubsystem>()
		                                           : nullptr;
	if (not IsValid(ShakeSubsystem))
	{
		return;
	}
	FRTSCameraShakeRequest Request;
	Request.M_WorldLocation = M_GroundZero;
	Request.M_CalibreMm = RadixiteNukeConstants::ShakeCalibreMm;
	Request.M_EventType = ERTSCameraShakeEventType::Explosion;
	ShakeSubsystem->RequestExplosionShake(Request);
}

void ARadixiteNuke::ScheduleBlastShakePulses()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	for (int32 Pulse = 1; Pulse < M_Visuals.BlastShakePulses; ++Pulse)
	{
		FTimerHandle PulseHandle;
		World->GetTimerManager().SetTimer(
			PulseHandle, FTimerDelegate::CreateWeakLambda(this, [this]() { RequestBlastCameraShake(); }),
			Pulse * M_Visuals.BlastShakePulseIntervalSeconds, false);
	}
}

void ARadixiteNuke::ShowCrater()
{
	using namespace RadixiteNukeConstants;
	const FRadixiteNukeCrater& Crater = M_Visuals.Crater;
	UWorld* World = GetWorld();
	if (Crater.RimRadius <= 0.f || not IsValid(World))
	{
		return;
	}
	if (UMaterialInterface* PitMaterial = Crater.PitDecalMaterial.Get())
	{
		const float DecalHalfSize = Crater.RimRadius / Crater.DecalRimFraction;
		M_CraterDecal->SetDecalMaterial(PitMaterial);
		M_CraterDecal->DecalSize = FVector(CraterDecalDepthCm, DecalHalfSize, DecalHalfSize);
		// Pitched to look straight down; the yaw then spins the painted pit about the vertical.
		M_CraterDecal->SetRelativeRotation(FRotator(-90.f, FMath::FRandRange(0.f, 360.f), 0.f));
		M_CraterDecal->SetVisibility(true);
	}
	UMaterialInterface* RimMaterial = Crater.RimMaterial.Get();
	if (not IsValid(RimMaterial) || Crater.RimHeight <= 0.f)
	{
		return;
	}
	if (Crater.bRimCarriesVehicles)
	{
		FRTS_CollisionSetup::SetupVehicleRampCollision(M_CraterRim);
		// The ramp setup is made for invisible hulls; this rim is the visible ground and wears the pit decal.
		M_CraterRim->SetReceivesDecals(true);
	}
	const FRadixiteNukeGroundSampler Ground(*World, M_GroundZero);
	RadixiteNukeCraterRim::BuildRimMesh(*M_CraterRim, Crater, Ground, Crater.bRimCarriesVehicles);
	M_CraterRim->SetMaterial(0, RimMaterial);
	M_CraterRim->SetVisibility(true);
}

void ARadixiteNuke::SpawnDetonationLayers()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	// Copied: a timer fires after this frame, and the layers stay in M_Visuals for the lifetime of the actor.
	for (int32 LayerIndex = 0; LayerIndex < M_Visuals.DetonationLayers.Num(); ++LayerIndex)
	{
		const FRadixiteNukeDetonationLayer& Layer = M_Visuals.DetonationLayers[LayerIndex];
		if (Layer.Effect.IsNull())
		{
			continue;
		}
		for (int32 CopyIndex = 0; CopyIndex < Layer.Count; ++CopyIndex)
		{
			const float Delay = FMath::FRandRange(Layer.MinDelaySeconds,
			                                      FMath::Max(Layer.MinDelaySeconds, Layer.MaxDelaySeconds));
			if (Delay <= 0.f)
			{
				SpawnDetonationLayerCopy(Layer, CopyIndex);
				continue;
			}
			FTimerHandle CopyHandle;
			World->GetTimerManager().SetTimer(
				CopyHandle, FTimerDelegate::CreateWeakLambda(this, [this, LayerIndex, CopyIndex]()
				{
					if (M_Visuals.DetonationLayers.IsValidIndex(LayerIndex))
					{
						SpawnDetonationLayerCopy(M_Visuals.DetonationLayers[LayerIndex], CopyIndex);
					}
				}), Delay, false);
		}
	}
}

void ARadixiteNuke::SpawnDetonationLayerCopy(const FRadixiteNukeDetonationLayer& Layer, const int32 CopyIndex)
{
	UNiagaraSystem* Effect = Layer.Effect.Get();
	UWorld* World = GetWorld();
	if (not IsValid(Effect) || not IsValid(World))
	{
		return;
	}
	// Even spread around ground zero with a little jitter, so a ring of copies does not read as a pattern.
	constexpr float AngleJitter = 0.35f;
	const float Angle = UE_TWO_PI * (CopyIndex + FMath::FRandRange(-AngleJitter, AngleJitter))
		/ FMath::Max(Layer.Count, 1);
	const float Distance = M_BlastRadius * FMath::FRandRange(Layer.MinRadiusFraction,
	                                                         FMath::Max(Layer.MinRadiusFraction,
	                                                                    Layer.MaxRadiusFraction));
	const FVector2D SpotXY = FVector2D(M_GroundZero) + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Distance;
	const FRadixiteNukeGroundSampler Ground(*World, M_GroundZero);
	const FVector Spot(SpotXY.X, SpotXY.Y, Ground.GetGroundHeight(SpotXY));
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Effect, Spot, FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f),
	                                               FVector(Layer.Scale));
}

void ARadixiteNuke::ScheduleAfterBlastSounds()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	for (int32 SoundIndex = 0; SoundIndex < M_Visuals.AfterBlastSounds.Num(); ++SoundIndex)
	{
		const FRadixiteNukeDelayedSound& DelayedSound = M_Visuals.AfterBlastSounds[SoundIndex];
		if (DelayedSound.Sound.IsNull())
		{
			continue;
		}
		const auto PlayDelayedSound = [this, SoundIndex]()
		{
			if (not M_Visuals.AfterBlastSounds.IsValidIndex(SoundIndex))
			{
				return;
			}
			const FRadixiteNukeDelayedSound& Entry = M_Visuals.AfterBlastSounds[SoundIndex];
			if (USoundBase* Sound = Entry.Sound.Get())
			{
				UGameplayStatics::PlaySound2D(this, Sound, Entry.VolumeMultiplier, Entry.PitchMultiplier);
			}
		};
		if (DelayedSound.DelaySeconds <= 0.f)
		{
			PlayDelayedSound();
			continue;
		}
		FTimerHandle SoundHandle;
		World->GetTimerManager().SetTimer(SoundHandle, FTimerDelegate::CreateWeakLambda(this, PlayDelayedSound),
		                                  DelayedSound.DelaySeconds, false);
	}
}

void ARadixiteNuke::StartDebrisRain()
{
	UWorld* World = GetWorld();
	if (M_Visuals.DebrisRain.Count <= 0 || M_Visuals.DebrisRain.Meshes.IsEmpty() || not IsValid(World))
	{
		return;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	ARadixiteNukeDebrisRain* DebrisRain = World->SpawnActor<ARadixiteNukeDebrisRain>(
		ARadixiteNukeDebrisRain::StaticClass(), M_GroundZero, FRotator::ZeroRotator, SpawnParameters);
	if (not IsValid(DebrisRain))
	{
		return;
	}
	DebrisRain->StartRain(M_Visuals.DebrisRain, M_BlastRadius);
}

bool ARadixiteNuke::GetIsBlastInView() const
{
	using namespace RadixiteNukeConstants;
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (not IsValid(PlayerController) || not IsValid(PlayerController->PlayerCameraManager))
	{
		return false;
	}
	const float CameraDistance = FVector::Dist(PlayerController->PlayerCameraManager->GetCameraLocation(),
	                                           M_GroundZero);
	if (CameraDistance > M_Visuals.ImpactFeel.MaxCameraDistance)
	{
		return false;
	}
	FVector2D ScreenLocation;
	if (not PlayerController->ProjectWorldLocationToScreen(M_GroundZero, ScreenLocation))
	{
		return false;
	}
	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
	const FVector2D Margin(ViewportWidth * InViewScreenMargin, ViewportHeight * InViewScreenMargin);
	return ScreenLocation.X >= -Margin.X && ScreenLocation.X <= ViewportWidth + Margin.X
		&& ScreenLocation.Y >= -Margin.Y && ScreenLocation.Y <= ViewportHeight + Margin.Y;
}

void ARadixiteNuke::StartImpactFeel()
{
	const FRadixiteNukeImpactFeel& ImpactFeel = M_Visuals.ImpactFeel;
	const bool bHasImpactFeel = ImpactFeel.HitStopSeconds > 0.f || ImpactFeel.ScreenFlashSeconds > 0.f;
	if (not bHasImpactFeel || not GetIsBlastInView())
	{
		return;
	}
	if (ImpactFeel.ScreenFlashSeconds > 0.f)
	{
		SetupScreenFlash();
	}
	if (ImpactFeel.HitStopSeconds > 0.f)
	{
		StartHitStop();
	}
}

void ARadixiteNuke::StartHitStop()
{
	UWorld* World = GetWorld();
	AWorldSettings* WorldSettings = IsValid(World) ? World->GetWorldSettings() : nullptr;
	if (not IsValid(WorldSettings))
	{
		return;
	}
	M_TimeDilationBeforeHitStop = WorldSettings->TimeDilation;
	UGameplayStatics::SetGlobalTimeDilation(
		this, M_TimeDilationBeforeHitStop * M_Visuals.ImpactFeel.HitStopTimeDilation);
	// Read back: the world settings clamp the dilation.
	M_HitStopTimeDilation = WorldSettings->TimeDilation;
	bM_IsHitStopActive = true;
	// Timers run on game time, which now passes at the hit-stop speed.
	const float HitStopGameSeconds = M_Visuals.ImpactFeel.HitStopSeconds * M_HitStopTimeDilation;
	FTimerHandle HitStopHandle;
	World->GetTimerManager().SetTimer(HitStopHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		EndHitStop();
	}), FMath::Max(HitStopGameSeconds, UE_KINDA_SMALL_NUMBER), false);
}

void ARadixiteNuke::EndHitStop()
{
	if (not bM_IsHitStopActive)
	{
		return;
	}
	bM_IsHitStopActive = false;
	const UWorld* World = GetWorld();
	const AWorldSettings* WorldSettings = IsValid(World) ? World->GetWorldSettings() : nullptr;
	if (not IsValid(WorldSettings))
	{
		return;
	}
	// Something else (a test, a game speed change) set the speed meanwhile: leave theirs.
	if (not FMath::IsNearlyEqual(WorldSettings->TimeDilation, M_HitStopTimeDilation))
	{
		return;
	}
	UGameplayStatics::SetGlobalTimeDilation(this, M_TimeDilationBeforeHitStop);
}

void ARadixiteNuke::SetupScreenFlash()
{
	const FRadixiteNukeImpactFeel& ImpactFeel = M_Visuals.ImpactFeel;
	FPostProcessSettings& Settings = M_ScreenFlash->Settings;
	Settings.bOverride_AutoExposureBias = true;
	Settings.AutoExposureBias = ImpactFeel.ScreenFlashExposureBias;
	Settings.bOverride_BloomIntensity = true;
	Settings.BloomIntensity = ImpactFeel.ScreenFlashBloomIntensity;
	Settings.bOverride_SceneFringeIntensity = true;
	Settings.SceneFringeIntensity = ImpactFeel.ScreenFringeIntensity;
	M_ScreenFlash->BlendWeight = 1.f;
	M_ScreenFlash->bEnabled = true;
	bM_IsScreenFlashing = true;
}

void ARadixiteNuke::TickScreenFlash()
{
	if (not bM_IsScreenFlashing)
	{
		return;
	}
	const float FlashAlpha = 1.f - FMath::Clamp(M_PhaseSeconds / M_Visuals.ImpactFeel.ScreenFlashSeconds, 0.f, 1.f);
	// Snaps white and eases out, like an eye recovering from the flash.
	M_ScreenFlash->BlendWeight = FlashAlpha * FlashAlpha * FlashAlpha;
	if (FlashAlpha > 0.f)
	{
		return;
	}
	M_ScreenFlash->bEnabled = false;
	bM_IsScreenFlashing = false;
}

void ARadixiteNuke::TickBlastVisuals() const
{
	using namespace RadixiteNukeConstants;
	const float FlashAlpha = 1.f - FMath::Clamp(M_PhaseSeconds / M_Visuals.FlashFadeSeconds, 0.f, 1.f);
	M_FlashLight->SetIntensity(M_Visuals.FlashIntensity * FlashAlpha * FlashAlpha);
	if (FlashAlpha <= 0.f)
	{
		M_FlashLight->SetVisibility(false);
	}

	const float ShockwaveAlpha = FMath::Clamp(M_PhaseSeconds / ShockwaveSeconds, 0.f, 1.f);
	if (IsValid(M_ShockwaveMaterial))
	{
		M_ShockwaveMaterial->SetScalarParameterValue(ProgressParameter, ShockwaveAlpha);
	}
	if (ShockwaveAlpha >= 1.f)
	{
		M_ShockwavePlane->SetVisibility(false);
	}

	if (IsValid(M_ScarMaterial))
	{
		// Without radiation the scar is plain burnt ground from the start.
		const float RadiationAlpha = M_Settings.RadiationSeconds > 0.f
			                             ? FMath::Clamp(M_PhaseSeconds / M_Settings.RadiationSeconds, 0.f, 1.f)
			                             : 1.f;
		M_ScarMaterial->SetScalarParameterValue(GlowParameter, 1.f - RadiationAlpha);
	}
}

void ARadixiteNuke::TickRadiating(const float DeltaSeconds)
{
	if (M_PhaseSeconds >= M_NextRadiationPulseSeconds && M_PhaseSeconds <= M_Settings.RadiationSeconds)
	{
		M_NextRadiationPulseSeconds += M_Settings.RadiationPulseSeconds;
		DealRadiationPulse();
	}
	// Without radiation the flash and the shockwave still need their ticks to fade out.
	const float BlastVisualSeconds = FMath::Max(M_Visuals.FlashFadeSeconds, RadixiteNukeConstants::ShockwaveSeconds);
	if (M_PhaseSeconds > FMath::Max(M_Settings.RadiationSeconds, BlastVisualSeconds))
	{
		EndRadiation();
	}
}

void ARadixiteNuke::DealRadiationPulse() const
{
	FRTS_AOE::DealDamageInRadiusAsync(
		GetDamageCauser(),
		M_GroundZero,
		M_BlastRadius,
		M_Settings.RadiationDamagePerPulse,
		M_Settings.BlastFalloffExponent,
		ERTSDamageType::Radiation,
		GetAffectedUnits(),
		EShieldDamageSource::AreaOfEffect);
}

void ARadixiteNuke::EndRadiation()
{
	M_Phase = ERadixiteNukePhase::Spent;
	SetActorTickEnabled(false);
	// The tick that fades the flash stops here; a flash set longer than the blast visuals just ends.
	M_ScreenFlash->bEnabled = false;
	bM_IsScreenFlashing = false;
	if (IsValid(M_RadiationEffect))
	{
		M_RadiationEffect->Deactivate();
	}
}

AActor* ARadixiteNuke::GetDamageCauser() const
{
	if (FRTSPlayers::IsHumanPlayer(M_OwningPlayer))
	{
		return FRTS_Statics::GetRTSController(this);
	}
	// The controller of the AI player that fired; a map without one for that player keeps the default controller.
	AEnemyController* OwnController = FRTS_Statics::GetEnemyControllerForPlayer(this, M_OwningPlayer);
	return IsValid(OwnController) ? OwnController : FRTS_Statics::GetEnemyController(this);
}

FAoeInstigator ARadixiteNuke::GetAffectedUnits() const
{
	// Authored from the firing player's point of view: Player = its own and allied units, Enemy = its hostiles.
	return FAoeInstigator::FromOwnerRelativeLogic(M_OwningPlayer, M_Settings.AffectedUnits);
}
