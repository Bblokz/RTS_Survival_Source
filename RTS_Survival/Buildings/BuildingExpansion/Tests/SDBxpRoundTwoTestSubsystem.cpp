// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "SDBxpRoundTwoTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BxpBonuses/PanzerwerferReloadBonus/BehaviourWeaponWurfrahmenShop.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BxpBonuses/PanzerwerferReloadBonus/PanzerwerferReloadBonusSubsystem.h"
#include "RTS_Survival/Buildings/BuildingExpansion/Interface/BuildingExpansionOwner.h"
#include "RTS_Survival/Behaviours/BehaviourComp.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Environment/DestructableEnvActor/WallChunkDamage/WallHoleDamageComponent.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowType.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Player/ConstructionPreview/CPPConstructionPreview.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSSDBxp2Test, Log, All);

namespace SDBxpRoundTwoTestPrivate
{
	constexpr float StartDelaySeconds = 4.f;
	constexpr float CameraTraceLength = 500000.f;
	// Speeds up building, the deploys are fast-forwarded.
	constexpr float TestTimeDilation = 10.f;
	constexpr float FastForwardDelaySeconds = 1.f;
	constexpr float SpawnTimeoutSeconds = 40.f;
	constexpr float DeployTimeoutSeconds = 60.f;
	constexpr float BxpLoadTimeoutSeconds = 30.f;
	constexpr float BxpBuildTimeoutSeconds = 90.f;
	constexpr float PanzerwerferBonusTimeoutSeconds = 20.f;
	constexpr float SniperFireTimeoutSeconds = 40.f;
	constexpr float PackUpTimeoutSeconds = 60.f;
	constexpr float ReplacedBxpTimeoutSeconds = 40.f;
	constexpr int32 TestResourceAmount = 1000000;
	constexpr int32 RequiredSlotCount = 3;
	// The barracks deploys beside the depot; the Panzerwerfers park in front of the depot.
	const FVector BarracksOffsets[] = {
		// -Y of the pack-up test's anchor lost the barracks within seconds (enemies on RTS_ToCopySmall); +X first.
		FVector(4500.f, 0.f, 0.f), FVector(-4500.f, 0.f, 0.f), FVector(0.f, 4500.f, 0.f), FVector(0.f, -4000.f, 0.f)
	};
	const FVector EarlyPanzerwerferOffset = FVector(2600.f, 1200.f, 0.f);
	const FVector LatePanzerwerferOffset = FVector(2600.f, -1200.f, 0.f);
	// In front of the barracks (+X), inside the sniper's arc and range.
	const FVector EnemyOffsetFromBarracks = FVector(4000.f, 600.f, 0.f);
	// Behind the loft: outside the -90..90 arc.
	const FVector BehindLoftOffset = FVector(-2500.f, 0.f, 0.f);
	constexpr float ReloadTolerance = 0.05f;
	constexpr float TestHitCalibreMm = 75.f;
	// Debug shots start this far out from each aim point and fly back at it.
	constexpr float TestHitStandOff = 400.f;
	constexpr float TestHitReach = 800.f;
	const FName SniperLoftSocketName = TEXT("Turret");
	const TCHAR* const EnemyClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/Rus_Vehicle_T3476.Rus_Vehicle_T3476_C");
	constexpr uint8 EnemyPlayer = 2;
	constexpr uint8 PlayerIndex = 1;
	const TCHAR* const PanzerwerferClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Light/Panzer38T/Ger_Vehicle_Panzer38T_R.Ger_Vehicle_Panzer38T_R_C");
	const FString TestTag = TEXT("RTS_SDBXP2_TEST");

	void SetRTSComponentTemplateOwners(UClass* UnitClass, const uint8 OwningPlayer,
	                                   TArray<TPair<TWeakObjectPtr<UObject>, uint8>>& OutRestore)
	{
		const FByteProperty* OwnerProperty = FindFProperty<FByteProperty>(URTSComponent::StaticClass(),
		                                                                  TEXT("OwningPlayer"));
		if (OwnerProperty == nullptr)
		{
			return;
		}
		TArray<UActorComponent*> Templates;
		for (UClass* Class = UnitClass; Class != nullptr; Class = Class->GetSuperClass())
		{
			UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(Class);
			if (BlueprintClass == nullptr || BlueprintClass->SimpleConstructionScript == nullptr)
			{
				continue;
			}
			for (const USCS_Node* Node : BlueprintClass->SimpleConstructionScript->GetAllNodes())
			{
				Templates.Add(Node != nullptr ? Node->ComponentTemplate : nullptr);
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
		const FByteProperty* OwnerProperty = FindFProperty<FByteProperty>(URTSComponent::StaticClass(),
		                                                                  TEXT("OwningPlayer"));
		for (const TPair<TWeakObjectPtr<UObject>, uint8>& Entry : Restore)
		{
			if (OwnerProperty != nullptr && Entry.Key.IsValid())
			{
				*OwnerProperty->ContainerPtrToValuePtr<uint8>(Entry.Key.Get()) = Entry.Value;
			}
		}
	}
}

bool USDBxpRoundTwoTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), TEXT("SDBxpRoundTwoValidate"));
}

TStatId USDBxpRoundTwoTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USDBxpRoundTwoTestSubsystem, STATGROUP_Tickables);
}

void USDBxpRoundTwoTestSubsystem::Tick(const float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	switch (M_Stage)
	{
	case ESDBxpRoundTwoTestStage::WaitingForStart:
		TickWaitingForStart();
		return;
	case ESDBxpRoundTwoTestStage::WaitingForSpawns:
		TickWaitingForSpawns();
		return;
	case ESDBxpRoundTwoTestStage::WaitingForDeploy:
		TickWaitingForDeploy();
		return;
	case ESDBxpRoundTwoTestStage::WaitingForBxpLoaded:
		TickWaitingForBxpLoaded();
		return;
	case ESDBxpRoundTwoTestStage::WaitingForBxpsBuilt:
		TickWaitingForBxpsBuilt();
		return;
	case ESDBxpRoundTwoTestStage::WaitingForLatePanzerwerfer:
		TickWaitingForLatePanzerwerfer();
		return;
	case ESDBxpRoundTwoTestStage::WaitingForSniperShots:
		TickWaitingForSniperShots();
		return;
	case ESDBxpRoundTwoTestStage::WaitingForPackUp:
		TickWaitingForPackUp();
		return;
	case ESDBxpRoundTwoTestStage::WaitingForRedeploy:
		TickWaitingForRedeploy();
		return;
	case ESDBxpRoundTwoTestStage::WaitingForReplacedBxps:
		TickWaitingForReplacedBxps();
		return;
	case ESDBxpRoundTwoTestStage::Finished:
		return;
	}
}

