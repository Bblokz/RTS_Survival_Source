// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SonnengewehrBeam.h"

#include "Components/AudioComponent.h"
#include "Components/DecalComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/ArmorCalculation.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"
#include "UObject/ConstructorHelpers.h"

namespace SonnengewehrBeamConstants
{
	constexpr uint8 HumanPlayer = 1;
	// The engine's basic shapes are 100 cm across; the cylinder and sphere have their pivot in the middle.
	constexpr float BasicShapeSizeCm = 100.f;
	constexpr float WarningPlanePaddingCm = 300.f;
	constexpr float WarningPlaneHeightCm = 40.f;
	constexpr float ImpactGlowHeightCm = 30.f;
	constexpr float ImpactFlashHeightFraction = 0.55f;
	constexpr float LightHeightCm = 450.f;
	constexpr float ScorchDepthCm = 1500.f;
	constexpr float ScorchFadeOutSeconds = 4.f;
	// The burnt trail glows hottest right behind the beam and cools over this distance.
	constexpr float ScorchHotLengthCm = 700.f;

	// The beam reaches what is under it from below the ground up to well above the tallest vehicle.
	constexpr float DamageBoxBelowGroundCm = 600.f;
	constexpr float DamageBoxAboveGroundCm = 1800.f;

	// Last part of the warning in which a faint aiming ray flickers at the start of the strip.
	constexpr float AimingRaySeconds = 1.5f;
	constexpr float AimingRayStrength = 0.25f;
	constexpr float AimingRayFlickerHz = 9.f;
	// Part of the ignition in which the beam swells to full strength.
	constexpr float IgnitionSwellFraction = 0.4f;

	constexpr float CoreEdgePower = 0.6f;
	constexpr float GlowEdgePower = 1.6f;
	constexpr float HaloEdgePower = 3.f;
	constexpr float FlashEdgePower = 0.35f;
	constexpr float FlashIntensityFactor = 1.5f;
	// The beam layers fade out over this part of their diameter above the ground, so their wide cylinders do not
	// show a hard ellipse where they meet the ground; the impact glow covers the foot of the beam.
	constexpr float GroundFadePerDiameter = 0.6f;

	const FName ColorParameter(TEXT("Color"));
	const FName IntensityParameter(TEXT("Intensity"));
	const FName StrengthParameter(TEXT("Strength"));
	const FName EdgePowerParameter(TEXT("EdgePower"));
	const FName GroundZParameter(TEXT("GroundZ"));
	const FName GroundFadeParameter(TEXT("GroundFade"));

	const FName PlaneLengthParameter(TEXT("PlaneLength"));
	const FName PlaneWidthParameter(TEXT("PlaneWidth"));
	const FName StripLengthParameter(TEXT("StripLength"));
	const FName AreaRadiusParameter(TEXT("AreaRadius"));
	const FName IsDrawingParameter(TEXT("IsDrawing"));

	const FName StartXParameter(TEXT("StartX"));
	const FName StartYParameter(TEXT("StartY"));
	const FName DirXParameter(TEXT("DirX"));
	const FName DirYParameter(TEXT("DirY"));
	const FName HeadParameter(TEXT("Head"));
	const FName HalfWidthParameter(TEXT("HalfWidth"));
	const FName HotLengthParameter(TEXT("HotLength"));
	const FName GlowParameter(TEXT("Glow"));
}

