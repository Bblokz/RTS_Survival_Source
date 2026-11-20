// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.
#include "HqLaserBxpTestSubsystem.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS
#include "Camera/PlayerCameraManager.h"
#include "Engine/StaticMeshSocket.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansionEnums.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BXPConstructionRules/BXPConstructionRules.h"
#include "RTS_Survival/Buildings/BuildingExpansion/Interface/BuildingExpansionOwner.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Environment/DestructableEnvActor/WallChunkDamage/WallHoleDamageComponent.h"
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
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSHqLaserTest, Log, All);

namespace HqLaserBxpTestPrivate
{
	constexpr float StartDelaySeconds = 4.f;
	constexpr float CameraTraceLength = 500000.f;
	// Speeds up the expansion's construction; the deploy is fast-forwarded.
	constexpr float TestTimeDilation = 10.f;
	constexpr float FastForwardDelaySeconds = 1.f;
	constexpr float TruckSpawnTimeoutSeconds = 30.f;
	constexpr float DeployTimeoutSeconds = 60.f;
	constexpr float BxpLoadTimeoutSeconds = 30.f;
	constexpr float BxpBuildTimeoutSeconds = 90.f;
	constexpr float LaserDamageTimeoutSeconds = 60.f;
	constexpr int32 TestResourceAmount = 1000000;
	// Slot 0 stays empty, as in the pack-up test, so a slot index that skips empty slots shows.
	constexpr int32 LaserSlotIndex = 1;
	constexpr uint8 PlayerIndex = 1;
	constexpr uint8 EnemyPlayer = 2;
	// In front of the HQ, inside the laser's (doubled) range.
	const FVector EnemyOffsetFromHq = FVector(3000.f, 0.f, 0.f);
	const TCHAR* const EnemyClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/Rus_Vehicle_T3476.Rus_Vehicle_T3476_C");
}

bool UHqLaserBxpTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), TEXT("HqLaserBxpValidate"));
}

TStatId UHqLaserBxpTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UHqLaserBxpTestSubsystem, STATGROUP_Tickables);
}

void UHqLaserBxpTestSubsystem::Tick(const float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	switch (M_Stage)
	{
	case EHqLaserBxpTestStage::WaitingForStart:
		TickWaitingForStart();
		return;
	case EHqLaserBxpTestStage::WaitingForTruckSpawn:
		TickWaitingForTruckSpawn();
		return;
	case EHqLaserBxpTestStage::WaitingForDeploy:
		TickWaitingForDeploy();
		return;
	case EHqLaserBxpTestStage::WaitingForBxpLoaded:
		TickWaitingForBxpLoaded();
		return;
	case EHqLaserBxpTestStage::WaitingForBxpBuilt:
		TickWaitingForBxpBuilt();
		return;
	case EHqLaserBxpTestStage::WaitingForLaserDamage:
		TickWaitingForLaserDamage();
		return;
	case EHqLaserBxpTestStage::Finished:
		return;
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Start, HQ truck and deploy
// ---------------------------------------------------------------------------------------------------------------

void UHqLaserBxpTestSubsystem::TickWaitingForStart()
{
	using namespace HqLaserBxpTestPrivate;
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
	UE_LOG(LogRTSHqLaserTest, Display, TEXT("RTS_HQLASER_TEST START anchor=%s"), *M_Anchor.ToCompactString());
	GivePlayerResources();
	SpawnHqTruck();
}

void UHqLaserBxpTestSubsystem::SpawnHqTruck()
{
	ARTSAsyncSpawner* Spawner = FRTS_Statics::GetAsyncSpawner(this);
	if (not IsValid(Spawner))
	{
		FailAndFinish(TEXT("async_spawner_found"));
		return;
	}
	TWeakObjectPtr<UHqLaserBxpTestSubsystem> WeakThis(this);
	const bool bRequested = Spawner->AsyncSpawnOptionAtLocation(
		FTrainingOption(EAllUnitType::UNType_Nomadic, static_cast<uint8>(ENomadicSubtype::Nomadic_GerHq)),
		M_Anchor,
		this,
		0,
		[WeakThis](const FTrainingOption&, AActor* SpawnedActor, const int32)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->M_Hq = Cast<ANomadicVehicle>(SpawnedActor);
			}
		},
		FRotator::ZeroRotator);
	Check(bRequested, TEXT("hq_truck_spawn_requested"));
	if (not bRequested)
	{
		Finish();
		return;
	}
	EnterStage(EHqLaserBxpTestStage::WaitingForTruckSpawn);
}