void USDBxpRoundTwoTestSubsystem::Check(const bool bCondition, const FString& Description, const FString& Details)
{
	if (bCondition)
	{
		++M_PassedCheckCount;
		UE_LOG(LogRTSSDBxp2Test, Display, TEXT("RTS_SDBXP2_TEST CHECK PASS %s %s"), *Description, *Details);
		return;
	}
	++M_FailedCheckCount;
	UE_LOG(LogRTSSDBxp2Test, Error, TEXT("RTS_SDBXP2_TEST CHECK FAIL %s %s"), *Description, *Details);
}

float USDBxpRoundTwoTestSubsystem::GetNow() const
{
	const UWorld* World = GetWorld();
	// Real time: the test dilates game time.
	return IsValid(World) ? World->GetRealTimeSeconds() : 0.f;
}

float USDBxpRoundTwoTestSubsystem::GetStageElapsed() const
{
	return GetNow() - M_StageSeconds;
}

void USDBxpRoundTwoTestSubsystem::EnterStage(const ESDBxpRoundTwoTestStage NewStage)
{
	M_Stage = NewStage;
	M_StageSeconds = GetNow();
	bM_StageActionDone = false;
	UE_LOG(LogRTSSDBxp2Test, Display, TEXT("RTS_SDBXP2_TEST STAGE %s"), *UEnum::GetValueAsString(NewStage));
}

void USDBxpRoundTwoTestSubsystem::FailAndFinish(const FString& Description, const FString& Details)
{
	Check(false, Description, Details);
	Finish();
}

void USDBxpRoundTwoTestSubsystem::Finish()
{
	const bool bPassed = M_FailedCheckCount == 0 && M_PassedCheckCount > 0;
	UE_LOG(LogRTSSDBxp2Test, Display, TEXT("RTS_SDBXP2_TEST RESULT %s passed=%d failed=%d"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"), M_PassedCheckCount, M_FailedCheckCount);
	EnterStage(ESDBxpRoundTwoTestStage::Finished);
	UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
	if (APlayerController* Controller = GetPlayerController())
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}

ACPPController* USDBxpRoundTwoTestSubsystem::GetPlayerController() const
{
	return FRTS_Statics::GetRTSController(GetWorld());
}

bool USDBxpRoundTwoTestSubsystem::GetBxpOwner(const int32 BuildingIndex,
                                              TScriptInterface<IBuildingExpansionOwner>& OutOwner) const
{
	if (not M_Buildings.IsValidIndex(BuildingIndex))
	{
		return false;
	}
	ANomadicVehicle* Truck = M_Buildings[BuildingIndex].Truck.Get();
	if (not IsValid(Truck))
	{
		return false;
	}
	OutOwner.SetObject(Truck);
	OutOwner.SetInterface(static_cast<IBuildingExpansionOwner*>(Truck));
	return true;
}

ABuildingExpansion* USDBxpRoundTwoTestSubsystem::GetPlacedBxp(const int32 BuildingIndex, const int32 BxpIndex) const
{
	if (not M_Buildings.IsValidIndex(BuildingIndex)
		|| not M_Buildings[BuildingIndex].SlotIndices.IsValidIndex(BxpIndex))
	{
		return nullptr;
	}
	const ANomadicVehicle* Truck = M_Buildings[BuildingIndex].Truck.Get();
	return IsValid(Truck)
		       ? Truck->GetBuildingExpansionAtIndex(M_Buildings[BuildingIndex].SlotIndices[BxpIndex])
		       : nullptr;
}

// ---------------------------------------------------------------------------------------------------------------
// Start and spawns
// ---------------------------------------------------------------------------------------------------------------

void USDBxpRoundTwoTestSubsystem::TickWaitingForStart()
{
	using namespace SDBxpRoundTwoTestPrivate;
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
		FailAndFinish(TEXT("found_ground_anchor"));
		return;
	}
	GivePlayerResources();
	SetupBuildings();
	SpawnTrucksAndEarlyPanzerwerfer();
}

bool USDBxpRoundTwoTestSubsystem::StartGameAndGetIsReady() const
{
	ACPPController* Controller = GetPlayerController();
	if (not IsValid(Controller))
	{
		return false;
	}
	// The map boots paused behind the start-game widget; start it as the player's "Press To Start" does.
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

bool USDBxpRoundTwoTestSubsystem::FindAnchor(FVector& OutAnchor) const
{
	using namespace SDBxpRoundTwoTestPrivate;
	// FParse::Value stops at commas, so the anchor comes as two values.
	float AnchorX = 0.f;
	float AnchorY = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("SDBxpAnchorX="), AnchorX)
		&& FParse::Value(FCommandLine::Get(), TEXT("SDBxpAnchorY="), AnchorY))
	{
		return GlobalAbilityHelpers::TryFindGroundLocation(this, FVector(AnchorX, AnchorY, 0.f), OutAnchor);
	}
	const UWorld* World = GetWorld();
	const APlayerController* Controller = GetPlayerController();
	if (not IsValid(Controller) || not IsValid(Controller->PlayerCameraManager))
	{
		return false;
	}
	const FVector CameraLocation = Controller->PlayerCameraManager->GetCameraLocation();
	const FVector CameraForward = Controller->PlayerCameraManager->GetCameraRotation().Vector();
	FHitResult GroundHit;
	if (not World->LineTraceSingleByChannel(GroundHit, CameraLocation, CameraLocation + CameraForward * CameraTraceLength,
	                                        ECC_Visibility))
	{
		return false;
	}
	return GlobalAbilityHelpers::TryFindGroundLocation(this, GroundHit.Location, OutAnchor);
}