ASonnengewehrBeam::ASonnengewehrBeam()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetCanBeDamaged(false);

	M_Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(M_Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	const auto CreateInertMesh = [this](const TCHAR* Name, USceneComponent* Parent, UStaticMesh* Mesh)
	{
		UStaticMeshComponent* Component = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Component->SetupAttachment(Parent);
		Component->SetStaticMesh(Mesh);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCanEverAffectNavigation(false);
		Component->SetReceivesDecals(false);
		Component->SetCastShadow(false);
		Component->SetVisibility(false);
		return Component;
	};

	M_WarningPlane = CreateInertMesh(TEXT("WarningPlane"), M_Root, PlaneMesh.Object);

	M_BeamRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BeamRoot"));
	M_BeamRoot->SetupAttachment(M_Root);
	M_BeamCore = CreateInertMesh(TEXT("BeamCore"), M_BeamRoot, CylinderMesh.Object);
	M_BeamGlow = CreateInertMesh(TEXT("BeamGlow"), M_BeamRoot, CylinderMesh.Object);
	M_BeamHalo = CreateInertMesh(TEXT("BeamHalo"), M_BeamRoot, CylinderMesh.Object);
	M_ImpactFlash = CreateInertMesh(TEXT("ImpactFlash"), M_BeamRoot, SphereMesh.Object);
	M_ImpactGlow = CreateInertMesh(TEXT("ImpactGlow"), M_BeamRoot, PlaneMesh.Object);

	M_BeamLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("BeamLight"));
	M_BeamLight->SetupAttachment(M_BeamRoot);
	M_BeamLight->SetCastShadows(false);
	M_BeamLight->SetIntensity(0.f);
	M_BeamLight->SetVisibility(false);

	M_BeamAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("BeamAudio"));
	M_BeamAudio->SetupAttachment(M_BeamRoot);
	M_BeamAudio->SetAutoActivate(false);

	M_ScorchDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("ScorchDecal"));
	M_ScorchDecal->SetupAttachment(M_Root);
	// Decals project along their X axis: this one looks straight down.
	M_ScorchDecal->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	M_ScorchDecal->SetVisibility(false);
}

void ASonnengewehrBeam::StartStrike(const FVector& StripStart, const FVector& StripEnd, const float BeamHalfWidth,
                                    const FSonnengewehrSettings& Settings, const FSonnengewehrVisuals& Visuals,
                                    const uint8 OwningPlayer)
{
	const FVector Line(StripEnd.X - StripStart.X, StripEnd.Y - StripStart.Y, 0.f);
	M_StripStart = StripStart;
	M_StripDirection = Line.IsNearlyZero() ? FVector::ForwardVector : Line.GetSafeNormal();
	M_StripLength = Line.Size();
	M_BeamHalfWidth = BeamHalfWidth;
	M_Settings = Settings;
	M_Visuals = Visuals;
	M_OwningPlayer = OwningPlayer;
	SetActorLocationAndRotation(StripStart, M_StripDirection.Rotation());

	SetupWarningArea();
	SetupBeamLayers();
	SetupImpact();
	SetupScorch();
	MoveBeamTo(0.f);
	if (USoundBase* WarningSound = Visuals.WarningSound.Get())
	{
		UGameplayStatics::PlaySound2D(this, WarningSound);
	}
	M_Phase = ESonnengewehrPhase::Warning;
	M_PhaseSeconds = 0.f;
	SetActorTickEnabled(true);
}

FVector ASonnengewehrBeam::GetBeamGroundLocation() const
{
	return M_BeamRoot->GetComponentLocation();
}

void ASonnengewehrBeam::SetupWarningArea()
{
	using namespace SonnengewehrBeamConstants;
	UMaterialInterface* WarningMaterial = M_Visuals.WarningAreaMaterial.Get();
	if (not IsValid(WarningMaterial))
	{
		return;
	}
	const float PlaneWidth = 2.f * (M_BeamHalfWidth + WarningPlanePaddingCm);
	const float PlaneLength = M_StripLength + PlaneWidth;
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(WarningMaterial, this);
	Material->SetScalarParameterValue(PlaneLengthParameter, PlaneLength);
	Material->SetScalarParameterValue(PlaneWidthParameter, PlaneWidth);
	Material->SetScalarParameterValue(StripLengthParameter, M_StripLength);
	Material->SetScalarParameterValue(AreaRadiusParameter, M_BeamHalfWidth);
	Material->SetScalarParameterValue(IsDrawingParameter, 1.f);
	M_WarningPlane->SetMaterial(0, Material);
	const FVector Middle = M_StripStart + M_StripDirection * (M_StripLength * 0.5f);
	M_WarningPlane->SetWorldLocationAndRotation(Middle + FVector(0.f, 0.f, WarningPlaneHeightCm),
	                                            M_StripDirection.Rotation());
	M_WarningPlane->SetWorldScale3D(FVector(PlaneLength / BasicShapeSizeCm, PlaneWidth / BasicShapeSizeCm, 1.f));
	M_WarningPlane->SetVisibility(true);
}

