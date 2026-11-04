// Copyright (C) Bas Blokzijl - All rights reserved.

#include "BreakthroughHQTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainerComponent/TrainerComponent.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Resources/ResourceDropOff/ResourceDropOff.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicAttachments/NomadicSkeletalAttachment/NomadicSkeletalAttachment.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSBreakthroughHQTest, Log, All);

namespace BreakthroughHQTestPrivate
{
	const TCHAR* const ValidateSwitch = TEXT("BreakthroughHQValidate");
	const TCHAR* const CaptureSwitch = TEXT("BreakthroughHQCapture=");
	// Ger_HQ_Truck_Breakthrough; the map also holds German HQ trucks.
	const TCHAR* const HQClassTag = TEXT("Breakthrough");
	const TCHAR* const BuildingMeshName = TEXT("SM_BF_HQ");
	constexpr float StartDelaySeconds = 3.f;
	constexpr float ExpandTimeoutSeconds = 120.f;
	// Unattended runs never draw the truck, so its expansion montage may never finish on its own.
	constexpr float ConversionFastForwardSeconds = 30.f;
	constexpr float AttachmentSettleSeconds = 2.f;
	constexpr float LevelSettleSeconds = 0.5f;
	// Stock levels the stores go through: up to full, then back down, so both directions swap meshes.
	const TArray<int32> StockLevels = {0, 10, 25, 50, 75, 100, 40, 5};
	// The levels that have a mesh (SM_BF_HQ_<Store>At<Level>), from the Blueprint's storage levels.
	const TArray<int32> MeshLevels = {10, 25, 50, 75, 100};
	const TArray<TPair<ERTSResourceType, FName>> Stores = {
		{ERTSResourceType::Resource_Radixite, FName(TEXT("Radixite"))},
		{ERTSResourceType::Resource_Metal, FName(TEXT("Metal"))},
		{ERTSResourceType::Resource_VehicleParts, FName(TEXT("VehicleParts"))}
	};
	const TArray<FName> ExpectedSockets = {
		TEXT("Antenna1"), TEXT("Antenna2"), TEXT("BXP1"), TEXT("BXP2"), TEXT("BXP3"), TEXT("Banner1"), TEXT("Banner2"),
		TEXT("CraneStow"), TEXT("Crane"), TEXT("CraneRail"), TEXT("DropOff"), TEXT("Metal"), TEXT("NavEnd"),
		TEXT("NavLeft"), TEXT("Panzerschreck"), TEXT("Radar"), TEXT("Radixite"), TEXT("Train"), TEXT("TrainingPreview"),
		TEXT("UnitExit"), TEXT("VehicleParts"), TEXT("WayPoint"), TEXT("WeldLeft"), TEXT("WeldRight"), TEXT("Origin")
	};
	// Class name fragments of the attachments the Blueprint spawns.
	const TArray<FString> ExpectedAttachments = {
		TEXT("Attachment_BF_HQ_RailCrane"), TEXT("Attachment_BF_HQ_FixedCrane"), TEXT("Attachment_BF_HQ_Dressing"),
		TEXT("Attachment_HQRadar"), TEXT("Attachment_Banner")
	};
	const FName HookBone(TEXT("hook"));
	const FName RailTruckBone(TEXT("truck"));
	const FName FixedJibBone(TEXT("jib"));
	const FName WorkSection(TEXT("Work"));
	// The cranes' Out sections take 13.4 s; this is well inside both Work loops.
	constexpr float WorkCheckSeconds = 20.f;
	constexpr float WorkingSeconds = 45.f;
	constexpr float GoingHomeTimeoutSeconds = 60.f;
	constexpr float CraneLogIntervalSeconds = 1.f;
	// The rail truck must leave its bay: the working spot is about 15 m along the track.
	constexpr float MinRailTravelCm = 500.f;
	// Hooks closer than this would mean both cranes are in the slot at once.
	constexpr float MinHookSeparationCm = 250.f;
	constexpr float RestToleranceCm = 5.f;
	const TArray<float> CraneCaptureSeconds = {5.f, 12.f, 18.f, 24.f, 30.f, 36.f};
	constexpr float GroundTraceHeightCm = 5000.f;
	constexpr int32 FramesToWriteLastCapture = 3;

