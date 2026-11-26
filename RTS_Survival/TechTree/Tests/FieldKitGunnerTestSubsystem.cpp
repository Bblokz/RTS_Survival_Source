// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "FieldKitGunnerTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS
#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/PlayerTechManager/PlayerTechManager.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/TechTree/Technologies/FieldKits/EmbeddedGunnerKit/TE_EmbeddedGunnerKit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/Turret/EmbeddedInfantry/EmbeddedInfantryTurret.h"
#include "RTS_Survival/Weapons/Turret/EmbeddedInfantry/EmbeddedTurretMountComponent.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSFieldKitTest, Log, All);

namespace FieldKitGunnerTestPrivate
{
	constexpr ETechnology KitTechnology = ETechnology::Tech_FieldKit_HatchGunnerMG42;
	constexpr float StartDelaySeconds = 2.f;
	// Long enough for every host's own mounts (the StandardPlus MG 34) to come out.
	constexpr float BeforeResearchSeconds = 2.f;
	constexpr float MountTimeoutSeconds = 8.f;
	constexpr float MountSettleSeconds = 1.5f;
	constexpr float AttackGroundTimeoutSeconds = 40.f;
	constexpr float GroundTraceHalfHeight = 100000.f;
	constexpr float UnitLift = 100.f;
	constexpr int32 HostsPerRow = 12;
	constexpr float HostSpacing = 850.f;
	constexpr float RowSpacing = 2600.f;
	// Fraction of the gunner's range the attack-ground target lies ahead of its tank.
	constexpr float GroundTargetRangeFraction = 0.5f;
	constexpr float MinimumGroundTargetDistance = 600.f;
	const FString KitGunnerClassName = TEXT("Ger_EIT_MG42_Scavenger_C");
	const FString ReplacedStandardPlusGunner = TEXT("Ger_EIT_MG34_Scavenger_C");
	const FVector2D DefaultAnchor = FVector2D(16796.f, -15808.f);

	void OverrideRTSComponentTemplateOwners(UClass* UnitClass, const uint8 OwningPlayer,
	                                        TArray<TPair<TWeakObjectPtr<UObject>, uint8>>& OutRestore)
	{
		const FByteProperty* OwnerProperty = FindFProperty<FByteProperty>(URTSComponent::StaticClass(), TEXT("OwningPlayer"));
		if (OwnerProperty == nullptr)
		{
			return;
		}
		TArray<UActorComponent*> Templates;
		for (UClass* Class = UnitClass; Class != nullptr; Class = Class->GetSuperClass())
		{
			UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(Class);
			if (BlueprintClass == nullptr)
			{
				continue;
			}
			if (BlueprintClass->SimpleConstructionScript != nullptr)
			{
				for (const USCS_Node* Node : BlueprintClass->SimpleConstructionScript->GetAllNodes())
				{
					Templates.Add(Node != nullptr ? Node->ComponentTemplate : nullptr);
				}
			}
			if (UInheritableComponentHandler* Handler = BlueprintClass->GetInheritableComponentHandler(false))
			{
				for (auto RecordIterator = Handler->CreateRecordIterator(); RecordIterator; ++RecordIterator)
				{
					Templates.Add(RecordIterator->ComponentTemplate);
				}
			}
		}
		for (UActorComponent* Template : Templates)
		{
			if (not IsValid(Template) || not Template->IsA<URTSComponent>())
			{
				continue;
			}
			uint8* Value = OwnerProperty->ContainerPtrToValuePtr<uint8>(Template);
			OutRestore.Add({Template, *Value});
			*Value = OwningPlayer;
		}
	}

	void RestoreRTSComponentTemplateOwners(const TArray<TPair<TWeakObjectPtr<UObject>, uint8>>& Restore)
	{
		const FByteProperty* OwnerProperty = FindFProperty<FByteProperty>(URTSComponent::StaticClass(), TEXT("OwningPlayer"));
		for (const TPair<TWeakObjectPtr<UObject>, uint8>& Entry : Restore)
		{
			if (OwnerProperty != nullptr && Entry.Key.IsValid())
			{
				*OwnerProperty->ContainerPtrToValuePtr<uint8>(Entry.Key.Get()) = Entry.Value;
			}
		}
	}