void USDBxpRoundTwoTestSubsystem::GivePlayerResources() const
{
	using namespace SDBxpRoundTwoTestPrivate;
	const ACPPController* Controller = GetPlayerController();
	UPlayerResourceManager* Resources = IsValid(Controller) ? Controller->GetPlayerResourceManager() : nullptr;
	if (not IsValid(Resources))
	{
		return;
	}
	const TArray<ERTSResourceType> ResourceTypes = {
		ERTSResourceType::Resource_Radixite, ERTSResourceType::Resource_Metal, ERTSResourceType::Resource_VehicleParts,
		ERTSResourceType::Resource_Fuel, ERTSResourceType::Resource_Ammo, ERTSResourceType::Resource_Intel,
		ERTSResourceType::Blueprint_Weapon, ERTSResourceType::Blueprint_Vehicle, ERTSResourceType::Blueprint_Energy,
		ERTSResourceType::Blueprint_Construction,
	};
	for (const ERTSResourceType ResourceType : ResourceTypes)
	{
		Resources->CheatAddStorage(ResourceType, TestResourceAmount);
		Resources->AddResource(ResourceType, TestResourceAmount);
	}
}

void USDBxpRoundTwoTestSubsystem::SetupBuildings()
{
	using namespace SDBxpRoundTwoTestPrivate;
	M_Buildings.SetNum(2);
	M_Buildings[0].Subtype = ENomadicSubtype::Nomadic_GerMechanizedDepot;
	M_Buildings[0].BxpTypes = {
		EBuildingExpansionType::BTX_GerMechDepotRadioVan, EBuildingExpansionType::BTX_GerMechDepotWurfrahmenShop
	};
	M_Buildings[0].Location = M_Anchor;
	M_Buildings[1].Subtype = ENomadicSubtype::Nomadic_GerBarracks;
	M_Buildings[1].BxpTypes = {EBuildingExpansionType::BTX_GerBarracksSniperLoft};
	// The first side of the depot with ground under it (small test maps end close to the anchor).
	for (const FVector& Offset : BarracksOffsets)
	{
		FVector BarracksGround;
		if (GlobalAbilityHelpers::TryFindGroundLocation(this, M_Anchor + Offset, BarracksGround))
		{
			M_Buildings[1].Location = BarracksGround;
			UE_LOG(LogRTSSDBxp2Test, Display, TEXT("RTS_SDBXP2_TEST barracks at %s"), *BarracksGround.ToCompactString());
			return;
		}
	}
	M_Buildings[1].Location = M_Anchor + BarracksOffsets[0];
}

void USDBxpRoundTwoTestSubsystem::SpawnTrucksAndEarlyPanzerwerfer()
{
	using namespace SDBxpRoundTwoTestPrivate;
	if (not IsValid(FRTS_Statics::GetAsyncSpawner(this)))
	{
		FailAndFinish(TEXT("async_spawner_found"));
		return;
	}
	SpawnTruck(0);
	SpawnTruck(1);
	FVector PanzerwerferGround = M_Anchor + EarlyPanzerwerferOffset;
	GlobalAbilityHelpers::TryFindGroundLocation(this, PanzerwerferGround, PanzerwerferGround);
	SpawnPanzerwerfer(PanzerwerferGround, false);
	EnterStage(ESDBxpRoundTwoTestStage::WaitingForSpawns);
}

void USDBxpRoundTwoTestSubsystem::SpawnTruck(const int32 BuildingIndex)
{
	ARTSAsyncSpawner* Spawner = FRTS_Statics::GetAsyncSpawner(this);
	FSDBxpRoundTwoTestBuilding& Building = M_Buildings[BuildingIndex];
	TWeakObjectPtr<USDBxpRoundTwoTestSubsystem> WeakThis(this);
	const bool bRequested = Spawner->AsyncSpawnOptionAtLocation(
		FTrainingOption(EAllUnitType::UNType_Nomadic, static_cast<uint8>(Building.Subtype)),
		Building.Location,
		this,
		BuildingIndex,
		[WeakThis](const FTrainingOption&, AActor* SpawnedActor, const int32 SpawnID)
		{
			UE_LOG(LogRTSSDBxp2Test, Display, TEXT("RTS_SDBXP2_TEST truck spawned id=%d actor=%s"), SpawnID,
			       *GetNameSafe(SpawnedActor));
			if (WeakThis.IsValid() && WeakThis->M_Buildings.IsValidIndex(SpawnID))
			{
				WeakThis->M_Buildings[SpawnID].Truck = Cast<ANomadicVehicle>(SpawnedActor);
			}
		},
		FRotator(0.f, Building.Yaw, 0.f));
	Check(bRequested, TEXT("truck_spawn_requested"), UEnum::GetValueAsString(Building.Subtype));
}

void USDBxpRoundTwoTestSubsystem::SpawnPanzerwerfer(const FVector& Location, const bool bIsLate)
{
	using namespace SDBxpRoundTwoTestPrivate;
	// Tank_Panzerwerfer has no unit Blueprint (not in the spawner's training map): the Pz 38(t) R carries the rocket turret.
	ATankMaster* Panzerwerfer = Cast<ATankMaster>(SpawnUnitForPlayer(PanzerwerferClassPath,
	                                                                 FTransform(FRotator::ZeroRotator, Location),
	                                                                 PlayerIndex));
	const bool bIsPanzerwerfer = IsValid(Panzerwerfer)
		&& UPanzerwerferReloadBonusSubsystem::GetIsPanzerwerfer(*Panzerwerfer);
	Check(bIsPanzerwerfer, bIsLate ? TEXT("late_panzerwerfer_spawned") : TEXT("early_panzerwerfer_spawned"),
	      GetNameSafe(Panzerwerfer));
	if (bIsLate)
	{
		M_LatePanzerwerfer = Panzerwerfer;
		return;
	}
	M_EarlyPanzerwerfer = Panzerwerfer;
}

