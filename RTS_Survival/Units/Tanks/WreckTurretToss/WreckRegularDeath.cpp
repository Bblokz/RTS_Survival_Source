#include "WreckRegularDeath.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "RTS_Survival/Game/GameState/GameExplosionManager/ExplosionManager.h"
#include "RTS_Survival/Units/Tanks/WreckTurretToss/WreckDeathVariantSubsystem.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

DEFINE_LOG_CATEGORY_STATIC(LogWreckRegularDeath, Log, All);

namespace WreckRegularDeathConstants
{
	const TCHAR* const CharOverlayMaterialPath =
		TEXT("/Game/RTS_Survival/SpecialEffects/AllExplosions/Mesh/EngineDeck/M_WreckChar_Overlay.M_WreckChar_Overlay");
	const TCHAR* const DebrisSystemPath =
		TEXT("/Game/RTS_Survival/SpecialEffects/AllExplosions/WreckDebris/NS_WreckDebrisBurst.NS_WreckDebrisBurst");
	const TCHAR* const ChunkMaterialPath =
		TEXT("/Game/RTS_Survival/SpecialEffects/AllExplosions/WreckDebris/M_WreckDebrisChunk.M_WreckDebrisChunk");
	const TCHAR* const GermanWheelSystemPath =
		TEXT("/Game/RTS_Survival/SpecialEffects/AllExplosions/WreckDebris/NS_WreckWheelBurst_Ger.NS_WreckWheelBurst_Ger");
	const TCHAR* const SovietWheelSystemPath =
		TEXT("/Game/RTS_Survival/SpecialEffects/AllExplosions/WreckDebris/NS_WreckWheelBurst_Rus.NS_WreckWheelBurst_Rus");

	// Overlay material parameters (M_WreckChar_Overlay), shared with the engine fire death.
	const FName CharCentreParam = TEXT("CharCentre");
	const FName CharRadiusParam = TEXT("CharRadius");
	const FName EmberStartTimeParam = TEXT("EmberStartTime");
	const FName EmberCoolDownParam = TEXT("EmberCoolDown");

	// NS_WreckDebrisBurst user parameters.
	const FName DebrisPieceCountParam = TEXT("PieceCount");
	const FName DebrisThrowDirectionParam = TEXT("ThrowDirection");
	const FName DebrisConeAngleParam = TEXT("ConeAngle");
	const FName DebrisHullRadiusParam = TEXT("HullRadius");
	const FName DebrisMaterialParam = TEXT("DebrisMaterial");
	const FName DebrisScaleParam = TEXT("DebrisScale");

	// Hull sockets the wreck Blueprints already blow up; the chain reaction reuses them.
	const FName ExplosionSocketNames[] = {TEXT("Exp"), TEXT("Exp1"), TEXT("Exp2")};
	// Turret explosions sit a little above the turret pivot, like the Blueprint's own turret blast.
	constexpr float TurretExplosionHeight = 50.f;
	// Random hull-top points and the char centre stay this share of the half extent from the hull's middle.
	constexpr float HullTopSpreadShare = 0.6f;
	// The char centre sits a little below the hull top so the sides char too.
	constexpr float CharCentreBelowTop = 40.f;
	// The debris bursts from just above the hull's middle.
	constexpr float DebrisOriginAboveCentre = 0.5f;
	// Burst budget slots a Debris burst holds; HeavyDebris takes its count from the settings.
	constexpr int32 DebrisBurstSlots = 1;
	// The wheels leave from the hull's side this share of its half width out, at this share of its height from the
	// bottom (the road wheels), spread along this share of its half length.
	constexpr float WheelOriginSideShare = 0.9f;
	constexpr float WheelOriginHeightShare = 0.2f;
	constexpr float WheelSpreadLengthShare = 0.5f;
}

