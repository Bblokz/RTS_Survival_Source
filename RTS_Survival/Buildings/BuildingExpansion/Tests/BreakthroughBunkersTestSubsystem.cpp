// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.
#include "BreakthroughBunkersTestSubsystem.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS
#include "EngineUtils.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Environment/DestructableEnvActor/WallChunkDamage/WallHoleDamageComponent.h"
#include "RTS_Survival/FactionSystem/Factions/Factions.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingMenuManager.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainerComponent/TrainerComponent.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingLayout/TrainingLayoutTypes.h"
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
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Requirement/RTSRequirement.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSBtBunkersTest, Log, All);

namespace BreakthroughBunkersTestPrivate
{
	constexpr float StartDelaySeconds = 4.f;
	constexpr float CameraTraceLength = 500000.f;
	// Speeds up the 40 s pours; the deploy is fast-forwarded.
	constexpr float TestTimeDilation = 10.f;
	constexpr float FastForwardDelaySeconds = 1.f;
	constexpr float TruckSpawnTimeoutSeconds = 30.f;
	constexpr float DeployTimeoutSeconds = 60.f;
	constexpr float BunkerBuildTimeoutSeconds = 60.f;
	constexpr float SpotMatchDistance = 150.f;
	constexpr float SpotSearchMinRadius = 1800.f;
	constexpr float SpotSearchMaxRadius = 5000.f;
	constexpr float SpotSearchRadiusStep = 400.f;
	constexpr float SpotSearchAngleStep = 20.f;
	constexpr float GroundTraceHalfHeight = 5000.f;
	constexpr int32 TestResourceAmount = 1000000;
	constexpr int32 VisibleSlotsPerSubSection = 9;
	const EBuildingExpansionType Bunkers[] = {
		EBuildingExpansionType::BTX_GerFlak88Bunker, EBuildingExpansionType::BTX_GerFlakvierlingBunker,
		EBuildingExpansionType::BTX_GerPZ38TBunker
	};
	constexpr int32 BunkerCount = UE_ARRAY_COUNT(Bunkers);

	FTrainingOption MakeOption(const EBuildingExpansionType Type)
	{
		return FTrainingOption(EAllUnitType::UNType_BuildingExpansion, static_cast<uint8>(Type));
	}

	FIntPoint GetPrice(const EBuildingExpansionType Type)
	{
		using namespace DeveloperSettings::GameBalance::UnitCosts::Bxps;
		switch (Type)
		{
		case EBuildingExpansionType::BTX_GerFlak88Bunker:
			return FIntPoint(GerFlak88Bunker_Radixite, GerFlak88Bunker_Metal);
		case EBuildingExpansionType::BTX_GerFlakvierlingBunker:
			return FIntPoint(GerFlakvierlingBunker_Radixite, GerFlakvierlingBunker_Metal);
		case EBuildingExpansionType::BTX_GerPZ38TBunker:
			return FIntPoint(GerPZ38TBunker_Radixite, GerPZ38TBunker_Metal);
		default:
			return FIntPoint::ZeroValue;
		}
	}
}

bool UBreakthroughBunkersTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), TEXT("BreakthroughBunkersValidate"));
}

TStatId UBreakthroughBunkersTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBreakthroughBunkersTestSubsystem, STATGROUP_Tickables);
}