void USDBxpRoundTwoTestSubsystem::TickWaitingForSpawns()
{
	using namespace SDBxpRoundTwoTestPrivate;
	const bool bTrucks = M_Buildings[0].Truck.IsValid() && M_Buildings[1].Truck.IsValid();
	const float BaseReload = GetPanzerwerferReloadSpeed(M_EarlyPanzerwerfer.Get());
	if (not bTrucks || BaseReload <= 0.f)
	{
		if (GetStageElapsed() > SpawnTimeoutSeconds)
		{
			FailAndFinish(TEXT("trucks_and_early_panzerwerfer_spawned"),
			              FString::Printf(TEXT("depot=%s barracks=%s panzerwerfer=%s reload=%.2f"),
			                              *GetNameSafe(M_Buildings[0].Truck.Get()),
			                              *GetNameSafe(M_Buildings[1].Truck.Get()),
			                              *GetNameSafe(M_EarlyPanzerwerfer.Get()), BaseReload));
		}
		return;
	}
	M_BaseReloadSpeed = BaseReload;
	Check(true, TEXT("trucks_and_early_panzerwerfer_spawned"), FString::Printf(TEXT("base_reload=%.2f"), BaseReload));
	CheckPanzerwerferReload(M_EarlyPanzerwerfer.Get(), false, TEXT("before_shop"));
	UGameplayStatics::SetGlobalTimeDilation(this, TestTimeDilation);
	EnterStage(ESDBxpRoundTwoTestStage::WaitingForDeploy);
}

// ---------------------------------------------------------------------------------------------------------------
// Deploy and placement
// ---------------------------------------------------------------------------------------------------------------

bool USDBxpRoundTwoTestSubsystem::TickDeployAndGetIsBuilding(const int32 BuildingIndex)
{
	ANomadicVehicle* Truck = M_Buildings[BuildingIndex].Truck.Get();
	if (not IsValid(Truck))
	{
		return false;
	}
	if (Truck->GetNomadicStatus() == ENomadStatus::Building)
	{
		return true;
	}
	if (Truck->GetNomadicStatus() == ENomadStatus::Truck)
	{
		// The convert order of the command card, on the truck's own spot; repeated until the truck accepts it.
		const ECommandQueueError Error = Truck->CreateBuildingAtLocation(
			Truck->GetActorLocation(), FRotator(0.f, M_Buildings[BuildingIndex].Yaw, 0.f), true);
		bM_StageActionDone |= Error == ECommandQueueError::NoError;
		return false;
	}
	if (bM_StageActionDone && GetStageElapsed() > SDBxpRoundTwoTestPrivate::FastForwardDelaySeconds)
	{
		// Skips the truck rotation, the truck montage and the deploy animation, which wait on rendering headless.
		return Truck->ShippingTest_ForceFinishConversionToBuilding();
	}
	return false;
}

void USDBxpRoundTwoTestSubsystem::TickWaitingForDeploy()
{
	using namespace SDBxpRoundTwoTestPrivate;
	const bool bDepotBuilt = TickDeployAndGetIsBuilding(0);
	const bool bBarracksBuilt = TickDeployAndGetIsBuilding(1);
	if (not bDepotBuilt || not bBarracksBuilt)
	{
		if (GetStageElapsed() > DeployTimeoutSeconds)
		{
			FailAndFinish(TEXT("buildings_deployed"), FString::Printf(TEXT("depot=%d barracks=%d"),
			                                                           bDepotBuilt ? 1 : 0, bBarracksBuilt ? 1 : 0));
		}
		return;
	}
	Check(true, TEXT("buildings_deployed"));
	if (not PickSlotsForBuilding(0) || not PickSlotsForBuilding(1))
	{
		Finish();
		return;
	}
	M_PlacingBuilding = 0;
	M_PlacingBxp = 0;
	StartPlacingNextBxp();
}

bool USDBxpRoundTwoTestSubsystem::PickSlotsForBuilding(const int32 BuildingIndex)
{
	using namespace SDBxpRoundTwoTestPrivate;
	FSDBxpRoundTwoTestBuilding& Building = M_Buildings[BuildingIndex];
	const ANomadicVehicle* Truck = Building.Truck.Get();
	const FString Name = UEnum::GetValueAsString(Building.Subtype);
	const int32 SlotCount = Truck->GetBuildingExpansions().Num();
	Check(SlotCount >= RequiredSlotCount, Name + TEXT("_has_three_slots"), FString::Printf(TEXT("slots=%d"), SlotCount));
	const TArray<FBxpOptionData> Options = Truck->GetUnlockedBuildingExpansionTypes();
	Building.SlotIndices.Reset();
	for (const EBuildingExpansionType Type : Building.BxpTypes)
	{
		const bool bOffered = Options.ContainsByPredicate([Type](const FBxpOptionData& Option)
		{
			return Option.ExpansionType == Type
				&& Option.BxpConstructionRules.ConstructionType == EBxpConstructionType::AtBuildingOrigin;
		});
		Check(bOffered, Name + TEXT("_offers_origin_option"), UEnum::GetValueAsString(Type));
		if (not bOffered)
		{
			return false;
		}
		// Slot 0 stays empty, as in the pack-up test, so a slot index that skips empty slots shows.
		Building.SlotIndices.Add(Building.SlotIndices.Num() + 1);
	}
	return SlotCount >= RequiredSlotCount;
}

void USDBxpRoundTwoTestSubsystem::StartPlacingNextBxp()
{
	if (M_PlacingBxp >= M_Buildings[M_PlacingBuilding].BxpTypes.Num())
	{
		++M_PlacingBuilding;
		M_PlacingBxp = 0;
	}
	if (not M_Buildings.IsValidIndex(M_PlacingBuilding))
	{
		EnterStage(ESDBxpRoundTwoTestStage::WaitingForBxpsBuilt);
		return;
	}
	ACPPController* Controller = GetPlayerController();
	TScriptInterface<IBuildingExpansionOwner> Owner;
	if (not IsValid(Controller) || not GetBxpOwner(M_PlacingBuilding, Owner))
	{
		FailAndFinish(TEXT("controller_and_owner_valid_for_option_click"),
		              FString::Printf(TEXT("building=%d truck=%s"), M_PlacingBuilding,
		                              *GetNameSafe(M_Buildings[M_PlacingBuilding].Truck.Get())));
		return;
	}
	const FSDBxpRoundTwoTestBuilding& Building = M_Buildings[M_PlacingBuilding];
	const EBuildingExpansionType Type = Building.BxpTypes[M_PlacingBxp];
	const TArray<FBxpOptionData> Options = Building.Truck->GetUnlockedBuildingExpansionTypes();
	const FBxpOptionData* Option = Options.FindByPredicate([Type](const FBxpOptionData& Each)
	{
		return Each.ExpansionType == Type;
	});
	if (Option == nullptr)
	{
		FailAndFinish(TEXT("option_still_offered"), UEnum::GetValueAsString(Type));
		return;
	}
	// What the option button does when the player clicks it for this slot.
	Controller->ExpandBuildingWithBxpAndStartPreview(*Option, Owner, Building.SlotIndices[M_PlacingBxp], false);
	EnterStage(ESDBxpRoundTwoTestStage::WaitingForBxpLoaded);
}

