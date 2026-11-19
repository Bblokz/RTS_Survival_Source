// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.
#include "BxpHolesQATestSubsystem.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Environment/DestructableEnvActor/WallChunkDamage/WallHoleDamageComponent.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSBxpHolesQA, Log, All);

namespace BxpHolesQATestPrivate
{
	constexpr float StartDelaySeconds = 4.f;
	constexpr float CameraTraceLength = 500000.f;
	constexpr float SettleSeconds = 1.5f;
	constexpr float AfterDamageSeconds = 1.0f;
	constexpr float AfterDeathSeconds = 3.0f;
	constexpr float TestHitCalibreMm = 75.f;
	constexpr float TestHitStandOff = 400.f;
	constexpr float TestHitReach = 800.f;
	// Health shares the expansion is damaged down to, one step per notify level a legacy graph listens to.
	constexpr float HealthSteps[] = {0.70f, 0.60f, 0.45f, 0.30f, 0.20f};
	const FString Bxp = TEXT("/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/");
	const FString Comm = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerT2/CommunicationsCenter/BXPs/");

	FString ClassPath(const FString& PackagePath)
	{
		FString Name;
		PackagePath.Split(TEXT("/"), nullptr, &Name, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		return PackagePath + TEXT(".") + Name + TEXT("_C");
	}

	TArray<FString> GetClassPaths()
	{
		const TArray<FString> Packages = {
			Bxp + TEXT("GerBxps/HQ/HQRadar/Ger_Bxp_HQRadar"),
			Bxp + TEXT("GerBxps/HQ/HQFlakDefense/Ger_Bxp_HQPlatform"),
			Bxp + TEXT("GerBxps/HQ/HarvesterHub/Ger_Bxp_HQHarvesterHub"),
			Bxp + TEXT("GerBxps/HQ/RepairHub/Ger_Bxp_HQRepairBay"),
			Bxp + TEXT("GerBxps/HQ/HQTower/Ger_Bxp_HQTower"),
			Bxp + TEXT("GerBxps/Refinery/RadixConvert/Ger_Ref_Converter"),
			Bxp + TEXT("GerBxps/Refinery/Kaskade/Ger_Bxp_RefineryKaskade"),
			Bxp + TEXT("GerBxps/Refinery/FlakStand/Ger_Bxp_RefineryFlakStand"),
			Bxp + TEXT("GerBxps/Barracks/RadixReactor/Ger_Bxp_BarracksReactorExpansion"),
			Bxp + TEXT("GerBxps/Barracks/FuelCell/Ger_Bxp_BarracksFuelExpansion"),
			Bxp + TEXT("GerBxps/Forge/RadixReactor/Ger_Bxp_LFactoryRadixiteReactor"),
			Bxp + TEXT("GerBxps/Forge/RadixFuel/Ger_Bxp_LFactoryFuelExpansion"),
			Bxp + TEXT("GerBxps/Forge/SchreckFrame/Ger_Bxp_LFactorySchreckFrame"),
			Bxp + TEXT("GerBxps/MetalVault/Smelter/Ger_Bxp_MetalVaultSmelter"),
			Comm + TEXT("ObservationPost/Ger_Bxp_CommObservationPost"),
			Comm + TEXT("MG42Nests/Ger_Bxp_CommMG42Nests"),
			Comm + TEXT("RequisitionOffice/Ger_Bxp_CommRequisitionOffice"),
			Comm + TEXT("CipherRoom/Ger_Bxp_CommCipherRoom"),
			Bxp + TEXT("Childs/37mmFlak/RTS_BP_BXP_37mmFlak"),
			Bxp + TEXT("Childs/88mmFlak/RTS_BP_BXP_Flak88"),
			Bxp + TEXT("Childs/LeFH105/RTS_BP_BXP_LeFH105mm"),
			Bxp + TEXT("Childs/Economic/BigSolarEXP/BXP_Solar_Big"),
			Bxp + TEXT("GerPits/RailPakBunker/Ger_BXP_RailPakBunker"),
			Bxp + TEXT("GerBxps/MechDepot/RadioVan/Ger_Bxp_MechDepotRadioVan"),
			Bxp + TEXT("GerBxps/MechDepot/WurfrahmenShop/Ger_Bxp_MechDepotWurfrahmenShop"),
			Bxp + TEXT("GerBxps/Barracks/SniperLoft/Ger_Bxp_BarracksSniperLoft"),
			// Map-placed capture bunkers and the expansions modelled but not offered yet.
			Bxp + TEXT("GerBxps/CaptureBunkers/Ger_Bxp_CaptureBunker37mm"),
			Bxp + TEXT("GerBxps/CaptureBunkers/Ger_Bxp_CaptureBunkerTurret"),
			Bxp + TEXT("GerBxps/HQ/HQLaser/Ger_Bxp_HQLaserPlatform"),
			// The flak and Flakvierling bunkers swap slot materials on their health levels: the material restore.
			Bxp + TEXT("GerBxps/Bunkers/FlakBunker/Ger_BXP_FlakBunker"),
			Bxp + TEXT("GerBxps/Bunkers/Flak88Bunker/Ger_BXP_Flak88Bunker"),
			Bxp + TEXT("GerBxps/Bunkers/FlakvierlingBunker/Ger_BXP_FlakvierlingBunker"),
			Bxp + TEXT("GerBxps/Bunkers/PZ38TTurretBunker/Ger_BXP_PZ38TTurretBunker"),
		};
		FString Only;
		FParse::Value(FCommandLine::Get(), TEXT("BxpQAOnly="), Only);
		TArray<FString> Out;
		for (const FString& Package : Packages)
		{
			if (Only.IsEmpty() || Package.Contains(Only))
			{
				Out.Add(ClassPath(Package));
			}
		}
		return Out;
	}

	/** The material shown on a slot, unwrapped from a dynamic instance so it compares with the asset. */
	UMaterialInterface* GetSlotSource(const UStaticMeshComponent& Mesh, const int32 Slot)
	{
		UMaterialInterface* Material = Mesh.GetMaterial(Slot);
		if (const UMaterialInstanceDynamic* Dynamic = Cast<UMaterialInstanceDynamic>(Material))
		{
			return Dynamic->Parent;
		}
		return Material;
	}
}

bool UBxpHolesQATestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), TEXT("BxpHolesQAValidate"));
}

