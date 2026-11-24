// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyHarvestTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RTS_Survival/Enemy/StrategicAI/BlackboardIdleUnitEntry.h"
#include "RTS_Survival/Enemy/StrategicAI/StrategicAIHelpers.h"
#include "RTS_Survival/Enemy/StrategicAI/BlackboardIdleUnitsResult/FBlackboardIdleUnitsResult.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/Game/GameState/GameResourceManager/GameResourceManager.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/Resources/Harvester/Harvester.h"
#include "RTS_Survival/Resources/Harvester/HarvesterStatus/HarvesterStatus.h"
#include "RTS_Survival/Resources/ResourceComponent/ResourceComponent.h"
#include "RTS_Survival/Resources/ResourceDropOff/ResourceDropOff.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSEnemyHarvestTest, Log, All);

namespace EnemyHarvestTestPrivate
{
	const TCHAR* const ValidateSwitch = TEXT("EnemyHarvestValidate");
	const TCHAR* const AnchorSwitch = TEXT("EnemyHarvestAt=");
	const TCHAR* const HarvesterPath =
		TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Light/Panzer1/Ger_Harvester_Panzer1.Ger_Harvester_Panzer1_C");
	const TCHAR* const CubeMeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");
	const FName DropOffSocketName = TEXT("DropOff");
	constexpr uint8 HumanPlayer = 1;
	constexpr uint8 EnemyPlayer = 2;
	constexpr float StartDelaySeconds = 3.f;
	constexpr float UnitSetupSeconds = 3.f;
	constexpr float HarvestTimeoutSeconds = 420.f;
	constexpr float RetargetTimeoutSeconds = 180.f;
	constexpr float IdleAfterDrainAllTimeoutSeconds = 150.f;
	// Idle this long before the retry field appears, so only the retry tick can pick it up.
	constexpr float IdleBeforeRetryFieldSeconds = 3.f;
	// The autonomous retry runs every 10 s; the async field data refreshes every 1.25 s.
	constexpr float RetryTimeoutSeconds = 25.f;
	constexpr float FreshFieldSideOffsetCm = 3500.f;
	// Crossed layout along one axis through the field: each harvester starts next to the other player's drop-off, so
	// only the owner filter sends it to its own one.
	constexpr float HarvesterOffsetCm = 1800.f;
	constexpr float DropOffOffsetCm = 2600.f;
	constexpr float HarvesterSpawnLiftCm = 150.f;
	constexpr float DropOffCubeHalfHeightCm = 50.f;
	constexpr int32 DropOffRadixiteCapacity = 3000;
	constexpr int32 LayoutDirectionAttempts = 4;
	constexpr float LayoutDirectionStepDegrees = 90.f;
	// Safety net for a harvest animation that never ends; the Pz I Blueprint ends its own after about 30 s.
	constexpr float ForceAnimationFinishSeconds = 60.f;
	constexpr int32 StockRemovalProbe = 10;
	constexpr int32 MaxDrainCalls = 100000;
	constexpr int32 FramesBeforeExit = 3;
}

bool UEnemyHarvestTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), EnemyHarvestTestPrivate::ValidateSwitch);
}

TStatId UEnemyHarvestTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UEnemyHarvestTestSubsystem, STATGROUP_Tickables);
}