FWreckRegularDeathSettings::FWreckRegularDeathSettings()
{
	Pose.CharOverlayMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(WreckRegularDeathConstants::CharOverlayMaterialPath));
	Debris.DebrisSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(WreckRegularDeathConstants::DebrisSystemPath));
	Debris.ChunkMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(WreckRegularDeathConstants::ChunkMaterialPath));
	WheelThrow.GermanWheelSystem = TSoftObjectPtr<UNiagaraSystem>(
		FSoftObjectPath(WreckRegularDeathConstants::GermanWheelSystemPath));
	WheelThrow.SovietWheelSystem = TSoftObjectPtr<UNiagaraSystem>(
		FSoftObjectPath(WreckRegularDeathConstants::SovietWheelSystemPath));
}

void FWreckRegularDeath::StartOnWreck(AActor* Wreck, const FWreckRegularDeathRequest& Request)
{
	if (not IsValid(Wreck))
	{
		return;
	}
	UStaticMeshComponent* Hull = FindNamedMesh(*Wreck, Request.Settings.HullComponentName);
	// Other spawned debris has no hull to vary.
	if (Hull == nullptr)
	{
		return;
	}
	UStaticMeshComponent* Turret = FindNamedMesh(*Wreck, Request.Settings.TurretComponentName);

	VaryPose(*Wreck, Turret, Request.Settings.Pose);
	ApplyChar(*Wreck, *Hull, Request.Settings.Pose);
	ERegularDeathVariant PlayedVariant = Request.Variant;
	if (PlayedVariant == ERegularDeathVariant::WheelThrow && not StartWheelThrow(*Wreck, *Hull, Request))
	{
		// Wheeled vehicles, unseen deaths and a full burst budget throw debris instead, if anything.
		PlayedVariant = ERegularDeathVariant::Debris;
	}
	if (PlayedVariant == ERegularDeathVariant::Debris || PlayedVariant == ERegularDeathVariant::HeavyDebris)
	{
		PlayedVariant = StartDebrisWithFallback(*Wreck, *Hull, Request);
	}
	if (PlayedVariant == ERegularDeathVariant::ChainReaction)
	{
		StartChainReaction(*Wreck, *Hull, Turret, Request.Settings.ChainReaction);
	}
	UE_LOG(LogWreckRegularDeath, Log,
	       TEXT("RTS_WRD wreck=%s subtype=%s drawn=%s played=%s kill_shot=%s seen=%d track_wheels=%d"),
	       *Wreck->GetName(), *UEnum::GetValueAsString(Request.TankSubtype), *UEnum::GetValueAsString(Request.Variant),
	       *UEnum::GetValueAsString(PlayedVariant), *Request.KillShotDirection.ToCompactString(),
	       Request.bIsSeen ? 1 : 0, Request.bHasTrackWheels ? 1 : 0);
}

UStaticMeshComponent* FWreckRegularDeath::FindNamedMesh(const AActor& Wreck, const FName ComponentName)
{
	TInlineComponentArray<UStaticMeshComponent*> MeshComponents(&Wreck);
	for (UStaticMeshComponent* MeshComponent : MeshComponents)
	{
		if (IsValid(MeshComponent) && MeshComponent->GetFName() == ComponentName)
		{
			return MeshComponent;
		}
	}
	return nullptr;
}

void FWreckRegularDeath::VaryPose(AActor& Wreck, UStaticMeshComponent* Turret,
                                  const FWreckRegularDeathPoseSettings& Pose)
{
	if (not Pose.bVaryPose)
	{
		return;
	}
	const float HullYaw = FMath::FRandRange(-Pose.HullYawJitterDegrees, Pose.HullYawJitterDegrees);
	Wreck.AddActorWorldRotation(FRotator(0.f, HullYaw, 0.f));
	if (not IsValid(Turret))
	{
		return;
	}
	const float TurretYaw = FMath::FRandRange(Pose.TurretYawDegrees.X, Pose.TurretYawDegrees.Y)
		* (FMath::RandBool() ? 1.f : -1.f);
	const float TurretPitch = FMath::FRandRange(-Pose.TurretTiltDegrees, Pose.TurretTiltDegrees);
	const float TurretRoll = FMath::FRandRange(-Pose.TurretTiltDegrees, Pose.TurretTiltDegrees);
	Turret->AddLocalRotation(FRotator(TurretPitch, TurretYaw, TurretRoll));
	Turret->AddWorldOffset(FVector(FMath::RandPointInCircle(Pose.TurretShiftCm), 0.f));
}