	// Cameras in the building mesh's frame (cm): +X is the unit exit, the stores are on -Y.
	const TArray<FB4HowitzerCameraView> OverviewViews = {
		{TEXT("overview"), FVector(3200.f, -3000.f, 3300.f), FVector(0.f, -200.f, 300.f), 55.f},
		{TEXT("rear"), FVector(-3000.f, 2600.f, 2800.f), FVector(0.f, 0.f, 300.f), 55.f},
		{TEXT("left"), FVector(600.f, 3600.f, 2500.f), FVector(0.f, 400.f, 300.f), 55.f}
	};
	const TArray<FB4HowitzerCameraView> StoreViews = {
		{TEXT("stores"), FVector(1700.f, -2300.f, 1500.f), FVector(600.f, -900.f, 330.f), 40.f}
	};
	const TArray<FB4HowitzerCameraView> CraneViews = {
		{TEXT("hall"), FVector(1900.f, -1900.f, 2100.f), FVector(240.f, 0.f, 500.f), 45.f},
		{TEXT("bay"), FVector(-200.f, -2400.f, 1700.f), FVector(-500.f, -300.f, 500.f), 45.f}
	};

	int32 GetMeshLevelFor(const int32 Percentage)
	{
		int32 Best = 0;
		for (const int32 Level : MeshLevels)
		{
			Best = Percentage >= Level ? Level : Best;
		}
		return Best;
	}
}

bool UBreakthroughHQTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), BreakthroughHQTestPrivate::ValidateSwitch);
}

TStatId UBreakthroughHQTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBreakthroughHQTestSubsystem, STATGROUP_Tickables);
}