TStatId UBxpHolesQATestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBxpHolesQATestSubsystem, STATGROUP_Tickables);
}

void UBxpHolesQATestSubsystem::Tick(const float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	switch (M_Stage)
	{
	case EBxpHolesQATestStage::WaitingForStart:
		TickWaitingForStart();
		return;
	case EBxpHolesQATestStage::SpawningNext:
		TickSpawningNext();
		return;
	case EBxpHolesQATestStage::WaitingForSettle:
		TickWaitingForSettle();
		return;
	case EBxpHolesQATestStage::WaitingAfterDamage:
		TickWaitingAfterDamage();
		return;
	case EBxpHolesQATestStage::WaitingAfterDeath:
		TickWaitingAfterDeath();
		return;
	case EBxpHolesQATestStage::Finished:
		return;
	}
}

void UBxpHolesQATestSubsystem::TickWaitingForStart()
{
	using namespace BxpHolesQATestPrivate;
	if (not StartGameAndGetIsReady())
	{
		return;
	}
	if (M_BootSeconds < 0.f)
	{
		M_BootSeconds = GetNow();
		return;
	}
	if (GetNow() - M_BootSeconds < StartDelaySeconds)
	{
		return;
	}
	if (not FindAnchor(M_Anchor))
	{
		Check(false, TEXT("anchor_found"));
		Finish();
		return;
	}
	M_ClassPaths = GetClassPaths();
	UE_LOG(LogRTSBxpHolesQA, Display, TEXT("RTS_BXPQA_TEST START classes=%d anchor=%s"), M_ClassPaths.Num(),
	       *M_Anchor.ToCompactString());
	EnterStage(EBxpHolesQATestStage::SpawningNext);
}