void FWreckRegularDeath::ApplyChar(AActor& Wreck, const UStaticMeshComponent& Hull,
                                   const FWreckRegularDeathPoseSettings& Pose)
{
	using namespace WreckRegularDeathConstants;
	const UWorld* World = Wreck.GetWorld();
	if (not Pose.bVaryChar || Pose.CharOverlayMaterial.IsNull() || not IsValid(World)
		|| FMath::FRand() < Pose.NoCharChance)
	{
		return;
	}
	// One small material, loaded once and then cached by the engine.
	UMaterialInterface* CharMaterial = Pose.CharOverlayMaterial.LoadSynchronous();
	if (not IsValid(CharMaterial))
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("FWreckRegularDeath::ApplyChar - could not load %s."), *Pose.CharOverlayMaterial.ToString()));
		return;
	}

	const FVector CharCentre = GetRandomHullTopPoint(Hull) - FVector(0.f, 0.f, CharCentreBelowTop);
	const float CharRadius = Hull.Bounds.SphereRadius
		* FMath::FRandRange(Pose.CharRadiusHullScale.X, Pose.CharRadiusHullScale.Y);
	UMaterialInstanceDynamic* CharInstance = UMaterialInstanceDynamic::Create(CharMaterial, &Wreck);
	CharInstance->SetVectorParameterValue(CharCentreParam, FLinearColor(CharCentre.X, CharCentre.Y, CharCentre.Z));
	CharInstance->SetScalarParameterValue(CharRadiusParam, CharRadius);
	// The material cools the embers with its own Time node, so nothing ticks here.
	CharInstance->SetScalarParameterValue(EmberStartTimeParam, World->GetTimeSeconds());
	CharInstance->SetScalarParameterValue(EmberCoolDownParam, Pose.EmberCoolDownSeconds);

	TInlineComponentArray<UStaticMeshComponent*> MeshComponents(&Wreck);
	for (UStaticMeshComponent* MeshComponent : MeshComponents)
	{
		if (IsValid(MeshComponent))
		{
			MeshComponent->SetOverlayMaterial(CharInstance);
		}
	}
}

void FWreckRegularDeath::StartChainReaction(const AActor& Wreck, const UStaticMeshComponent& Hull,
                                            const UStaticMeshComponent* Turret,
                                            const FWreckChainReactionSettings& Chain)
{
	UGameExplosionsManager* ExplosionsManager = FRTS_Statics::GetGameExplosionsManager(&Wreck);
	if (not IsValid(ExplosionsManager))
	{
		return;
	}
	const TArray<FVector> Anchors = GetChainReactionAnchors(Hull, Turret);
	const int32 SecondaryCount = FMath::RandRange(Chain.SecondaryCount.X, Chain.SecondaryCount.Y);
	float Delay = Chain.FirstDelaySeconds;
	for (int32 SecondaryIndex = 0; SecondaryIndex < SecondaryCount && Delay <= Chain.MaxDurationSeconds; ++SecondaryIndex)
	{
		const bool bUseHullTop = Anchors.IsEmpty() || FMath::FRand() < Chain.RandomHullTopChance;
		const FVector Location = bUseHullTop ? GetRandomHullTopPoint(Hull) : Anchors[FMath::RandRange(0, Anchors.Num() - 1)];
		ExplosionsManager->SpawnRTSExplosionAtLocation(Chain.SecondaryExplosionType, Location, true, Delay);
		Delay += FMath::FRandRange(Chain.GapSeconds.X, Chain.GapSeconds.Y);
	}
	if (FMath::FRand() < Chain.LateExplosionChance)
	{
		const FVector LateLocation = Anchors.IsEmpty()
			                             ? GetRandomHullTopPoint(Hull)
			                             : Anchors[FMath::RandRange(0, Anchors.Num() - 1)];
		ExplosionsManager->SpawnRTSExplosionAtLocation(
			Chain.LateExplosionType, LateLocation, true,
			FMath::FRandRange(Chain.LateExplosionDelaySeconds.X, Chain.LateExplosionDelaySeconds.Y));
	}
}