void UBreakthroughHQTestSubsystem::Tick(const float DeltaTime)
{
	if (bM_IsExitPending)
	{
		TickExitPending();
		return;
	}
	const UWorld* World = GetWorld();
	if (M_Phase == EBreakthroughHQTestPhase::Inactive || not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	if (M_PhaseStartSeconds < 0.f)
	{
		M_PhaseStartSeconds = World->GetTimeSeconds();
		UE_LOG(LogRTSBreakthroughHQTest, Display, TEXT("RTS_BF_HQ_TEST started map=%s"), *World->GetName());
	}
	KeepAttachmentsAnimating();
	switch (M_Phase)
	{
	case EBreakthroughHQTestPhase::WaitingForStart:
		TickWaitingForStart();
		return;
	case EBreakthroughHQTestPhase::Expanding:
		TickExpanding();
		return;
	case EBreakthroughHQTestPhase::StockLevels:
		TickStockLevels();
		return;
	case EBreakthroughHQTestPhase::CranesWorking:
		TickCranesWorking();
		return;
	case EBreakthroughHQTestPhase::CranesGoingHome:
		TickCranesGoingHome();
		return;
	default:
		return;
	}
}

void UBreakthroughHQTestSubsystem::TickWaitingForStart()
{
	using namespace BreakthroughHQTestPrivate;
	StartGameAndUnpause();
	if (GetPhaseSeconds() < StartDelaySeconds)
	{
		return;
	}
	FString CaptureDirectory;
	if (FParse::Value(FCommandLine::Get(), CaptureSwitch, CaptureDirectory))
	{
		M_CaptureRig.Init(*GetWorld(), CaptureDirectory);
	}
	if (not FindHQ())
	{
		Check(false, TEXT("hq_found"), TEXT("no player-owned Ger_HQ_Truck_Breakthrough on the map"));
		Finish();
		return;
	}
	ANomadicVehicle* HQ = M_HQ.Get();
	if (HQ->GetIsInTruckMode())
	{
		const ECommandQueueError Error = HQ->CreateBuildingAtLocation(HQ->GetActorLocation(), HQ->GetActorRotation(), true);
		Check(Error == ECommandQueueError::NoError, TEXT("hq_expand_ordered"));
	}
	EnterPhase(EBreakthroughHQTestPhase::Expanding);
}

void UBreakthroughHQTestSubsystem::TickExpanding()
{
	using namespace BreakthroughHQTestPrivate;
	StartGameAndUnpause();
	ANomadicVehicle* HQ = M_HQ.Get();
	if (not IsValid(HQ))
	{
		Check(false, TEXT("hq_alive_while_expanding"));
		Finish();
		return;
	}
	const UTrainerComponent* Trainer = HQ->GetTrainerComponent();
	const bool bIsExpanded = HQ->GetNomadicStatus() == ENomadStatus::Building && IsValid(Trainer)
		&& Trainer->GetIsAbleToTrain();
	if (not bIsExpanded)
	{
		if (GetPhaseSeconds() >= ConversionFastForwardSeconds)
		{
			(void)HQ->ShippingTest_ForceFinishConversionToBuilding();
		}
		if (GetPhaseSeconds() > ExpandTimeoutSeconds)
		{
			Check(false, TEXT("hq_expanded"), FString::Printf(TEXT("status=%d"), static_cast<int32>(HQ->GetNomadicStatus())));
			Finish();
		}
		return;
	}
	// Attachments spawn with the building; give their BeginPlay a moment before checking them.
	const float Now = GetWorld()->GetTimeSeconds();
	if (M_ExpandedSeconds < 0.f)
	{
		M_ExpandedSeconds = Now;
	}
	if (Now - M_ExpandedSeconds < AttachmentSettleSeconds)
	{
		return;
	}
	UE_LOG(LogRTSBreakthroughHQTest, Display, TEXT("RTS_BF_HQ_TEST hq_expanded at=%s yaw=%.1f"),
	       *HQ->GetActorLocation().ToString(), HQ->GetActorRotation().Yaw);
	CheckBuilding();
	CheckAttachments();
	CheckDropOffAndExit();
	CaptureViews(TEXT("expanded"), OverviewViews);
	M_LevelIndex = 0;
	bM_IsLevelFilled = false;
	EnterPhase(EBreakthroughHQTestPhase::StockLevels);
}

void UBreakthroughHQTestSubsystem::TickStockLevels()
{
	using namespace BreakthroughHQTestPrivate;
	if (M_LevelIndex >= StockLevels.Num())
	{
		StartCranes();
		EnterPhase(EBreakthroughHQTestPhase::CranesWorking);
		return;
	}
	const int32 Percentage = StockLevels[M_LevelIndex];
	if (not bM_IsLevelFilled)
	{
		FillStoresTo(Percentage);
		bM_IsLevelFilled = true;
		M_PhaseStartSeconds = GetWorld()->GetTimeSeconds();
		return;
	}
	if (GetPhaseSeconds() < LevelSettleSeconds)
	{
		return;
	}
	CheckFillMeshes(Percentage);
	CaptureViews(FString::Printf(TEXT("stock_%03d"), Percentage), StoreViews);
	if (Percentage == 100)
	{
		CaptureViews(TEXT("stock_100"), OverviewViews);
	}
	++M_LevelIndex;
	bM_IsLevelFilled = false;
}

void UBreakthroughHQTestSubsystem::TickCranesWorking()
{
	using namespace BreakthroughHQTestPrivate;
	if (GetWorld()->GetTimeSeconds() >= M_NextCraneLogSeconds)
	{
		M_NextCraneLogSeconds = GetWorld()->GetTimeSeconds() + CraneLogIntervalSeconds;
		LogCranes();
	}
	TrackCranesAwayFromRest();
	if (CraneCaptureSeconds.IsValidIndex(M_NextCraneCapture) && GetPhaseSeconds() >= CraneCaptureSeconds[M_NextCraneCapture])
	{
		CaptureViews(FString::Printf(TEXT("cranes_%02.0fs"), CraneCaptureSeconds[M_NextCraneCapture]), CraneViews);
		++M_NextCraneCapture;
	}
	if (GetPhaseSeconds() < WorkingSeconds)
	{
		return;
	}
	for (const FBreakthroughHQCraneRest& Rest : M_Cranes)
	{
		Check(Rest.MaxDistanceFromRestCm >= MinRailTravelCm || Rest.TrackedBone != RailTruckBone,
		      TEXT("rail_crane_left_its_bay"), FString::Printf(TEXT("max_travel_cm=%.0f"), Rest.MaxDistanceFromRestCm));
	}
	Check(M_MinHookDistanceCm >= MinHookSeparationCm, TEXT("crane_hooks_never_meet"),
	      FString::Printf(TEXT("min_hook_distance_cm=%.0f"), M_MinHookDistanceCm));
	SetCranesWorking(false);
	EnterPhase(EBreakthroughHQTestPhase::CranesGoingHome);
}

void UBreakthroughHQTestSubsystem::TickCranesGoingHome()
{
	using namespace BreakthroughHQTestPrivate;
	if (GetWorld()->GetTimeSeconds() >= M_NextCraneLogSeconds)
	{
		M_NextCraneLogSeconds = GetWorld()->GetTimeSeconds() + CraneLogIntervalSeconds;
		LogCranes();
	}
	const bool bTimedOut = GetPhaseSeconds() > GoingHomeTimeoutSeconds;
	if (GetAreCraneMontagesPlaying() && not bTimedOut)
	{
		return;
	}
	Check(not bTimedOut, TEXT("cranes_finished_going_home"), FString::Printf(TEXT("after=%.1fs"), GetPhaseSeconds()));
	for (const FBreakthroughHQCraneRest& Rest : M_Cranes)
	{
		const ANomadicAttachmentSkeletal* Crane = Rest.Crane.Get();
		const USkeletalMeshComponent* Mesh = IsValid(Crane) ? Crane->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
		const float Distance = IsValid(Mesh)
			                       ? FVector::Dist(Mesh->GetBoneLocation(Rest.TrackedBone), Rest.RestLocation)
			                       : TNumericLimits<float>::Max();
		Check(Distance <= RestToleranceCm, TEXT("crane_back_at_rest"),
		      FString::Printf(TEXT("crane=%s bone=%s distance_cm=%.1f went_home_after=%.1fs"),
		                      IsValid(Crane) ? *Crane->GetClass()->GetName() : TEXT("none"), *Rest.TrackedBone.ToString(),
		                      Distance, GetPhaseSeconds()));
	}
	CaptureViews(TEXT("cranes_home"), CraneViews);
	Finish();
}

// ---------------------------------------------------------------------------------------------------------------
// Checks

bool UBreakthroughHQTestSubsystem::FindHQ()
{
	for (TActorIterator<ANomadicVehicle> NomadicIterator(GetWorld()); NomadicIterator; ++NomadicIterator)
	{
		ANomadicVehicle* Nomadic = *NomadicIterator;
		const URTSComponent* RTSComponent = IsValid(Nomadic) ? Nomadic->GetRTSComponent() : nullptr;
		if (IsValid(RTSComponent) && RTSComponent->GetOwningPlayer() == 1
			&& Nomadic->GetClass()->GetName().Contains(BreakthroughHQTestPrivate::HQClassTag))
		{
			M_HQ = Nomadic;
			UE_LOG(LogRTSBreakthroughHQTest, Display, TEXT("RTS_BF_HQ_TEST hq=%s class=%s"), *Nomadic->GetName(),
			       *Nomadic->GetClass()->GetName());
			return true;
		}
	}
	return false;
}

void UBreakthroughHQTestSubsystem::CheckBuilding()
{
	using namespace BreakthroughHQTestPrivate;
	const UStaticMeshComponent* BuildingMesh = GetBuildingMesh();
	const UStaticMesh* Mesh = IsValid(BuildingMesh) ? BuildingMesh->GetStaticMesh() : nullptr;
	Check(IsValid(Mesh) && Mesh->GetName() == BuildingMeshName, TEXT("building_mesh"),
	      IsValid(Mesh) ? Mesh->GetName() : TEXT("none"));
	if (not IsValid(BuildingMesh))
	{
		return;
	}
	TArray<FString> MissingSockets;
	for (const FName& SocketName : ExpectedSockets)
	{
		if (not BuildingMesh->DoesSocketExist(SocketName))
		{
			MissingSockets.Add(SocketName.ToString());
		}
	}
	Check(MissingSockets.IsEmpty(), TEXT("building_sockets"), FString::Printf(
		      TEXT("expected=%d missing=[%s]"), ExpectedSockets.Num(), *FString::Join(MissingSockets, TEXT(","))));
	const FBox Bounds = BuildingMesh->Bounds.GetBox();
	UE_LOG(LogRTSBreakthroughHQTest, Display, TEXT("RTS_BF_HQ_TEST building_size_m=%.1fx%.1fx%.1f"),
	       Bounds.GetSize().X / 100.f, Bounds.GetSize().Y / 100.f, Bounds.GetSize().Z / 100.f);
}

void UBreakthroughHQTestSubsystem::CheckAttachments()
{
	using namespace BreakthroughHQTestPrivate;
	TArray<AActor*> Attached;
	M_HQ->GetAttachedActors(Attached, true, true);
	TArray<FString> Found;
	for (const AActor* Actor : Attached)
	{
		if (not IsValid(Actor) || not IsValid(Actor->GetRootComponent()))
		{
			continue;
		}
		const FString ClassName = Actor->GetClass()->GetName();
		Found.Add(ClassName);
		UE_LOG(LogRTSBreakthroughHQTest, Display, TEXT("RTS_BF_HQ_TEST attachment class=%s socket=%s at=%s"), *ClassName,
		       *Actor->GetRootComponent()->GetAttachSocketName().ToString(), *Actor->GetActorLocation().ToString());
	}
	for (const FString& Expected : ExpectedAttachments)
	{
		const bool bIsFound = Found.ContainsByPredicate([&Expected](const FString& ClassName)
		{
			return ClassName.Contains(Expected);
		});
		Check(bIsFound, TEXT("attachment_spawned"), Expected);
	}
}

void UBreakthroughHQTestSubsystem::CheckDropOffAndExit()
{
	using namespace BreakthroughHQTestPrivate;
	const UResourceDropOff* DropOff = M_HQ->GetResourceDropOff();
	Check(IsValid(DropOff), TEXT("drop_off_exists"));
	for (const TPair<ERTSResourceType, FName>& Store : Stores)
	{
		const int32 Capacity = IsValid(DropOff) ? DropOff->GetDropOffCapacity(Store.Key) : 0;
		Check(Capacity > 0, TEXT("store_capacity"), FString::Printf(TEXT("store=%s capacity=%d stored=%d"),
		      *Store.Value.ToString(), Capacity, IsValid(DropOff) ? DropOff->GetDropOffAmountStored(Store.Key) : 0));
	}
	// Trained units appear at WayPoint: it must stand on open ground, not on or in the building.
	const UStaticMeshComponent* BuildingMesh = GetBuildingMesh();
	for (const FName SocketName : {FName(TEXT("WayPoint")), FName(TEXT("UnitExit")), FName(TEXT("DropOff"))})
	{
		const FVector SocketLocation = BuildingMesh->GetSocketLocation(SocketName);
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(BreakthroughHQExit), false);
		const bool bHit = GetWorld()->LineTraceSingleByChannel(
			Hit, SocketLocation + FVector(0.f, 0.f, GroundTraceHeightCm), SocketLocation - FVector(0.f, 0.f, GroundTraceHeightCm),
			ECC_Visibility, Params);
		const AActor* HitActor = bHit ? Hit.GetActor() : nullptr;
		Check(bHit && HitActor != M_HQ.Get(), TEXT("socket_on_open_ground"), FString::Printf(
			      TEXT("socket=%s hit=%s ground_above_socket_cm=%.0f"), *SocketName.ToString(),
			      IsValid(HitActor) ? *HitActor->GetName() : TEXT("none"), bHit ? Hit.ImpactPoint.Z - SocketLocation.Z : 0.f));
	}
}