void ASonnengewehrBeam::SetupBeamLayers()
{
	using namespace SonnengewehrBeamConstants;
	const FSonnengewehrVisuals& Visuals = M_Visuals;
	const float Height = Visuals.BeamHeight;
	M_BeamLayerMaterials.Reset();
	M_BeamLayerMaterials.Add(SetupBeamLayer(M_BeamCore, Visuals.CoreColor, Visuals.CoreIntensity, CoreEdgePower,
	                                        Visuals.CoreDiameter, Height));
	M_BeamLayerMaterials.Add(SetupBeamLayer(M_BeamGlow, Visuals.GlowColor, Visuals.GlowIntensity, GlowEdgePower,
	                                        Visuals.GlowDiameter, Height));
	M_BeamLayerMaterials.Add(SetupBeamLayer(M_BeamHalo, Visuals.HaloColor, Visuals.HaloIntensity, HaloEdgePower,
	                                        Visuals.HaloDiameter, Height));
	// The flash is a squashed sphere: its height is the diameter scaled down, so it hugs the ground.
	M_BeamLayerMaterials.Add(SetupBeamLayer(M_ImpactFlash, Visuals.CoreColor,
	                                        Visuals.CoreIntensity * FlashIntensityFactor, FlashEdgePower,
	                                        Visuals.ImpactFlashDiameter,
	                                        Visuals.ImpactFlashDiameter * ImpactFlashHeightFraction));
	M_ImpactFlash->SetRelativeLocation(FVector::ZeroVector);
}

UMaterialInstanceDynamic* ASonnengewehrBeam::SetupBeamLayer(UStaticMeshComponent* Layer, const FLinearColor& Color,
                                                           const float Intensity, const float EdgePower,
                                                           const float Diameter, const float Height)
{
	using namespace SonnengewehrBeamConstants;
	UMaterialInterface* BeamMaterial = M_Visuals.BeamMaterial.Get();
	if (not IsValid(BeamMaterial) || not IsValid(Layer))
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BeamMaterial, this);
	Material->SetVectorParameterValue(ColorParameter, Color);
	Material->SetScalarParameterValue(IntensityParameter, Intensity);
	Material->SetScalarParameterValue(EdgePowerParameter, EdgePower);
	// The flash sphere sits on the ground on purpose; only the beam cylinders fade into it.
	const bool bIsBeamColumn = Layer != M_ImpactFlash;
	Material->SetScalarParameterValue(GroundFadeParameter, bIsBeamColumn ? Diameter * GroundFadePerDiameter : 0.f);
	Material->SetScalarParameterValue(StrengthParameter, 0.f);
	Layer->SetMaterial(0, Material);
	// The cylinder's pivot is in its middle: half of it stands above the ground.
	Layer->SetRelativeLocation(FVector(0.f, 0.f, Height * 0.5f));
	Layer->SetRelativeScale3D(FVector(Diameter / BasicShapeSizeCm, Diameter / BasicShapeSizeCm,
	                                  Height / BasicShapeSizeCm));
	return Material;
}