void USDBxpRoundTwoTestSubsystem::TickWaitingForBxpLoaded()
{
	using namespace SDBxpRoundTwoTestPrivate;
	ACPPController* Controller = GetPlayerController();
	const EBuildingExpansionType Type = M_Buildings[M_PlacingBuilding].BxpTypes[M_PlacingBxp];
	if (not IsValid(Controller) || not Controller->MapTest_GetIsBxpWaitingForPlacement())
	{
		if (GetStageElapsed() > BxpLoadTimeoutSeconds)
		{
			FailAndFinish(TEXT("bxp_loaded_for_placement"), UEnum::GetValueAsString(Type));
		}
		return;
	}
	const UStaticMeshComponent* AttachMesh = M_Buildings[M_PlacingBuilding].Truck->GetAttachToMeshComponent();
	if (not IsValid(AttachMesh) || not IsValid(Controller->CPPConstructionPreviewRef))
	{
		FailAndFinish(TEXT("found_placement_click_location"), UEnum::GetValueAsString(Type));
		return;
	}
	// Origin expansions snap to the building origin; headless there is no cursor, so prime the preview there.
	FVector ClickLocation = AttachMesh->GetComponentLocation();
	Controller->CPPConstructionPreviewRef->ShippingTest_PrimeBxpPlacementAtLocation(ClickLocation);
	const bool bPlaced = Controller->MapTest_PlaceLoadedBxpAtLocation(ClickLocation);
	Check(bPlaced, TEXT("bxp_placed_by_click"), UEnum::GetValueAsString(Type));
	if (not bPlaced)
	{
		Finish();
		return;
	}
	++M_PlacingBxp;
	StartPlacingNextBxp();
}

bool USDBxpRoundTwoTestSubsystem::GetAreAllBxpsBuilt(const int32 BuildingIndex) const
{
	for (int32 BxpIndex = 0; BxpIndex < M_Buildings[BuildingIndex].BxpTypes.Num(); ++BxpIndex)
	{
		const ABuildingExpansion* Bxp = GetPlacedBxp(BuildingIndex, BxpIndex);
		if (not IsValid(Bxp) || Bxp->GetBuildingExpansionStatus() != EBuildingExpansionStatus::BXS_Built)
		{
			return false;
		}
	}
	return true;
}

void USDBxpRoundTwoTestSubsystem::TickWaitingForBxpsBuilt()
{
	using namespace SDBxpRoundTwoTestPrivate;
	if (not GetAreAllBxpsBuilt(0) || not GetAreAllBxpsBuilt(1))
	{
		if (GetStageElapsed() > BxpBuildTimeoutSeconds)
		{
			FailAndFinish(TEXT("bxps_built"));
		}
		return;
	}
	Check(true, TEXT("bxps_built"));
	CheckBuiltBxps();
	FVector LateGround = M_Anchor + LatePanzerwerferOffset;
	GlobalAbilityHelpers::TryFindGroundLocation(this, LateGround, LateGround);
	SpawnPanzerwerfer(LateGround, true);
	EnterStage(ESDBxpRoundTwoTestStage::WaitingForLatePanzerwerfer);
}

// ---------------------------------------------------------------------------------------------------------------
// Checks on the built expansions
// ---------------------------------------------------------------------------------------------------------------

void USDBxpRoundTwoTestSubsystem::CheckBuiltBxps()
{
	using namespace DeveloperSettings::GameBalance::VisionRadii::BxpVision;
	ABuildingExpansion* RadioVan = GetPlacedBxp(0, 0);
	ABuildingExpansion* WurfrahmenShop = GetPlacedBxp(0, 1);
	ABuildingExpansion* SniperLoft = GetPlacedBxp(1, 0);
	if (not IsValid(RadioVan) || not IsValid(WurfrahmenShop) || not IsValid(SniperLoft))
	{
		FailAndFinish(TEXT("all_three_bxps_valid"));
		return;
	}
	Check(RadioVan->GetBuildingExpansionType() == EBuildingExpansionType::BTX_GerMechDepotRadioVan
	      && WurfrahmenShop->GetBuildingExpansionType() == EBuildingExpansionType::BTX_GerMechDepotWurfrahmenShop
	      && SniperLoft->GetBuildingExpansionType() == EBuildingExpansionType::BTX_GerBarracksSniperLoft,
	      TEXT("bxp_types_match_blueprints"),
	      UEnum::GetValueAsString(RadioVan->GetBuildingExpansionType()) + TEXT(" ")
	      + UEnum::GetValueAsString(WurfrahmenShop->GetBuildingExpansionType()) + TEXT(" ")
	      + UEnum::GetValueAsString(SniperLoft->GetBuildingExpansionType()));
	CheckVision(*RadioVan, GerMechDepotRadioVanVisionRadius);
	CheckVision(*SniperLoft, GerBarracksSniperLoftVisionRadius);
	const UPanzerwerferReloadBonusSubsystem* BonusSubsystem =
		GetWorld()->GetSubsystem<UPanzerwerferReloadBonusSubsystem>();
	Check(IsValid(BonusSubsystem) && BonusSubsystem->GetIsBonusActiveForPlayer(1), TEXT("reload_bonus_active_with_shop"));
	CheckPanzerwerferReload(M_EarlyPanzerwerfer.Get(), true, TEXT("existing_with_shop"));
	CheckHoles(*RadioVan);
	CheckHoles(*WurfrahmenShop);
	CheckHoles(*SniperLoft);
}