void UHqLaserBxpTestSubsystem::TickWaitingForTruckSpawn()
{
	using namespace HqLaserBxpTestPrivate;
	if (M_Hq.IsValid())
	{
		Check(true, TEXT("hq_truck_spawned"), M_Hq->GetName());
		UGameplayStatics::SetGlobalTimeDilation(this, TestTimeDilation);
		EnterStage(EHqLaserBxpTestStage::WaitingForDeploy);
		return;
	}
	if (GetStageElapsed() > TruckSpawnTimeoutSeconds)
	{
		FailAndFinish(TEXT("hq_truck_spawned"));
	}
}

void UHqLaserBxpTestSubsystem::TickWaitingForDeploy()
{
	using namespace HqLaserBxpTestPrivate;
	ANomadicVehicle* Hq = M_Hq.Get();
	if (not IsValid(Hq))
	{
		FailAndFinish(TEXT("hq_alive_while_deploying"));
		return;
	}
	if (Hq->GetNomadicStatus() == ENomadStatus::Building)
	{
		Check(true, TEXT("hq_deployed"));
		ClickLaserOption();
		return;
	}
	if (not bM_StageActionDone && Hq->GetNomadicStatus() == ENomadStatus::Truck)
	{
		// The convert order of the command card, on the truck's own spot.
		const ECommandQueueError Error = Hq->CreateBuildingAtLocation(Hq->GetActorLocation(), FRotator::ZeroRotator,
		                                                              true);
		bM_StageActionDone = Error == ECommandQueueError::NoError;
		return;
	}
	if (bM_StageActionDone && GetStageElapsed() > FastForwardDelaySeconds)
	{
		// Skips the truck rotation, the truck montage and the deploy animation, which wait on rendering headless.
		Hq->ShippingTest_ForceFinishConversionToBuilding();
	}
	if (GetStageElapsed() > DeployTimeoutSeconds)
	{
		FailAndFinish(TEXT("hq_deployed"));
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Option click, placement and construction
// ---------------------------------------------------------------------------------------------------------------

void UHqLaserBxpTestSubsystem::ClickLaserOption()
{
	using namespace HqLaserBxpTestPrivate;
	ANomadicVehicle* Hq = M_Hq.Get();
	ACPPController* Controller = GetPlayerController();
	const TArray<FBxpOptionData> Options = Hq->GetUnlockedBuildingExpansionTypes();
	const FBxpOptionData* LaserOption = Options.FindByPredicate([](const FBxpOptionData& Option)
	{
		return Option.ExpansionType == EBuildingExpansionType::BTX_GerHQLaser;
	});
	Check(LaserOption != nullptr, TEXT("hq_offers_laser_platform"), FString::Printf(TEXT("options=%d"), Options.Num()));
	if (LaserOption == nullptr || not IsValid(Controller))
	{
		Finish();
		return;
	}
	Check(LaserOption->BxpConstructionRules.ConstructionType == EBxpConstructionType::Socket,
	      TEXT("laser_is_socket_expansion"),
	      Global_GetBxpConstructionTypeEnumAsString(LaserOption->BxpConstructionRules.ConstructionType));
	const int32 SlotCount = Hq->GetBuildingExpansions().Num();
	Check(SlotCount > LaserSlotIndex, TEXT("hq_has_free_slot"), FString::Printf(TEXT("slots=%d"), SlotCount));
	TScriptInterface<IBuildingExpansionOwner> Owner;
	Owner.SetObject(Hq);
	Owner.SetInterface(static_cast<IBuildingExpansionOwner*>(Hq));
	M_RadixiteBeforeClick = GetResource(ERTSResourceType::Resource_Radixite);
	M_MetalBeforeClick = GetResource(ERTSResourceType::Resource_Metal);
	// What the option button does when the player clicks it for this slot.
	Controller->ExpandBuildingWithBxpAndStartPreview(*LaserOption, Owner, LaserSlotIndex, false);
	EnterStage(EHqLaserBxpTestStage::WaitingForBxpLoaded);
}

void UHqLaserBxpTestSubsystem::TickWaitingForBxpLoaded()
{
	using namespace HqLaserBxpTestPrivate;
	ACPPController* Controller = GetPlayerController();
	if (not IsValid(Controller) || not Controller->MapTest_GetIsBxpWaitingForPlacement())
	{
		if (GetStageElapsed() > BxpLoadTimeoutSeconds)
		{
			FailAndFinish(TEXT("laser_loaded_for_placement"));
		}
		return;
	}
	const ANomadicVehicle* Hq = M_Hq.Get();
	const UStaticMeshComponent* AttachMesh = IsValid(Hq) ? Hq->GetAttachToMeshComponent() : nullptr;
	const TArray<UStaticMeshSocket*> FreeSockets = IsValid(Hq) ? Hq->GetFreeSocketList(LaserSlotIndex)
		                                              : TArray<UStaticMeshSocket*>();
	if (not IsValid(AttachMesh) || FreeSockets.IsEmpty() || FreeSockets[0] == nullptr
		|| not IsValid(Controller->CPPConstructionPreviewRef))
	{
		FailAndFinish(TEXT("found_free_hq_socket"), FString::Printf(TEXT("free=%d"), FreeSockets.Num()));
		return;
	}
	FVector ClickLocation = AttachMesh->GetSocketLocation(FreeSockets[0]->SocketName);
	// The preview tick snaps to the socket under the cursor; headless there is no cursor, so prime it.
	Controller->CPPConstructionPreviewRef->ShippingTest_PrimeBxpPlacementAtLocation(ClickLocation);
	const bool bPlaced = Controller->MapTest_PlaceLoadedBxpAtLocation(ClickLocation);
	Check(bPlaced, TEXT("laser_placed_on_socket"), FreeSockets[0]->SocketName.ToString());
	if (not bPlaced)
	{
		Finish();
		return;
	}
	EnterStage(EHqLaserBxpTestStage::WaitingForBxpBuilt);
}

void UHqLaserBxpTestSubsystem::TickWaitingForBxpBuilt()
{
	using namespace HqLaserBxpTestPrivate;
	ABuildingExpansion* Laser = GetLaser();
	if (not IsValid(Laser) || Laser->GetBuildingExpansionStatus() != EBuildingExpansionStatus::BXS_Built)
	{
		if (GetStageElapsed() > BxpBuildTimeoutSeconds)
		{
			FailAndFinish(TEXT("laser_built"), GetNameSafe(Laser));
		}
		return;
	}
	Check(true, TEXT("laser_built"), Laser->GetName());
	CheckBuiltLaser(*Laser);
	SpawnEnemyInFront(*Laser);
}

// ---------------------------------------------------------------------------------------------------------------
// Checks on the built laser
// ---------------------------------------------------------------------------------------------------------------

void UHqLaserBxpTestSubsystem::CheckBuiltLaser(ABuildingExpansion& Laser)
{
	using namespace DeveloperSettings::GameBalance::UnitCosts::Bxps;
	const int32 RadixitePaid = M_RadixiteBeforeClick - GetResource(ERTSResourceType::Resource_Radixite);
	const int32 MetalPaid = M_MetalBeforeClick - GetResource(ERTSResourceType::Resource_Metal);
	Check(RadixitePaid == GerHQLaser_Radixite && MetalPaid == GerHQLaser_Metal, TEXT("laser_price_taken"),
	      FString::Printf(TEXT("radixite=%d metal=%d"), RadixitePaid, MetalPaid));
	Check(Laser.GetBuildingExpansionType() == EBuildingExpansionType::BTX_GerHQLaser, TEXT("laser_type_from_blueprint"),
	      UEnum::GetValueAsString(Laser.GetBuildingExpansionType()));
	Check(IsValid(Laser.FindComponentByClass<UWallHoleDamageComponent>()), TEXT("laser_has_procedural_holes"));
	ACPPTurretsMaster* Turret = GetLaserTurret();
	TArray<UWeaponState*> Weapons = IsValid(Turret) ? Turret->GetWeapons() : TArray<UWeaponState*>();
	const bool bStrahlkanone = Weapons.Num() > 0 && IsValid(Weapons[0])
		&& Weapons[0]->GetRawWeaponData().WeaponName == EWeaponName::Strahlkanone39;
	Check(bStrahlkanone, TEXT("laser_turret_has_strahlkanone39"),
	      FString::Printf(TEXT("turret=%s weapons=%d"), *GetNameSafe(Turret), Weapons.Num()));
}

void UHqLaserBxpTestSubsystem::SpawnEnemyInFront(const ABuildingExpansion& Laser)
{
	using namespace HqLaserBxpTestPrivate;
	UWorld* World = GetWorld();
	UClass* EnemyClass = LoadClass<AActor>(nullptr, EnemyClassPath);
	FVector EnemyGround = M_Anchor + EnemyOffsetFromHq;
	GlobalAbilityHelpers::TryFindGroundLocation(this, EnemyGround, EnemyGround);
	const FRotator FacingHq = (Laser.GetActorLocation() - EnemyGround).GetSafeNormal2D().Rotation();
	AActor* Enemy = IsValid(World) && IsValid(EnemyClass)
		                ? FRTSUnitOwnerSpawning::SpawnUnitForPlayer(*World, EnemyClass,
		                                                            FTransform(FacingHq, EnemyGround), EnemyPlayer)
		                : nullptr;
	Check(IsValid(Enemy), TEXT("enemy_tank_spawned_in_front"), EnemyGround.ToCompactString());
	const UHealthComponent* Health = IsValid(Enemy) ? Enemy->FindComponentByClass<UHealthComponent>() : nullptr;
	if (not IsValid(Health))
	{
		Finish();
		return;
	}
	M_Enemy = Enemy;
	M_EnemyStartHealth = Health->GetCurrentHealth();
	EnterStage(EHqLaserBxpTestStage::WaitingForLaserDamage);
}

void UHqLaserBxpTestSubsystem::TickWaitingForLaserDamage()
{
	using namespace HqLaserBxpTestPrivate;
	const AActor* Enemy = M_Enemy.Get();
	const UHealthComponent* Health = IsValid(Enemy) ? Enemy->FindComponentByClass<UHealthComponent>() : nullptr;
	const bool bDamaged = not IsValid(Health) || Health->GetCurrentHealth() < M_EnemyStartHealth;
	if (not bDamaged && GetStageElapsed() <= LaserDamageTimeoutSeconds)
	{
		return;
	}
	const ACPPTurretsMaster* Turret = GetLaserTurret();
	Check(bDamaged, TEXT("laser_damages_enemy_tank"),
	      FString::Printf(TEXT("health=%.0f start=%.0f targeted=%d"), IsValid(Health) ? Health->GetCurrentHealth() : 0.f,
	                      M_EnemyStartHealth,
	                      IsValid(Turret) && Enemy != nullptr && Turret->GetCurrentTargetActor() == Enemy ? 1 : 0));
	Finish();
}

ABuildingExpansion* UHqLaserBxpTestSubsystem::GetLaser() const
{
	const ANomadicVehicle* Hq = M_Hq.Get();
	return IsValid(Hq) ? Hq->GetBuildingExpansionAtIndex(HqLaserBxpTestPrivate::LaserSlotIndex) : nullptr;
}

ACPPTurretsMaster* UHqLaserBxpTestSubsystem::GetLaserTurret() const
{
	const ABuildingExpansion* Laser = GetLaser();
	if (not IsValid(Laser))
	{
		return nullptr;
	}
	const TArray<ACPPTurretsMaster*> Turrets = Laser->GetTurrets();
	return Turrets.Num() > 0 ? Turrets[0] : nullptr;
}

// ---------------------------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------------------------

bool UHqLaserBxpTestSubsystem::StartGameAndGetIsReady() const
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

bool UHqLaserBxpTestSubsystem::FindAnchor(FVector& OutAnchor) const
{
	using namespace HqLaserBxpTestPrivate;
	// FParse::Value stops at commas, so the anchor comes as two values.
	float AnchorX = 0.f;
	float AnchorY = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("HqLaserAnchorX="), AnchorX)
		&& FParse::Value(FCommandLine::Get(), TEXT("HqLaserAnchorY="), AnchorY))
	{
		return GlobalAbilityHelpers::TryFindGroundLocation(this, FVector(AnchorX, AnchorY, 0.f), OutAnchor);
	}
	const APlayerController* Controller = GetPlayerController();
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

void UHqLaserBxpTestSubsystem::GivePlayerResources() const
{
	using namespace HqLaserBxpTestPrivate;
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

int32 UHqLaserBxpTestSubsystem::GetResource(const ERTSResourceType ResourceType) const
{
	const ACPPController* Controller = GetPlayerController();
	const UPlayerResourceManager* Resources = IsValid(Controller) ? Controller->GetPlayerResourceManager() : nullptr;
	return IsValid(Resources) ? Resources->GetResourceAmount(ResourceType) : 0;
}

ACPPController* UHqLaserBxpTestSubsystem::GetPlayerController() const
{
	return FRTS_Statics::GetRTSController(GetWorld());
}

void UHqLaserBxpTestSubsystem::Check(const bool bCondition, const FString& Description, const FString& Details)
{
	if (bCondition)
	{
		++M_PassedCheckCount;
	}
	else
	{
		++M_FailedCheckCount;
	}
	UE_LOG(LogRTSHqLaserTest, Display, TEXT("RTS_HQLASER_TEST CHECK %s %s %s"), bCondition ? TEXT("PASS") : TEXT("FAIL"),
	       *Description, *Details);
}

void UHqLaserBxpTestSubsystem::EnterStage(const EHqLaserBxpTestStage NewStage)
{
	M_Stage = NewStage;
	M_StageSeconds = GetNow();
	bM_StageActionDone = false;
}

float UHqLaserBxpTestSubsystem::GetNow() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetRealTimeSeconds() : 0.f;
}

float UHqLaserBxpTestSubsystem::GetStageElapsed() const
{
	return GetNow() - M_StageSeconds;
}

void UHqLaserBxpTestSubsystem::FailAndFinish(const FString& Description, const FString& Details)
{
	Check(false, Description, Details);
	Finish();
}

void UHqLaserBxpTestSubsystem::Finish()
{
	const bool bPassed = M_FailedCheckCount == 0 && M_PassedCheckCount > 0;
	UE_LOG(LogRTSHqLaserTest, Display, TEXT("RTS_HQLASER_TEST RESULT %s passed=%d failed=%d"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"), M_PassedCheckCount, M_FailedCheckCount);
	EnterStage(EHqLaserBxpTestStage::Finished);
	UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
	if (APlayerController* Controller = GetPlayerController())
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}

#else

bool UHqLaserBxpTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UHqLaserBxpTestSubsystem::Tick(const float DeltaTime)
{
}

TStatId UHqLaserBxpTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UHqLaserBxpTestSubsystem, STATGROUP_Tickables);
}

#endif
