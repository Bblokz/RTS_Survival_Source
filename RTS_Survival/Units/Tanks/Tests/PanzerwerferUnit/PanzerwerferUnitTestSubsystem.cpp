// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "PanzerwerferUnitTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RTS_Survival/Behaviours/BehaviourComp.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BxpBonuses/PanzerwerferReloadBonus/BehaviourWeaponWurfrahmenShop.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BxpBonuses/PanzerwerferReloadBonus/PanzerwerferReloadBonusSubsystem.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainerComponent/TrainerComponent.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingLayout/TrainingLayoutDataAsset.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingLayout/TrainingLayoutSubsystem.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingLayout/TrainingLayoutTypes.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Missions/Tests/MissionMapTestLibrary.h"
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
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSPzwUnitTest, Log, All);

namespace PanzerwerferUnitTestPrivate
{
	constexpr float StartDelaySeconds = 4.f;
	constexpr float CameraTraceLength = 500000.f;
	// Speeds up training, driving and building; the deploy is fast-forwarded.
	constexpr float TestTimeDilation = 10.f;
	constexpr float FastForwardDelaySeconds = 1.f;
	constexpr float SpawnTimeoutSeconds = 40.f;
	constexpr float DeployTimeoutSeconds = 60.f;
	constexpr float TrainTimeoutSeconds = 60.f;
	constexpr float MoveTimeoutSeconds = 30.f;
	constexpr float ShopLoadTimeoutSeconds = 30.f;
	constexpr float ShopBuildTimeoutSeconds = 90.f;
	constexpr float RocketTimeoutSeconds = 60.f;
	constexpr float PackUpTimeoutSeconds = 60.f;
	constexpr float WreckTimeoutSeconds = 30.f;
	constexpr int32 TestResourceAmount = 1000000;
	constexpr int32 RequiredRockets = 3;
	constexpr float MoveDistance = 2500.f;
	constexpr float RequiredMoveDistance = 800.f;
	// Inside the 15 cm rocket's medium artillery range (7480).
	constexpr float EnemyDistance = 5000.f;
	constexpr float WreckSearchRadius = 1500.f;
	constexpr float KillDamage = 1000000.f;
	constexpr float ReloadTolerance = 0.05f;
	// The Panzerwerfer 42: ten 15 cm tubes.
	constexpr int32 ExpectedMagCapacity = 10;
	constexpr float ExpectedCalibreMm = 150.f;
	constexpr uint8 PlayerIndex = 1;
	constexpr uint8 EnemyPlayer = 2;
	const FVector Pz38tROffset = FVector(2600.f, -1500.f, 0.f);
	const TCHAR* const ExpectedUnitClassName = TEXT("Ger_Vehicle_Panzerwerfer_C");
	const TCHAR* const ExpectedWreckClassName = TEXT("Env_Dest_Panzerwerfer_C");
	const TCHAR* const Pz38tRClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Light/Panzer38T/Ger_Vehicle_Panzer38T_R.Ger_Vehicle_Panzer38T_R_C");
	// Unarmed (the Soviet fuel tanker): a target that can not one-shot the thin-skinned Panzerwerfer.
	const TCHAR* const EnemyClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_ArmoredCar/SovietTanker/Rus_ArmoredCar_SovietTanker.Rus_ArmoredCar_SovietTanker_C");

	FTrainingOption GetPanzerwerferOption()
	{
		return FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(ETankSubtype::Tank_Panzerwerfer));
	}

	void SetRTSComponentTemplateOwners(UClass* UnitClass, const uint8 OwningPlayer,
	                                   TArray<TPair<TWeakObjectPtr<UObject>, uint8>>& OutRestore)
	{
		const FByteProperty* OwnerProperty = FindFProperty<FByteProperty>(URTSComponent::StaticClass(),
		                                                                  TEXT("OwningPlayer"));
		if (OwnerProperty == nullptr)
		{
			return;
		}
		for (UClass* Class = UnitClass; Class != nullptr; Class = Class->GetSuperClass())
		{
			const UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(Class);
			if (BlueprintClass == nullptr || BlueprintClass->SimpleConstructionScript == nullptr)
			{
				continue;
			}
			for (const USCS_Node* Node : BlueprintClass->SimpleConstructionScript->GetAllNodes())
			{
				UActorComponent* Template = Node != nullptr ? Node->ComponentTemplate : nullptr;
				if (not IsValid(Template) || not Template->IsA<URTSComponent>())
				{
					continue;
				}
				uint8* Value = OwnerProperty->ContainerPtrToValuePtr<uint8>(Template);
				OutRestore.Add({Template, *Value});
				*Value = OwningPlayer;
			}
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

bool UPanzerwerferUnitTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), TEXT("PanzerwerferUnitValidate"));
}