	FString GetMountMeshAssetName(const AEmbeddedInfantryTurret* Gunner)
	{
		const USceneComponent* MountComponent = IsValid(Gunner) ? Gunner->GetParentComponent() : nullptr;
		const UStaticMeshComponent* MountMesh = IsValid(MountComponent)
			? Cast<UStaticMeshComponent>(MountComponent->GetAttachParent())
			: nullptr;
		return IsValid(MountMesh) ? GetNameSafe(MountMesh->GetStaticMesh()) : GetNameSafe(
			IsValid(MountComponent) ? MountComponent->GetAttachParent() : nullptr);
	}
}

bool UFieldKitGunnerTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), TEXT("FieldKitGunnerValidate"));
}

TStatId UFieldKitGunnerTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFieldKitGunnerTestSubsystem, STATGROUP_Tickables);
}

void UFieldKitGunnerTestSubsystem::Tick(const float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	switch (M_Phase)
	{
	case EFieldKitGunnerTestPhase::WaitingForStart:
		TickWaitingForStart();
		return;
	case EFieldKitGunnerTestPhase::BeforeResearch:
		TickBeforeResearch();
		return;
	case EFieldKitGunnerTestPhase::WaitingForMounts:
		TickWaitingForMounts();
		return;
	case EFieldKitGunnerTestPhase::AttackingGround:
		TickAttackingGround();
		return;
	case EFieldKitGunnerTestPhase::Finished:
		return;
	}
}

void UFieldKitGunnerTestSubsystem::Check(const bool bCondition, const FString& Description, const FString& Details)
{
	if (bCondition)
	{
		++M_PassedCheckCount;
		UE_LOG(LogRTSFieldKitTest, Display, TEXT("RTS_FIELDKIT_TEST CHECK PASS %s %s"), *Description, *Details);
		return;
	}
	++M_FailedCheckCount;
	UE_LOG(LogRTSFieldKitTest, Error, TEXT("RTS_FIELDKIT_TEST CHECK FAIL %s %s"), *Description, *Details);
}

float UFieldKitGunnerTestSubsystem::GetNow() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.f;
}

void UFieldKitGunnerTestSubsystem::EnterPhase(const EFieldKitGunnerTestPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = GetNow();
}