void USDBxpRoundTwoTestSubsystem::CheckVision(const ABuildingExpansion& Bxp, const float ExpectedVisionRadius)
{
	const UFowComp* FowComponent = Bxp.FindComponentByClass<UFowComp>();
	const float VisionRadius = IsValid(FowComponent) ? FowComponent->GetVisionRadius() : 0.f;
	const bool bActive = IsValid(FowComponent) && FowComponent->GetFowBehaviour() == EFowBehaviour::Fow_Active;
	Check(FMath::IsNearlyEqual(VisionRadius, ExpectedVisionRadius) && bActive,
	      UEnum::GetValueAsString(Bxp.GetBuildingExpansionType()) + TEXT("_vision_radius_active"),
	      FString::Printf(TEXT("radius=%.0f expected=%.0f active=%d"), VisionRadius, ExpectedVisionRadius,
	                      bActive ? 1 : 0));
}

void USDBxpRoundTwoTestSubsystem::CheckHoles(ABuildingExpansion& Bxp)
{
	using namespace SDBxpRoundTwoTestPrivate;
	const FString Name = UEnum::GetValueAsString(Bxp.GetBuildingExpansionType());
	UWallHoleDamageComponent* Holes = Bxp.FindComponentByClass<UWallHoleDamageComponent>();
	UStaticMeshComponent* WallMesh = IsValid(Holes) ? Holes->GetWallMeshComponent() : nullptr;
	Check(IsValid(WallMesh), Name + TEXT("_has_wall_hole_component"));
	if (not IsValid(WallMesh))
	{
		return;
	}
	TArray<FVector> LocalAimPoints;
	Bxp.GetAimOffsetPoints(LocalAimPoints);
	const FVector Centre = WallMesh->Bounds.Origin;
	int32 Hits = 0;
	float RemovedArea = 0.f;
	for (const FVector& LocalAimPoint : LocalAimPoints)
	{
		const FVector AimPoint = Bxp.GetActorTransform().TransformPosition(LocalAimPoint);
		const FVector Outward = (AimPoint - Centre).GetSafeNormal2D();
		const FVector ShotStart = AimPoint + Outward * TestHitStandOff;
		const FVector ShotEnd = ShotStart - Outward * TestHitReach;
		FHitResult Hit;
		if (not WallMesh->LineTraceComponent(Hit, ShotStart, ShotEnd, FCollisionQueryParams(NAME_None, true)))
		{
			UE_LOG(LogRTSSDBxp2Test, Display, TEXT("RTS_SDBXP2_TEST HOLE_SHOT %s aim=%s missed collision"), *Name,
			       *LocalAimPoint.ToCompactString());
			continue;
		}
		++Hits;
		const float Removed = Holes->Debug_FireTestHit(Hit.Location, -Outward, TestHitCalibreMm);
		RemovedArea += Removed;
		UE_LOG(LogRTSSDBxp2Test, Display, TEXT("RTS_SDBXP2_TEST HOLE_SHOT %s aim=%s hit=%s removed=%.3f holes=%d"),
		       *Name, *LocalAimPoint.ToCompactString(),
		       *Bxp.GetActorTransform().InverseTransformPosition(Hit.Location).ToCompactString(), Removed,
		       Holes->GetHoleCount());
	}
	FString SlotMaterials;
	for (int32 Slot = 0; Slot < WallMesh->GetNumMaterials(); ++Slot)
	{
		SlotMaterials += GetNameSafe(WallMesh->GetMaterial(Slot)) + TEXT(" ");
	}
	Check(LocalAimPoints.Num() >= 3 && Hits > 0 && Holes->GetHoleCount() > 0, Name + TEXT("_shell_holes_cut"),
	      FString::Printf(TEXT("aim_points=%d hits=%d holes=%d removed_m2=%.3f materials=%s"), LocalAimPoints.Num(),
	                      Hits, Holes->GetHoleCount(), RemovedArea, *SlotMaterials));
}

float USDBxpRoundTwoTestSubsystem::GetPanzerwerferReloadSpeed(const ATankMaster* Panzerwerfer) const
{
	if (not IsValid(Panzerwerfer))
	{
		return 0.f;
	}
	for (const UWeaponState* Weapon : FRTSWeaponHelpers::GetWeaponsMountedOnTank(Panzerwerfer))
	{
		const EWeaponName WeaponName = IsValid(Weapon) ? Weapon->GetRawWeaponData().WeaponName : EWeaponName::DEFAULT_WEAPON;
		if (WeaponName == EWeaponName::Panzerwerfer || WeaponName == EWeaponName::Panzerwerfer_Small)
		{
			return Weapon->GetRawWeaponData().ReloadSpeed;
		}
	}
	return 0.f;
}

void USDBxpRoundTwoTestSubsystem::CheckPanzerwerferReload(const ATankMaster* Panzerwerfer, const bool bExpectBonus,
                                                          const FString& Moment)
{
	using namespace SDBxpRoundTwoTestPrivate;
	using DeveloperSettings::GameBalance::UnitCosts::Bxps::GerMechDepotWurfrahmenShopPanzerwerferReloadMlt;
	const float Reload = GetPanzerwerferReloadSpeed(Panzerwerfer);
	const float Expected = bExpectBonus
		                       ? M_BaseReloadSpeed * GerMechDepotWurfrahmenShopPanzerwerferReloadMlt
		                       : M_BaseReloadSpeed;
	const UBehaviourComp* BehaviourComponent = IsValid(Panzerwerfer)
		                                           ? Panzerwerfer->FindComponentByClass<UBehaviourComp>()
		                                           : nullptr;
	const bool bHasBehaviour = IsValid(BehaviourComponent)
		&& IsValid(BehaviourComponent->GetBehaviourByClass(UBehaviourWeaponWurfrahmenShop::StaticClass()));
	Check(FMath::IsNearlyEqual(Reload, Expected, ReloadTolerance) && bHasBehaviour == bExpectBonus,
	      TEXT("panzerwerfer_reload_") + Moment,
	      FString::Printf(TEXT("reload=%.2f expected=%.2f base=%.2f behaviour=%d"), Reload, Expected,
	                      M_BaseReloadSpeed, bHasBehaviour ? 1 : 0));
}