TStatId UPanzerwerferUnitTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UPanzerwerferUnitTestSubsystem, STATGROUP_Tickables);
}

void UPanzerwerferUnitTestSubsystem::Tick(const float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	switch (M_Stage)
	{
	case EPanzerwerferUnitTestStage::WaitingForStart:
		TickWaitingForStart();
		return;
	case EPanzerwerferUnitTestStage::WaitingForSpawns:
		TickWaitingForSpawns();
		return;
	case EPanzerwerferUnitTestStage::WaitingForDeploy:
		TickWaitingForDeploy();
		return;
	case EPanzerwerferUnitTestStage::WaitingForTrainedUnit:
		TickWaitingForTrainedUnit();
		return;
	case EPanzerwerferUnitTestStage::WaitingForMove:
		TickWaitingForMove();
		return;
	case EPanzerwerferUnitTestStage::WaitingForShopLoaded:
		TickWaitingForShopLoaded();
		return;
	case EPanzerwerferUnitTestStage::WaitingForShopBuilt:
		TickWaitingForShopBuilt();
		return;
	case EPanzerwerferUnitTestStage::WaitingForRockets:
		TickWaitingForRockets();
		return;
	case EPanzerwerferUnitTestStage::WaitingForPackUp:
		TickWaitingForPackUp();
		return;
	case EPanzerwerferUnitTestStage::WaitingForWreck:
		TickWaitingForWreck();
		return;
	case EPanzerwerferUnitTestStage::Finished:
		return;
	}
}

void UPanzerwerferUnitTestSubsystem::Check(const bool bCondition, const FString& Description, const FString& Details)
{
	if (bCondition)
	{
		++M_PassedCheckCount;
		UE_LOG(LogRTSPzwUnitTest, Display, TEXT("RTS_PZW_UNIT_TEST CHECK PASS %s %s"), *Description, *Details);
		return;
	}
	++M_FailedCheckCount;
	UE_LOG(LogRTSPzwUnitTest, Error, TEXT("RTS_PZW_UNIT_TEST CHECK FAIL %s %s"), *Description, *Details);
}

float UPanzerwerferUnitTestSubsystem::GetNow() const
{
	const UWorld* World = GetWorld();
	// Real time: the test dilates game time.
	return IsValid(World) ? World->GetRealTimeSeconds() : 0.f;
}

float UPanzerwerferUnitTestSubsystem::GetStageElapsed() const
{
	return GetNow() - M_StageSeconds;
}

void UPanzerwerferUnitTestSubsystem::EnterStage(const EPanzerwerferUnitTestStage NewStage)
{
	M_Stage = NewStage;
	M_StageSeconds = GetNow();
	bM_StageActionDone = false;
	UE_LOG(LogRTSPzwUnitTest, Display, TEXT("RTS_PZW_UNIT_TEST STAGE %s"), *UEnum::GetValueAsString(NewStage));
}

void UPanzerwerferUnitTestSubsystem::FailAndFinish(const FString& Description, const FString& Details)
{
	Check(false, Description, Details);
	Finish();
}

