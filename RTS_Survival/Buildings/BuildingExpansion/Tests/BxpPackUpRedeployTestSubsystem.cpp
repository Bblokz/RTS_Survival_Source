// Copyright (C) Bas Blokzijl - All rights reserved.

#include "BxpPackUpRedeployTestSubsystem.h"

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
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Player/ConstructionPreview/CPPConstructionPreview.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSBxpRedeployTest, Log, All);

namespace BxpPackUpRedeployTestPrivate
{
	constexpr float StartDelaySeconds = 4.f;
	constexpr float CameraTraceLength = 500000.f;
	constexpr float FirstDeployYaw = 90.f;
	constexpr float RedeployYaw = 180.f;
	// Speeds up building the expansions and the pack-up; the deploys are fast-forwarded.
	constexpr float TestTimeDilation = 10.f;
	constexpr float FastForwardDelaySeconds = 1.f;
	constexpr float TruckSpawnTimeoutSeconds = 30.f;
	constexpr float DeployTimeoutSeconds = 60.f;
	constexpr float BxpLoadTimeoutSeconds = 30.f;
	constexpr float BxpBuildTimeoutSeconds = 60.f;
	constexpr float PackUpTimeoutSeconds = 60.f;
	constexpr float ReplacedBxpTimeoutSeconds = 30.f;
	constexpr float LocationTolerance = 5.f;
	constexpr float AngleToleranceDegrees = 1.f;
	constexpr int32 TestResourceAmount = 1000000;
	// Slot 0 stays empty so a packed expansion later in the array exposes a slot index that skips empty slots.
	constexpr int32 EmptySlotIndex = 0;
	constexpr int32 SocketSlotIndex = 1;
	constexpr int32 OriginSlotIndex = 2;
	constexpr int32 RequiredSlotCount = 3;

	FString YawText(const float Yaw)
	{
		return FString::Printf(TEXT("%.1f"), FRotator::NormalizeAxis(Yaw));
	}

	bool GetIsYawEqual(const float YawA, const float YawB)
	{
		return FMath::Abs(FRotator::NormalizeAxis(YawA - YawB)) <= AngleToleranceDegrees;
	}
}

bool UBxpPackUpRedeployTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), TEXT("BxpPackUpRedeployValidate"));
}

TStatId UBxpPackUpRedeployTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBxpPackUpRedeployTestSubsystem, STATGROUP_Tickables);
}

void UBxpPackUpRedeployTestSubsystem::Tick(const float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	switch (M_Stage)
	{
	case EBxpPackUpRedeployTestStage::WaitingForStart:
		TickWaitingForStart();
		return;
	case EBxpPackUpRedeployTestStage::WaitingForTruckSpawn:
		TickWaitingForTruckSpawn();
		return;
	case EBxpPackUpRedeployTestStage::WaitingForFirstDeploy:
		TickWaitingForFirstDeploy();
		return;
	case EBxpPackUpRedeployTestStage::WaitingForBxpLoaded:
		TickWaitingForBxpLoaded();
		return;
	case EBxpPackUpRedeployTestStage::WaitingForBxpsBuilt:
		TickWaitingForBxpsBuilt();
		return;
	case EBxpPackUpRedeployTestStage::WaitingForPackUp:
		TickWaitingForPackUp();
		return;
	case EBxpPackUpRedeployTestStage::WaitingForRedeploy:
		TickWaitingForRedeploy();
		return;
	case EBxpPackUpRedeployTestStage::WaitingForReplacedBxps:
		TickWaitingForReplacedBxps();
		return;
	case EBxpPackUpRedeployTestStage::Finished:
		return;
	}
}

void UBxpPackUpRedeployTestSubsystem::Check(const bool bCondition, const FString& Description, const FString& Details)
{
	if (bCondition)
	{
		++M_PassedCheckCount;
		UE_LOG(LogRTSBxpRedeployTest, Display, TEXT("RTS_BXP_REDEPLOY_TEST CHECK PASS %s %s"), *Description, *Details);
		return;
	}
	++M_FailedCheckCount;
	UE_LOG(LogRTSBxpRedeployTest, Error, TEXT("RTS_BXP_REDEPLOY_TEST CHECK FAIL %s %s"), *Description, *Details);
}

float UBxpPackUpRedeployTestSubsystem::GetNow() const
{
	const UWorld* World = GetWorld();
	// Real time: the test dilates game time.
	return IsValid(World) ? World->GetRealTimeSeconds() : 0.f;
}