void UEnemyHarvestTestSubsystem::Tick(const float DeltaTime)
{
	if (bM_IsExitPending)
	{
		TickExitPending();
		return;
	}
	const UWorld* World = GetWorld();
	if (M_Phase == EEnemyHarvestTestPhase::Inactive || not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	if (M_PhaseStartSeconds < 0.f)
	{
		M_PhaseStartSeconds = World->GetTimeSeconds();
		UE_LOG(LogRTSEnemyHarvestTest, Display, TEXT("RTS_ENEMY_HARVEST_TEST started map=%s"), *World->GetName());
	}
	switch (M_Phase)
	{
	case EEnemyHarvestTestPhase::WaitingForStart:
		TickWaitingForStart();
		return;
	case EEnemyHarvestTestPhase::WaitingForUnits:
		TickWaitingForUnits();
		return;
	case EEnemyHarvestTestPhase::Harvesting:
		TickHarvesting(DeltaTime);
		return;
	case EEnemyHarvestTestPhase::WaitingForRetarget:
		TickWaitingForRetarget(DeltaTime);
		return;
	case EEnemyHarvestTestPhase::WaitingForIdleAfterDrainAll:
		TickWaitingForIdleAfterDrainAll(DeltaTime);
		return;
	case EEnemyHarvestTestPhase::WaitingForRetry:
		TickWaitingForRetry(DeltaTime);
		return;
	default:
		return;
	}
}

void UEnemyHarvestTestSubsystem::TickWaitingForStart()
{
	using namespace EnemyHarvestTestPrivate;
	StartGameAndUnpause();
	if (GetPhaseSeconds() < StartDelaySeconds)
	{
		return;
	}
	UResourceComponent* Field = FindTestField();
	Check(IsValid(Field), TEXT("radixite_field_found"));
	if (not IsValid(Field))
	{
		Finish();
		return;
	}
	M_Field = Field;
	if (IsValid(Field) && IsValid(Field->GetOwner()))
	{
		M_FieldClass = Field->GetOwner()->GetClass();
		M_FieldTransform = Field->GetOwner()->GetActorTransform();
	}
	const FVector FieldLocation = Field->GetResourceLocationNotThreadSafe();

	// The first direction where all four places have ground.
	FVector Axis = FVector::ForwardVector;
	TArray<FVector> Grounds;
	for (int32 Attempt = 0; Attempt < LayoutDirectionAttempts; ++Attempt)
	{
		Axis = FVector::ForwardVector.RotateAngleAxis(Attempt * LayoutDirectionStepDegrees, FVector::UpVector);
		Grounds.Reset();
		for (const float Offset : {HarvesterOffsetCm, DropOffOffsetCm, -HarvesterOffsetCm, -DropOffOffsetCm})
		{
			FVector Ground;
			if (GlobalAbilityHelpers::TryFindGroundLocation(this, FieldLocation + Axis * Offset, Ground))
			{
				Grounds.Add(Ground);
			}
		}
		if (Grounds.Num() == 4)
		{
			break;
		}
	}
	M_LayoutAxis = Axis;
	Check(Grounds.Num() == 4, TEXT("layout_has_ground"), FieldLocation.ToString());
	if (Grounds.Num() != 4)
	{
		Finish();
		return;
	}

	// The player-2 drop-off may not add to the human's storage; the player-1 one adds its capacity as always.
	const int32 StorageBefore = GetHumanRadixiteStorage();
	M_EnemyDropOff = SpawnDropOffActor(Grounds[1], EnemyPlayer);
	const int32 StorageAfterEnemyDropOff = GetHumanRadixiteStorage();
	M_PlayerDropOff = SpawnDropOffActor(Grounds[3], HumanPlayer);
	const int32 StorageAfterPlayerDropOff = GetHumanRadixiteStorage();
	Check(M_EnemyDropOff.IsValid() && M_PlayerDropOff.IsValid(), TEXT("dropoffs_spawned"));
	Check(StorageAfterEnemyDropOff == StorageBefore, TEXT("enemy_dropoff_not_in_human_storage"),
	      FString::Printf(TEXT("before=%d after=%d"), StorageBefore, StorageAfterEnemyDropOff));
	Check(StorageAfterPlayerDropOff == StorageAfterEnemyDropOff + DropOffRadixiteCapacity,
	      TEXT("player_dropoff_in_human_storage"),
	      FString::Printf(TEXT("before=%d after=%d"), StorageAfterEnemyDropOff, StorageAfterPlayerDropOff));

	const ACPPGameState* GameState = GetWorld()->GetGameState<ACPPGameState>();
	const UGameResourceManager* ResourceManager = IsValid(GameState) ? GameState->GetGameResourceManager() : nullptr;
	Check(IsValid(ResourceManager)
	      && ResourceManager->GetCopyOfResourceDropOffsOfPlayer(EnemyPlayer).Contains(M_EnemyDropOff)
	      && not ResourceManager->GetCopyOfResourceDropOffsOfPlayer(HumanPlayer).Contains(M_EnemyDropOff),
	      TEXT("enemy_dropoff_registered_for_player_2"));

	// Each harvester starts next to the other player's drop-off.
	M_PlayerHarvester = MakeTestHarvester(SpawnHarvester(Grounds[0], HumanPlayer), HumanPlayer);
	M_EnemyHarvester = MakeTestHarvester(SpawnHarvester(Grounds[2], EnemyPlayer), EnemyPlayer);
	Check(M_PlayerHarvester.Harvester.IsValid() && M_EnemyHarvester.Harvester.IsValid(), TEXT("harvesters_spawned"));
	if (not M_PlayerHarvester.Harvester.IsValid() || not M_EnemyHarvester.Harvester.IsValid())
	{
		Finish();
		return;
	}
	UE_LOG(LogRTSEnemyHarvestTest, Display,
	       TEXT("RTS_ENEMY_HARVEST_TEST layout field=%s axis=%s p1_harvester=%s p2_dropoff=%s p2_harvester=%s p1_dropoff=%s"),
	       *GetNameSafe(Field->GetOwner()), *Axis.ToString(), *Grounds[0].ToString(), *Grounds[1].ToString(),
	       *Grounds[2].ToString(), *Grounds[3].ToString());
	EnterPhase(EEnemyHarvestTestPhase::WaitingForUnits);
}

UResourceComponent* UEnemyHarvestTestSubsystem::FindTestField() const
{
	const ACPPGameState* GameState = GetWorld()->GetGameState<ACPPGameState>();
	const UGameResourceManager* ResourceManager = IsValid(GameState) ? GameState->GetGameResourceManager() : nullptr;
	if (not IsValid(ResourceManager))
	{
		return nullptr;
	}
	FVector2D Anchor = FVector2D::ZeroVector;
	FString AnchorText;
	if (FParse::Value(FCommandLine::Get(), EnemyHarvestTestPrivate::AnchorSwitch, AnchorText))
	{
		FString X, Y;
		if (AnchorText.Split(TEXT(","), &X, &Y))
		{
			Anchor = FVector2D(FCString::Atof(*X), FCString::Atof(*Y));
		}
	}
	UResourceComponent* BestField = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	for (const TWeakObjectPtr<UResourceComponent>& Resource : ResourceManager->GetCopyOfMapResources())
	{
		if (not Resource.IsValid() || Resource->GetResourceType() != ERTSResourceType::Resource_Radixite
			|| not Resource->StillContainsResources())
		{
			continue;
		}
		const FVector Location = Resource->GetResourceLocationNotThreadSafe();
		const float DistanceSquared = FVector2D::DistSquared(FVector2D(Location.X, Location.Y), Anchor);
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestField = Resource.Get();
		}
	}
	return BestField;
}