void UPanzerwerferUnitTestSubsystem::Finish()
{
	const bool bPassed = M_FailedCheckCount == 0 && M_PassedCheckCount > 0;
	UE_LOG(LogRTSPzwUnitTest, Display, TEXT("RTS_PZW_UNIT_TEST RESULT %s passed=%d failed=%d"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"), M_PassedCheckCount, M_FailedCheckCount);
	EnterStage(EPanzerwerferUnitTestStage::Finished);
	UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
	if (APlayerController* Controller = GetPlayerController())
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}

ACPPController* UPanzerwerferUnitTestSubsystem::GetPlayerController() const
{
	return FRTS_Statics::GetRTSController(GetWorld());
}

// ---------------------------------------------------------------------------------------------------------------
// Start, spawns and deploy
// ---------------------------------------------------------------------------------------------------------------

void UPanzerwerferUnitTestSubsystem::TickWaitingForStart()
{
	using namespace PanzerwerferUnitTestPrivate;
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
	SpawnDepotAndPz38tR();
}

bool UPanzerwerferUnitTestSubsystem::StartGameAndGetIsReady() const
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

bool UPanzerwerferUnitTestSubsystem::FindAnchor(FVector& OutAnchor) const
{
	using namespace PanzerwerferUnitTestPrivate;
	// FParse::Value stops at commas, so the anchor comes as two values.
	float AnchorX = 0.f;
	float AnchorY = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("PzwAnchorX="), AnchorX)
		&& FParse::Value(FCommandLine::Get(), TEXT("PzwAnchorY="), AnchorY))
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

void UPanzerwerferUnitTestSubsystem::GivePlayerResources() const
{
	using namespace PanzerwerferUnitTestPrivate;
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

void UPanzerwerferUnitTestSubsystem::SpawnDepotAndPz38tR()
{
	using namespace PanzerwerferUnitTestPrivate;
	ARTSAsyncSpawner* Spawner = FRTS_Statics::GetAsyncSpawner(this);
	if (not IsValid(Spawner))
	{
		FailAndFinish(TEXT("async_spawner_found"));
		return;
	}
	Check(Spawner->GetTrainingOptionClass(GetPanzerwerferOption()).ToSoftObjectPath().ToString().Contains(
		      ExpectedUnitClassName), TEXT("spawner_maps_panzerwerfer_to_its_blueprint"),
	      Spawner->GetTrainingOptionClass(GetPanzerwerferOption()).ToSoftObjectPath().ToString());
	TWeakObjectPtr<UPanzerwerferUnitTestSubsystem> WeakThis(this);
	const bool bRequested = Spawner->AsyncSpawnOptionAtLocation(
		FTrainingOption(EAllUnitType::UNType_Nomadic, static_cast<uint8>(ENomadicSubtype::Nomadic_GerMechanizedDepot)),
		M_Anchor, this, 0,
		[WeakThis](const FTrainingOption&, AActor* SpawnedActor, const int32)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->M_Depot = Cast<ANomadicVehicle>(SpawnedActor);
			}
		},
		FRotator::ZeroRotator);
	Check(bRequested, TEXT("depot_spawn_requested"));
	FVector Pz38tRGround = M_Anchor + Pz38tROffset;
	GlobalAbilityHelpers::TryFindGroundLocation(this, Pz38tRGround, Pz38tRGround);
	M_Pz38tR = Cast<ATankMaster>(SpawnUnitForPlayer(Pz38tRClassPath, FTransform(Pz38tRGround), PlayerIndex));
	EnterStage(EPanzerwerferUnitTestStage::WaitingForSpawns);
}