void UBreakthroughHQTestSubsystem::FillStoresTo(const int32 Percentage) const
{
	using namespace BreakthroughHQTestPrivate;
	UResourceDropOff* DropOff = M_HQ.IsValid() ? M_HQ->GetResourceDropOff() : nullptr;
	if (not IsValid(DropOff))
	{
		return;
	}
	for (const TPair<ERTSResourceType, FName>& Store : Stores)
	{
		const int32 Capacity = DropOff->GetDropOffCapacity(Store.Key);
		const int32 Stored = DropOff->GetDropOffAmountStored(Store.Key);
		const int32 Target = FMath::CeilToInt(Capacity * Percentage / 100.f);
		if (Target > Stored)
		{
			(void)DropOff->DropOffResources(Store.Key, Target - Stored);
		}
		else if (Target < Stored)
		{
			DropOff->RemoveResources(Store.Key, Stored - Target);
		}
	}
}

void UBreakthroughHQTestSubsystem::CheckFillMeshes(const int32 Percentage)
{
	using namespace BreakthroughHQTestPrivate;
	const UResourceDropOff* DropOff = M_HQ.IsValid() ? M_HQ->GetResourceDropOff() : nullptr;
	const int32 MeshLevel = GetMeshLevelFor(Percentage);
	for (const TPair<ERTSResourceType, FName>& Store : Stores)
	{
		const UStaticMeshComponent* Fill = FindFillComponent(Store.Value);
		const UStaticMesh* Mesh = IsValid(Fill) ? Fill->GetStaticMesh() : nullptr;
		const FString Expected = MeshLevel > 0
			                         ? FString::Printf(TEXT("SM_BF_HQ_%sAt%d"), *Store.Value.ToString(), MeshLevel)
			                         : TEXT("none");
		const FString Shown = IsValid(Mesh) ? Mesh->GetName() : TEXT("none");
		const int32 Stored = IsValid(DropOff) ? DropOff->GetDropOffAmountStored(Store.Key) : -1;
		const int32 Capacity = IsValid(DropOff) ? DropOff->GetDropOffCapacity(Store.Key) : -1;
		Check(Shown == Expected, TEXT("store_fill_mesh"), FString::Printf(
			      TEXT("store=%s percent=%d stored=%d/%d expected=%s shown=%s"), *Store.Value.ToString(), Percentage,
			      Stored, Capacity, *Expected, *Shown));
	}
}