ERegularDeathVariant FWreckRegularDeath::StartDebrisWithFallback(const AActor& Wreck, UStaticMeshComponent& Hull,
                                                                 const FWreckRegularDeathRequest& Request)
{
	if (Request.Variant == ERegularDeathVariant::HeavyDebris && StartDebris(Wreck, Hull, Request, true))
	{
		return ERegularDeathVariant::HeavyDebris;
	}
	// A heavy burst that does not fit the budget may still fit as a regular one.
	if (StartDebris(Wreck, Hull, Request, false))
	{
		return ERegularDeathVariant::Debris;
	}
	return ERegularDeathVariant::ChainReaction;
}

bool FWreckRegularDeath::StartDebris(const AActor& Wreck, UStaticMeshComponent& Hull,
                                     const FWreckRegularDeathRequest& Request, const bool bIsHeavy)
{
	using namespace WreckRegularDeathConstants;
	const FWreckNiagaraDebrisSettings& Debris = Request.Settings.Debris;
	const int32 BurstSlots = bIsHeavy ? Debris.HeavyBurstSlots : DebrisBurstSlots;
	UWreckDeathVariantSubsystem* VariantSubsystem = GetSubsystemForBurst(Wreck, Request.bIsSeen, Debris.bOnlyWhenSeen,
	                                                                     BurstSlots);
	if (not IsValid(VariantSubsystem) || Debris.DebrisSystem.IsNull())
	{
		return false;
	}
	UNiagaraSystem* DebrisSystem = Debris.DebrisSystem.LoadSynchronous();
	if (not IsValid(DebrisSystem))
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("FWreckRegularDeath::StartDebris - could not load %s."), *Debris.DebrisSystem.ToString()));
		return false;
	}

	const FBoxSphereBounds HullBounds = Hull.Bounds;
	const bool bHasKillShot = not FVector(Request.KillShotDirection.X, Request.KillShotDirection.Y, 0.f).IsNearlyZero();
	FWreckMeshBurstSpawn Burst;
	Burst.Origin = HullBounds.Origin + FVector(0.f, 0.f, HullBounds.BoxExtent.Z * DebrisOriginAboveCentre);
	Burst.PieceCount = GetDebrisPieceCount(Debris, Hull, bIsHeavy);
	Burst.ThrowDirection = GetDebrisThrowDirection(Debris, Request.KillShotDirection);
	Burst.ConeDegrees = bHasKillShot ? Debris.ShotConeDegrees : Debris.UpConeDegrees;
	Burst.SpreadRadius = HullBounds.SphereRadius;
	Burst.Material = VariantSubsystem->GetDebrisMaterialForHull(Hull.GetMaterial(0),
	                                                            Debris.ChunkMaterial.LoadSynchronous());
	Burst.BurstSlots = BurstSlots;
	Burst.LieSeconds = Debris.LieSeconds;
	Burst.SinkSeconds = Debris.SinkSeconds;
	return SpawnMeshBurst(Wreck, *DebrisSystem, Burst, *VariantSubsystem);
}