AActor* UPanzerwerferUnitTestSubsystem::SpawnUnitForPlayer(const TCHAR* ClassPath, const FTransform& Transform,
                                                           const uint8 OwningPlayer) const
{
	using namespace PanzerwerferUnitTestPrivate;
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

void UPanzerwerferUnitTestSubsystem::TickWaitingForSpawns()
{
	using namespace PanzerwerferUnitTestPrivate;
	const UWeaponState* Pz38tRWeapon = GetRocketWeapon(M_Pz38tR.Get());
	if (not M_Depot.IsValid() || not IsValid(Pz38tRWeapon))
	{
		if (GetStageElapsed() > SpawnTimeoutSeconds)
		{
			FailAndFinish(TEXT("depot_and_pz38t_r_spawned"),
			              FString::Printf(TEXT("depot=%s pz38t_r=%s"), *GetNameSafe(M_Depot.Get()),
			                              *GetNameSafe(M_Pz38tR.Get())));
		}
		return;
	}
	M_Pz38tRBaseReloadSpeed = Pz38tRWeapon->GetRawWeaponData().ReloadSpeed;
	Check(true, TEXT("depot_and_pz38t_r_spawned"), FString::Printf(TEXT("pz38t_r_reload=%.2f"), M_Pz38tRBaseReloadSpeed));
	UGameplayStatics::SetGlobalTimeDilation(this, TestTimeDilation);
	EnterStage(EPanzerwerferUnitTestStage::WaitingForDeploy);
}

void UPanzerwerferUnitTestSubsystem::TickWaitingForDeploy()
{
	using namespace PanzerwerferUnitTestPrivate;
	ANomadicVehicle* Depot = M_Depot.Get();
	if (not IsValid(Depot))
	{
		FailAndFinish(TEXT("depot_alive_for_deploy"));
		return;
	}
	if (Depot->GetNomadicStatus() == ENomadStatus::Truck)
	{
		// The convert order of the command card, on the truck's own spot; repeated until the truck accepts it.
		const ECommandQueueError Error = Depot->CreateBuildingAtLocation(Depot->GetActorLocation(),
		                                                                 FRotator::ZeroRotator, true);
		bM_StageActionDone |= Error == ECommandQueueError::NoError;
	}
	else if (Depot->GetNomadicStatus() != ENomadStatus::Building && bM_StageActionDone
		&& GetStageElapsed() > FastForwardDelaySeconds)
	{
		// Skips the truck rotation, montage and deploy animation, which wait on rendering headless.
		Depot->ShippingTest_ForceFinishConversionToBuilding();
	}
	if (Depot->GetNomadicStatus() != ENomadStatus::Building)
	{
		if (GetStageElapsed() > DeployTimeoutSeconds)
		{
			FailAndFinish(TEXT("depot_deployed"));
		}
		return;
	}
	Check(true, TEXT("depot_deployed"));
	ClickPanzerwerferTrainingOption();
}

void UPanzerwerferUnitTestSubsystem::ClickPanzerwerferTrainingOption()
{
	using namespace PanzerwerferUnitTestPrivate;
	ANomadicVehicle* Depot = M_Depot.Get();
	UTrainerComponent* Trainer = Depot->GetTrainerComponent();
	const FTrainingOption Option = GetPanzerwerferOption();
	Check(IsValid(Trainer) && Trainer->GetTrainingOptions().Contains(Option), TEXT("depot_options_include_panzerwerfer"));
	int32 SubSectionIndex = INDEX_NONE;
	int32 SlotIndex = INDEX_NONE;
	if (IsValid(Trainer))
	{
		const TArray<FTrainingSubSectionState>& SubSections = Trainer->GetTrainingSubSections();
		for (int32 Index = 0; Index < SubSections.Num() && SlotIndex == INDEX_NONE; ++Index)
		{
			SlotIndex = SubSections[Index].Options.IndexOfByKey(Option);
			SubSectionIndex = SlotIndex == INDEX_NONE ? INDEX_NONE : Index;
		}
	}
	Check(SlotIndex != INDEX_NONE, TEXT("depot_menu_shows_panzerwerfer"),
	      FString::Printf(TEXT("sub_section=%d slot=%d"), SubSectionIndex, SlotIndex));
	// The layout asset was re-saved for the new slot; both doctrines must still have their own layouts.
	UTrainingLayoutSubsystem* LayoutSubsystem = UTrainingLayoutSubsystem::Get(this);
	const UTrainingLayoutDataAsset* Layout = IsValid(LayoutSubsystem) ? LayoutSubsystem->GetTrainingLayout() : nullptr;
	Check(IsValid(Layout) && Layout->FactionLayouts.Contains(ERTSFaction::GerStrikeDivision)
	      && Layout->FactionLayouts.Contains(ERTSFaction::GerBreakthroughDoctrine),
	      TEXT("layout_keeps_both_doctrines"), GetNameSafe(Layout));
	// What the training button does when the player clicks it (menu manager path, costs paid).
	const int32 ClickResult = UMissionMapTestLibrary::MapTest_ClickTrainingOption(Depot, Option);
	constexpr int32 Queued = 1;
	Check(ClickResult == Queued, TEXT("panzerwerfer_training_click_queued"), FString::Printf(TEXT("result=%d"), ClickResult));
	if (ClickResult != Queued)
	{
		Finish();
		return;
	}
	EnterStage(EPanzerwerferUnitTestStage::WaitingForTrainedUnit);
}

// ---------------------------------------------------------------------------------------------------------------
// The trained unit: checks, move
// ---------------------------------------------------------------------------------------------------------------

ATankMaster* UPanzerwerferUnitTestSubsystem::FindTrainedPanzerwerfer() const
{
	for (TActorIterator<ATankMaster> It(GetWorld()); It; ++It)
	{
		const URTSComponent* RTSComponent = It->GetRTSComponent();
		if (IsValid(RTSComponent) && RTSComponent->GetUnitType() == EAllUnitType::UNType_Tank
			&& RTSComponent->GetSubtypeAsTankSubtype() == ETankSubtype::Tank_Panzerwerfer
			&& RTSComponent->GetOwningPlayer() == PanzerwerferUnitTestPrivate::PlayerIndex)
		{
			return *It;
		}
	}
	return nullptr;
}

UWeaponState* UPanzerwerferUnitTestSubsystem::GetRocketWeapon(const ATankMaster* Tank)
{
	if (not IsValid(Tank))
	{
		return nullptr;
	}
	for (UWeaponState* Weapon : FRTSWeaponHelpers::GetWeaponsMountedOnTank(Tank))
	{
		const EWeaponName WeaponName = IsValid(Weapon) ? Weapon->GetRawWeaponData().WeaponName : EWeaponName::DEFAULT_WEAPON;
		if (WeaponName == EWeaponName::Panzerwerfer || WeaponName == EWeaponName::Panzerwerfer_Small)
		{
			return Weapon;
		}
	}
	return nullptr;
}

void UPanzerwerferUnitTestSubsystem::TickWaitingForTrainedUnit()
{
	using namespace PanzerwerferUnitTestPrivate;
	ATankMaster* Panzerwerfer = FindTrainedPanzerwerfer();
	const UTrainerComponent* Trainer = M_Depot.IsValid() ? M_Depot->GetTrainerComponent() : nullptr;
	// The trainer spawns the unit hidden when training starts; it is handed over when the queue empties.
	const bool bTrainingDone = IsValid(Trainer) && Trainer->GetIsQueueEmpty();
	if (not bTrainingDone || not IsValid(Panzerwerfer) || not IsValid(GetRocketWeapon(Panzerwerfer)))
	{
		if (GetStageElapsed() > TrainTimeoutSeconds)
		{
			FailAndFinish(TEXT("depot_trained_panzerwerfer"), GetNameSafe(Panzerwerfer));
		}
		return;
	}
	M_Panzerwerfer = Panzerwerfer;
	CheckTrainedUnit(*Panzerwerfer);
	// Drive and fire at normal speed (the duel bench never drove under dilation); build the shop fast again.
	UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
	M_MoveStart = Panzerwerfer->GetActorLocation();
	const FVector Destination = M_MoveStart + Panzerwerfer->GetActorForwardVector() * MoveDistance;
	const ECommandQueueError Error = Panzerwerfer->MoveToLocation(Destination, true, Panzerwerfer->GetActorRotation());
	Check(Error == ECommandQueueError::NoError, TEXT("move_order_accepted"), UEnum::GetValueAsString(Error));
	EnterStage(EPanzerwerferUnitTestStage::WaitingForMove);
}

void UPanzerwerferUnitTestSubsystem::CheckTrainedUnit(ATankMaster& Panzerwerfer)
{
	using namespace PanzerwerferUnitTestPrivate;
	const FString ClassName = Panzerwerfer.GetClass()->GetName();
	Check(ClassName == ExpectedUnitClassName, TEXT("trained_unit_is_panzerwerfer_blueprint"), ClassName);
	Check(UPanzerwerferReloadBonusSubsystem::GetIsPanzerwerfer(Panzerwerfer), TEXT("bonus_subsystem_sees_panzerwerfer"));
	const UWeaponState* Weapon = GetRocketWeapon(&Panzerwerfer);
	const FWeaponData& Data = Weapon->GetRawWeaponData();
	M_BaseReloadSpeed = Data.ReloadSpeed;
	Check(Data.WeaponName == EWeaponName::Panzerwerfer && Data.MagCapacity == ExpectedMagCapacity
	      && FMath::IsNearlyEqual(Data.WeaponCalibre, ExpectedCalibreMm),
	      TEXT("weapon_is_15cm_panzerwerfer"),
	      FString::Printf(TEXT("weapon=%s calibre=%.0f mag=%d reload=%.2f range=%.0f damage=%.1f"),
	                      *UEnum::GetValueAsString(Data.WeaponName), Data.WeaponCalibre, Data.MagCapacity,
	                      Data.ReloadSpeed, Data.Range, Data.BaseDamage));
	const UHealthComponent* Health = Panzerwerfer.FindComponentByClass<UHealthComponent>();
	const URTSComponent* RTSComponent = Panzerwerfer.GetRTSComponent();
	Check(IsValid(Health) && Health->GetMaxHealth() > 0.f, TEXT("unit_has_health"),
	      FString::Printf(TEXT("max_health=%.0f voice=%s"), IsValid(Health) ? Health->GetMaxHealth() : 0.f,
	                      IsValid(RTSComponent) ? *UEnum::GetValueAsString(RTSComponent->GetUnitVoiceLineType())
	                                            : TEXT("none")));
	CheckReload(&Panzerwerfer, M_BaseReloadSpeed, false, TEXT("trained_before_shop"));
}

void UPanzerwerferUnitTestSubsystem::TickWaitingForMove()
{
	using namespace PanzerwerferUnitTestPrivate;
	const ATankMaster* Panzerwerfer = M_Panzerwerfer.Get();
	const float Moved = IsValid(Panzerwerfer) ? FVector::Dist2D(Panzerwerfer->GetActorLocation(), M_MoveStart) : 0.f;
	if (Moved < RequiredMoveDistance && GetStageElapsed() <= MoveTimeoutSeconds)
	{
		return;
	}
	Check(Moved >= RequiredMoveDistance, TEXT("panzerwerfer_drove"), FString::Printf(TEXT("moved=%.0f"), Moved));
	UGameplayStatics::SetGlobalTimeDilation(this, TestTimeDilation);
	StartPlacingShop();
}

// ---------------------------------------------------------------------------------------------------------------
// Wurfrahmen shop and the reload bonus
// ---------------------------------------------------------------------------------------------------------------

void UPanzerwerferUnitTestSubsystem::StartPlacingShop()
{
	const bool bStarted = UMissionMapTestLibrary::MapTest_StartBxpPlacement(
		M_Depot.Get(), EBuildingExpansionType::BTX_GerMechDepotWurfrahmenShop);
	Check(bStarted, TEXT("wurfrahmen_shop_option_clicked"));
	if (not bStarted)
	{
		Finish();
		return;
	}
	EnterStage(EPanzerwerferUnitTestStage::WaitingForShopLoaded);
}

void UPanzerwerferUnitTestSubsystem::TickWaitingForShopLoaded()
{
	using namespace PanzerwerferUnitTestPrivate;
	ACPPController* Controller = GetPlayerController();
	if (not IsValid(Controller) || not Controller->MapTest_GetIsBxpWaitingForPlacement())
	{
		if (GetStageElapsed() > ShopLoadTimeoutSeconds)
		{
			FailAndFinish(TEXT("wurfrahmen_shop_loaded_for_placement"));
		}
		return;
	}
	const UStaticMeshComponent* AttachMesh = M_Depot.IsValid() ? M_Depot->GetAttachToMeshComponent() : nullptr;
	if (not IsValid(AttachMesh) || not IsValid(Controller->CPPConstructionPreviewRef))
	{
		FailAndFinish(TEXT("found_shop_click_location"));
		return;
	}
	// Origin expansions snap to the building origin; headless there is no cursor, so prime the preview there.
	FVector ClickLocation = AttachMesh->GetComponentLocation();
	Controller->CPPConstructionPreviewRef->ShippingTest_PrimeBxpPlacementAtLocation(ClickLocation);
	const bool bPlaced = Controller->MapTest_PlaceLoadedBxpAtLocation(ClickLocation);
	Check(bPlaced, TEXT("wurfrahmen_shop_placed_by_click"));
	if (not bPlaced)
	{
		Finish();
		return;
	}
	EnterStage(EPanzerwerferUnitTestStage::WaitingForShopBuilt);
}

void UPanzerwerferUnitTestSubsystem::TickWaitingForShopBuilt()
{
	using namespace PanzerwerferUnitTestPrivate;
	const UPanzerwerferReloadBonusSubsystem* BonusSubsystem =
		GetWorld()->GetSubsystem<UPanzerwerferReloadBonusSubsystem>();
	const bool bActive = IsValid(BonusSubsystem) && BonusSubsystem->GetIsBonusActiveForPlayer(PlayerIndex);
	const UWeaponState* Weapon = GetRocketWeapon(M_Panzerwerfer.Get());
	const bool bReloadDropped = IsValid(Weapon) && Weapon->GetRawWeaponData().ReloadSpeed < M_BaseReloadSpeed;
	if ((not bActive || not bReloadDropped) && GetStageElapsed() <= ShopBuildTimeoutSeconds)
	{
		return;
	}
	Check(bActive, TEXT("reload_bonus_active_with_shop"));
	CheckReload(M_Panzerwerfer.Get(), M_BaseReloadSpeed, true, TEXT("panzerwerfer_with_shop"));
	CheckReload(M_Pz38tR.Get(), M_Pz38tRBaseReloadSpeed, true, TEXT("pz38t_r_with_shop"));
	UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
	SpawnEnemyAndAttack();
}

void UPanzerwerferUnitTestSubsystem::CheckReload(const ATankMaster* Tank, const float BaseReload,
                                                 const bool bExpectBonus, const FString& Moment)
{
	using namespace PanzerwerferUnitTestPrivate;
	using DeveloperSettings::GameBalance::UnitCosts::Bxps::GerMechDepotWurfrahmenShopPanzerwerferReloadMlt;
	const UWeaponState* Weapon = GetRocketWeapon(Tank);
	const float Reload = IsValid(Weapon) ? Weapon->GetRawWeaponData().ReloadSpeed : 0.f;
	const float Expected = bExpectBonus ? BaseReload * GerMechDepotWurfrahmenShopPanzerwerferReloadMlt : BaseReload;
	const UBehaviourComp* BehaviourComponent = IsValid(Tank) ? Tank->FindComponentByClass<UBehaviourComp>() : nullptr;
	const bool bHasBehaviour = IsValid(BehaviourComponent)
		&& IsValid(BehaviourComponent->GetBehaviourByClass(UBehaviourWeaponWurfrahmenShop::StaticClass()));
	Check(IsValid(Weapon) && FMath::IsNearlyEqual(Reload, Expected, ReloadTolerance) && bHasBehaviour == bExpectBonus,
	      TEXT("reload_") + Moment,
	      FString::Printf(TEXT("reload=%.2f expected=%.2f base=%.2f behaviour=%d"), Reload, Expected, BaseReload,
	                      bHasBehaviour ? 1 : 0));
}

// ---------------------------------------------------------------------------------------------------------------
// Rockets at an enemy tank
// ---------------------------------------------------------------------------------------------------------------

void UPanzerwerferUnitTestSubsystem::SpawnEnemyAndAttack()
{
	using namespace PanzerwerferUnitTestPrivate;
	ATankMaster* Panzerwerfer = M_Panzerwerfer.Get();
	if (not IsValid(Panzerwerfer))
	{
		FailAndFinish(TEXT("panzerwerfer_alive_for_fire"));
		return;
	}
	const FVector From = Panzerwerfer->GetActorLocation();
	FVector EnemyGround = From + Panzerwerfer->GetActorForwardVector() * EnemyDistance;
	GlobalAbilityHelpers::TryFindGroundLocation(this, EnemyGround, EnemyGround);
	const FRotator FacingAway = (EnemyGround - From).GetSafeNormal2D().Rotation();
	M_Enemy = SpawnUnitForPlayer(EnemyClassPath, FTransform(FacingAway, EnemyGround), EnemyPlayer);
	Check(M_Enemy.IsValid(), TEXT("enemy_tank_spawned"), EnemyGround.ToCompactString());
	const ECommandQueueError Error = M_Enemy.IsValid()
		                                 ? Panzerwerfer->AttackActor(M_Enemy.Get(), true)
		                                 : ECommandQueueError::CommandDataInvalid;
	Check(Error == ECommandQueueError::NoError, TEXT("attack_order_accepted"), UEnum::GetValueAsString(Error));
	M_RocketsFired = 0;
	M_LastMagazine = INDEX_NONE;
	EnterStage(EPanzerwerferUnitTestStage::WaitingForRockets);
}

void UPanzerwerferUnitTestSubsystem::KeepAlive(AActor* Actor) const
{
	if (not IsValid(Actor))
	{
		return;
	}
	if (UHealthComponent* Health = Actor->FindComponentByClass<UHealthComponent>())
	{
		Health->SetCurrentHealth(Health->GetMaxHealth());
	}
}

void UPanzerwerferUnitTestSubsystem::CountRockets()
{
	const UWeaponState* Weapon = GetRocketWeapon(M_Panzerwerfer.Get());
	if (not IsValid(Weapon))
	{
		return;
	}
	const int32 Magazine = Weapon->GetCurrentMagCapacity();
	if (M_LastMagazine != INDEX_NONE)
	{
		M_RocketsFired += FMath::Max(0, M_LastMagazine - Magazine);
	}
	M_LastMagazine = Magazine;
}

void UPanzerwerferUnitTestSubsystem::TickWaitingForRockets()
{
	using namespace PanzerwerferUnitTestPrivate;
	CountRockets();
	KeepAlive(M_Enemy.Get());
	KeepAlive(M_Panzerwerfer.Get());
	if (M_RocketsFired < RequiredRockets && GetStageElapsed() <= RocketTimeoutSeconds)
	{
		return;
	}
	const ATankMaster* Panzerwerfer = M_Panzerwerfer.Get();
	const float Distance = IsValid(Panzerwerfer) && M_Enemy.IsValid()
		                       ? FVector::Dist(Panzerwerfer->GetActorLocation(), M_Enemy->GetActorLocation())
		                       : 0.f;
	Check(M_RocketsFired >= RequiredRockets, TEXT("panzerwerfer_fired_rockets_at_enemy"),
	      FString::Printf(TEXT("rockets=%d distance=%.0f"), M_RocketsFired, Distance));
	if (AActor* Enemy = M_Enemy.Get())
	{
		Enemy->Destroy();
	}
	ACPPController* Controller = GetPlayerController();
	if (not IsValid(Controller) || not M_Depot.IsValid())
	{
		FailAndFinish(TEXT("controller_and_depot_valid_for_pack_up"));
		return;
	}
	Controller->ConvertBackToVehicle(M_Depot.Get());
	EnterStage(EPanzerwerferUnitTestStage::WaitingForPackUp);
}

// ---------------------------------------------------------------------------------------------------------------
// Pack-up, death, wreck
// ---------------------------------------------------------------------------------------------------------------

void UPanzerwerferUnitTestSubsystem::TickWaitingForPackUp()
{
	using namespace PanzerwerferUnitTestPrivate;
	const ANomadicVehicle* Depot = M_Depot.Get();
	if (not IsValid(Depot) || Depot->GetNomadicStatus() != ENomadStatus::Truck)
	{
		if (GetStageElapsed() > PackUpTimeoutSeconds)
		{
			FailAndFinish(TEXT("depot_packed_into_truck"));
		}
		return;
	}
	Check(true, TEXT("depot_packed_into_truck"));
	const UPanzerwerferReloadBonusSubsystem* BonusSubsystem =
		GetWorld()->GetSubsystem<UPanzerwerferReloadBonusSubsystem>();
	Check(IsValid(BonusSubsystem) && not BonusSubsystem->GetIsBonusActiveForPlayer(PlayerIndex),
	      TEXT("reload_bonus_gone_after_pack_up"));
	CheckReload(M_Panzerwerfer.Get(), M_BaseReloadSpeed, false, TEXT("panzerwerfer_after_pack_up"));
	CheckReload(M_Pz38tR.Get(), M_Pz38tRBaseReloadSpeed, false, TEXT("pz38t_r_after_pack_up"));
	KillPanzerwerfer();
}

void UPanzerwerferUnitTestSubsystem::KillPanzerwerfer()
{
	using namespace PanzerwerferUnitTestPrivate;
	ATankMaster* Panzerwerfer = M_Panzerwerfer.Get();
	if (not IsValid(Panzerwerfer))
	{
		FailAndFinish(TEXT("panzerwerfer_alive_for_kill"));
		return;
	}
	M_DeathLocation = Panzerwerfer->GetActorLocation();
	UGameplayStatics::ApplyDamage(Panzerwerfer, KillDamage, nullptr, nullptr, nullptr);
	EnterStage(EPanzerwerferUnitTestStage::WaitingForWreck);
}

AActor* UPanzerwerferUnitTestSubsystem::FindWreckNear(const FVector& Location) const
{
	using namespace PanzerwerferUnitTestPrivate;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->GetClass()->GetName() == ExpectedWreckClassName
			&& FVector::Dist2D(It->GetActorLocation(), Location) <= WreckSearchRadius)
		{
			return *It;
		}
	}
	return nullptr;
}