const UStaticMeshComponent* UBreakthroughHQTestSubsystem::FindFillComponent(const FName SocketName) const
{
	const UStaticMeshComponent* BuildingMesh = GetBuildingMesh();
	if (not IsValid(BuildingMesh))
	{
		return nullptr;
	}
	TArray<UStaticMeshComponent*> MeshComponents;
	M_HQ->GetComponents<UStaticMeshComponent>(MeshComponents);
	for (const UStaticMeshComponent* Component : MeshComponents)
	{
		if (IsValid(Component) && Component->GetAttachParent() == BuildingMesh
			&& Component->GetAttachSocketName() == SocketName)
		{
			return Component;
		}
	}
	return nullptr;
}

// ---------------------------------------------------------------------------------------------------------------
// Cranes

void UBreakthroughHQTestSubsystem::StartCranes()
{
	using namespace BreakthroughHQTestPrivate;
	TArray<AActor*> Attached;
	M_HQ->GetAttachedActors(Attached, true, true);
	for (AActor* Actor : Attached)
	{
		ANomadicAttachmentSkeletal* Crane = Cast<ANomadicAttachmentSkeletal>(Actor);
		if (not IsValid(Crane) || not Crane->GetClass()->GetName().Contains(TEXT("BF_HQ")))
		{
			continue;
		}
		const USkeletalMeshComponent* Mesh = Crane->FindComponentByClass<USkeletalMeshComponent>();
		FBreakthroughHQCraneRest Rest;
		Rest.Crane = Crane;
		Rest.TrackedBone = Crane->GetClass()->GetName().Contains(TEXT("Rail")) ? RailTruckBone : FixedJibBone;
		Rest.RestLocation = IsValid(Mesh) ? Mesh->GetBoneLocation(Rest.TrackedBone) : FVector::ZeroVector;
		M_Cranes.Add(Rest);
	}
	Check(M_Cranes.Num() == 2, TEXT("two_cranes"), FString::Printf(TEXT("found=%d"), M_Cranes.Num()));
	M_NextCraneLogSeconds = 0.f;
	M_NextCraneCapture = 0;
	CaptureViews(TEXT("cranes_00s"), CraneViews);
	SetCranesWorking(true);
}