float UBxpPackUpRedeployTestSubsystem::GetStageElapsed() const
{
	return GetNow() - M_StageSeconds;
}

void UBxpPackUpRedeployTestSubsystem::EnterStage(const EBxpPackUpRedeployTestStage NewStage)
{
	M_Stage = NewStage;
	M_StageSeconds = GetNow();
	bM_StageActionDone = false;
	UE_LOG(LogRTSBxpRedeployTest, Display, TEXT("RTS_BXP_REDEPLOY_TEST STAGE %s"), *UEnum::GetValueAsString(NewStage));
}

void UBxpPackUpRedeployTestSubsystem::FailAndFinish(const FString& Description, const FString& Details)
{
	Check(false, Description, Details);
	Finish();
}

void UBxpPackUpRedeployTestSubsystem::Finish()
{
	const bool bPassed = M_FailedCheckCount == 0 && M_PassedCheckCount > 0;
	UE_LOG(LogRTSBxpRedeployTest, Display, TEXT("RTS_BXP_REDEPLOY_TEST RESULT %s passed=%d failed=%d"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"), M_PassedCheckCount, M_FailedCheckCount);
	EnterStage(EBxpPackUpRedeployTestStage::Finished);
	UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
	if (APlayerController* Controller = GetPlayerController())
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}

ACPPController* UBxpPackUpRedeployTestSubsystem::GetPlayerController() const
{
	return FRTS_Statics::GetRTSController(GetWorld());
}

bool UBxpPackUpRedeployTestSubsystem::GetHqBxpOwner(TScriptInterface<IBuildingExpansionOwner>& OutOwner) const
{
	ANomadicVehicle* Hq = M_Hq.Get();
	if (not IsValid(Hq))
	{
		return false;
	}
	OutOwner.SetObject(Hq);
	OutOwner.SetInterface(static_cast<IBuildingExpansionOwner*>(Hq));
	return true;
}

// ---------------------------------------------------------------------------------------------------------------
// Start, spawn and first deploy
// ---------------------------------------------------------------------------------------------------------------

void UBxpPackUpRedeployTestSubsystem::TickWaitingForStart()
{
	using namespace BxpPackUpRedeployTestPrivate;
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
	SpawnHqTruck();
}

bool UBxpPackUpRedeployTestSubsystem::StartGameAndGetIsReady() const
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

bool UBxpPackUpRedeployTestSubsystem::FindAnchor(FVector& OutAnchor) const
{
	using namespace BxpPackUpRedeployTestPrivate;
	// FParse::Value stops at commas, so the anchor comes as two values.
	float AnchorX = 0.f;
	float AnchorY = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("BxpRedeployAnchorX="), AnchorX)
		&& FParse::Value(FCommandLine::Get(), TEXT("BxpRedeployAnchorY="), AnchorY))
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

void UBxpPackUpRedeployTestSubsystem::GivePlayerResources() const
{
	using namespace BxpPackUpRedeployTestPrivate;
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

void UBxpPackUpRedeployTestSubsystem::SpawnHqTruck()
{
	using namespace BxpPackUpRedeployTestPrivate;
	ARTSAsyncSpawner* Spawner = FRTS_Statics::GetAsyncSpawner(this);
	if (not IsValid(Spawner))
	{
		FailAndFinish(TEXT("async_spawner_found"));
		return;
	}
	TWeakObjectPtr<UBxpPackUpRedeployTestSubsystem> WeakThis(this);
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
		FRotator(0.f, FirstDeployYaw, 0.f));
	Check(bRequested, TEXT("hq_truck_spawn_requested"), M_Anchor.ToCompactString());
	if (not bRequested)
	{
		Finish();
		return;
	}
	EnterStage(EBxpPackUpRedeployTestStage::WaitingForTruckSpawn);
}

void UBxpPackUpRedeployTestSubsystem::TickWaitingForTruckSpawn()
{
	using namespace BxpPackUpRedeployTestPrivate;
	if (M_Hq.IsValid())
	{
		Check(true, TEXT("hq_truck_spawned"), M_Hq->GetName());
		UGameplayStatics::SetGlobalTimeDilation(this, TestTimeDilation);
		EnterStage(EBxpPackUpRedeployTestStage::WaitingForFirstDeploy);
		return;
	}
	if (GetStageElapsed() > TruckSpawnTimeoutSeconds)
	{
		FailAndFinish(TEXT("hq_truck_spawned"));
	}
}

bool UBxpPackUpRedeployTestSubsystem::TickDeployAndGetIsBuilding(const float BuildYaw)
{
	ANomadicVehicle* Hq = M_Hq.Get();
	if (not IsValid(Hq))
	{
		return false;
	}
	if (Hq->GetNomadicStatus() == ENomadStatus::Building)
	{
		return true;
	}
	if (not bM_StageActionDone && Hq->GetNomadicStatus() == ENomadStatus::Truck)
	{
		// The convert order of the command card, on the truck's own spot.
		const ECommandQueueError Error = Hq->CreateBuildingAtLocation(Hq->GetActorLocation(),
		                                                              FRotator(0.f, BuildYaw, 0.f), true);
		bM_StageActionDone = Error == ECommandQueueError::NoError;
		return false;
	}
	if (bM_StageActionDone && GetStageElapsed() > BxpPackUpRedeployTestPrivate::FastForwardDelaySeconds)
	{
		// Skips the truck rotation, the truck montage and the deploy animation, which wait on rendering headless.
		return Hq->ShippingTest_ForceFinishConversionToBuilding();
	}
	return false;
}

void UBxpPackUpRedeployTestSubsystem::TickWaitingForFirstDeploy()
{
	using namespace BxpPackUpRedeployTestPrivate;
	if (not TickDeployAndGetIsBuilding(FirstDeployYaw))
	{
		if (GetStageElapsed() > DeployTimeoutSeconds)
		{
			FailAndFinish(TEXT("hq_first_deploy_finished"));
		}
		return;
	}
	Check(true, TEXT("hq_first_deploy_finished"));
	if (not PickExpansionOptions())
	{
		Finish();
		return;
	}
	StartPlacingSlot(0);
}

bool UBxpPackUpRedeployTestSubsystem::PickExpansionOptions()
{
	using namespace BxpPackUpRedeployTestPrivate;
	ANomadicVehicle* Hq = M_Hq.Get();
	const int32 SlotCount = Hq->GetBuildingExpansions().Num();
	Check(SlotCount >= RequiredSlotCount, TEXT("hq_has_three_expansion_slots"), FString::Printf(TEXT("slots=%d"), SlotCount));
	const TArray<FBxpOptionData> Options = Hq->GetUnlockedBuildingExpansionTypes();
	const FBxpOptionData* SocketOption = Options.FindByPredicate([](const FBxpOptionData& Option)
	{
		return Option.BxpConstructionRules.ConstructionType == EBxpConstructionType::Socket;
	});
	const FBxpOptionData* OriginOption = Options.FindByPredicate([](const FBxpOptionData& Option)
	{
		return Option.BxpConstructionRules.ConstructionType == EBxpConstructionType::AtBuildingOrigin;
	});
	Check(SocketOption != nullptr, TEXT("hq_offers_socket_expansion"));
	Check(OriginOption != nullptr, TEXT("hq_offers_origin_expansion"));
	if (SlotCount < RequiredSlotCount || SocketOption == nullptr || OriginOption == nullptr)
	{
		return false;
	}
	M_Slots.SetNum(2);
	M_Slots[0].Option = *SocketOption;
	M_Slots[0].SlotIndex = SocketSlotIndex;
	M_Slots[1].Option = *OriginOption;
	M_Slots[1].SlotIndex = OriginSlotIndex;
	for (const FBxpPackUpRedeployTestSlot& Slot : M_Slots)
	{
		UE_LOG(LogRTSBxpRedeployTest, Display, TEXT("RTS_BXP_REDEPLOY_TEST slot=%d type=%s rules=%s"), Slot.SlotIndex,
		       *UEnum::GetValueAsString(Slot.Option.ExpansionType),
		       *Global_GetBxpConstructionTypeEnumAsString(Slot.Option.BxpConstructionRules.ConstructionType));
	}
	return true;
}

void UBxpPackUpRedeployTestSubsystem::StartPlacingSlot(const int32 SlotArrayIndex)
{
	ACPPController* Controller = GetPlayerController();
	TScriptInterface<IBuildingExpansionOwner> Owner;
	if (not IsValid(Controller) || not GetHqBxpOwner(Owner))
	{
		FailAndFinish(TEXT("controller_and_owner_valid_for_option_click"));
		return;
	}
	M_PlacingSlotArrayIndex = SlotArrayIndex;
	const FBxpPackUpRedeployTestSlot& Slot = M_Slots[SlotArrayIndex];
	// What the option button does when the player clicks it for this slot.
	Controller->ExpandBuildingWithBxpAndStartPreview(Slot.Option, Owner, Slot.SlotIndex, false);
	EnterStage(EBxpPackUpRedeployTestStage::WaitingForBxpLoaded);
}

void UBxpPackUpRedeployTestSubsystem::TickWaitingForBxpLoaded()
{
	using namespace BxpPackUpRedeployTestPrivate;
	ACPPController* Controller = GetPlayerController();
	FBxpPackUpRedeployTestSlot& Slot = M_Slots[M_PlacingSlotArrayIndex];
	if (not IsValid(Controller) || not Controller->MapTest_GetIsBxpWaitingForPlacement())
	{
		if (GetStageElapsed() > BxpLoadTimeoutSeconds)
		{
			FailAndFinish(TEXT("bxp_loaded_for_placement"), FString::FromInt(Slot.SlotIndex));
		}
		return;
	}
	FVector ClickLocation;
	if (not GetPlacementClickLocation(Slot, ClickLocation) || not IsValid(Controller->CPPConstructionPreviewRef))
	{
		FailAndFinish(TEXT("found_placement_click_location"), FString::FromInt(Slot.SlotIndex));
		return;
	}
	// The preview tick snaps to the socket or origin under the cursor; headless there is no cursor, so prime it.
	Controller->CPPConstructionPreviewRef->ShippingTest_PrimeBxpPlacementAtLocation(ClickLocation);
	const bool bPlaced = Controller->MapTest_PlaceLoadedBxpAtLocation(ClickLocation);
	Check(bPlaced, TEXT("bxp_placed_by_click"), FString::Printf(TEXT("slot=%d"), Slot.SlotIndex));
	if (not bPlaced)
	{
		Finish();
		return;
	}
	RecordFirstPlacement(Slot);
	if (M_PlacingSlotArrayIndex + 1 < M_Slots.Num())
	{
		StartPlacingSlot(M_PlacingSlotArrayIndex + 1);
		return;
	}
	EnterStage(EBxpPackUpRedeployTestStage::WaitingForBxpsBuilt);
}

bool UBxpPackUpRedeployTestSubsystem::GetPlacementClickLocation(const FBxpPackUpRedeployTestSlot& Slot,
                                                                FVector& OutLocation) const
{
	const ANomadicVehicle* Hq = M_Hq.Get();
	const UStaticMeshComponent* AttachMesh = IsValid(Hq) ? Hq->GetAttachToMeshComponent() : nullptr;
	if (not IsValid(AttachMesh))
	{
		return false;
	}
	if (Slot.Option.BxpConstructionRules.ConstructionType != EBxpConstructionType::Socket)
	{
		OutLocation = AttachMesh->GetComponentLocation();
		return true;
	}
	const TArray<UStaticMeshSocket*> FreeSockets = Hq->GetFreeSocketList(Slot.SlotIndex);
	if (FreeSockets.IsEmpty() || FreeSockets[0] == nullptr)
	{
		return false;
	}
	OutLocation = AttachMesh->GetSocketLocation(FreeSockets[0]->SocketName);
	return true;
}

void UBxpPackUpRedeployTestSubsystem::RecordFirstPlacement(FBxpPackUpRedeployTestSlot& Slot) const
{
	const ANomadicVehicle* Hq = M_Hq.Get();
	const ABuildingExpansion* Bxp = Hq->GetBuildingExpansionAtIndex(Slot.SlotIndex);
	const UStaticMeshComponent* AttachMesh = Hq->GetAttachToMeshComponent();
	if (not IsValid(Bxp) || not IsValid(AttachMesh))
	{
		return;
	}
	const FBuildingExpansionItem* Item = Hq->GetBuildingExpansionItemAtIndex(Slot.SlotIndex);
	Slot.SocketName = Item != nullptr ? Item->BxpConstructionRules.SocketName : NAME_None;
	Slot.WorldLocation = Bxp->GetActorLocation();
	Slot.WorldRotation = Bxp->GetActorRotation();
	Slot.RelativeToBuilding = Bxp->GetActorTransform().GetRelativeTransform(AttachMesh->GetComponentTransform());
	Slot.BuildingYaw = AttachMesh->GetComponentRotation().Yaw;
	Slot.bPlaced = true;
	UE_LOG(LogRTSBxpRedeployTest, Display,
	       TEXT("RTS_BXP_REDEPLOY_TEST FIRST_PLACEMENT slot=%d socket=%s bxp_yaw=%s building_yaw=%s relative_yaw=%s loc=%s"),
	       Slot.SlotIndex, *Slot.SocketName.ToString(),
	       *BxpPackUpRedeployTestPrivate::YawText(Slot.WorldRotation.Yaw),
	       *BxpPackUpRedeployTestPrivate::YawText(Slot.BuildingYaw),
	       *BxpPackUpRedeployTestPrivate::YawText(Slot.RelativeToBuilding.Rotator().Yaw),
	       *Slot.WorldLocation.ToCompactString());
}

void UBxpPackUpRedeployTestSubsystem::TickWaitingForBxpsBuilt()
{
	using namespace BxpPackUpRedeployTestPrivate;
	const ANomadicVehicle* Hq = M_Hq.Get();
	if (not IsValid(Hq))
	{
		FailAndFinish(TEXT("hq_valid_while_building_bxps"));
		return;
	}
	bool bAllBuilt = true;
	for (const FBxpPackUpRedeployTestSlot& Slot : M_Slots)
	{
		const ABuildingExpansion* Bxp = Hq->GetBuildingExpansionAtIndex(Slot.SlotIndex);
		bAllBuilt &= IsValid(Bxp) && Bxp->GetBuildingExpansionStatus() == EBuildingExpansionStatus::BXS_Built;
	}
	if (not bAllBuilt)
	{
		if (GetStageElapsed() > BxpBuildTimeoutSeconds)
		{
			FailAndFinish(TEXT("bxps_built_before_pack_up"));
		}
		return;
	}
	Check(true, TEXT("bxps_built_before_pack_up"));
	for (const FBxpPackUpRedeployTestSlot& Slot : M_Slots)
	{
		// BP_OnStartBxpConstruction or the deploy animation could have moved the expansion after placement.
		const ABuildingExpansion* Bxp = Hq->GetBuildingExpansionAtIndex(Slot.SlotIndex);
		Check(GetIsYawEqual(Bxp->GetActorRotation().Yaw, Slot.WorldRotation.Yaw),
		      FString::Printf(TEXT("slot%d_yaw_unchanged_once_built"), Slot.SlotIndex),
		      FString::Printf(TEXT("placed=%s built=%s"), *YawText(Slot.WorldRotation.Yaw),
		                      *YawText(Bxp->GetActorRotation().Yaw)));
	}
	ACPPController* Controller = GetPlayerController();
	if (not IsValid(Controller))
	{
		FailAndFinish(TEXT("controller_valid_for_pack_up"));
		return;
	}
	Controller->ConvertBackToVehicle(M_Hq.Get());
	EnterStage(EBxpPackUpRedeployTestStage::WaitingForPackUp);
}

// ---------------------------------------------------------------------------------------------------------------
// Pack-up and redeploy
// ---------------------------------------------------------------------------------------------------------------

void UBxpPackUpRedeployTestSubsystem::TickWaitingForPackUp()
{
	using namespace BxpPackUpRedeployTestPrivate;
	const ANomadicVehicle* Hq = M_Hq.Get();
	if (not IsValid(Hq) || Hq->GetNomadicStatus() != ENomadStatus::Truck)
	{
		if (GetStageElapsed() > PackUpTimeoutSeconds)
		{
			FailAndFinish(TEXT("hq_packed_into_truck"));
		}
		return;
	}
	Check(true, TEXT("hq_packed_into_truck"));
	CheckSlotsAfterPackUp();
	EnterStage(EBxpPackUpRedeployTestStage::WaitingForRedeploy);
}

void UBxpPackUpRedeployTestSubsystem::CheckSlotsAfterPackUp()
{
	using namespace BxpPackUpRedeployTestPrivate;
	const ANomadicVehicle* Hq = M_Hq.Get();
	const FBuildingExpansionItem* EmptyItem = Hq->GetBuildingExpansionItemAtIndex(EmptySlotIndex);
	Check(EmptyItem != nullptr && EmptyItem->ExpansionStatus != EBuildingExpansionStatus::BXS_PackedUp,
	      TEXT("empty_slot0_not_packed_after_pack_up"));
	for (const FBxpPackUpRedeployTestSlot& Slot : M_Slots)
	{
		const FBuildingExpansionItem* Item = Hq->GetBuildingExpansionItemAtIndex(Slot.SlotIndex);
		// The destroyed actor stays referenced in the slot until garbage collection; it must no longer be valid.
		const bool bPacked = Item != nullptr && Item->ExpansionStatus == EBuildingExpansionStatus::BXS_PackedUp
			&& Item->ExpansionType == Slot.Option.ExpansionType && not IsValid(Item->Expansion);
		Check(bPacked, FString::Printf(TEXT("slot%d_packed_with_type"), Slot.SlotIndex),
		      Item != nullptr
			      ? UEnum::GetValueAsString(Item->ExpansionType) + TEXT(" ") + UEnum::GetValueAsString(Item->ExpansionStatus)
			      : FString(TEXT("no item")));
	}
}

void UBxpPackUpRedeployTestSubsystem::TickWaitingForRedeploy()
{
	using namespace BxpPackUpRedeployTestPrivate;
	if (not TickDeployAndGetIsBuilding(RedeployYaw))
	{
		if (GetStageElapsed() > DeployTimeoutSeconds)
		{
			FailAndFinish(TEXT("hq_redeploy_finished"));
		}
		return;
	}
	Check(true, TEXT("hq_redeploy_finished"),
	      FString::Printf(TEXT("building_yaw=%s"), *YawText(M_Hq->GetAttachToMeshComponent()->GetComponentRotation().Yaw)));
	EnterStage(EBxpPackUpRedeployTestStage::WaitingForReplacedBxps);
}

void UBxpPackUpRedeployTestSubsystem::TickWaitingForReplacedBxps()
{
	using namespace BxpPackUpRedeployTestPrivate;
	const ANomadicVehicle* Hq = M_Hq.Get();
	if (not IsValid(Hq))
	{
		FailAndFinish(TEXT("hq_valid_after_redeploy"));
		return;
	}
	// Wait until the batch load has placed what it is going to place, then judge every slot.
	const bool bBatchLoading = Hq->GetIsAsyncBatchLoadingAttachedPackedExpansions();
	bool bAllBack = true;
	for (const FBxpPackUpRedeployTestSlot& Slot : M_Slots)
	{
		bAllBack &= IsValid(Hq->GetBuildingExpansionAtIndex(Slot.SlotIndex));
	}
	if ((bBatchLoading || not bAllBack) && GetStageElapsed() <= ReplacedBxpTimeoutSeconds)
	{
		return;
	}
	Check(not bBatchLoading, TEXT("batch_load_finished"));
	const FBuildingExpansionItem* EmptyItem = Hq->GetBuildingExpansionItemAtIndex(EmptySlotIndex);
	Check(EmptyItem != nullptr && EmptyItem->Expansion == nullptr
	      && EmptyItem->ExpansionType == EBuildingExpansionType::BXT_Invalid,
	      TEXT("empty_slot0_still_empty_after_redeploy"),
	      EmptyItem != nullptr ? UEnum::GetValueAsString(EmptyItem->ExpansionType) : FString(TEXT("no item")));
	for (const FBxpPackUpRedeployTestSlot& Slot : M_Slots)
	{
		CheckReplacedSlot(Slot);
	}
	CheckNoSlotLeftPacked();
	Finish();
}

void UBxpPackUpRedeployTestSubsystem::CheckReplacedSlot(const FBxpPackUpRedeployTestSlot& Slot)
{
	using namespace BxpPackUpRedeployTestPrivate;
	const ANomadicVehicle* Hq = M_Hq.Get();
	const FBuildingExpansionItem* Item = Hq->GetBuildingExpansionItemAtIndex(Slot.SlotIndex);
	const ABuildingExpansion* Bxp = Item != nullptr ? Item->Expansion : nullptr;
	const FString SlotName = FString::Printf(TEXT("slot%d"), Slot.SlotIndex);
	Check(IsValid(Bxp) && Item->ExpansionType == Slot.Option.ExpansionType
	      && Bxp->GetBuildingExpansionType() == Slot.Option.ExpansionType,
	      SlotName + TEXT("_same_expansion_back_in_same_slot"),
	      Item != nullptr ? UEnum::GetValueAsString(Item->ExpansionType) + TEXT(" ")
	                        + UEnum::GetValueAsString(Item->ExpansionStatus) : FString(TEXT("no item")));
	if (not IsValid(Bxp))
	{
		return;
	}
	Check(Item->BxpConstructionRules.SocketName == Slot.SocketName, SlotName + TEXT("_same_socket"),
	      Item->BxpConstructionRules.SocketName.ToString());
	const UStaticMeshComponent* AttachMesh = Hq->GetAttachToMeshComponent();
	const FTransform NowRelative = Bxp->GetActorTransform().GetRelativeTransform(AttachMesh->GetComponentTransform());
	const float NowBuildingYaw = AttachMesh->GetComponentRotation().Yaw;
	Check(NowRelative.GetLocation().Equals(Slot.RelativeToBuilding.GetLocation(), LocationTolerance),
	      SlotName + TEXT("_same_location_on_building"),
	      FString::Printf(TEXT("first=%s now=%s"), *Slot.RelativeToBuilding.GetLocation().ToCompactString(),
	                      *NowRelative.GetLocation().ToCompactString()));
	if (Slot.Option.BxpConstructionRules.ConstructionType == EBxpConstructionType::Socket)
	{
		Check(GetIsYawEqual(NowRelative.Rotator().Yaw, Slot.RelativeToBuilding.Rotator().Yaw),
		      SlotName + TEXT("_socket_same_rotation_on_building"),
		      FString::Printf(TEXT("first_relative=%s now_relative=%s"), *YawText(Slot.RelativeToBuilding.Rotator().Yaw),
		                      *YawText(NowRelative.Rotator().Yaw)));
		return;
	}
	CheckOriginRotationAgainstBuilding(Slot, Bxp->GetActorRotation(), NowBuildingYaw);
}

void UBxpPackUpRedeployTestSubsystem::CheckOriginRotationAgainstBuilding(
	const FBxpPackUpRedeployTestSlot& Slot, const FRotator& NowRotation, const float NowBuildingYaw)
{
	using namespace BxpPackUpRedeployTestPrivate;
	// Origin expansions turn with their building (yaw only), at the first placement and on re-placement, so the
	// rotation relative to the building is 0 both times.
	const FString SlotName = FString::Printf(TEXT("slot%d"), Slot.SlotIndex);
	const float FirstRelativeYaw = Slot.WorldRotation.Yaw - Slot.BuildingYaw;
	const float NowRelativeYaw = NowRotation.Yaw - NowBuildingYaw;
	UE_LOG(LogRTSBxpRedeployTest, Display,
	       TEXT("RTS_BXP_REDEPLOY_TEST ORIGIN_ROTATION first_world=%s first_building=%s now_world=%s now_building=%s "
		       "first_relative=%s now_relative=%s"),
	       *YawText(Slot.WorldRotation.Yaw), *YawText(Slot.BuildingYaw), *YawText(NowRotation.Yaw),
	       *YawText(NowBuildingYaw), *YawText(FirstRelativeYaw), *YawText(NowRelativeYaw));
	Check(GetIsYawEqual(FirstRelativeYaw, 0.f), SlotName + TEXT("_origin_faces_building_at_first_placement"),
	      FString::Printf(TEXT("first_relative=%s"), *YawText(FirstRelativeYaw)));
	Check(GetIsYawEqual(NowRelativeYaw, FirstRelativeYaw), SlotName + TEXT("_origin_same_rotation_on_building"),
	      FString::Printf(TEXT("first_relative=%s now_relative=%s"), *YawText(FirstRelativeYaw),
	                      *YawText(NowRelativeYaw)));
}

void UBxpPackUpRedeployTestSubsystem::CheckNoSlotLeftPacked()
{
	const ANomadicVehicle* Hq = M_Hq.Get();
	const TArray<FBuildingExpansionItem>& Items = Hq->GetBuildingExpansions();
	int32 PackedCount = 0;
	for (const FBuildingExpansionItem& Item : Items)
	{
		PackedCount += Item.ExpansionStatus == EBuildingExpansionStatus::BXS_PackedUp ? 1 : 0;
	}
	Check(PackedCount == 0, TEXT("no_slot_left_packed_after_redeploy"), FString::Printf(TEXT("packed=%d"), PackedCount));
}

#else

bool UBxpPackUpRedeployTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UBxpPackUpRedeployTestSubsystem::Tick(const float DeltaTime)
{
}

TStatId UBxpPackUpRedeployTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBxpPackUpRedeployTestSubsystem, STATGROUP_Tickables);
}

#endif