void USDBxpRoundTwoTestSubsystem::TickWaitingForLatePanzerwerfer()
{
	using namespace SDBxpRoundTwoTestPrivate;
	using DeveloperSettings::GameBalance::UnitCosts::Bxps::GerMechDepotWurfrahmenShopPanzerwerferReloadMlt;
	const float LateReload = GetPanzerwerferReloadSpeed(M_LatePanzerwerfer.Get());
	const float Expected = M_BaseReloadSpeed * GerMechDepotWurfrahmenShopPanzerwerferReloadMlt;
	// The weapon behaviour reaches weapons a moment after BeginPlay.
	if (not FMath::IsNearlyEqual(LateReload, Expected, ReloadTolerance)
		&& GetStageElapsed() <= PanzerwerferBonusTimeoutSeconds)
	{
		return;
	}
	CheckPanzerwerferReload(M_LatePanzerwerfer.Get(), true, TEXT("spawned_after_shop"));
	CheckSniperTurretAndSpawnEnemy();
}

// ---------------------------------------------------------------------------------------------------------------
// Sniper loft
// ---------------------------------------------------------------------------------------------------------------

ACPPTurretsMaster* USDBxpRoundTwoTestSubsystem::GetSniperTurret() const
{
	const ABuildingExpansion* SniperLoft = GetPlacedBxp(1, 0);
	if (not IsValid(SniperLoft))
	{
		return nullptr;
	}
	const TArray<ACPPTurretsMaster*> Turrets = SniperLoft->GetTurrets();
	return Turrets.Num() > 0 ? Turrets[0] : nullptr;
}

void USDBxpRoundTwoTestSubsystem::CheckSniperTurretAndSpawnEnemy()
{
	using namespace SDBxpRoundTwoTestPrivate;
	ACPPTurretsMaster* SniperTurret = GetSniperTurret();
	const ABuildingExpansion* SniperLoft = GetPlacedBxp(1, 0);
	Check(IsValid(SniperTurret), TEXT("sniper_turret_registered"), GetNameSafe(SniperTurret));
	if (not IsValid(SniperTurret) || not IsValid(SniperLoft))
	{
		StartPackUp(0);
		return;
	}
	TArray<UWeaponState*> Weapons = SniperTurret->GetWeapons();
	const bool bKarSniper = Weapons.Num() == 1 && IsValid(Weapons[0])
		&& Weapons[0]->GetRawWeaponData().WeaponName == EWeaponName::Kar_Sniper;
	Check(bKarSniper, TEXT("sniper_turret_has_kar_sniper"), FString::Printf(TEXT("weapons=%d"), Weapons.Num()));
	const FVector LoftDeck = SniperTurret->GetActorLocation();
	const FVector Front = M_Buildings[1].Location + SniperLoft->GetActorRotation().RotateVector(EnemyOffsetFromBarracks);
	const FVector Behind = LoftDeck + SniperLoft->GetActorRotation().RotateVector(BehindLoftOffset);
	Check(SniperTurret->GetCanTraverseToLocation(Front) && not SniperTurret->GetCanTraverseToLocation(Behind),
	      TEXT("sniper_arc_front_only"), FString::Printf(TEXT("deck=%s"), *LoftDeck.ToCompactString()));
	FVector EnemyGround = Front;
	GlobalAbilityHelpers::TryFindGroundLocation(this, Front, EnemyGround);
	const FRotator FacingLoft = (LoftDeck - EnemyGround).GetSafeNormal2D().Rotation();
	M_Enemy = SpawnUnitForPlayer(EnemyClassPath, FTransform(FacingLoft, EnemyGround), EnemyPlayer);
	Check(M_Enemy.IsValid(), TEXT("enemy_tank_spawned_in_front"), EnemyGround.ToCompactString());
	M_SniperShots = 0;
	M_LastSniperMagazine = INDEX_NONE;
	EnterStage(ESDBxpRoundTwoTestStage::WaitingForSniperShots);
}