void UPanzerwerferUnitTestSubsystem::TickWaitingForWreck()
{
	using namespace PanzerwerferUnitTestPrivate;
	const AActor* Wreck = FindWreckNear(M_DeathLocation);
	const bool bGone = not M_Panzerwerfer.IsValid() || M_Panzerwerfer->IsActorBeingDestroyed();
	if ((not IsValid(Wreck) || not bGone) && GetStageElapsed() <= WreckTimeoutSeconds)
	{
		return;
	}
	Check(bGone, TEXT("panzerwerfer_died"));
	const UStaticMeshComponent* WreckMesh = IsValid(Wreck) ? Wreck->FindComponentByClass<UStaticMeshComponent>() : nullptr;
	Check(IsValid(Wreck), TEXT("panzerwerfer_wreck_spawned"),
	      FString::Printf(TEXT("wreck=%s mesh=%s"), *GetNameSafe(Wreck),
	                      IsValid(WreckMesh) ? *GetNameSafe(WreckMesh->GetStaticMesh()) : TEXT("none")));
	Finish();
}

#else

bool UPanzerwerferUnitTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UPanzerwerferUnitTestSubsystem::Tick(const float DeltaTime)
{
}

TStatId UPanzerwerferUnitTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UPanzerwerferUnitTestSubsystem, STATGROUP_Tickables);
}

#endif