void UBxpHolesQATestSubsystem::TickSpawningNext()
{
	++M_ClassIndex;
	if (not M_ClassPaths.IsValidIndex(M_ClassIndex))
	{
		Finish();
		return;
	}
	const FString Name = GetCurrentName();
	UClass* BxpClass = LoadClass<ABuildingExpansion>(nullptr, *M_ClassPaths[M_ClassIndex]);
	Check(IsValid(BxpClass), Name + TEXT("_class_loads"));
	if (not IsValid(BxpClass))
	{
		return;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABuildingExpansion* Spawned = GetWorld()->SpawnActor<ABuildingExpansion>(BxpClass, M_Anchor, FRotator::ZeroRotator,
	                                                                         SpawnParameters);
	Check(IsValid(Spawned), Name + TEXT("_spawns"));
	if (not IsValid(Spawned))
	{
		return;
	}
	M_Bxp = Spawned;
	EnterStage(EBxpHolesQATestStage::WaitingForSettle);
}

void UBxpHolesQATestSubsystem::TickWaitingForSettle()
{
	using namespace BxpHolesQATestPrivate;
	if (GetStageElapsed() < SettleSeconds)
	{
		return;
	}
	ABuildingExpansion* BxpActor = M_Bxp.Get();
	if (not IsValid(BxpActor))
	{
		Check(false, GetCurrentName() + TEXT("_alive_after_spawn"));
		EnterStage(EBxpHolesQATestStage::SpawningNext);
		return;
	}
	CutTestHoles(*BxpActor);
	DamagePastNotifyLevels(*BxpActor);
	EnterStage(EBxpHolesQATestStage::WaitingAfterDamage);
}

void UBxpHolesQATestSubsystem::TickWaitingAfterDamage()
{
	using namespace BxpHolesQATestPrivate;
	if (GetStageElapsed() < AfterDamageSeconds)
	{
		return;
	}
	ABuildingExpansion* BxpActor = M_Bxp.Get();
	if (not IsValid(BxpActor))
	{
		Check(false, GetCurrentName() + TEXT("_alive_after_damage"));
		EnterStage(EBxpHolesQATestStage::SpawningNext);
		return;
	}
	CheckMeshAndMaterialsKept(*BxpActor, TEXT("after_damage"));
	UGameplayStatics::ApplyDamage(BxpActor, 1000000.f, nullptr, nullptr, UDamageType::StaticClass());
	EnterStage(EBxpHolesQATestStage::WaitingAfterDeath);
}

void UBxpHolesQATestSubsystem::TickWaitingAfterDeath()
{
	using namespace BxpHolesQATestPrivate;
	if (GetStageElapsed() < AfterDeathSeconds)
	{
		return;
	}
	ABuildingExpansion* BxpActor = M_Bxp.Get();
	const bool bDead = not IsValid(BxpActor) || not BxpActor->IsUnitAlive();
	Check(bDead, GetCurrentName() + TEXT("_dies"));
	if (IsValid(BxpActor))
	{
		BxpActor->Destroy();
	}
	M_Bxp.Reset();
	EnterStage(EBxpHolesQATestStage::SpawningNext);
}

void UBxpHolesQATestSubsystem::CutTestHoles(ABuildingExpansion& BxpActor)
{
	using namespace BxpHolesQATestPrivate;
	const FString Name = GetCurrentName();
	UWallHoleDamageComponent* Holes = BxpActor.FindComponentByClass<UWallHoleDamageComponent>();
	UStaticMeshComponent* WallMesh = IsValid(Holes) ? Holes->GetWallMeshComponent() : nullptr;
	Check(IsValid(WallMesh), Name + TEXT("_has_wall_hole_component"));
	if (not IsValid(WallMesh))
	{
		return;
	}
	M_MeshAfterSpawn = WallMesh->GetStaticMesh();
	TArray<FVector> LocalAimPoints;
	BxpActor.GetAimOffsetPoints(LocalAimPoints);
	const FVector Centre = WallMesh->Bounds.Origin;
	int32 Hits = 0;
	float RemovedArea = 0.f;
	for (const FVector& LocalAimPoint : LocalAimPoints)
	{
		const FVector AimPoint = BxpActor.GetActorTransform().TransformPosition(LocalAimPoint);
		FVector Outward = (AimPoint - Centre).GetSafeNormal2D();
		if (Outward.IsNearlyZero())
		{
			Outward = FVector::ForwardVector;
		}
		const FVector ShotStart = AimPoint + Outward * TestHitStandOff;
		const FVector ShotEnd = ShotStart - Outward * TestHitReach;
		FHitResult Hit;
		if (not WallMesh->LineTraceComponent(Hit, ShotStart, ShotEnd, FCollisionQueryParams(NAME_None, true)))
		{
			UE_LOG(LogRTSBxpHolesQA, Display, TEXT("RTS_BXPQA_TEST HOLE_SHOT %s aim=%s missed"), *Name,
			       *LocalAimPoint.ToCompactString());
			continue;
		}
		++Hits;
		RemovedArea += Holes->Debug_FireTestHit(Hit.Location, -Outward, TestHitCalibreMm);
	}
	FString SlotMaterials;
	M_MaterialsAfterHoles.Reset();
	for (int32 Slot = 0; Slot < WallMesh->GetNumMaterials(); ++Slot)
	{
		UMaterialInterface* Source = GetSlotSource(*WallMesh, Slot);
		M_MaterialsAfterHoles.Add(Source);
		SlotMaterials += GetNameSafe(Source) + TEXT(" ");
	}
	CheckDebrisMaterials(*Holes);
	Check(Hits > 0 && Holes->GetHoleCount() > 0, Name + TEXT("_shell_holes_cut"),
	      FString::Printf(TEXT("mesh=%s aim_points=%d hits=%d holes=%d removed_m2=%.3f materials=%s"),
	                      *GetNameSafe(WallMesh->GetStaticMesh()), LocalAimPoints.Num(), Hits, Holes->GetHoleCount(),
	                      RemovedArea, *SlotMaterials));
}

void UBxpHolesQATestSubsystem::CheckDebrisMaterials(const UWallHoleDamageComponent& Holes)
{
	// Debris flies as Niagara mesh particles: a base material without that usage renders as the default material
	// in a cooked build (the editor would fix the flag silently).
	int32 PieceCount = 0;
	FString Missing;
	for (const TObjectPtr<UStaticMesh>& Piece : Holes.GetDebrisMeshes())
	{
		if (not IsValid(Piece))
		{
			continue;
		}
		++PieceCount;
		for (const FStaticMaterial& Slot : Piece->GetStaticMaterials())
		{
			const UMaterial* const Base = IsValid(Slot.MaterialInterface) ? Slot.MaterialInterface->GetMaterial() : nullptr;
			if (not IsValid(Base) || not Base->bUsedWithNiagaraMeshParticles)
			{
				Missing += GetNameSafe(Base) + TEXT(" ");
			}
		}
	}
	Check(PieceCount > 0 && Missing.IsEmpty(), GetCurrentName() + TEXT("_debris_niagara_ready"),
	      FString::Printf(TEXT("pieces=%d missing_usage=%s"), PieceCount, *Missing));
}

void UBxpHolesQATestSubsystem::DamagePastNotifyLevels(ABuildingExpansion& BxpActor)
{
	using namespace BxpHolesQATestPrivate;
	const UHealthComponent* Health = BxpActor.FindComponentByClass<UHealthComponent>();
	if (not IsValid(Health))
	{
		Check(false, GetCurrentName() + TEXT("_has_health"));
		return;
	}
	for (const float Step : HealthSteps)
	{
		const float Target = Health->GetMaxHealth() * Step;
		const float Damage = Health->GetCurrentHealth() - Target;
		if (Damage <= 0.f)
		{
			continue;
		}
		// Divided by the reduction so the health really reaches the step on armoured expansions.
		const float Reduction = FMath::Max(Health->GetDamageReductionMultiplier(), 0.01f);
		UGameplayStatics::ApplyDamage(&BxpActor, Damage / Reduction, nullptr, nullptr, UDamageType::StaticClass());
		CheckMeshAndMaterialsKept(BxpActor, FString::Printf(TEXT("at_%d_percent"), FMath::RoundToInt(Step * 100.f)));
	}
	UE_LOG(LogRTSBxpHolesQA, Display, TEXT("RTS_BXPQA_TEST %s health after steps %.0f / %.0f"), *GetCurrentName(),
	       Health->GetCurrentHealth(), Health->GetMaxHealth());
}

void UBxpHolesQATestSubsystem::CheckMeshAndMaterialsKept(ABuildingExpansion& BxpActor, const FString& Moment)
{
	using namespace BxpHolesQATestPrivate;
	const UWallHoleDamageComponent* Holes = BxpActor.FindComponentByClass<UWallHoleDamageComponent>();
	const UStaticMeshComponent* WallMesh = IsValid(Holes) ? Holes->GetWallMeshComponent() : nullptr;
	if (not IsValid(WallMesh))
	{
		return;
	}
	const bool bMeshKept = WallMesh->GetStaticMesh() == M_MeshAfterSpawn.Get();
	bool bMaterialsKept = M_MaterialsAfterHoles.Num() == WallMesh->GetNumMaterials();
	for (int32 Slot = 0; bMaterialsKept && Slot < M_MaterialsAfterHoles.Num(); ++Slot)
	{
		bMaterialsKept = GetSlotSource(*WallMesh, Slot) == M_MaterialsAfterHoles[Slot].Get();
	}
	Check(bMeshKept && bMaterialsKept, GetCurrentName() + TEXT("_mesh_and_holes_kept_") + Moment,
	      FString::Printf(TEXT("mesh=%s expected=%s materials_kept=%d holes=%d"),
	                      *GetNameSafe(WallMesh->GetStaticMesh()), *GetNameSafe(M_MeshAfterSpawn.Get()),
	                      bMaterialsKept ? 1 : 0, Holes->GetHoleCount()));
}

FString UBxpHolesQATestSubsystem::GetCurrentName() const
{
	if (not M_ClassPaths.IsValidIndex(M_ClassIndex))
	{
		return TEXT("none");
	}
	FString Name;
	M_ClassPaths[M_ClassIndex].Split(TEXT("."), nullptr, &Name, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	Name.RemoveFromEnd(TEXT("_C"));
	return Name;
}

void UBxpHolesQATestSubsystem::Check(const bool bCondition, const FString& Description, const FString& Details)
{
	if (bCondition)
	{
		++M_PassedCheckCount;
		UE_LOG(LogRTSBxpHolesQA, Display, TEXT("RTS_BXPQA_TEST CHECK PASS %s %s"), *Description, *Details);
		return;
	}
	++M_FailedCheckCount;
	UE_LOG(LogRTSBxpHolesQA, Error, TEXT("RTS_BXPQA_TEST CHECK FAIL %s %s"), *Description, *Details);
}

void UBxpHolesQATestSubsystem::EnterStage(const EBxpHolesQATestStage NewStage)
{
	M_Stage = NewStage;
	M_StageSeconds = GetNow();
}

float UBxpHolesQATestSubsystem::GetNow() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetRealTimeSeconds() : 0.f;
}

float UBxpHolesQATestSubsystem::GetStageElapsed() const
{
	return GetNow() - M_StageSeconds;
}

bool UBxpHolesQATestSubsystem::StartGameAndGetIsReady() const
{
	ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	if (not IsValid(Controller))
	{
		return false;
	}
	UPlayerStartGameControl* StartGameControl = Controller->FindComponentByClass<UPlayerStartGameControl>();
	if (IsValid(StartGameControl) && StartGameControl->GetIsWaitingForPlayerToStart())
	{
		StartGameControl->PlayerStartedGame();
	}
	if (Controller->GetIsGameOnPause())
	{
		Controller->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
	return true;
}

bool UBxpHolesQATestSubsystem::FindAnchor(FVector& OutAnchor) const
{
	using namespace BxpHolesQATestPrivate;
	float AnchorX = 0.f;
	float AnchorY = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("BxpQAAnchorX="), AnchorX)
		&& FParse::Value(FCommandLine::Get(), TEXT("BxpQAAnchorY="), AnchorY))
	{
		return GlobalAbilityHelpers::TryFindGroundLocation(this, FVector(AnchorX, AnchorY, 0.f), OutAnchor);
	}
	const ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	if (not IsValid(Controller) || not IsValid(Controller->PlayerCameraManager))
	{
		return false;
	}
	const FVector CameraLocation = Controller->PlayerCameraManager->GetCameraLocation();
	const FVector CameraForward = Controller->PlayerCameraManager->GetCameraRotation().Vector();
	FHitResult GroundHit;
	if (not GetWorld()->LineTraceSingleByChannel(GroundHit, CameraLocation,
	                                             CameraLocation + CameraForward * CameraTraceLength, ECC_Visibility))
	{
		return false;
	}
	return GlobalAbilityHelpers::TryFindGroundLocation(this, GroundHit.Location, OutAnchor);
}

void UBxpHolesQATestSubsystem::Finish()
{
	const bool bPassed = M_FailedCheckCount == 0 && M_PassedCheckCount > 0;
	UE_LOG(LogRTSBxpHolesQA, Display, TEXT("RTS_BXPQA_TEST RESULT %s passed=%d failed=%d"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"), M_PassedCheckCount, M_FailedCheckCount);
	EnterStage(EBxpHolesQATestStage::Finished);
	if (APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld()))
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}

#else

bool UBxpHolesQATestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UBxpHolesQATestSubsystem::Tick(const float DeltaTime)
{
}

TStatId UBxpHolesQATestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBxpHolesQATestSubsystem, STATGROUP_Tickables);
}

#endif