AActor* USDBxpRoundTwoTestSubsystem::SpawnUnitForPlayer(const TCHAR* ClassPath, const FTransform& Transform,
                                                        const uint8 OwningPlayer) const
{
	using namespace SDBxpRoundTwoTestPrivate;
	UWorld* World = GetWorld();
	UClass* UnitClass = LoadClass<AActor>(nullptr, ClassPath);
	if (not IsValid(World) || not IsValid(UnitClass))
	{
		return nullptr;
	}
	// The owner must be in place before BeginPlay, so the unit registers (and is targeted) as that player's.
	TArray<TPair<TWeakObjectPtr<UObject>, uint8>> TemplateOwnersToRestore;
	SetRTSComponentTemplateOwners(UnitClass, OwningPlayer, TemplateOwnersToRestore);
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

void USDBxpRoundTwoTestSubsystem::CountSniperShots(ACPPTurretsMaster& SniperTurret)
{
	TArray<UWeaponState*> Weapons = SniperTurret.GetWeapons();
	if (Weapons.Num() == 0 || not IsValid(Weapons[0]))
	{
		return;
	}
	const int32 Magazine = Weapons[0]->GetCurrentMagCapacity();
	if (M_LastSniperMagazine != INDEX_NONE)
	{
		M_SniperShots += FMath::Max(0, M_LastSniperMagazine - Magazine);
	}
	M_LastSniperMagazine = Magazine;
}

void USDBxpRoundTwoTestSubsystem::TickWaitingForSniperShots()
{
	using namespace SDBxpRoundTwoTestPrivate;
	ACPPTurretsMaster* SniperTurret = GetSniperTurret();
	if (IsValid(SniperTurret))
	{
		CountSniperShots(*SniperTurret);
	}
	// Keep the target alive: one sniper rifle should not decide this check by killing the tank between ticks.
	if (AActor* Enemy = M_Enemy.Get())
	{
		if (UHealthComponent* Health = Enemy->FindComponentByClass<UHealthComponent>())
		{
			Health->SetCurrentHealth(Health->GetMaxHealth());
		}
	}
	const bool bTargeted = IsValid(SniperTurret) && M_Enemy.IsValid()
		&& SniperTurret->GetCurrentTargetActor() == M_Enemy.Get();
	if (M_SniperShots == 0 && GetStageElapsed() <= SniperFireTimeoutSeconds)
	{
		return;
	}
	Check(M_SniperShots > 0, TEXT("sniper_fired_at_enemy"),
	      FString::Printf(TEXT("shots=%d targeted_now=%d"), M_SniperShots, bTargeted ? 1 : 0));
	if (AActor* Enemy = M_Enemy.Get())
	{
		Enemy->Destroy();
	}
	StartPackUp(0);
}

// ---------------------------------------------------------------------------------------------------------------
// Pack-up and redeploy, one building after the other
// ---------------------------------------------------------------------------------------------------------------

void USDBxpRoundTwoTestSubsystem::StartPackUp(const int32 BuildingIndex)
{
	M_PackingBuilding = BuildingIndex;
	ACPPController* Controller = GetPlayerController();
	ANomadicVehicle* Truck = M_Buildings[BuildingIndex].Truck.Get();
	if (not IsValid(Controller) || not IsValid(Truck))
	{
		FailAndFinish(TEXT("controller_and_truck_valid_for_pack_up"));
		return;
	}
	Controller->ConvertBackToVehicle(Truck);
	EnterStage(ESDBxpRoundTwoTestStage::WaitingForPackUp);
}

void USDBxpRoundTwoTestSubsystem::TickWaitingForPackUp()
{
	using namespace SDBxpRoundTwoTestPrivate;
	const ANomadicVehicle* Truck = M_Buildings[M_PackingBuilding].Truck.Get();
	const FString Name = UEnum::GetValueAsString(M_Buildings[M_PackingBuilding].Subtype);
	if (not IsValid(Truck) || Truck->GetNomadicStatus() != ENomadStatus::Truck)
	{
		if (GetStageElapsed() > PackUpTimeoutSeconds)
		{
			FailAndFinish(Name + TEXT("_packed_into_truck"));
		}
		return;
	}
	Check(true, Name + TEXT("_packed_into_truck"));
	for (int32 BxpIndex = 0; BxpIndex < M_Buildings[M_PackingBuilding].SlotIndices.Num(); ++BxpIndex)
	{
		const FBuildingExpansionItem* Item = Truck->GetBuildingExpansionItemAtIndex(
			M_Buildings[M_PackingBuilding].SlotIndices[BxpIndex]);
		Check(Item != nullptr && Item->ExpansionStatus == EBuildingExpansionStatus::BXS_PackedUp
		      && Item->ExpansionType == M_Buildings[M_PackingBuilding].BxpTypes[BxpIndex],
		      Name + TEXT("_slot_packed"), UEnum::GetValueAsString(M_Buildings[M_PackingBuilding].BxpTypes[BxpIndex]));
	}
	if (M_PackingBuilding == 0)
	{
		const UPanzerwerferReloadBonusSubsystem* BonusSubsystem =
			GetWorld()->GetSubsystem<UPanzerwerferReloadBonusSubsystem>();
		Check(IsValid(BonusSubsystem) && not BonusSubsystem->GetIsBonusActiveForPlayer(1),
		      TEXT("reload_bonus_gone_after_pack_up"));
		CheckPanzerwerferReload(M_EarlyPanzerwerfer.Get(), false, TEXT("existing_after_pack_up"));
		CheckPanzerwerferReload(M_LatePanzerwerfer.Get(), false, TEXT("late_after_pack_up"));
	}
	EnterStage(ESDBxpRoundTwoTestStage::WaitingForRedeploy);
}

void USDBxpRoundTwoTestSubsystem::TickWaitingForRedeploy()
{
	using namespace SDBxpRoundTwoTestPrivate;
	if (not TickDeployAndGetIsBuilding(M_PackingBuilding))
	{
		if (GetStageElapsed() > DeployTimeoutSeconds)
		{
			FailAndFinish(TEXT("redeploy_finished"), UEnum::GetValueAsString(M_Buildings[M_PackingBuilding].Subtype));
		}
		return;
	}
	Check(true, TEXT("redeploy_finished"), UEnum::GetValueAsString(M_Buildings[M_PackingBuilding].Subtype));
	EnterStage(ESDBxpRoundTwoTestStage::WaitingForReplacedBxps);
}

void USDBxpRoundTwoTestSubsystem::TickWaitingForReplacedBxps()
{
	using namespace SDBxpRoundTwoTestPrivate;
	const ANomadicVehicle* Truck = M_Buildings[M_PackingBuilding].Truck.Get();
	const FString Name = UEnum::GetValueAsString(M_Buildings[M_PackingBuilding].Subtype);
	const bool bBatchLoading = IsValid(Truck) && Truck->GetIsAsyncBatchLoadingAttachedPackedExpansions();
	if ((bBatchLoading || not GetAreAllBxpsBuilt(M_PackingBuilding)) && GetStageElapsed() <= ReplacedBxpTimeoutSeconds)
	{
		return;
	}
	Check(GetAreAllBxpsBuilt(M_PackingBuilding), Name + TEXT("_bxps_back_and_built_after_redeploy"));
	for (int32 BxpIndex = 0; BxpIndex < M_Buildings[M_PackingBuilding].BxpTypes.Num(); ++BxpIndex)
	{
		const ABuildingExpansion* Bxp = GetPlacedBxp(M_PackingBuilding, BxpIndex);
		Check(IsValid(Bxp) && Bxp->GetBuildingExpansionType() == M_Buildings[M_PackingBuilding].BxpTypes[BxpIndex],
		      Name + TEXT("_same_bxp_in_same_slot"),
		      UEnum::GetValueAsString(M_Buildings[M_PackingBuilding].BxpTypes[BxpIndex]));
	}
	if (M_PackingBuilding == 0)
	{
		CheckPanzerwerferReload(M_EarlyPanzerwerfer.Get(), true, TEXT("existing_after_redeploy"));
		StartPackUp(1);
		return;
	}
	Check(IsValid(GetSniperTurret()), TEXT("sniper_turret_back_after_redeploy"));
	Finish();
}

#else

bool USDBxpRoundTwoTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void USDBxpRoundTwoTestSubsystem::Tick(const float DeltaTime)
{
}

TStatId USDBxpRoundTwoTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USDBxpRoundTwoTestSubsystem, STATGROUP_Tickables);
}

#endif