void ASonnengewehrBeam::SetupImpact()
{
	using namespace SonnengewehrBeamConstants;
	const float GlowScale = M_Visuals.ImpactGlowDiameter / BasicShapeSizeCm;
	M_ImpactGlow->SetRelativeLocation(FVector(0.f, 0.f, ImpactGlowHeightCm));
	M_ImpactGlow->SetRelativeScale3D(FVector(GlowScale, GlowScale, 1.f));
	if (UMaterialInterface* ImpactMaterial = M_Visuals.ImpactMaterial.Get())
	{
		M_ImpactMaterial = UMaterialInstanceDynamic::Create(ImpactMaterial, this);
		M_ImpactMaterial->SetVectorParameterValue(ColorParameter, M_Visuals.GlowColor);
		M_ImpactMaterial->SetScalarParameterValue(IntensityParameter, M_Visuals.ImpactIntensity);
		M_ImpactMaterial->SetScalarParameterValue(StrengthParameter, 0.f);
		M_ImpactGlow->SetMaterial(0, M_ImpactMaterial);
	}

	M_BeamLight->SetLightColor(M_Visuals.LightColor);
	M_BeamLight->SetAttenuationRadius(M_Visuals.LightRadius);
	M_BeamLight->SetRelativeLocation(FVector(0.f, 0.f, LightHeightCm));

	if (USoundBase* BeamSound = M_Visuals.BeamSound.Get())
	{
		M_BeamAudio->SetSound(BeamSound);
	}
}

void ASonnengewehrBeam::SetupScorch()
{
	using namespace SonnengewehrBeamConstants;
	UMaterialInterface* ScorchMaterial = M_Visuals.ScorchMaterial.Get();
	if (not IsValid(ScorchMaterial))
	{
		return;
	}
	M_ScorchMaterial = UMaterialInstanceDynamic::Create(ScorchMaterial, this);
	M_ScorchMaterial->SetScalarParameterValue(StartXParameter, M_StripStart.X);
	M_ScorchMaterial->SetScalarParameterValue(StartYParameter, M_StripStart.Y);
	M_ScorchMaterial->SetScalarParameterValue(DirXParameter, M_StripDirection.X);
	M_ScorchMaterial->SetScalarParameterValue(DirYParameter, M_StripDirection.Y);
	M_ScorchMaterial->SetScalarParameterValue(HalfWidthParameter, M_BeamHalfWidth);
	M_ScorchMaterial->SetScalarParameterValue(HotLengthParameter, ScorchHotLengthCm);
	M_ScorchMaterial->SetScalarParameterValue(HeadParameter, 0.f);
	M_ScorchMaterial->SetScalarParameterValue(GlowParameter, 1.f);
	M_ScorchDecal->SetDecalMaterial(M_ScorchMaterial);
	// A square box around the whole strip: the shader cuts the burnt path out of it in world space, so the decal's
	// own orientation does not matter.
	const float HalfSize = M_StripLength * 0.5f + 2.f * M_BeamHalfWidth;
	M_ScorchDecal->DecalSize = FVector(ScorchDepthCm, HalfSize, HalfSize);
	const FVector Middle = M_StripStart + M_StripDirection * (M_StripLength * 0.5f);
	M_ScorchDecal->SetWorldLocation(GetGroundAt(Middle));
}

void ASonnengewehrBeam::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	M_PhaseSeconds += DeltaSeconds;
	switch (M_Phase)
	{
	case ESonnengewehrPhase::Warning:
		TickWarning();
		return;
	case ESonnengewehrPhase::Igniting:
		TickIgniting(DeltaSeconds);
		return;
	case ESonnengewehrPhase::Sweeping:
		TickSweeping(DeltaSeconds);
		return;
	case ESonnengewehrPhase::Fading:
		TickFading();
		return;
	case ESonnengewehrPhase::Spent:
		TickSpent(DeltaSeconds);
		return;
	case ESonnengewehrPhase::Inactive:
		return;
	}
}