bool FWreckRegularDeath::StartWheelThrow(const AActor& Wreck, const UStaticMeshComponent& Hull,
                                         const FWreckRegularDeathRequest& Request)
{
	using namespace WreckRegularDeathConstants;
	const FWreckWheelThrowSettings& WheelThrow = Request.Settings.WheelThrow;
	const TSoftObjectPtr<UNiagaraSystem>& WheelSystemPath = Global_GetIsSovietTank(Request.TankSubtype)
		                                                        ? WheelThrow.SovietWheelSystem
		                                                        : WheelThrow.GermanWheelSystem;
	const UStaticMesh* HullMesh = Hull.GetStaticMesh();
	if (not Request.bHasTrackWheels || Request.TankSubtype == ETankSubtype::Tank_None || WheelSystemPath.IsNull()
		|| not IsValid(HullMesh))
	{
		return false;
	}
	UWreckDeathVariantSubsystem* VariantSubsystem = GetSubsystemForBurst(
		Wreck, Request.bIsSeen, Request.Settings.Debris.bOnlyWhenSeen, DebrisBurstSlots);
	if (not IsValid(VariantSubsystem))
	{
		return false;
	}
	UNiagaraSystem* WheelSystem = WheelSystemPath.LoadSynchronous();
	if (not IsValid(WheelSystem))
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("FWreckRegularDeath::StartWheelThrow - could not load %s."), *WheelSystemPath.ToString()));
		return false;
	}

	// The hull's own frame: the wreck may stand at any yaw, so world-aligned bounds would pick the wrong side.
	const FBox LocalHullBox = HullMesh->GetBoundingBox();
	const float Side = GetWheelThrowSide(Hull, Request.KillShotDirection);
	const FVector LocalOrigin(LocalHullBox.GetCenter().X,
	                          LocalHullBox.GetCenter().Y + Side * LocalHullBox.GetExtent().Y * WheelOriginSideShare,
	                          LocalHullBox.Min.Z + LocalHullBox.GetSize().Z * WheelOriginHeightShare);
	const FTransform& HullTransform = Hull.GetComponentTransform();
	const FVector SideDirection = (HullTransform.TransformVectorNoScale(FVector::RightVector) * Side).GetSafeNormal2D();
	FWreckMeshBurstSpawn Burst;
	Burst.Origin = HullTransform.TransformPosition(LocalOrigin);
	Burst.PieceCount = GetWheelCount(WheelThrow, Request.TankSubtype);
	Burst.ThrowDirection = FMath::Lerp(FVector::UpVector, SideDirection, WheelThrow.SideLean).GetSafeNormal();
	Burst.ConeDegrees = WheelThrow.ConeDegrees;
	Burst.SpreadRadius = LocalHullBox.GetExtent().X * HullTransform.GetScale3D().X * WheelSpreadLengthShare;
	Burst.BurstSlots = DebrisBurstSlots;
	Burst.LieSeconds = WheelThrow.LieSeconds;
	Burst.SinkSeconds = WheelThrow.SinkSeconds;
	return SpawnMeshBurst(Wreck, *WheelSystem, Burst, *VariantSubsystem);
}

int32 FWreckRegularDeath::GetWheelCount(const FWreckWheelThrowSettings& WheelThrow, const ETankSubtype TankSubtype)
{
	const FIntPoint WheelRange = Global_GetIsHeavyTank(TankSubtype)
		                             ? WheelThrow.HeavyTankWheels
		                             : Global_GetIsMediumTank(TankSubtype)
		                             ? WheelThrow.MediumTankWheels
		                             : Global_GetIsLightTank(TankSubtype)
		                             ? WheelThrow.LightTankWheels
		                             : WheelThrow.OtherTrackedWheels;
	return FMath::Max(1, FMath::RandRange(WheelRange.X, WheelRange.Y));
}

float FWreckRegularDeath::GetWheelThrowSide(const UStaticMeshComponent& Hull, const FVector& KillShotDirection)
{
	const FVector ShotHorizontal = FVector(KillShotDirection.X, KillShotDirection.Y, 0.f).GetSafeNormal();
	if (ShotHorizontal.IsNearlyZero())
	{
		return FMath::RandBool() ? 1.f : -1.f;
	}
	// A shot travelling towards the hull's right exits through its right side.
	return FVector::DotProduct(ShotHorizontal, Hull.GetRightVector()) >= 0.f ? 1.f : -1.f;
}

UWreckDeathVariantSubsystem* FWreckRegularDeath::GetSubsystemForBurst(const AActor& Wreck, const bool bIsSeen,
                                                                      const bool bOnlyWhenSeen,
                                                                      const int32 BurstSlots)
{
	UWorld* World = Wreck.GetWorld();
	UWreckDeathVariantSubsystem* VariantSubsystem = IsValid(World)
		                                                ? World->GetSubsystem<UWreckDeathVariantSubsystem>()
		                                                : nullptr;
	if ((bOnlyWhenSeen && not bIsSeen) || not IsValid(VariantSubsystem)
		|| not VariantSubsystem->GetCanStartDebrisBurst(BurstSlots))
	{
		return nullptr;
	}
	return VariantSubsystem;
}