void UBreakthroughHQTestSubsystem::SetCranesWorking(const bool bWorking) const
{
	// The HQ does the same through FNomadicAttachmentHelpers when a training item starts and when the queue empties.
	for (const FBreakthroughHQCraneRest& Rest : M_Cranes)
	{
		if (ANomadicAttachmentSkeletal* Crane = Rest.Crane.Get())
		{
			Crane->SetAttachmentAnimationState(bWorking
				                                   ? EAttachmentAnimationState::MontageAndAO
				                                   : EAttachmentAnimationState::AOOnly);
		}
	}
	UE_LOG(LogRTSBreakthroughHQTest, Display, TEXT("RTS_BF_HQ_TEST cranes_working=%d"), bWorking ? 1 : 0);
}

void UBreakthroughHQTestSubsystem::LogCranes()
{
	using namespace BreakthroughHQTestPrivate;
	TArray<FVector> Hooks;
	FString Line;
	for (const FBreakthroughHQCraneRest& Rest : M_Cranes)
	{
		const ANomadicAttachmentSkeletal* Crane = Rest.Crane.Get();
		const USkeletalMeshComponent* Mesh = IsValid(Crane) ? Crane->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
		if (not IsValid(Mesh))
		{
			continue;
		}
		const UAnimInstance* Anim = Mesh->GetAnimInstance();
		const UAnimMontage* Montage = IsValid(Anim) ? Anim->GetCurrentActiveMontage() : nullptr;
		const FName Section = IsValid(Montage) ? Anim->Montage_GetCurrentSection(Montage) : NAME_None;
		const FVector Hook = Mesh->GetBoneLocation(HookBone);
		Hooks.Add(Hook);
		Line += FString::Printf(TEXT(" %s[section=%s hook=%s %s_from_rest_cm=%.0f]"), *Crane->GetClass()->GetName(),
		                        *Section.ToString(), *Hook.ToCompactString(), *Rest.TrackedBone.ToString(),
		                        FVector::Dist(Mesh->GetBoneLocation(Rest.TrackedBone), Rest.RestLocation));
		if (M_Phase == EBreakthroughHQTestPhase::CranesWorking && FMath::IsNearlyEqual(GetPhaseSeconds(), WorkCheckSeconds,
			CraneLogIntervalSeconds * 0.5f))
		{
			Check(Section == WorkSection, TEXT("crane_in_work_loop"), Crane->GetClass()->GetName());
		}
	}
	if (Hooks.Num() == 2 && M_Phase == EBreakthroughHQTestPhase::CranesWorking)
	{
		M_MinHookDistanceCm = FMath::Min(M_MinHookDistanceCm, FVector::Dist(Hooks[0], Hooks[1]));
	}
	UE_LOG(LogRTSBreakthroughHQTest, Display, TEXT("RTS_BF_HQ_TEST cranes t=%.1f%s"), GetPhaseSeconds(), *Line);
}