void ASonnengewehrBeam::TickWarning()
{
	using namespace SonnengewehrBeamConstants;
	if (M_PhaseSeconds >= M_Settings.WarningSeconds)
	{
		StartIgnition();
		return;
	}
	const float AimingRayStart = M_Settings.WarningSeconds - AimingRaySeconds;
	if (M_PhaseSeconds < AimingRayStart)
	{
		return;
	}
	// Only the thin core flickers on: the mirror is finding its aim.
	const float Flicker = 0.5f + 0.5f * FMath::Sin(M_PhaseSeconds * AimingRayFlickerHz * UE_TWO_PI);
	const float Ramp = (M_PhaseSeconds - AimingRayStart) / AimingRaySeconds;
	M_BeamCore->SetVisibility(true);
	if (M_BeamLayerMaterials.IsValidIndex(0) && IsValid(M_BeamLayerMaterials[0]))
	{
		M_BeamLayerMaterials[0]->SetScalarParameterValue(StrengthParameter, AimingRayStrength * Ramp * Flicker);
	}
}

void ASonnengewehrBeam::StartIgnition()
{
	M_Phase = ESonnengewehrPhase::Igniting;
	M_PhaseSeconds = 0.f;
	for (UStaticMeshComponent* Layer : {M_BeamCore.Get(), M_BeamGlow.Get(), M_BeamHalo.Get(), M_ImpactFlash.Get(),
	                                    M_ImpactGlow.Get()})
	{
		Layer->SetVisibility(true);
	}
	M_BeamLight->SetVisibility(true);
	M_ScorchDecal->SetVisibility(IsValid(M_ScorchMaterial));
	if (IsValid(M_BeamAudio->GetSound()))
	{
		M_BeamAudio->Play();
	}
	// The beam burns where it comes down straight away.
	TickDamage(0.f, true);
	TickIgniting(0.f);
}

void ASonnengewehrBeam::TickIgniting(const float DeltaSeconds)
{
	using namespace SonnengewehrBeamConstants;
	const float SwellSeconds = FMath::Max(M_Settings.IgnitionSeconds * IgnitionSwellFraction, KINDA_SMALL_NUMBER);
	const float Swell = FMath::Clamp(M_PhaseSeconds / SwellSeconds, 0.f, 1.f);
	SetBeamStrength(FMath::InterpEaseOut(0.f, 1.f, Swell, 2.f));
	TickDamage(DeltaSeconds, false);
	if (M_PhaseSeconds >= M_Settings.IgnitionSeconds)
	{
		StartSweep();
	}
}

void ASonnengewehrBeam::StartSweep()
{
	M_Phase = ESonnengewehrPhase::Sweeping;
	M_PhaseSeconds = 0.f;
	SetBeamStrength(1.f);
}

void ASonnengewehrBeam::TickSweeping(const float DeltaSeconds)
{
	const float Alpha = FMath::Clamp(M_PhaseSeconds / M_Settings.SweepSeconds, 0.f, 1.f);
	MoveBeamTo(M_StripLength * Alpha);
	if (Alpha >= 1.f)
	{
		TickDamage(DeltaSeconds, true);
		FinishSweep();
		return;
	}
	TickDamage(DeltaSeconds, false);
}

void ASonnengewehrBeam::FinishSweep()
{
	M_Phase = ESonnengewehrPhase::Fading;
	M_PhaseSeconds = 0.f;
	M_WarningPlane->SetVisibility(false);
	OnSweepFinished.Broadcast(this);
}

void ASonnengewehrBeam::TickFading()
{
	const float Alpha = FMath::Clamp(M_PhaseSeconds / M_Settings.FadeSeconds, 0.f, 1.f);
	SetBeamStrength(1.f - Alpha);
	if (Alpha < 1.f)
	{
		return;
	}
	M_Phase = ESonnengewehrPhase::Spent;
	M_PhaseSeconds = 0.f;
	for (UStaticMeshComponent* Layer : {M_BeamCore.Get(), M_BeamGlow.Get(), M_BeamHalo.Get(), M_ImpactFlash.Get(),
	                                    M_ImpactGlow.Get()})
	{
		Layer->SetVisibility(false);
	}
	M_BeamLight->SetVisibility(false);
	M_BeamAudio->FadeOut(M_Settings.FadeSeconds, 0.f);
	const float FadeOutStart = FMath::Max(M_Settings.ScorchLifeSeconds - SonnengewehrBeamConstants::ScorchFadeOutSeconds,
	                                      M_Settings.ScorchCoolSeconds);
	if (M_ScorchDecal->IsVisible())
	{
		M_ScorchDecal->SetFadeOut(FadeOutStart, SonnengewehrBeamConstants::ScorchFadeOutSeconds, true);
		return;
	}
	SetLifeSpan(FadeOutStart + SonnengewehrBeamConstants::ScorchFadeOutSeconds);
}

