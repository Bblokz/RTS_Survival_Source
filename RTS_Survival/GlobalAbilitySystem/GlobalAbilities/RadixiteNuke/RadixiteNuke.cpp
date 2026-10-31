// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RadixiteNuke.h"

#include "Components/DecalComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/RTSComponents/ShieldComponent/ShieldTypes.h"
#include "RTS_Survival/Subsystems/CameraShakeSubsystem/RTSCameraShakeSubsystem.h"
#include "RTS_Survival/Utils/AOE/FRTS_AOE.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/WeaponData/RTSDamageTypes/RTSDamageTypes.h"
#include "UObject/ConstructorHelpers.h"

namespace RadixiteNukeConstants
{
	constexpr uint8 HumanPlayer = 1;
	// /Engine/BasicShapes/Plane is 100 x 100 cm.
	constexpr float PlaneSizeCm = 100.f;
	constexpr float PlanePaddingCm = 300.f;
	constexpr float PlaneHeightCm = 40.f;
	constexpr float ScarDepthCm = 2000.f;
	constexpr float ScarSizeFraction = 1.15f;

	constexpr float FlashRadiusFraction = 3.f;
	constexpr float FlashFadeSeconds = 2.5f;
	constexpr float ShockwaveSeconds = 2.f;
	// Steep but not vertical, so the warhead reads as arriving from somewhere.
	constexpr float FallSidewaysFraction = 0.35f;
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
		UGameplayStatics::SpawnSoundAtLocation(this, WarningSound, GroundZero);
	}
	M_Phase = ERadixiteNukePhase::Warning;
	M_PhaseSeconds = 0.f;
	SetActorTickEnabled(true);
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
	if (not IsValid(ScarMaterial))
	{
		return;
	}
	const float ScarHalfSize = M_BlastRadius * ScarSizeFraction;
	M_ScarMaterial = UMaterialInstanceDynamic::Create(ScarMaterial, this);
	M_ScarMaterial->SetScalarParameterValue(GlowParameter, 1.f);
	M_ScarDecal->SetDecalMaterial(M_ScarMaterial);
	M_ScarDecal->DecalSize = FVector(ScarDepthCm, ScarHalfSize, ScarHalfSize);
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
	if (UNiagaraSystem* TrailEffect = M_Visuals.WarheadTrailEffect.Get())
	{
		M_TrailEffect->SetAsset(TrailEffect);
		M_TrailEffect->Activate(true);
	}
	TickFalling();
}

void ARadixiteNuke::TickFalling()
{
	using namespace RadixiteNukeConstants;
	const FVector FallStart = M_GroundZero + FVector(M_Settings.FallStartHeight * FallSidewaysFraction, 0.f,
	                                                 M_Settings.FallStartHeight);
	const float Alpha = FMath::Clamp(M_PhaseSeconds / M_Settings.FallSeconds, 0.f, 1.f);
	// It keeps accelerating down to the ground.
	const FVector Location = FMath::Lerp(FallStart, M_GroundZero, Alpha * Alpha);
	const FRotator FlightRotation = (M_GroundZero - FallStart).Rotation();
	M_WarheadMesh->SetWorldLocationAndRotation(
		Location, (FlightRotation.Quaternion() * M_Visuals.WarheadRotation.Quaternion()).Rotator());
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

	M_FlashLight->SetLightColor(M_Visuals.FlashColor);
	M_FlashLight->SetAttenuationRadius(M_BlastRadius * FlashRadiusFraction);
	M_FlashLight->SetRelativeLocation(FVector(0.f, 0.f, M_BlastRadius * 0.5f));
	M_FlashLight->SetVisibility(true);
	M_ShockwavePlane->SetVisibility(IsValid(M_ShockwaveMaterial));
	M_ScarDecal->SetVisibility(IsValid(M_ScarMaterial));

	if (UNiagaraSystem* DetonationEffect = M_Visuals.DetonationEffect.Get())
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, DetonationEffect, M_GroundZero, FRotator::ZeroRotator,
		                                               FVector(M_Visuals.DetonationEffectScale));
	}
	if (UNiagaraSystem* RadiationEffect = M_Visuals.RadiationEffect.Get())
	{
		M_RadiationEffect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, RadiationEffect, M_GroundZero);
	}
	if (USoundBase* DetonationSound = M_Visuals.DetonationSound.Get())
	{
		UGameplayStatics::PlaySound2D(this, DetonationSound);
	}
	RequestBlastCameraShake();
	DealBlastDamage();
	TickBlastVisuals();
}

void ARadixiteNuke::DealBlastDamage() const
{
	AActor* DamageCauser = GetDamageCauser();
	FRTS_AOE::DealDamageInRadiusAsync(
		DamageCauser,
		M_GroundZero,
		M_CoreRadius,
		M_Settings.CoreDamage,
		M_Settings.BlastFalloffExponent,
		ERTSDamageType::Kinetic,
		M_Settings.AffectedUnits,
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
		M_Settings.AffectedUnits,
		EShieldDamageSource::AreaOfEffect,
		TArray<TWeakObjectPtr<AActor>>(),
		EVehicleModuleSplashPolicy::DamageExternalModules,
		static_cast<EWeaponShellType>(0),
		EAoeFalloffDistance::ClosestCollision);
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

void ARadixiteNuke::TickBlastVisuals() const
{
	using namespace RadixiteNukeConstants;
	const float FlashAlpha = 1.f - FMath::Clamp(M_PhaseSeconds / FlashFadeSeconds, 0.f, 1.f);
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

	if (IsValid(M_ScarMaterial) && M_Settings.RadiationSeconds > 0.f)
	{
		const float RadiationAlpha = FMath::Clamp(M_PhaseSeconds / M_Settings.RadiationSeconds, 0.f, 1.f);
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
	if (M_PhaseSeconds > M_Settings.RadiationSeconds)
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
		M_Settings.AffectedUnits,
		EShieldDamageSource::AreaOfEffect);
}

void ARadixiteNuke::EndRadiation()
{
	M_Phase = ERadixiteNukePhase::Spent;
	SetActorTickEnabled(false);
	if (IsValid(M_RadiationEffect))
	{
		M_RadiationEffect->Deactivate();
	}
}

AActor* ARadixiteNuke::GetDamageCauser() const
{
	if (M_OwningPlayer == RadixiteNukeConstants::HumanPlayer)
	{
		return FRTS_Statics::GetRTSController(this);
	}
	return FRTS_Statics::GetEnemyController(this);
}