UResourceDropOff* UEnemyHarvestTestSubsystem::SpawnDropOffActor(const FVector& Location, const uint8 OwningPlayer)
{
	using namespace EnemyHarvestTestPrivate;
	UWorld* World = GetWorld();
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, CubeMeshPath);
	if (not IsValid(World))
	{
		return nullptr;
	}
	const FTransform SpawnTransform(FRotator::ZeroRotator, Location + FVector(0.f, 0.f, DropOffCubeHalfHeightCm));
	AActor* DropOffActor = World->SpawnActorDeferred<AActor>(
		AActor::StaticClass(), SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (not IsValid(DropOffActor))
	{
		return nullptr;
	}
	UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(DropOffActor, TEXT("DropOffMesh"));
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetStaticMesh(CubeMesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
	DropOffActor->SetRootComponent(Mesh);
	DropOffActor->AddInstanceComponent(Mesh);
	Mesh->RegisterComponent();

	// The owner is set before BeginPlay, as FRTSUnitOwnerSpawning does for Blueprint units.
	URTSComponent* RTSComponent = NewObject<URTSComponent>(DropOffActor, TEXT("RTSComponent"));
	if (const FByteProperty* OwnerProperty = FindFProperty<FByteProperty>(
		URTSComponent::StaticClass(), TEXT("OwningPlayer")))
	{
		OwnerProperty->SetPropertyValue_InContainer(RTSComponent, OwningPlayer);
	}
	if (const FFloatProperty* RadiusProperty = FindFProperty<FFloatProperty>(
		URTSComponent::StaticClass(), TEXT("FormationUnitInnerRadius")))
	{
		RadiusProperty->SetPropertyValue_InContainer(RTSComponent, DropOffCubeHalfHeightCm);
	}
	RTSComponent->SetUnitType(EAllUnitType::UNType_BuildingExpansion);
	RTSComponent->SetAddToGameState(false);
	DropOffActor->AddInstanceComponent(RTSComponent);
	RTSComponent->RegisterComponent();

	// Not named "ResourceDropOff": the player-2 drop-off must be quiet because of its owner, not the old name rule.
	const FName DropOffName = OwningPlayer == HumanPlayer ? FName(TEXT("ResourceDropOff")) : FName(TEXT("EnemyStock"));
	UResourceDropOff* DropOff = NewObject<UResourceDropOff>(DropOffActor, DropOffName);
	DropOffActor->AddInstanceComponent(DropOff);
	DropOff->RegisterComponent();
	DropOffActor->FinishSpawning(SpawnTransform);

	TMap<ERTSResourceType, int32> Capacity;
	Capacity.Add(ERTSResourceType::Resource_Radixite, DropOffRadixiteCapacity);
	DropOff->MapTest_InitResourceDropOff(Capacity, Mesh, DropOffSocketName);
	return DropOff;
}

AActor* UEnemyHarvestTestSubsystem::SpawnHarvester(const FVector& Location, const uint8 OwningPlayer) const
{
	using namespace EnemyHarvestTestPrivate;
	UWorld* World = GetWorld();
	UClass* HarvesterClass = LoadClass<AActor>(nullptr, HarvesterPath);
	if (not IsValid(World) || not IsValid(HarvesterClass) || not M_Field.IsValid())
	{
		return nullptr;
	}
	const FVector ToField = M_Field->GetResourceLocationNotThreadSafe() - Location;
	const FTransform SpawnTransform(FRotator(0.f, ToField.Rotation().Yaw, 0.f),
	                                Location + FVector(0.f, 0.f, HarvesterSpawnLiftCm));
	return FRTSUnitOwnerSpawning::SpawnUnitForPlayer(*World, HarvesterClass, SpawnTransform, OwningPlayer);
}

FEnemyHarvestTestHarvester UEnemyHarvestTestSubsystem::MakeTestHarvester(AActor* HarvesterActor,
                                                                         const uint8 OwningPlayer) const
{
	FEnemyHarvestTestHarvester TestHarvester;
	TestHarvester.Actor = HarvesterActor;
	TestHarvester.Harvester = IsValid(HarvesterActor) ? HarvesterActor->FindComponentByClass<UHarvester>() : nullptr;
	TestHarvester.OwningPlayer = OwningPlayer;
	return TestHarvester;
}

void UEnemyHarvestTestSubsystem::TickWaitingForUnits()
{
	using namespace EnemyHarvestTestPrivate;
	StartGameAndUnpause();
	if (GetPhaseSeconds() < UnitSetupSeconds)
	{
		return;
	}
	UHarvester* EnemyHarvester = M_EnemyHarvester.Harvester.Get();
	ICommands* PlayerCommands = Cast<ICommands>(M_PlayerHarvester.Actor.Get());
	if (not IsValid(EnemyHarvester) || PlayerCommands == nullptr || not M_Field.IsValid())
	{
		Check(false, TEXT("units_ready"));
		Finish();
		return;
	}
	Check(EnemyHarvester->GetOwningPlayer() == EnemyPlayer, TEXT("enemy_harvester_owner"),
	      FString::FromInt(EnemyHarvester->GetOwningPlayer()));
	M_HumanRadixiteBefore = GetHumanRadixite();
	M_HumanStoredBefore = GetStoredRadixiteOfPlayer(HumanPlayer);
	M_EnemyStoredBefore = GetStoredRadixiteOfPlayer(EnemyPlayer);

	// No order for the AI harvester; the player's harvester gets a normal harvest order.
	EnemyHarvester->StartAutonomousHarvest(ERTSResourceType::Resource_Radixite);
	Check(EnemyHarvester->GetIsHarvestingAutonomously(), TEXT("autonomous_harvest_started"));
	const ECommandQueueError Error = PlayerCommands->HarvestResource(M_Field->GetOwner(), true);
	Check(Error == ECommandQueueError::NoError, TEXT("player_harvest_ordered"),
	      FString::FromInt(static_cast<int32>(Error)));
	UE_LOG(LogRTSEnemyHarvestTest, Display,
	       TEXT("RTS_ENEMY_HARVEST_TEST baseline human_radixite=%d p1_stored=%d p2_stored=%d"),
	       M_HumanRadixiteBefore, M_HumanStoredBefore, M_EnemyStoredBefore);
	EnterPhase(EEnemyHarvestTestPhase::Harvesting);
}

void UEnemyHarvestTestSubsystem::TickHarvesting(const float DeltaTime)
{
	using namespace EnemyHarvestTestPrivate;
	TrackHarvester(M_EnemyHarvester, DeltaTime);
	TrackHarvester(M_PlayerHarvester, DeltaTime);
	if (not M_EnemyHarvester.Actor.IsValid() || not M_PlayerHarvester.Actor.IsValid())
	{
		Check(false, TEXT("harvesters_alive"));
		FinishWithResults();
		return;
	}
	const bool bEnemyDelivered = GetStoredRadixiteOfPlayer(EnemyPlayer) > M_EnemyStoredBefore;
	const bool bPlayerDelivered = GetStoredRadixiteOfPlayer(HumanPlayer) > M_HumanStoredBefore;
	if (GetPhaseSeconds() > HarvestTimeoutSeconds)
	{
		Check(false, TEXT("both_delivered_in_time"),
		      FString::Printf(TEXT("p2_delivered=%d p1_delivered=%d"), bEnemyDelivered ? 1 : 0,
		                      bPlayerDelivered ? 1 : 0));
		FinishWithResults();
		return;
	}
	if (not bEnemyDelivered || not bPlayerDelivered)
	{
		return;
	}
	const UHarvester* EnemyHarvester = M_EnemyHarvester.Harvester.Get();
	if (not IsValid(EnemyHarvester) || not EnemyHarvester->GetTargetResource().IsValid())
	{
		// Wait until it has picked its next field again.
		return;
	}
	UE_LOG(LogRTSEnemyHarvestTest, Display, TEXT("RTS_ENEMY_HARVEST_TEST both_delivered after=%.1fs"),
	       GetPhaseSeconds());
	CheckAIPoolFilter();
	// The player-1 harvester has proven its part; left idle it grabs and empties the fresh fields this test spawns
	// for the AI harvester (small crystals take one harvester), so take it off the map.
	if (AActor* PlayerHarvesterActor = M_PlayerHarvester.Actor.Get())
	{
		PlayerHarvesterActor->Destroy();
	}
	DrainRetargetField();
	if (not bM_CanTestRetarget)
	{
		FinishWithResults();
		return;
	}
	EnterPhase(EEnemyHarvestTestPhase::WaitingForRetarget);
}

void UEnemyHarvestTestSubsystem::DrainRetargetField()
{
	using namespace EnemyHarvestTestPrivate;
	UResourceComponent* Field = M_EnemyHarvester.Harvester->GetTargetResource().Get();
	M_DrainedField = Field;
	// The test map's fields are small and the player-1 harvester empties them too: add a full one first so there
	// always is a next field to move on to.
	const FVector Side = M_LayoutAxis.RotateAngleAxis(LayoutDirectionStepDegrees, FVector::UpVector);
	const UResourceComponent* FreshField = SpawnFreshField(Side * FreshFieldSideOffsetCm);
	bM_CanTestRetarget = IsValid(FreshField);
	Check(bM_CanTestRetarget, TEXT("fresh_field_spawned"));
	if (not bM_CanTestRetarget)
	{
		return;
	}
	DrainField(*Field);
	UE_LOG(LogRTSEnemyHarvestTest, Display, TEXT("RTS_ENEMY_HARVEST_TEST drained field=%s fresh_field=%s"),
	       *GetNameSafe(Field->GetOwner()), *GetNameSafe(FreshField->GetOwner()));
}

UResourceComponent* UEnemyHarvestTestSubsystem::SpawnFreshField(const FVector& Offset) const
{
	UWorld* World = GetWorld();
	FVector Ground;
	if (not IsValid(World) || not M_FieldClass
		|| not GlobalAbilityHelpers::TryFindGroundLocation(this, M_FieldTransform.GetLocation() + Offset, Ground))
	{
		return nullptr;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const AActor* NewField = World->SpawnActor<AActor>(M_FieldClass,
	                                                  FTransform(M_FieldTransform.GetRotation(), Ground),
	                                                  SpawnParameters);
	return IsValid(NewField) ? NewField->FindComponentByClass<UResourceComponent>() : nullptr;
}

void UEnemyHarvestTestSubsystem::DrainField(UResourceComponent& Field) const
{
	int32 DrainCalls = 0;
	while (Field.StillContainsResources() && DrainCalls < EnemyHarvestTestPrivate::MaxDrainCalls)
	{
		(void)Field.HarvestResource(MAX_int32);
		++DrainCalls;
	}
}

void UEnemyHarvestTestSubsystem::DrainAllRadixite() const
{
	const ACPPGameState* GameState = GetWorld()->GetGameState<ACPPGameState>();
	const UGameResourceManager* ResourceManager = IsValid(GameState) ? GameState->GetGameResourceManager() : nullptr;
	if (not IsValid(ResourceManager))
	{
		return;
	}
	for (const TWeakObjectPtr<UResourceComponent>& Resource : ResourceManager->GetCopyOfMapResources())
	{
		if (Resource.IsValid() && Resource->GetResourceType() == ERTSResourceType::Resource_Radixite)
		{
			DrainField(*Resource.Get());
		}
	}
}

bool UEnemyHarvestTestSubsystem::GetIsEnemyHarvesterIdle() const
{
	ICommands* Commands = Cast<ICommands>(M_EnemyHarvester.Actor.Get());
	const UHarvester* Harvester = M_EnemyHarvester.Harvester.Get();
	return Commands != nullptr && IsValid(Harvester) && Commands->GetActiveCommandID() == EAbilityID::IdIdle
		&& Harvester->MapTest_GetHarvestStatus() == EHarvesterAIAction::NoHarvestingAction;
}

void UEnemyHarvestTestSubsystem::TickWaitingForIdleAfterDrainAll(const float DeltaTime)
{
	using namespace EnemyHarvestTestPrivate;
	TrackHarvester(M_EnemyHarvester, DeltaTime);
	TrackHarvester(M_PlayerHarvester, DeltaTime);
	M_SecondsEnemyIdle = GetIsEnemyHarvesterIdle() ? M_SecondsEnemyIdle + DeltaTime : 0.f;
	if (M_SecondsEnemyIdle >= IdleBeforeRetryFieldSeconds)
	{
		Check(true, TEXT("idle_without_fields"), FString::Printf(TEXT("after=%.1fs"), GetPhaseSeconds()));
		const FVector Side = M_LayoutAxis.RotateAngleAxis(LayoutDirectionStepDegrees, FVector::UpVector);
		M_RetryField = SpawnFreshField(-Side * FreshFieldSideOffsetCm);
		Check(M_RetryField.IsValid(), TEXT("retry_field_spawned"));
		if (not M_RetryField.IsValid())
		{
			FinishWithResults();
			return;
		}
		EnterPhase(EEnemyHarvestTestPhase::WaitingForRetry);
		return;
	}
	if (GetPhaseSeconds() > IdleAfterDrainAllTimeoutSeconds)
	{
		Check(false, TEXT("idle_without_fields"));
		FinishWithResults();
	}
}

void UEnemyHarvestTestSubsystem::TickWaitingForRetry(const float DeltaTime)
{
	using namespace EnemyHarvestTestPrivate;
	TrackHarvester(M_EnemyHarvester, DeltaTime);
	TrackHarvester(M_PlayerHarvester, DeltaTime);
	const UHarvester* EnemyHarvester = M_EnemyHarvester.Harvester.Get();
	const bool bFoundField = IsValid(EnemyHarvester) && EnemyHarvester->GetTargetResource().IsValid()
		&& EnemyHarvester->GetTargetResource()->StillContainsResources();
	if (bFoundField)
	{
		Check(true, TEXT("retry_finds_new_field"), FString::Printf(
			      TEXT("field=%s after=%.1fs"), *GetNameSafe(EnemyHarvester->GetTargetResource()->GetOwner()),
			      GetPhaseSeconds()));
		FinishWithResults();
		return;
	}
	if (GetPhaseSeconds() > RetryTimeoutSeconds)
	{
		Check(false, TEXT("retry_finds_new_field"));
		FinishWithResults();
	}
}

void UEnemyHarvestTestSubsystem::TickWaitingForRetarget(const float DeltaTime)
{
	using namespace EnemyHarvestTestPrivate;
	TrackHarvester(M_EnemyHarvester, DeltaTime);
	TrackHarvester(M_PlayerHarvester, DeltaTime);
	const UHarvester* EnemyHarvester = M_EnemyHarvester.Harvester.Get();
	if (not IsValid(EnemyHarvester))
	{
		Check(false, TEXT("enemy_harvester_alive"));
		FinishWithResults();
		return;
	}
	const TWeakObjectPtr<UResourceComponent> Target = EnemyHarvester->GetTargetResource();
	const EHarvesterAIAction Status = EnemyHarvester->MapTest_GetHarvestStatus();
	const bool bWorksNewField = Target.IsValid() && Target != M_DrainedField
		&& (Status == EHarvesterAIAction::MoveToResource || Status == EHarvesterAIAction::PlayHarvestAnimation
			|| Status == EHarvesterAIAction::HarvestResource);
	if (bWorksNewField)
	{
		Check(true, TEXT("retargets_after_field_dry"),
		      FString::Printf(TEXT("new_field=%s after=%.1fs"), *GetNameSafe(Target->GetOwner()), GetPhaseSeconds()));
		Check(EnemyHarvester->GetIsHarvestingAutonomously(), TEXT("still_autonomous"));
		// Now take all work away: it must go idle, then its retry must find a field that appears later.
		DrainAllRadixite();
		M_SecondsEnemyIdle = 0.f;
		UE_LOG(LogRTSEnemyHarvestTest, Display, TEXT("RTS_ENEMY_HARVEST_TEST drained all radixite"));
		EnterPhase(EEnemyHarvestTestPhase::WaitingForIdleAfterDrainAll);
		return;
	}
	if (GetPhaseSeconds() > RetargetTimeoutSeconds)
	{
		Check(false, TEXT("retargets_after_field_dry"),
		      FString::Printf(TEXT("target=%s status=%s"), *GetNameSafe(Target.IsValid() ? Target->GetOwner() : nullptr),
		                      *GetStringHarvesterAIStatus(Status)));
		FinishWithResults();
	}
}

void UEnemyHarvestTestSubsystem::TrackHarvester(FEnemyHarvestTestHarvester& TestHarvester, const float DeltaTime) const
{
	using namespace EnemyHarvestTestPrivate;
	UHarvester* Harvester = TestHarvester.Harvester.Get();
	if (not IsValid(Harvester))
	{
		return;
	}
	const int32 Cargo = GetCargo(TestHarvester);
	TestHarvester.MaxCargoSeen = FMath::Max(TestHarvester.MaxCargoSeen, Cargo);
	const UResourceDropOff* TargetDropOff = Harvester->MapTest_GetTargetDropOff();
	if (IsValid(TargetDropOff) && TargetDropOff->GetOwningPlayer() != TestHarvester.OwningPlayer)
	{
		++TestHarvester.WrongOwnerDropOffTicks;
		TestHarvester.WrongOwnerDropOffName = GetNameSafe(TargetDropOff->GetOwner());
	}
	const EHarvesterAIAction Status = Harvester->MapTest_GetHarvestStatus();
	if (static_cast<uint8>(Status) != TestHarvester.LastLoggedStatus)
	{
		TestHarvester.LastLoggedStatus = static_cast<uint8>(Status);
		UE_LOG(LogRTSEnemyHarvestTest, Display,
		       TEXT("RTS_ENEMY_HARVEST_TEST status player=%d status=%s cargo=%d dropoff=%s t=%.1f"),
		       TestHarvester.OwningPlayer, *GetStringHarvesterAIStatus(Status), Cargo,
		       *GetNameSafe(IsValid(TargetDropOff) ? TargetDropOff->GetOwner() : nullptr), GetPhaseSeconds());
	}
	// Fallback for a harvest animation that never ends because nothing draws the mesh.
	if (Status != EHarvesterAIAction::PlayHarvestAnimation)
	{
		TestHarvester.SecondsInHarvestAnimation = 0.f;
		return;
	}
	TestHarvester.SecondsInHarvestAnimation += DeltaTime;
	if (TestHarvester.SecondsInHarvestAnimation < ForceAnimationFinishSeconds)
	{
		return;
	}
	TestHarvester.SecondsInHarvestAnimation = 0.f;
	++TestHarvester.ForcedAnimationFinishes;
	UE_LOG(LogRTSEnemyHarvestTest, Display, TEXT("RTS_ENEMY_HARVEST_TEST forced_anim_finish player=%d"),
	       TestHarvester.OwningPlayer);
	Harvester->OnHarvestAnimationFinished();
}

void UEnemyHarvestTestSubsystem::CheckAIPoolFilter()
{
	AActor* EnemyActor = M_EnemyHarvester.Actor.Get();
	const URTSComponent* RTSComponent = IsValid(EnemyActor) ? EnemyActor->FindComponentByClass<URTSComponent>() : nullptr;
	if (not IsValid(RTSComponent))
	{
		Check(false, TEXT("ai_pool_filter"), TEXT("no RTS component"));
		return;
	}
	Check(FStrategicAIHelpers::GetIsExcludedFromEnemyUnitPools(EnemyActor), TEXT("ai_pool_filter_excludes_harvester"));
	FBlackboardIdleUnitEntry Entry(EnemyActor);
	Entry.UnitType = RTSComponent->GetUnitType();
	Entry.UnitSubtypeRaw = static_cast<int32>(RTSComponent->GetUnitSubType());
	FBlackboardIdleUnitsResult Picked;
	Picked.SetupResultForPickedEntries({Entry});
	Check(Picked.GetTotalUnits() == 0, TEXT("ai_pick_drops_harvester"),
	      FString::Printf(TEXT("picked=%d type=%d subtype=%d"), Picked.GetTotalUnits(),
	                      static_cast<int32>(Entry.UnitType), Entry.UnitSubtypeRaw));
}

void UEnemyHarvestTestSubsystem::FinishWithResults()
{
	using namespace EnemyHarvestTestPrivate;
	const int32 EnemyStoredDelta = GetStoredRadixiteOfPlayer(EnemyPlayer) - M_EnemyStoredBefore;
	const int32 PlayerStoredDelta = GetStoredRadixiteOfPlayer(HumanPlayer) - M_HumanStoredBefore;
	const int32 HumanDelta = GetHumanRadixite() - M_HumanRadixiteBefore;
	Check(M_EnemyHarvester.MaxCargoSeen > 0, TEXT("enemy_harvested"),
	      FString::Printf(TEXT("max_cargo=%d"), M_EnemyHarvester.MaxCargoSeen));
	Check(EnemyStoredDelta > 0, TEXT("enemy_dropoff_stock_rose"), FString::Printf(TEXT("delta=%d"), EnemyStoredDelta));
	Check(PlayerStoredDelta > 0, TEXT("player_dropoff_stock_rose"), FString::Printf(TEXT("delta=%d"), PlayerStoredDelta));
	Check(HumanDelta == PlayerStoredDelta, TEXT("human_resources_follow_only_player_dropoffs"),
	      FString::Printf(TEXT("human_delta=%d p1_stored_delta=%d p2_stored_delta=%d"), HumanDelta, PlayerStoredDelta,
	                      EnemyStoredDelta));
	for (const FEnemyHarvestTestHarvester* TestHarvester : {&M_EnemyHarvester, &M_PlayerHarvester})
	{
		Check(TestHarvester->WrongOwnerDropOffTicks == 0,
		      FString::Printf(TEXT("player_%d_harvester_only_own_dropoffs"), TestHarvester->OwningPlayer),
		      FString::Printf(TEXT("wrong_ticks=%d wrong=%s forced_anim_finishes=%d"),
		                      TestHarvester->WrongOwnerDropOffTicks, *TestHarvester->WrongOwnerDropOffName,
		                      TestHarvester->ForcedAnimationFinishes));
	}
	// The AI stock is spent through RemoveResources; that may never touch the human.
	UResourceDropOff* EnemyDropOff = M_EnemyDropOff.Get();
	const int32 EnemyStored = IsValid(EnemyDropOff)
		                          ? EnemyDropOff->GetDropOffAmountStored(ERTSResourceType::Resource_Radixite)
		                          : 0;
	if (EnemyStored > 0)
	{
		const int32 Removal = FMath::Min(StockRemovalProbe, EnemyStored);
		const int32 HumanBeforeRemoval = GetHumanRadixite();
		EnemyDropOff->RemoveResources(ERTSResourceType::Resource_Radixite, Removal);
		Check(EnemyDropOff->GetDropOffAmountStored(ERTSResourceType::Resource_Radixite) == EnemyStored - Removal
		      && GetHumanRadixite() == HumanBeforeRemoval, TEXT("enemy_stock_spend_leaves_human"),
		      FString::Printf(TEXT("removed=%d human_before=%d human_after=%d"), Removal, HumanBeforeRemoval,
		                      GetHumanRadixite()));
	}
	Finish();
}

int32 UEnemyHarvestTestSubsystem::GetCargo(const FEnemyHarvestTestHarvester& TestHarvester) const
{
	const UHarvester* Harvester = TestHarvester.Harvester.Get();
	if (not IsValid(Harvester))
	{
		return 0;
	}
	int32 Current = 0;
	ERTSResourceType Resource = ERTSResourceType::Resource_None;
	(void)Harvester->GetMaxCapacityForTargetResource(Current, Resource);
	return Resource == ERTSResourceType::Resource_Radixite ? Current : 0;
}

int32 UEnemyHarvestTestSubsystem::GetHumanRadixite() const
{
	const UPlayerResourceManager* ResourceManager = FRTS_Statics::GetPlayerResourceManager(GetWorld());
	return IsValid(ResourceManager) ? ResourceManager->GetResourceAmount(ERTSResourceType::Resource_Radixite) : -1;
}

int32 UEnemyHarvestTestSubsystem::GetHumanRadixiteStorage() const
{
	const UPlayerResourceManager* ResourceManager = FRTS_Statics::GetPlayerResourceManager(GetWorld());
	return IsValid(ResourceManager) ? ResourceManager->GetResourceStorage(ERTSResourceType::Resource_Radixite) : -1;
}

int32 UEnemyHarvestTestSubsystem::GetStoredRadixiteOfPlayer(const uint8 OwningPlayer) const
{
	const ACPPGameState* GameState = GetWorld()->GetGameState<ACPPGameState>();
	const UGameResourceManager* ResourceManager = IsValid(GameState) ? GameState->GetGameResourceManager() : nullptr;
	if (not IsValid(ResourceManager))
	{
		return 0;
	}
	int32 Stored = 0;
	for (const TWeakObjectPtr<UResourceDropOff>& DropOff : ResourceManager->GetCopyOfResourceDropOffsOfPlayer(
		     static_cast<int8>(OwningPlayer)))
	{
		if (DropOff.IsValid() && DropOff->GetResourcesSupported().Contains(ERTSResourceType::Resource_Radixite))
		{
			Stored += DropOff->GetDropOffAmountStored(ERTSResourceType::Resource_Radixite);
		}
	}
	return Stored;
}

void UEnemyHarvestTestSubsystem::StartGameAndUnpause() const
{
	ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	UPlayerStartGameControl* StartGameControl = IsValid(Controller)
		                                            ? Controller->FindComponentByClass<UPlayerStartGameControl>()
		                                            : nullptr;
	if (IsValid(StartGameControl) && StartGameControl->GetIsWaitingForPlayerToStart())
	{
		StartGameControl->PlayerStartedGame();
	}
	if (IsValid(Controller) && Controller->GetIsGameOnPause())
	{
		Controller->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
}

void UEnemyHarvestTestSubsystem::EnterPhase(const EEnemyHarvestTestPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.f;
}

float UEnemyHarvestTestSubsystem::GetPhaseSeconds() const
{
	return IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() - M_PhaseStartSeconds : 0.f;
}

void UEnemyHarvestTestSubsystem::Check(const bool bCondition, const FString& Description, const FString& Details)
{
	M_FailedCheckCount += bCondition ? 0 : 1;
	UE_LOG(LogRTSEnemyHarvestTest, Display, TEXT("RTS_ENEMY_HARVEST_TEST check=%s %s %s"),
	       bCondition ? TEXT("PASS") : TEXT("FAIL"), *Description, *Details);
}

void UEnemyHarvestTestSubsystem::Finish()
{
	M_Phase = EEnemyHarvestTestPhase::Inactive;
	UE_LOG(LogRTSEnemyHarvestTest, Display, TEXT("RTS_ENEMY_HARVEST_TEST RESULT %s failed_checks=%d"),
	       M_FailedCheckCount == 0 ? TEXT("PASS") : TEXT("FAIL"), M_FailedCheckCount);
	bM_IsExitPending = true;
	M_ExitFramesLeft = EnemyHarvestTestPrivate::FramesBeforeExit;
}

void UEnemyHarvestTestSubsystem::TickExitPending()
{
	if (--M_ExitFramesLeft > 0)
	{
		return;
	}
	bM_IsExitPending = false;
	FPlatformMisc::RequestExit(false, TEXT("EnemyHarvestValidate"));
}

#else

bool UEnemyHarvestTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UEnemyHarvestTestSubsystem::Tick(const float DeltaTime)
{
}

TStatId UEnemyHarvestTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UEnemyHarvestTestSubsystem, STATGROUP_Tickables);
}

#endif