void UFieldKitGunnerTestSubsystem::Finish()
{
	const bool bPassed = M_FailedCheckCount == 0 && M_PassedCheckCount > 0;
	UE_LOG(LogRTSFieldKitTest, Display, TEXT("RTS_FIELDKIT_TEST RESULT %s passed=%d failed=%d"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"), M_PassedCheckCount, M_FailedCheckCount);
	M_Phase = EFieldKitGunnerTestPhase::Finished;
	if (APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld()))
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}

void UFieldKitGunnerTestSubsystem::InitHostList()
{
	const FString Root = TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/");
	const auto Add = [this, &Root](const TCHAR* Label, const FString& RelativePackage, const TCHAR* ExpectedMesh)
	{
		FFieldKitGunnerHostUnderTest Host;
		Host.Label = Label;
		const FString AssetName = FPaths::GetBaseFilename(RelativePackage);
		Host.ClassPath = Root + RelativePackage + TEXT(".") + AssetName + TEXT("_C");
		Host.ExpectedMountMesh = ExpectedMesh;
		M_Hosts.Add(Host);
	};
	const TCHAR* PzIIITurret = TEXT("SM_Collision_PanzerIII_Turret");
	const TCHAR* PzIVTurret = TEXT("SM_Collision_PanzerIV_Turret");
	const TCHAR* Pz38tTurret = TEXT("SM_Collision_Turret_Pz38T");
	const TCHAR* Pz38tHull = TEXT("SM_Hull_PZ38_T");
	Add(TEXT("PzIII_M"), TEXT("Ger_Medium/PanzerIII/Ger_Vehicle_PanzerIII_M"), PzIIITurret);
	Add(TEXT("PzIII_M_Overcharged"), TEXT("Ger_Medium/PanzerIII/Overcharged/Ger_Vehicle_PanzerIII_M_Overcharged"),
	    PzIIITurret);
	Add(TEXT("PzIII_M_SpacedArmor"), TEXT("Ger_Medium/PanzerIII/SpacedArmor/Ger_Vehicle_PanzerIII_M_SpacedArmor"),
	    TEXT("SM_PanzerIIIM_SchreckShield"));
	Add(TEXT("PzIII_Flamm"), TEXT("Ger_Medium/PanzerIII/Ger_Vehicle_PanzerIII_Flamm"), PzIIITurret);
	Add(TEXT("Pz38t_R"), TEXT("Ger_Light/Panzer38T/Ger_Vehicle_Panzer38T_R"), Pz38tHull);
	Add(TEXT("Pz38t_R_IncendiarySalvo"),
	    TEXT("Ger_Light/Panzer38T/IncendiarySalvo/Ger_Vehicle_Panzer38T_R_IncendiarySalvo"),
	    TEXT("SM_Hull_Pz38tRockets_IncendiarySalvo"));
	Add(TEXT("Pz38t_R_PrecisionMissile"),
	    TEXT("Ger_Light/Panzer38T/PrecisionMissile/Ger_Vehicle_Panzer38T_R_PrecisionMissile"), Pz38tHull);
	Add(TEXT("Jaguar"), TEXT("Ger_Medium/Jaguar/Ger_Vehicle_Jaguar"), TEXT("SM_Ger_JaguarTurretBase"));
	Add(TEXT("Jaguar_Flanker"), TEXT("Ger_Medium/Jaguar/Flanker/Ger_Vehicle_Jaguar_Flanker"),
	    TEXT("SM_Jaguar_Flanker_TurretBase"));
	Add(TEXT("PzIV_G"), TEXT("Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_G"), PzIVTurret);
	Add(TEXT("PzIV_G_DoubleTurret"), TEXT("Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_G_DoubleTurret"),
	    TEXT("SM_Collision_PzIVG_DT_FrontTurret"));
	Add(TEXT("PzIV_H"), TEXT("Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_H"), PzIVTurret);
	Add(TEXT("PzIV_H_Ostwind"), TEXT("Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_H_Ostwind"), TEXT("SM_PanzerIV_H"));
	Add(TEXT("PzIV_H_StandardPlus"), TEXT("Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_H_StandardPlus"),
	    TEXT("SM_Collision_PzIVF1_Spotter_Turret"));
	M_Hosts.Last().ReplacedGunnerClassName = FieldKitGunnerTestPrivate::ReplacedStandardPlusGunner;
	Add(TEXT("Pz38t"), TEXT("Ger_Light/Panzer38T/Ger_Vehicle_Panzer38T"), Pz38tTurret);
	Add(TEXT("Pz38t_ATAmmoStock"), TEXT("Ger_Light/Panzer38T/Ger_Vehicle_Panzer38T_ATAmmoStock"),
	    TEXT("SM_Collision_Turret_Pz38t_ATAmmoStock"));
	Add(TEXT("Pz38t_Rocketeer"), TEXT("Ger_Light/Panzer38T/Ger_Vehicle_Panzer38T_Rocketeer"),
	    TEXT("SM_Collision_Turret_Pz38t_Rocketeer"));
	Add(TEXT("Pz38t_Rail"), TEXT("Ger_Light/Panzer38_RAIL/Ger_Vehicle_Panzer38_RAIL"), Pz38tTurret);
	Add(TEXT("Pz38t_Rail_Overcharge"), TEXT("Ger_Light/Panzer38_RAIL/Ger_Vehicle_Panzer38_RAIL_Overcharge"),
	    Pz38tTurret);
	Add(TEXT("Pz38t_Rail_Sniper"), TEXT("Ger_Light/Panzer38_RAIL/Ger_Vehicle_Panzer38_RAIL_Sniper"), Pz38tTurret);
	// Controls: left out of the kit on purpose.
	Add(TEXT("Control_PzIII_J"), TEXT("Ger_Medium/PanzerIII/Ger_Vehicle_PanzerIII_J"), TEXT(""));
	Add(TEXT("Control_PzIV_F1"), TEXT("Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_F1"), TEXT(""));
	// Built after the research: the kit must reach new tanks too.
	Add(TEXT("Pz38t_BuiltAfterResearch"), TEXT("Ger_Light/Panzer38T/Ger_Vehicle_Panzer38T"), Pz38tTurret);
	M_Hosts.Last().bSpawnedAfterResearch = true;
}

void UFieldKitGunnerTestSubsystem::TickWaitingForStart()
{
	using namespace FieldKitGunnerTestPrivate;
	ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	if (not IsValid(Controller))
	{
		return;
	}
	if (Controller->GetIsGameOnPause())
	{
		Controller->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
	if (M_PhaseStartSeconds < 0.f)
	{
		M_PhaseStartSeconds = GetNow();
		return;
	}
	if (GetNow() - M_PhaseStartSeconds < StartDelaySeconds)
	{
		return;
	}
	if (not GetGroundAt(DefaultAnchor, M_Anchor))
	{
		Check(false, TEXT("found_ground_anchor"), DefaultAnchor.ToString());
		Finish();
		return;
	}
	FString CaptureDirectory;
	if (FParse::Value(FCommandLine::Get(), TEXT("FKCapture="), CaptureDirectory))
	{
		M_CaptureRig.Init(*GetWorld(), CaptureDirectory);
	}
	InitHostList();
	for (int32 HostIndex = 0; HostIndex < M_Hosts.Num(); ++HostIndex)
	{
		if (not M_Hosts[HostIndex].bSpawnedAfterResearch)
		{
			Check(SpawnHost(M_Hosts[HostIndex], HostIndex), TEXT("spawned_host"), M_Hosts[HostIndex].Label);
		}
	}
	UE_LOG(LogRTSFieldKitTest, Display, TEXT("RTS_FIELDKIT_TEST START hosts=%d anchor=%s"), M_Hosts.Num(),
	       *M_Anchor.ToCompactString());
	EnterPhase(EFieldKitGunnerTestPhase::BeforeResearch);
}

bool UFieldKitGunnerTestSubsystem::SpawnHost(FFieldKitGunnerHostUnderTest& Host, const int32 SlotIndex)
{
	using namespace FieldKitGunnerTestPrivate;
	const int32 Row = SlotIndex / HostsPerRow;
	const int32 Column = SlotIndex % HostsPerRow;
	const FVector2D Spot(M_Anchor.X - Row * RowSpacing, M_Anchor.Y + Column * HostSpacing);
	FVector Ground;
	if (not GetGroundAt(Spot, Ground))
	{
		Ground = FVector(Spot.X, Spot.Y, M_Anchor.Z);
	}
	Host.Tank = SpawnUnitForPlayer(Host.ClassPath, FTransform(Ground + FVector(0.f, 0.f, UnitLift)), 1);
	return Host.Tank.IsValid();
}

void UFieldKitGunnerTestSubsystem::TickBeforeResearch()
{
	using namespace FieldKitGunnerTestPrivate;
	KeepHostsAlive();
	if (GetNow() - M_PhaseStartSeconds < BeforeResearchSeconds)
	{
		return;
	}
	for (const FFieldKitGunnerHostUnderTest& Host : M_Hosts)
	{
		if (not Host.Tank.IsValid())
		{
			continue;
		}
		const bool bIsControl = Host.ExpectedMountMesh.IsEmpty();
		const int32 KitMountCount = GetKitMounts(Host.Tank.Get()).Num();
		Check(bIsControl ? KitMountCount == 0 : KitMountCount == 1, TEXT("kit_mount_count"),
		      FString::Printf(TEXT("%s kit_mounts=%d"), *Host.Label, KitMountCount));
		int32 KitGunnersBefore = 0;
		bool bHasReplacedGunner = false;
		for (const AEmbeddedInfantryTurret* Gunner : GetMountedGunners(Host.Tank.Get()))
		{
			KitGunnersBefore += Gunner->GetClass()->GetName() == KitGunnerClassName ? 1 : 0;
			bHasReplacedGunner |= Gunner->GetClass()->GetName() == Host.ReplacedGunnerClassName;
		}
		Check(KitGunnersBefore == 0, TEXT("no_kit_gunner_before_research"), Host.Label);
		if (not Host.ReplacedGunnerClassName.IsEmpty())
		{
			Check(bHasReplacedGunner, TEXT("own_gunner_before_research"), Host.Label);
		}
	}
	ResearchKit();
	// A tank built after the research must get the gunner from its own BeginPlay.
	for (int32 HostIndex = 0; HostIndex < M_Hosts.Num(); ++HostIndex)
	{
		if (M_Hosts[HostIndex].bSpawnedAfterResearch)
		{
			Check(SpawnHost(M_Hosts[HostIndex], HostIndex), TEXT("spawned_host_after_research"),
			      M_Hosts[HostIndex].Label);
		}
	}
	EnterPhase(EFieldKitGunnerTestPhase::WaitingForMounts);
}

void UFieldKitGunnerTestSubsystem::ResearchKit()
{
	using namespace FieldKitGunnerTestPrivate;
	ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	UPlayerTechManager* TechManager = FRTS_Statics::GetPlayerTechManager(this);
	const bool bInControllerMap = IsValid(Controller) && Controller->GetTechnologyEffectsMap().Contains(KitTechnology);
	Check(bInControllerMap, TEXT("kit_in_controller_tech_map"));
	Check(IsValid(TechManager), TEXT("tech_manager"));
	if (bInControllerMap && IsValid(TechManager))
	{
		TechManager->OnTechResearched(KitTechnology);
		bM_ResearchedThroughTechManager = true;
		UE_LOG(LogRTSFieldKitTest, Display, TEXT("RTS_FIELDKIT_TEST research path=tech_manager"));
		return;
	}
	// Without the map entry, apply the effect directly so the placement checks still run.
	UE_LOG(LogRTSFieldKitTest, Display, TEXT("RTS_FIELDKIT_TEST research path=direct"));
	UTE_EmbeddedGunnerKit* Effect = NewObject<UTE_EmbeddedGunnerKit>(this);
	for (const FFieldKitGunnerHostUnderTest& Host : M_Hosts)
	{
		ATankMaster* Tank = Cast<ATankMaster>(Host.Tank.Get());
		const URTSComponent* RTSComponent = IsValid(Tank) ? Tank->GetRTSComponent() : nullptr;
		if (IsValid(RTSComponent) && Effect->GetTanksToApplyTo().Contains(RTSComponent->GetSubtypeAsTankSubtype()))
		{
			Effect->ApplyOnTank(Tank);
		}
	}
}

void UFieldKitGunnerTestSubsystem::TickWaitingForMounts()
{
	using namespace FieldKitGunnerTestPrivate;
	KeepHostsAlive();
	bool bAllMounted = true;
	for (FFieldKitGunnerHostUnderTest& Host : M_Hosts)
	{
		if (Host.ExpectedMountMesh.IsEmpty() || not Host.Tank.IsValid())
		{
			continue;
		}
		bool bHasKitGunner = false;
		for (AEmbeddedInfantryTurret* Gunner : GetMountedGunners(Host.Tank.Get()))
		{
			bHasKitGunner |= Gunner->GetClass()->GetName() == KitGunnerClassName;
		}
		bAllMounted &= bHasKitGunner;
	}
	const float Elapsed = GetNow() - M_PhaseStartSeconds;
	if ((not bAllMounted || Elapsed < MountSettleSeconds) && Elapsed < MountTimeoutSeconds)
	{
		return;
	}
	for (FFieldKitGunnerHostUnderTest& Host : M_Hosts)
	{
		CheckHostAfterResearch(Host);
	}
	for (const FFieldKitGunnerHostUnderTest& Host : M_Hosts)
	{
		CheckRepeatedMountRequestAddsNothing(Host);
		CaptureHost(Host);
	}
	StartAttackGround();
}

void UFieldKitGunnerTestSubsystem::CheckHostAfterResearch(FFieldKitGunnerHostUnderTest& Host)
{
	using namespace FieldKitGunnerTestPrivate;
	if (not Host.Tank.IsValid())
	{
		Check(false, TEXT("host_alive"), Host.Label);
		return;
	}
	TArray<AEmbeddedInfantryTurret*> KitGunners;
	bool bHasReplacedGunner = false;
	for (AEmbeddedInfantryTurret* Gunner : GetMountedGunners(Host.Tank.Get()))
	{
		if (Gunner->GetClass()->GetName() == KitGunnerClassName)
		{
			KitGunners.Add(Gunner);
		}
		bHasReplacedGunner |= Gunner->GetClass()->GetName() == Host.ReplacedGunnerClassName;
	}
	if (Host.ExpectedMountMesh.IsEmpty())
	{
		Check(KitGunners.IsEmpty(), TEXT("control_has_no_kit_gunner"), Host.Label);
		return;
	}
	Check(KitGunners.Num() == 1, TEXT("one_kit_gunner"), FString::Printf(TEXT("%s count=%d"), *Host.Label,
		      KitGunners.Num()));
	if (not Host.ReplacedGunnerClassName.IsEmpty())
	{
		Check(not bHasReplacedGunner, TEXT("own_gunner_replaced"), Host.Label);
	}
	if (KitGunners.IsEmpty())
	{
		LogHostMeshes(Host);
		return;
	}
	Host.Gunner = KitGunners[0];
	const FString MountMeshName = GetMountMeshAssetName(KitGunners[0]);
	const FTransform TankTransform = Host.Tank->GetActorTransform();
	const FVector LocalGunner = TankTransform.InverseTransformPosition(KitGunners[0]->GetActorLocation());
	if (not MountMeshName.Contains(Host.ExpectedMountMesh))
	{
		LogHostMeshes(Host);
	}
	Check(MountMeshName.Contains(Host.ExpectedMountMesh), TEXT("mounted_on_expected_mesh"),
	      FString::Printf(TEXT("%s mesh=%s expected=%s gunner_local=%s"), *Host.Label, *MountMeshName,
	                      *Host.ExpectedMountMesh, *LocalGunner.ToCompactString()));
	const ATankMaster* Tank = Cast<ATankMaster>(Host.Tank.Get());
	Check(IsValid(Tank) && Tank->GetTurrets().Contains(KitGunners[0]), TEXT("registered_with_host_turrets"),
	      Host.Label);
	Check(KitGunners[0]->GetWeapons().Num() == 1, TEXT("one_weapon"),
	      FString::Printf(TEXT("%s range=%.0f"), *Host.Label, KitGunners[0]->GetMaxWeaponRange()));
	CheckGunnerIsClean(Host);
}

void UFieldKitGunnerTestSubsystem::CheckRepeatedMountRequestAddsNothing(const FFieldKitGunnerHostUnderTest& Host)
{
	using namespace FieldKitGunnerTestPrivate;
	if (not Host.Tank.IsValid() || Host.ExpectedMountMesh.IsEmpty())
	{
		return;
	}
	for (UEmbeddedTurretMountComponent* Mount : GetKitMounts(Host.Tank.Get()))
	{
		Mount->RequestMount();
	}
	int32 KitGunnerCount = 0;
	for (const AEmbeddedInfantryTurret* Gunner : GetMountedGunners(Host.Tank.Get()))
	{
		KitGunnerCount += Gunner->GetClass()->GetName() == KitGunnerClassName ? 1 : 0;
	}
	Check(KitGunnerCount == 1, TEXT("repeated_request_adds_nothing"),
	      FString::Printf(TEXT("%s count=%d"), *Host.Label, KitGunnerCount));
}

void UFieldKitGunnerTestSubsystem::CheckGunnerIsClean(const FFieldKitGunnerHostUnderTest& Host)
{
	TArray<UPrimitiveComponent*> PrimitiveComponents;
	Host.Gunner->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	FString Offenders;
	for (const UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		const bool bClean = PrimitiveComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision
			&& not PrimitiveComponent->CanEverAffectNavigation();
		if (not bClean)
		{
			Offenders += PrimitiveComponent->GetName() + TEXT(" ");
		}
	}
	Check(Offenders.IsEmpty(), TEXT("no_collision_or_navigation"),
	      FString::Printf(TEXT("%s offenders=%s"), *Host.Label, *Offenders));
}

void UFieldKitGunnerTestSubsystem::StartAttackGround()
{
	using namespace FieldKitGunnerTestPrivate;
	for (FFieldKitGunnerHostUnderTest& Host : M_Hosts)
	{
		if (not Host.Gunner.IsValid() || not Host.Tank.IsValid())
		{
			continue;
		}
		const float Distance = FMath::Max(MinimumGroundTargetDistance,
		                                  Host.Gunner->GetMaxWeaponRange() * GroundTargetRangeFraction);
		const FVector Ahead = Host.Tank->GetActorLocation() + Host.Tank->GetActorForwardVector() * Distance;
		if (not GetGroundAt(FVector2D(Ahead), Host.GroundTarget))
		{
			Host.GroundTarget = Ahead;
		}
		ICommands* Commands = Cast<ICommands>(Host.Tank.Get());
		const ECommandQueueError Error = Commands != nullptr
			? Commands->AttackGround(Host.GroundTarget, true)
			: ECommandQueueError::QueueFull;
		Check(Error == ECommandQueueError::NoError, TEXT("attack_ground_ordered"), Host.Label);
		Host.Shots = 0;
		Host.LastMagazineCounts.Reset();
		CountShots(Host);
	}
	EnterPhase(EFieldKitGunnerTestPhase::AttackingGround);
}

void UFieldKitGunnerTestSubsystem::TickAttackingGround()
{
	using namespace FieldKitGunnerTestPrivate;
	KeepHostsAlive();
	bool bAllFired = true;
	for (FFieldKitGunnerHostUnderTest& Host : M_Hosts)
	{
		if (not Host.Gunner.IsValid())
		{
			continue;
		}
		CountShots(Host);
		bAllFired &= Host.Shots > 0;
	}
	const float Elapsed = GetNow() - M_PhaseStartSeconds;
	if (not bAllFired && Elapsed < AttackGroundTimeoutSeconds)
	{
		return;
	}
	for (const FFieldKitGunnerHostUnderTest& Host : M_Hosts)
	{
		if (Host.Gunner.IsValid())
		{
			Check(Host.Shots > 0, TEXT("gunner_fired_at_ground_target"),
			      FString::Printf(TEXT("%s shots=%d t=%.1f"), *Host.Label, Host.Shots, Elapsed));
		}
	}
	UE_LOG(LogRTSFieldKitTest, Display, TEXT("RTS_FIELDKIT_TEST research_through_tech_manager=%d"),
	       bM_ResearchedThroughTechManager ? 1 : 0);
	Finish();
}

void UFieldKitGunnerTestSubsystem::CountShots(FFieldKitGunnerHostUnderTest& Host) const
{
	AEmbeddedInfantryTurret* Gunner = Host.Gunner.Get();
	if (not IsValid(Gunner))
	{
		return;
	}
	TArray<UWeaponState*> Weapons = Gunner->GetWeapons();
	for (int32 WeaponSlot = 0; WeaponSlot < Weapons.Num(); ++WeaponSlot)
	{
		const int32 Magazine = IsValid(Weapons[WeaponSlot]) ? Weapons[WeaponSlot]->GetCurrentMagCapacity() : 0;
		if (not Host.LastMagazineCounts.IsValidIndex(WeaponSlot))
		{
			Host.LastMagazineCounts.Add(Magazine);
			continue;
		}
		Host.Shots += FMath::Max(0, Host.LastMagazineCounts[WeaponSlot] - Magazine);
		Host.LastMagazineCounts[WeaponSlot] = Magazine;
	}
}

void UFieldKitGunnerTestSubsystem::LogHostMeshes(const FFieldKitGunnerHostUnderTest& Host) const
{
	// Variants set their own meshes at runtime; list them so the kit socket can go on the right one.
	TArray<AActor*> Actors = {Host.Tank.Get()};
	Host.Tank->GetAttachedActors(Actors, false, true);
	for (const AActor* Actor : Actors)
	{
		TArray<UMeshComponent*> Meshes;
		Actor->GetComponents<UMeshComponent>(Meshes);
		for (const UMeshComponent* Mesh : Meshes)
		{
			const UStaticMeshComponent* StaticMesh = Cast<UStaticMeshComponent>(Mesh);
			const USkinnedMeshComponent* SkinnedMesh = Cast<USkinnedMeshComponent>(Mesh);
			const FString Asset = IsValid(StaticMesh) ? GetNameSafe(StaticMesh->GetStaticMesh())
				: IsValid(SkinnedMesh) ? GetNameSafe(SkinnedMesh->GetSkinnedAsset()) : TEXT("?");
			const FVector Local = Host.Tank->GetActorTransform().InverseTransformPosition(Mesh->GetComponentLocation());
			UE_LOG(LogRTSFieldKitTest, Display, TEXT("RTS_FIELDKIT_TEST MESH %s actor=%s component=%s asset=%s visible=%d at=%s bounds=%s"),
			       *Host.Label, *Actor->GetClass()->GetName(), *Mesh->GetName(), *Asset, Mesh->IsVisible() ? 1 : 0,
			       *Local.ToCompactString(), *Mesh->Bounds.BoxExtent.ToCompactString());
		}
	}
}

TArray<AEmbeddedInfantryTurret*> UFieldKitGunnerTestSubsystem::GetMountedGunners(const AActor* Tank) const
{
	TArray<AEmbeddedInfantryTurret*> Gunners;
	if (not IsValid(Tank))
	{
		return Gunners;
	}
	TArray<UEmbeddedTurretMountComponent*> Mounts;
	Tank->GetComponents<UEmbeddedTurretMountComponent>(Mounts);
	for (const UEmbeddedTurretMountComponent* Mount : Mounts)
	{
		if (AEmbeddedInfantryTurret* Gunner = Cast<AEmbeddedInfantryTurret>(Mount->GetMountedTurret()))
		{
			Gunners.Add(Gunner);
		}
	}
	return Gunners;
}

TArray<UEmbeddedTurretMountComponent*> UFieldKitGunnerTestSubsystem::GetKitMounts(const AActor* Tank) const
{
	TArray<UEmbeddedTurretMountComponent*> KitMounts;
	TArray<UEmbeddedTurretMountComponent*> Mounts;
	Tank->GetComponents<UEmbeddedTurretMountComponent>(Mounts);
	for (UEmbeddedTurretMountComponent* Mount : Mounts)
	{
		if (Mount->GetRequiredTechnology() == FieldKitGunnerTestPrivate::KitTechnology)
		{
			KitMounts.Add(Mount);
		}
	}
	return KitMounts;
}

bool UFieldKitGunnerTestSubsystem::GetGroundAt(const FVector2D& Location2D, FVector& OutGround) const
{
	using namespace FieldKitGunnerTestPrivate;
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	const FVector Start(Location2D.X, Location2D.Y, GroundTraceHalfHeight);
	const FVector End(Location2D.X, Location2D.Y, -GroundTraceHalfHeight);
	FHitResult GroundHit;
	const FCollisionObjectQueryParams ObjectParams(ECC_WorldStatic);
	if (not World->LineTraceSingleByObjectType(GroundHit, Start, End, ObjectParams))
	{
		return false;
	}
	OutGround = GroundHit.Location;
	return true;
}

AActor* UFieldKitGunnerTestSubsystem::SpawnUnitForPlayer(const FString& ClassPath, const FTransform& Transform,
                                                         const uint8 OwningPlayer) const
{
	using namespace FieldKitGunnerTestPrivate;
	UWorld* World = GetWorld();
	UClass* UnitClass = LoadClass<AActor>(nullptr, *ClassPath);
	if (not IsValid(World) || not IsValid(UnitClass))
	{
		UE_LOG(LogRTSFieldKitTest, Error, TEXT("RTS_FIELDKIT_TEST cannot load %s"), *ClassPath);
		return nullptr;
	}
	// The owner must be in place before BeginPlay, so the tank looks for the player's technologies.
	TArray<TPair<TWeakObjectPtr<UObject>, uint8>> TemplateOwnersToRestore;
	OverrideRTSComponentTemplateOwners(UnitClass, OwningPlayer, TemplateOwnersToRestore);
	AActor* Unit = World->SpawnActorDeferred<AActor>(UnitClass, Transform, nullptr, nullptr,
	                                                 ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (not IsValid(Unit))
	{
		RestoreRTSComponentTemplateOwners(TemplateOwnersToRestore);
		return nullptr;
	}
	if (URTSComponent* RTSComponent = Unit->FindComponentByClass<URTSComponent>())
	{
		RTSComponent->SetOwningPlayerRuntime(OwningPlayer);
	}
	Unit->FinishSpawning(Transform);
	RestoreRTSComponentTemplateOwners(TemplateOwnersToRestore);
	if (APawn* Pawn = Cast<APawn>(Unit); IsValid(Pawn) && not Pawn->GetController())
	{
		Pawn->SpawnDefaultController();
	}
	return Unit;
}

void UFieldKitGunnerTestSubsystem::KeepHostsAlive() const
{
	for (const FFieldKitGunnerHostUnderTest& Host : M_Hosts)
	{
		UHealthComponent* Health = Host.Tank.IsValid() ? Host.Tank->FindComponentByClass<UHealthComponent>() : nullptr;
		if (IsValid(Health))
		{
			Health->AddHealth(Health->GetMaxHealth());
		}
	}
}

void UFieldKitGunnerTestSubsystem::CaptureHost(const FFieldKitGunnerHostUnderTest& Host)
{
	if (not M_CaptureRig.GetIsEnabled() || not Host.Tank.IsValid())
	{
		return;
	}
	const TArray<FB4HowitzerCameraView> Views = {
		{TEXT("Side"), FVector(-60.f, -760.f, 330.f), FVector(-60.f, 0.f, 200.f), 50.f},
		{TEXT("RearQuarter"), FVector(-620.f, -460.f, 480.f), FVector(-40.f, 0.f, 200.f), 50.f},
		{TEXT("Top"), FVector(-160.f, -40.f, 950.f), FVector(-60.f, 0.f, 150.f), 55.f},
	};
	M_CaptureRig.CaptureViews(*GetWorld(), Host.Tank->GetActorTransform(), Views, Host.Label);
}

#else

bool UFieldKitGunnerTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UFieldKitGunnerTestSubsystem::Tick(const float DeltaTime)
{
}

TStatId UFieldKitGunnerTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFieldKitGunnerTestSubsystem, STATGROUP_Tickables);
}

#endif