void ASonnengewehrBeam::TickSpent(const float DeltaSeconds)
{
	const float CoolAlpha = FMath::Clamp(M_PhaseSeconds / M_Settings.ScorchCoolSeconds, 0.f, 1.f);
	UpdateScorch(1.f - CoolAlpha);
	if (CoolAlpha >= 1.f)
	{
		// The decal fade-out removes the actor; nothing changes until then.
		SetActorTickEnabled(false);
	}
}

void ASonnengewehrBeam::MoveBeamTo(const float Distance)
{
	M_BeamDistance = FMath::Clamp(Distance, 0.f, M_StripLength);
	const FVector Ground = GetGroundAt(M_StripStart + M_StripDirection * M_BeamDistance);
	M_BeamRoot->SetWorldLocationAndRotation(Ground, FRotator::ZeroRotator);
	for (UMaterialInstanceDynamic* Material : M_BeamLayerMaterials)
	{
		if (IsValid(Material))
		{
			Material->SetScalarParameterValue(SonnengewehrBeamConstants::GroundZParameter, Ground.Z);
		}
	}
	UpdateScorch(1.f);
}

void ASonnengewehrBeam::SetBeamStrength(const float Strength) const
{
	using namespace SonnengewehrBeamConstants;
	for (UMaterialInstanceDynamic* Material : M_BeamLayerMaterials)
	{
		if (IsValid(Material))
		{
			Material->SetScalarParameterValue(StrengthParameter, Strength);
		}
	}
	if (IsValid(M_ImpactMaterial))
	{
		M_ImpactMaterial->SetScalarParameterValue(StrengthParameter, Strength);
	}
	M_BeamLight->SetIntensity(M_Visuals.LightIntensity * Strength);
}

void ASonnengewehrBeam::UpdateScorch(const float Glow) const
{
	using namespace SonnengewehrBeamConstants;
	if (not IsValid(M_ScorchMaterial))
	{
		return;
	}
	M_ScorchMaterial->SetScalarParameterValue(HeadParameter, M_BeamDistance);
	M_ScorchMaterial->SetScalarParameterValue(GlowParameter, Glow);
}

void ASonnengewehrBeam::TickDamage(const float DeltaSeconds, const bool bForce)
{
	M_SecondsSinceDamageTick += DeltaSeconds;
	if (not bForce && M_SecondsSinceDamageTick < M_Settings.DamageIntervalSeconds)
	{
		return;
	}
	M_SecondsSinceDamageTick = 0.f;
	BurnUpTo(M_BeamDistance);
}

void ASonnengewehrBeam::BurnUpTo(const float Distance)
{
	using namespace SonnengewehrBeamConstants;
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	// One box from where the last tick burnt to where the beam is now, half a beam width longer at both ends: a unit
	// is burnt as soon as the edge of the beam reaches it, and nothing between two ticks is skipped.
	const float FromDistance = FMath::Min(M_BurntUpToDistance, Distance);
	const float HalfLength = (Distance - FromDistance) * 0.5f + M_BeamHalfWidth;
	const FVector BoxGround = GetGroundAt(M_StripStart + M_StripDirection * ((FromDistance + Distance) * 0.5f));
	const float HalfHeight = (DamageBoxBelowGroundCm + DamageBoxAboveGroundCm) * 0.5f;
	const FVector BoxCentre = BoxGround + FVector(0.f, 0.f, DamageBoxAboveGroundCm - HalfHeight);
	M_BurntUpToDistance = Distance;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SonnengewehrBeam), false, this);
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, BoxCentre, M_StripDirection.ToOrientationQuat(),
	                                GetBeamObjectQueryParams(),
	                                FCollisionShape::MakeBox(FVector(HalfLength, M_BeamHalfWidth, HalfHeight)),
	                                QueryParams);
	const FVector BeamPoint = GetBeamGroundLocation();
	TSet<AActor*> ActorsThisTick;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		BurnOverlap(Overlap, BeamPoint, ActorsThisTick);
	}
}