bool FWreckRegularDeath::SpawnMeshBurst(const AActor& Wreck, UNiagaraSystem& System, const FWreckMeshBurstSpawn& Burst,
                                        UWreckDeathVariantSubsystem& VariantSubsystem)
{
	using namespace WreckRegularDeathConstants;
	UNiagaraComponent* BurstComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		Wreck.GetWorld(), &System, Burst.Origin, FRotator::ZeroRotator, FVector::OneVector, true, false);
	if (not IsValid(BurstComponent))
	{
		return false;
	}
	BurstComponent->SetVariableInt(DebrisPieceCountParam, Burst.PieceCount);
	BurstComponent->SetVariableVec3(DebrisThrowDirectionParam, Burst.ThrowDirection);
	BurstComponent->SetVariableFloat(DebrisConeAngleParam, Burst.ConeDegrees);
	BurstComponent->SetVariableFloat(DebrisHullRadiusParam, Burst.SpreadRadius);
	if (IsValid(Burst.Material))
	{
		BurstComponent->SetVariableMaterial(DebrisMaterialParam, Burst.Material);
	}
	BurstComponent->SetVariableVec3(DebrisScaleParam, FVector::OneVector);
	BurstComponent->Activate(true);
	VariantSubsystem.RegisterDebrisBurst(BurstComponent, Burst.LieSeconds, Burst.SinkSeconds, Burst.BurstSlots);
	return true;
}

FVector FWreckRegularDeath::GetDebrisThrowDirection(const FWreckNiagaraDebrisSettings& Debris,
                                                    const FVector& KillShotDirection)
{
	const FVector ShotHorizontal = FVector(KillShotDirection.X, KillShotDirection.Y, 0.f).GetSafeNormal();
	// Only a killing projectile gives a direction; every other death throws straight up.
	if (ShotHorizontal.IsNearlyZero())
	{
		return FVector::UpVector;
	}
	// Away from the shooter: along the shot's travel, leaned up so the pieces still arc.
	return FMath::Lerp(FVector::UpVector, ShotHorizontal, Debris.KillDirectionLean).GetSafeNormal();
}

int32 FWreckRegularDeath::GetDebrisPieceCount(const FWreckNiagaraDebrisSettings& Debris,
                                              const UStaticMeshComponent& Hull, const bool bIsHeavy)
{
	const float PieceCount = FMath::GetMappedRangeValueClamped(
		Debris.HullRadiusRange,
		FVector2D(Debris.PieceCount.X, Debris.PieceCount.Y),
		Hull.Bounds.SphereRadius);
	const float VariantMultiplier = bIsHeavy ? Debris.HeavyPieceCountMultiplier : 1.f;
	return FMath::Max(1, FMath::RoundToInt(PieceCount * VariantMultiplier));
}

FVector FWreckRegularDeath::GetRandomHullTopPoint(const UStaticMeshComponent& Hull)
{
	using namespace WreckRegularDeathConstants;
	const FBoxSphereBounds HullBounds = Hull.Bounds;
	const FVector2D Spread(HullBounds.BoxExtent.X * HullTopSpreadShare, HullBounds.BoxExtent.Y * HullTopSpreadShare);
	return FVector(HullBounds.Origin.X + FMath::FRandRange(-Spread.X, Spread.X),
	               HullBounds.Origin.Y + FMath::FRandRange(-Spread.Y, Spread.Y),
	               HullBounds.Origin.Z + HullBounds.BoxExtent.Z);
}

TArray<FVector> FWreckRegularDeath::GetChainReactionAnchors(const UStaticMeshComponent& Hull,
                                                            const UStaticMeshComponent* Turret)
{
	using namespace WreckRegularDeathConstants;
	TArray<FVector> Anchors;
	for (const FName& SocketName : ExplosionSocketNames)
	{
		if (Hull.DoesSocketExist(SocketName))
		{
			Anchors.Add(Hull.GetSocketLocation(SocketName));
		}
	}
	if (IsValid(Turret))
	{
		Anchors.Add(Turret->GetComponentLocation() + FVector(0.f, 0.f, TurretExplosionHeight));
	}
	return Anchors;
}