void UBreakthroughHQTestSubsystem::TrackCranesAwayFromRest()
{
	for (FBreakthroughHQCraneRest& Rest : M_Cranes)
	{
		const ANomadicAttachmentSkeletal* Crane = Rest.Crane.Get();
		const USkeletalMeshComponent* Mesh = IsValid(Crane) ? Crane->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
		if (IsValid(Mesh))
		{
			Rest.MaxDistanceFromRestCm = FMath::Max(Rest.MaxDistanceFromRestCm,
			                                        FVector::Dist(Mesh->GetBoneLocation(Rest.TrackedBone), Rest.RestLocation));
		}
	}
}

bool UBreakthroughHQTestSubsystem::GetAreCraneMontagesPlaying() const
{
	for (const FBreakthroughHQCraneRest& Rest : M_Cranes)
	{
		const ANomadicAttachmentSkeletal* Crane = Rest.Crane.Get();
		const USkeletalMeshComponent* Mesh = IsValid(Crane) ? Crane->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
		const UAnimInstance* Anim = IsValid(Mesh) ? Mesh->GetAnimInstance() : nullptr;
		if (IsValid(Anim) && IsValid(Anim->GetCurrentActiveMontage()))
		{
			return true;
		}
	}
	return false;
}

void UBreakthroughHQTestSubsystem::KeepAttachmentsAnimating() const
{
	// Unattended runs never draw the HQ; its montages and the cranes' bones would not update otherwise.
	if (not M_HQ.IsValid())
	{
		return;
	}
	TArray<AActor*> Actors;
	M_HQ->GetAttachedActors(Actors, true, true);
	Actors.Add(M_HQ.Get());
	for (const AActor* Actor : Actors)
	{
		TArray<USkeletalMeshComponent*> SkeletalMeshes;
		if (IsValid(Actor))
		{
			Actor->GetComponents(SkeletalMeshes);
		}
		for (USkeletalMeshComponent* SkeletalMesh : SkeletalMeshes)
		{
			SkeletalMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Plumbing

void UBreakthroughHQTestSubsystem::StartGameAndUnpause() const
{
	// The map boots paused behind the start-game widget; start it as the player's "Press To Start" does.
	ACPPController* Controller = GetController();
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

void UBreakthroughHQTestSubsystem::CaptureViews(const FString& Label, const TArray<FB4HowitzerCameraView>& Views) const
{
	const UStaticMeshComponent* BuildingMesh = GetBuildingMesh();
	if (not M_CaptureRig.GetIsEnabled() || not IsValid(BuildingMesh))
	{
		return;
	}
	// The building mesh's frame without its scale, so the camera offsets stay in centimetres.
	FTransform Frame = BuildingMesh->GetComponentTransform();
	Frame.SetScale3D(FVector::OneVector);
	M_CaptureRig.CaptureViews(*GetWorld(), Frame, Views, Label);
}

void UBreakthroughHQTestSubsystem::EnterPhase(const EBreakthroughHQTestPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.f;
}

float UBreakthroughHQTestSubsystem::GetPhaseSeconds() const
{
	return IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() - M_PhaseStartSeconds : 0.f;
}

void UBreakthroughHQTestSubsystem::Check(const bool bCondition, const FString& Description, const FString& Details)
{
	M_FailedCheckCount += bCondition ? 0 : 1;
	UE_LOG(LogRTSBreakthroughHQTest, Display, TEXT("RTS_BF_HQ_TEST check=%s %s %s"),
	       bCondition ? TEXT("PASS") : TEXT("FAIL"), *Description, *Details);
}

void UBreakthroughHQTestSubsystem::Finish()
{
	M_Phase = EBreakthroughHQTestPhase::Inactive;
	UE_LOG(LogRTSBreakthroughHQTest, Display, TEXT("RTS_BF_HQ_TEST RESULT %s failed_checks=%d"),
	       M_FailedCheckCount == 0 ? TEXT("PASS") : TEXT("FAIL"), M_FailedCheckCount);
	bM_IsExitPending = true;
	M_ExitFramesLeft = BreakthroughHQTestPrivate::FramesToWriteLastCapture;
}

void UBreakthroughHQTestSubsystem::TickExitPending()
{
	if (--M_ExitFramesLeft > 0)
	{
		return;
	}
	bM_IsExitPending = false;
	FPlatformMisc::RequestExit(false, TEXT("BreakthroughHQValidate"));
}

ACPPController* UBreakthroughHQTestSubsystem::GetController() const
{
	return FRTS_Statics::GetRTSController(GetWorld());
}

const UStaticMeshComponent* UBreakthroughHQTestSubsystem::GetBuildingMesh() const
{
	return M_HQ.IsValid() ? M_HQ->MapTest_GetBuildingMeshComponent() : nullptr;
}

#else

bool UBreakthroughHQTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UBreakthroughHQTestSubsystem::Tick(float DeltaTime)
{
}

TStatId UBreakthroughHQTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBreakthroughHQTestSubsystem, STATGROUP_Tickables);
}

#endif