void UBreakthroughBunkersTestSubsystem::Tick(const float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	switch (M_Stage)
	{
	case EBreakthroughBunkersTestStage::WaitingForStart:
		TickWaitingForStart();
		return;
	case EBreakthroughBunkersTestStage::WaitingForTruckSpawn:
		TickWaitingForTruckSpawn();
		return;
	case EBreakthroughBunkersTestStage::WaitingForDeploy:
		TickWaitingForDeploy();
		return;
	case EBreakthroughBunkersTestStage::PlacingNextBunker:
		TickPlacingNextBunker();
		return;
	case EBreakthroughBunkersTestStage::WaitingForBunkerBuilt:
		TickWaitingForBunkerBuilt();
		return;
	case EBreakthroughBunkersTestStage::Finished:
		return;
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Start, HQ truck and deploy
// ---------------------------------------------------------------------------------------------------------------

void UBreakthroughBunkersTestSubsystem::TickWaitingForStart()
{
	using namespace BreakthroughBunkersTestPrivate;
	if (not StartGameAndGetIsReady())
	{
		return;
	}
	if (M_BootSeconds < 0.f)
	{
		M_BootSeconds = GetNow();
		// The HQ's training tabs are built from the player's doctrine when it deploys.
		if (URTSGameInstance* GameInstance = GetWorld()->GetGameInstance<URTSGameInstance>())
		{
			GameInstance->SetPlayerFaction(ERTSFaction::GerBreakthroughDoctrine);
		}
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
	UE_LOG(LogRTSBtBunkersTest, Display, TEXT("RTS_BTBUNKERS_TEST START anchor=%s"), *M_Anchor.ToCompactString());
	GivePlayerResources();
	ARTSAsyncSpawner* Spawner = FRTS_Statics::GetAsyncSpawner(this);
	if (not IsValid(Spawner))
	{
		FailAndFinish(TEXT("async_spawner_found"));
		return;
	}
	TWeakObjectPtr<UBreakthroughBunkersTestSubsystem> WeakThis(this);
	const bool bRequested = Spawner->AsyncSpawnOptionAtLocation(
		FTrainingOption(EAllUnitType::UNType_Nomadic, static_cast<uint8>(ENomadicSubtype::Nomadic_GerHq)),
		M_Anchor, this, 0,
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
	EnterStage(EBreakthroughBunkersTestStage::WaitingForTruckSpawn);
}

void UBreakthroughBunkersTestSubsystem::TickWaitingForTruckSpawn()
{
	using namespace BreakthroughBunkersTestPrivate;
	if (M_Hq.IsValid())
	{
		Check(true, TEXT("hq_truck_spawned"), M_Hq->GetName());
		UGameplayStatics::SetGlobalTimeDilation(this, TestTimeDilation);
		EnterStage(EBreakthroughBunkersTestStage::WaitingForDeploy);
		return;
	}
	if (GetStageElapsed() > TruckSpawnTimeoutSeconds)
	{
		FailAndFinish(TEXT("hq_truck_spawned"));
	}
}

void UBreakthroughBunkersTestSubsystem::TickWaitingForDeploy()
{
	using namespace BreakthroughBunkersTestPrivate;
	ANomadicVehicle* Hq = M_Hq.Get();
	if (not IsValid(Hq))
	{
		FailAndFinish(TEXT("hq_alive_while_deploying"));
		return;
	}
	UTrainerComponent* Trainer = Hq->GetTrainerComponent();
	if (Hq->GetNomadicStatus() == ENomadStatus::Building && IsValid(Trainer) && Trainer->GetIsAbleToTrain())
	{
		Check(true, TEXT("hq_deployed_and_training"));
		M_Trainer = Trainer;
		CheckBarriersTab();
		EnterStage(EBreakthroughBunkersTestStage::PlacingNextBunker);
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
		FailAndFinish(TEXT("hq_deployed_and_training"));
	}
}

void UBreakthroughBunkersTestSubsystem::CheckBarriersTab()
{
	using namespace BreakthroughBunkersTestPrivate;
	const UTrainerComponent* Trainer = M_Trainer.Get();
	for (const FTrainingSubSectionState& SubSection : Trainer->GetTrainingSubSections())
	{
		Check(SubSection.Options.Num() <= VisibleSlotsPerSubSection, TEXT("sub_section_fits_nine_slots"),
		      FString::Printf(TEXT("%s=%d"), *SubSection.DisplayName.ToString(), SubSection.Options.Num()));
	}
	const FTrainingSubSectionState* Barriers = Trainer->GetTrainingSubSections().FindByPredicate(
		[](const FTrainingSubSectionState& SubSection)
		{
			return SubSection.DisplayName.ToString() == TEXT("Barriers");
		});
	Check(Barriers != nullptr, TEXT("hq_has_barriers_tab"));
	if (Barriers == nullptr)
	{
		return;
	}
	Check(Barriers->Options.Contains(MakeOption(EBuildingExpansionType::BTX_GerRailPakBunker)),
	      TEXT("rail_pak_bunker_in_barriers_tab"));
	const ACPPController* Controller = GetPlayerController();
	const UMainGameUI* MainGameUI = IsValid(Controller) ? Controller->GetMainMenuUI() : nullptr;
	const UTrainingMenuManager* MenuManager = IsValid(MainGameUI) ? MainGameUI->GetTrainingMenuManager() : nullptr;
	const FTrainingOption RequiredTierTwo(EAllUnitType::UNType_Nomadic,
	                                      static_cast<uint8>(ENomadicSubtype::Nomadic_GerCommunicationCenter));
	for (const EBuildingExpansionType Bunker : Bunkers)
	{
		const FString Name = Global_GetBxpTypeEnumAsString(Bunker);
		Check(Barriers->Options.Contains(MakeOption(Bunker)), Name + TEXT("_in_barriers_tab"));
		const URTSRequirement* Requirement = IsValid(MenuManager)
			                                     ? MenuManager->GetTrainingOptionState(MakeOption(Bunker)).RTSRequirement
			                                     : nullptr;
		Check(IsValid(Requirement) && Requirement->GetRequiredUnit() == RequiredTierTwo,
		      Name + TEXT("_needs_communication_center"));
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Planning and building each bunker
// ---------------------------------------------------------------------------------------------------------------

void UBreakthroughBunkersTestSubsystem::TickPlacingNextBunker()
{
	using namespace BreakthroughBunkersTestPrivate;
	++M_BunkerIndex;
	if (M_BunkerIndex >= BunkerCount)
	{
		Finish();
		return;
	}
	ACPPController* Controller = GetPlayerController();
	UTrainerComponent* Trainer = M_Trainer.Get();
	const FString Name = Global_GetBxpTypeEnumAsString(GetCurrentType());
	if (not IsValid(Controller) || not IsValid(Trainer) || not IsValid(Controller->CPPConstructionPreviewRef))
	{
		FailAndFinish(Name + TEXT("_controller_and_trainer_valid"));
		return;
	}
	// The click on the option; the Communication Center requirement is left out so no tier II building is needed.
	Controller->StartFieldDefensePlacement(Trainer, MakeQueueItem(MakeOption(GetCurrentType())), nullptr);
	Check(Controller->MapTest_GetIsFieldDefensePreviewActive(), Name + TEXT("_preview_active"));
	FVector Spot;
	if (not FindValidSpot(Spot))
	{
		FailAndFinish(Name + TEXT("_valid_spot_found"));
		return;
	}
	M_RadixiteBeforePlan = GetResource(ERTSResourceType::Resource_Radixite);
	M_MetalBeforePlan = GetResource(ERTSResourceType::Resource_Metal);
	float InclineDegrees = 0.f;
	const bool bPlanned = Controller->MapTest_PlaceFieldDefenseAt(Spot, InclineDegrees);
	M_PlannedSpot = Controller->CPPConstructionPreviewRef->GetFieldDefensePlacementLocation();
	Check(bPlanned, Name + TEXT("_planned_by_click"), M_PlannedSpot.ToCompactString());
	if (not bPlanned)
	{
		Finish();
		return;
	}
	EnterStage(EBreakthroughBunkersTestStage::WaitingForBunkerBuilt);
}

void UBreakthroughBunkersTestSubsystem::TickWaitingForBunkerBuilt()
{
	using namespace BreakthroughBunkersTestPrivate;
	ABuildingExpansion* Bunker = FindBunkerNear(M_PlannedSpot);
	// Every frame: the last seen expansion of the type under test, so the frame it leaves the world is logged.
	if (ABuildingExpansion* Traced = M_Traced.Get())
	{
		const UHealthComponent* TracedHealth = Traced->FindComponentByClass<UHealthComponent>();
		M_TracedState = FString::Printf(TEXT("%s status=%d alive=%d hp=%.0f at=%s"), *Traced->GetName(),
		                                static_cast<int32>(Traced->GetBuildingExpansionStatus()),
		                                Traced->IsUnitAlive() ? 1 : 0,
		                                IsValid(TracedHealth) ? TracedHealth->GetCurrentHealth() : -1.f,
		                                *Traced->GetActorLocation().ToCompactString());
	}
	else if (not M_TracedState.IsEmpty())
	{
		UE_LOG(LogRTSBtBunkersTest, Display, TEXT("RTS_BTBUNKERS_TEST TRACE vanished t=%.2f last: %s"),
		       GetStageElapsed(), *M_TracedState);
		M_TracedState.Reset();
	}
	// Once a second: every expansion of the type under test, wherever it stands, with its state and health.
	const int32 WholeSeconds = FMath::FloorToInt32(GetStageElapsed());
	if (WholeSeconds != M_LastTraceSecond)
	{
		M_LastTraceSecond = WholeSeconds;
		for (TActorIterator<ABuildingExpansion> It(GetWorld()); It; ++It)
		{
			if (IsValid(*It) && It->GetBuildingExpansionType() == GetCurrentType())
			{
				M_Traced = *It;
				const UHealthComponent* Health = It->FindComponentByClass<UHealthComponent>();
				UE_LOG(LogRTSBtBunkersTest, Display, TEXT("RTS_BTBUNKERS_TEST TRACE t=%d %s status=%d field=%d at=%s hp=%.0f"),
				       WholeSeconds, *It->GetName(), static_cast<int32>(It->GetBuildingExpansionStatus()),
				       It->GetIsFieldDefense() ? 1 : 0, *It->GetActorLocation().ToCompactString(),
				       IsValid(Health) ? Health->GetCurrentHealth() : -1.f);
			}
		}
	}
	if (not IsValid(Bunker) || Bunker->GetBuildingExpansionStatus() != EBuildingExpansionStatus::BXS_Built)
	{
		if (GetStageElapsed() > BunkerBuildTimeoutSeconds)
		{
			// Every field defense in the world, so a bunker built off its planned spot or stuck mid-pour shows.
			FString Seen;
			for (TActorIterator<ABuildingExpansion> It(GetWorld()); It; ++It)
			{
				if (IsValid(*It) && It->GetIsFieldDefense())
				{
					Seen += FString::Printf(TEXT(" %s status=%d at=%s"), *It->GetName(),
					                        static_cast<int32>(It->GetBuildingExpansionStatus()),
					                        *It->GetActorLocation().ToCompactString());
				}
			}
			FailAndFinish(Global_GetBxpTypeEnumAsString(GetCurrentType()) + TEXT("_built"),
			              FString::Printf(TEXT("planned=%s near=%s seen:%s"), *M_PlannedSpot.ToCompactString(),
			                              *GetNameSafe(Bunker), *Seen));
		}
		return;
	}
	CheckBuiltBunker(*Bunker);
	EnterStage(EBreakthroughBunkersTestStage::PlacingNextBunker);
}

void UBreakthroughBunkersTestSubsystem::CheckBuiltBunker(ABuildingExpansion& Bunker)
{
	using namespace BreakthroughBunkersTestPrivate;
	const EBuildingExpansionType Type = GetCurrentType();
	const FString Name = Global_GetBxpTypeEnumAsString(Type);
	Check(true, Name + TEXT("_built"), Bunker.GetName());
	Check(Bunker.GetBuildingExpansionType() == Type, Name + TEXT("_type_from_blueprint"),
	      UEnum::GetValueAsString(Bunker.GetBuildingExpansionType()));
	Check(Bunker.GetIsFieldDefense(), Name + TEXT("_is_field_defense"));
	const FIntPoint Price = GetPrice(Type);
	const int32 RadixitePaid = M_RadixiteBeforePlan - GetResource(ERTSResourceType::Resource_Radixite);
	const int32 MetalPaid = M_MetalBeforePlan - GetResource(ERTSResourceType::Resource_Metal);
	Check(RadixitePaid == Price.X && MetalPaid == Price.Y, Name + TEXT("_price_taken"),
	      FString::Printf(TEXT("radixite=%d metal=%d expected=%d/%d"), RadixitePaid, MetalPaid, Price.X, Price.Y));
	Check(IsValid(Bunker.FindComponentByClass<UWallHoleDamageComponent>()), Name + TEXT("_has_procedural_holes"));
	const TArray<ACPPTurretsMaster*> Turrets = Bunker.GetTurrets();
	Check(Turrets.Num() > 0 && IsValid(Turrets[0]) && Turrets[0]->GetWeapons().Num() > 0, Name + TEXT("_has_armed_turret"),
	      FString::Printf(TEXT("turrets=%d"), Turrets.Num()));
}

EBuildingExpansionType UBreakthroughBunkersTestSubsystem::GetCurrentType() const
{
	using namespace BreakthroughBunkersTestPrivate;
	return M_BunkerIndex >= 0 && M_BunkerIndex < BunkerCount ? Bunkers[M_BunkerIndex] : EBuildingExpansionType::BXT_Invalid;
}

FTrainingQueueItem UBreakthroughBunkersTestSubsystem::MakeQueueItem(const FTrainingOption& Option) const
{
	// Built as the training menu builds it for a click on the option.
	FTrainingQueueItem QueueItem;
	QueueItem.ItemID = Option;
	const ACPPController* Controller = GetPlayerController();
	const UMainGameUI* MainGameUI = IsValid(Controller) ? Controller->GetMainMenuUI() : nullptr;
	const UTrainingMenuManager* MenuManager = IsValid(MainGameUI) ? MainGameUI->GetTrainingMenuManager() : nullptr;
	if (IsValid(MenuManager))
	{
		QueueItem.TotalTrainingTime = MenuManager->GetTrainingOptionState(Option).TrainingTime;
		QueueItem.RemainingTrainingTime = QueueItem.TotalTrainingTime;
	}
	if (const UTrainerComponent* Trainer = M_Trainer.Get())
	{
		QueueItem.WidgetIndex = Trainer->GetTrainingOptions().IndexOfByKey(Option);
	}
	bool bIsValidCosts = false;
	QueueItem.TotalResourceCosts = FRTS_Statics::GetResourceCostsOfTrainingOption(Option, bIsValidCosts, GetWorld());
	QueueItem.RemainingResourceCosts = QueueItem.TotalResourceCosts;
	return QueueItem;
}

bool UBreakthroughBunkersTestSubsystem::FindValidSpot(FVector& OutSpot) const
{
	using namespace BreakthroughBunkersTestPrivate;
	const ACPPController* Controller = GetPlayerController();
	const ANomadicVehicle* Hq = M_Hq.Get();
	const UWorld* World = GetWorld();
	if (not IsValid(Controller) || not IsValid(Hq) || not IsValid(Controller->CPPConstructionPreviewRef))
	{
		return false;
	}
	const FVector2D Center(Hq->GetActorLocation());
	const int32 AnglesPerRing = FMath::RoundToInt32(360.f / SpotSearchAngleStep);
	const int32 RingCount = FMath::FloorToInt32((SpotSearchMaxRadius - SpotSearchMinRadius) / SpotSearchRadiusStep) + 1;
	// Rings from the inside out, a full circle of candidates on each.
	for (int32 CandidateIndex = 0; CandidateIndex < RingCount * AnglesPerRing; ++CandidateIndex)
	{
		const float Radius = SpotSearchMinRadius + (CandidateIndex / AnglesPerRing) * SpotSearchRadiusStep;
		const float AngleRadians = FMath::DegreesToRadians((CandidateIndex % AnglesPerRing) * SpotSearchAngleStep);
		const FVector2D Candidate = Center + FVector2D(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians)) * Radius;
		const float Height = Hq->GetActorLocation().Z;
		FHitResult GroundHit;
		const FVector TraceStart(Candidate.X, Candidate.Y, Height + GroundTraceHalfHeight);
		const FVector TraceEnd(Candidate.X, Candidate.Y, Height - GroundTraceHalfHeight);
		if (not World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, COLLISION_TRACE_LANDSCAPE))
		{
			continue;
		}
		float InclineDegrees = 0.f;
		if (Controller->CPPConstructionPreviewRef->MapTest_EvaluateFieldDefensePlacement(GroundHit.ImpactPoint,
		                                                                                  InclineDegrees))
		{
			OutSpot = GroundHit.ImpactPoint;
			return true;
		}
	}
	return false;
}

ABuildingExpansion* UBreakthroughBunkersTestSubsystem::FindBunkerNear(const FVector& Spot) const
{
	for (TActorIterator<ABuildingExpansion> ExpansionIterator(GetWorld()); ExpansionIterator; ++ExpansionIterator)
	{
		ABuildingExpansion* Expansion = *ExpansionIterator;
		if (IsValid(Expansion) && Expansion->GetIsFieldDefense()
			&& FVector::Dist2D(Expansion->GetActorLocation(), Spot) < BreakthroughBunkersTestPrivate::SpotMatchDistance)
		{
			return Expansion;
		}
	}
	return nullptr;
}

// ---------------------------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------------------------

bool UBreakthroughBunkersTestSubsystem::StartGameAndGetIsReady() const
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

bool UBreakthroughBunkersTestSubsystem::FindAnchor(FVector& OutAnchor) const
{
	using namespace BreakthroughBunkersTestPrivate;
	// FParse::Value stops at commas, so the anchor comes as two values.
	float AnchorX = 0.f;
	float AnchorY = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("BtBunkersAnchorX="), AnchorX)
		&& FParse::Value(FCommandLine::Get(), TEXT("BtBunkersAnchorY="), AnchorY))
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

void UBreakthroughBunkersTestSubsystem::GivePlayerResources() const
{
	using namespace BreakthroughBunkersTestPrivate;
	const ACPPController* Controller = GetPlayerController();
	UPlayerResourceManager* Resources = IsValid(Controller) ? Controller->GetPlayerResourceManager() : nullptr;
	if (not IsValid(Resources))
	{
		return;
	}
	for (const ERTSResourceType ResourceType : {
		     ERTSResourceType::Resource_Radixite, ERTSResourceType::Resource_Metal,
		     ERTSResourceType::Resource_VehicleParts, ERTSResourceType::Resource_Fuel
	     })
	{
		Resources->CheatAddStorage(ResourceType, TestResourceAmount);
		Resources->AddResource(ResourceType, TestResourceAmount);
	}
}

int32 UBreakthroughBunkersTestSubsystem::GetResource(const ERTSResourceType ResourceType) const
{
	const ACPPController* Controller = GetPlayerController();
	const UPlayerResourceManager* Resources = IsValid(Controller) ? Controller->GetPlayerResourceManager() : nullptr;
	return IsValid(Resources) ? Resources->GetResourceAmount(ResourceType) : 0;
}

ACPPController* UBreakthroughBunkersTestSubsystem::GetPlayerController() const
{
	return FRTS_Statics::GetRTSController(GetWorld());
}

void UBreakthroughBunkersTestSubsystem::Check(const bool bCondition, const FString& Description,
                                              const FString& Details)
{
	if (bCondition)
	{
		++M_PassedCheckCount;
	}
	else
	{
		++M_FailedCheckCount;
	}
	UE_LOG(LogRTSBtBunkersTest, Display, TEXT("RTS_BTBUNKERS_TEST CHECK %s %s %s"),
	       bCondition ? TEXT("PASS") : TEXT("FAIL"), *Description, *Details);
}

void UBreakthroughBunkersTestSubsystem::EnterStage(const EBreakthroughBunkersTestStage NewStage)
{
	M_Stage = NewStage;
	M_StageSeconds = GetNow();
	bM_StageActionDone = false;
}

float UBreakthroughBunkersTestSubsystem::GetNow() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetRealTimeSeconds() : 0.f;
}

float UBreakthroughBunkersTestSubsystem::GetStageElapsed() const
{
	return GetNow() - M_StageSeconds;
}

void UBreakthroughBunkersTestSubsystem::FailAndFinish(const FString& Description, const FString& Details)
{
	Check(false, Description, Details);
	Finish();
}

void UBreakthroughBunkersTestSubsystem::Finish()
{
	const bool bPassed = M_FailedCheckCount == 0 && M_PassedCheckCount > 0;
	UE_LOG(LogRTSBtBunkersTest, Display, TEXT("RTS_BTBUNKERS_TEST RESULT %s passed=%d failed=%d"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"), M_PassedCheckCount, M_FailedCheckCount);
	EnterStage(EBreakthroughBunkersTestStage::Finished);
	UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
	if (APlayerController* Controller = GetPlayerController())
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}

#else

bool UBreakthroughBunkersTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UBreakthroughBunkersTestSubsystem::Tick(const float DeltaTime)
{
}

TStatId UBreakthroughBunkersTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBreakthroughBunkersTestSubsystem, STATGROUP_Tickables);
}

#endif