void ASonnengewehrBeam::BurnOverlap(const FOverlapResult& Overlap, const FVector& BeamPoint,
                                    TSet<AActor*>& ActorsThisTick)
{
	AActor* HitActor = Overlap.GetActor();
	if (not IsValid(HitActor))
	{
		return;
	}
	// Turrets and hull weapons are child actors of the vehicle, some with their own armour component: the beam burns
	// the vehicle once, not every part of it.
	AActor* Victim = HitActor;
	while (IsValid(Victim->GetParentActor()))
	{
		Victim = Victim->GetParentActor();
	}
	if (ActorsThisTick.Contains(Victim) || M_BurntActors.Contains(Victim))
	{
		return;
	}
	ActorsThisTick.Add(Victim);
	if (not RTSFunctionLibrary::RTSIsValid(Victim) || GetIsSparedOwnUnit(*Victim))
	{
		return;
	}
	M_BurntActors.Add(Victim);
	DealBeamDamage(*Victim, Overlap.GetComponent(), BeamPoint);
}

void ASonnengewehrBeam::DealBeamDamage(AActor& Victim, const UPrimitiveComponent* HitComponent,
                                       const FVector& BeamPoint)
{
	const FVector BeamPointAtVictim(BeamPoint.X, BeamPoint.Y, Victim.GetActorLocation().Z);
	const float Damage = GetLaserDamageOnVictim(Victim, HitComponent, BeamPointAtVictim);
	const FRTSBlastDamageEvent DamageEvent = FRTSWeaponHelpers::MakeBlastDamageEvent(ERTSDamageType::Laser,
		BeamPointAtVictim);
	Victim.TakeDamage(Damage, DamageEvent, nullptr, GetDamageCauser());
	// A killed victim is only marked for destruction, so it is still safe to pass on this frame.
	OnActorBurnt.Broadcast(this, &Victim, Damage);
}

float ASonnengewehrBeam::GetLaserDamageOnVictim(const AActor& Victim, const UPrimitiveComponent* HitComponent,
                                                const FVector& BeamPoint) const
{
	UArmorCalculation* ArmorCalculation = Victim.FindComponentByClass<UArmorCalculation>();
	if (not IsValid(ArmorCalculation))
	{
		// Same as a laser weapon hitting something without an armour component.
		return M_Settings.BeamDamage;
	}
	const FArmorSetup ArmorSetup = ArmorCalculation->GetArmorSetup();
	const UMeshComponent* Meshes[] = {ArmorSetup.MeshWithArmor0, ArmorSetup.MeshWithArmor1, ArmorSetup.MeshWithArmor2};
	const FArmorSettings* Plates[] = {ArmorSetup.ArmorSettings0, ArmorSetup.ArmorSettings1, ArmorSetup.ArmorSettings2};
	const int32 PlateCounts[] = {ArmorSetup.NumArmorPlates0, ArmorSetup.NumArmorPlates1, ArmorSetup.NumArmorPlates2};

	// The overlapped mesh when it carries armour, otherwise the first mesh that does (the hull).
	int32 Slot = INDEX_NONE;
	for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(Meshes)); ++Index)
	{
		const bool bHasPlates = IsValid(Meshes[Index]) && PlateCounts[Index] > 0;
		if (bHasPlates && (Slot == INDEX_NONE || Meshes[Index] == HitComponent))
		{
			Slot = Index;
		}
	}
	if (Slot == INDEX_NONE)
	{
		return M_Settings.BeamDamage;
	}

	// The plate nearest to the beam is the side it burns into; aiming at that plate's centre makes the laser
	// resistance of that side apply, the same as a laser weapon hitting it.
	const FTransform MeshTransform = Meshes[Slot]->GetComponentTransform();
	FVector PlateCentre = MeshTransform.GetLocation();
	float BestDistanceSquared = TNumericLimits<float>::Max();
	const int32 PlateCount = FMath::Min(PlateCounts[Slot], MaxArmorPlatesPerMesh);
	for (int32 PlateIndex = 0; PlateIndex < PlateCount; ++PlateIndex)
	{
		const FArmorSettings& Plate = Plates[Slot][PlateIndex];
		const FVector Centre = (Plate.ArmorBoxTransform * MeshTransform).TransformPosition(Plate.ArmorBox.GetCenter());
		const float DistanceSquared = FVector::DistSquared(Centre, BeamPoint);
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			PlateCentre = Centre;
		}
	}
	UMeshComponent* ArmorMesh = ArmorSetup.MeshWithArmor0;
	ArmorMesh = Slot == 1 ? ArmorSetup.MeshWithArmor1.Get() : ArmorMesh;
	ArmorMesh = Slot == 2 ? ArmorSetup.MeshWithArmor2.Get() : ArmorMesh;
	return ArmorCalculation->GetEffectiveDamageOnHit(ERTSDamageType::Laser, M_Settings.BeamDamage, ArmorMesh,
	                                                 PlateCentre);
}

FCollisionObjectQueryParams ASonnengewehrBeam::GetBeamObjectQueryParams() const
{
	// Same object types as the game's area damage (FRTS_AOE).
	FCollisionObjectQueryParams ObjectQueryParams;
	const ETriggerOverlapLogic Affected = GetBurntSides();
	if (Affected == ETriggerOverlapLogic::OverlapEnemy || Affected == ETriggerOverlapLogic::OverlapBoth)
	{
		ObjectQueryParams.AddObjectTypesToQuery(COLLISION_OBJ_ENEMY);
	}
	if (Affected == ETriggerOverlapLogic::OverlapPlayer || Affected == ETriggerOverlapLogic::OverlapBoth)
	{
		ObjectQueryParams.AddObjectTypesToQuery(COLLISION_OBJ_PLAYER);
	}
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Destructible);
	return ObjectQueryParams;
}

ETriggerOverlapLogic ASonnengewehrBeam::GetBurntSides() const
{
	if (M_Settings.bBurnOwnUnits)
	{
		return ETriggerOverlapLogic::OverlapBoth;
	}
	// The enemy object channel holds the AI's units; the player channel holds the human player's.
	return M_OwningPlayer == SonnengewehrBeamConstants::HumanPlayer
		       ? ETriggerOverlapLogic::OverlapEnemy
		       : ETriggerOverlapLogic::OverlapPlayer;
}

bool ASonnengewehrBeam::GetIsSparedOwnUnit(const AActor& Victim) const
{
	if (M_Settings.bBurnOwnUnits)
	{
		return false;
	}
	// Second gate behind the channel filter: destructibles and odd child-actor channels still reach the overlap.
	const URTSComponent* RTSComponent = Victim.FindComponentByClass<URTSComponent>();
	return IsValid(RTSComponent) && RTSComponent->GetOwningPlayer() == M_OwningPlayer;
}

FVector ASonnengewehrBeam::GetGroundAt(const FVector& Location) const
{
	FVector Ground = Location;
	GlobalAbilityHelpers::TryFindGroundLocation(this, Location, Ground);
	return Ground;
}

AActor* ASonnengewehrBeam::GetDamageCauser() const
{
	if (M_OwningPlayer == SonnengewehrBeamConstants::HumanPlayer)
	{
		return FRTS_Statics::GetRTSController(this);
	}
	return FRTS_Statics::GetEnemyController(this);
}
