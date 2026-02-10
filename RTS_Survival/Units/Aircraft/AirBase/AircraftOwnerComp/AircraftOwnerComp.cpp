#include "AircraftOwnerComp.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/StaticMeshSocket.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

UAircraftOwnerComp::UAircraftOwnerComp()
{
	// todo set to false (used for debugging)
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UAircraftOwnerComp::OnAirbaseDies()
{
	if (bM_DeathProcessed) return;
	bM_DeathProcessed = true;
	M_AirfieldRepairManager.OnAirfieldDies();

	// Snapshot valid aircraft first (avoid invalidation while clearing)
	TArray<TWeakObjectPtr<AAircraftMaster>> ToNotify;
	ToNotify.Reserve(M_Sockets.Num());
	for (const FSocketRecord& Rec : M_Sockets)
	{
		if (Rec.AssignedAircraft.IsValid())
		{
			ToNotify.Add(Rec.AssignedAircraft);
		}
	}

	// Notify aircraft
	for (const TWeakObjectPtr<AAircraftMaster>& WeakAc : ToNotify)
	{
		if (WeakAc.IsValid())
		{
			WeakAc->OnOwnerDies(this);
		}
	}

	// Clear assignments, unbind delegates, stop door timers
	for (int32 i = 0; i < M_Sockets.Num(); ++i)
	{
		ClearSocketRecByIndex(i);
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(M_Sockets[i].AnimTimerHandle);
		}
	}

	// Optional: nuke lookup maps for cleanliness
	M_AircraftToSocketIndex.Empty();
	M_DeathDelegateHandles.Empty();
}


void UAircraftOwnerComp::BeginDestroy()
{
	if (GetWorld())
	{
		for (auto& Rec : M_Sockets)
		{
			GetWorld()->GetTimerManager().ClearTimer(Rec.AnimTimerHandle);
		}
	}
	Super::BeginDestroy();
}

void UAircraftOwnerComp::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (!bM_DeathProcessed)
	{
		OnAirbaseDies();
		bM_DeathProcessed = true;
	}
	Super::EndPlay(EndPlayReason);
}

void UAircraftOwnerComp::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	if (!bM_DeathProcessed)
	{
		OnAirbaseDies();
		bM_DeathProcessed = true;
	}
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}


void UAircraftOwnerComp::TickComponent(float DeltaTime, ELevelTick TickType,
                                       FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

// -------------------- Validation helpers --------------------

bool UAircraftOwnerComp::GetIsValidBuildingMesh() const
{
	if (IsValid(M_BuildingMesh))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		GetOwner(), "M_BuildingMesh", "GetIsValidBuildingMesh", GetOwner());
	return false;
}

bool UAircraftOwnerComp::GetIsValidRoofSkelMesh() const
{
	if (IsValid(M_RoofMeshComp))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		GetOwner(), "M_RoofMeshComp", "GetIsValidRoofSkelMesh", GetOwner());
	return false;
}

bool UAircraftOwnerComp::GetIsValidRoofAnimInstance() const
{
	if (IsValid(M_RoofAnimInst))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		GetOwner(), "M_RoofAnimInst", "GetIsValidRoofAnimInstance", GetOwner());
	return false;
}


void UAircraftOwnerComp::UnpackAirbase(UMeshComponent* BuilidngMesh, const float RepairPowerMlt)
{
	if (not IsValid(BuilidngMesh))
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(this, "BuilidngMesh", "InitAirbase");
		return;
	}
	M_BuildingMesh = BuilidngMesh;

	DiscoverLandingSockets();
	if (M_Sockets.Num() <= 0)
	{
		RTSFunctionLibrary::ReportError("No Landing_* sockets found on building mesh for: " + GetOwner()->GetName());
		return;
	}

	// Create the roof
	if (not IsValid(M_Settings.AircraftRoofMesh))
	{
		RTSFunctionLibrary::ReportError("AircraftRoofMesh not set on AircraftOwnerComp for: " + GetOwner()->GetName());
		return;
	}
	if (not*M_Settings.AircraftRoofAnimBPClass)
	{
		RTSFunctionLibrary::ReportError(
			"AircraftRoofAnimBPClass not set on AircraftOwnerComp for: " + GetOwner()->GetName());
		return;
	}


	M_RoofMeshComp = NewObject<USkeletalMeshComponent>(GetOwner(), USkeletalMeshComponent::StaticClass(),
	                                                   TEXT("AirbaseRoof"));
	if (not IsValid(M_RoofMeshComp))
	{
		RTSFunctionLibrary::ReportError("Failed to create roof skeletal mesh component on: " + GetOwner()->GetName());
		return;
	}

	M_RoofMeshComp->SetSkeletalMesh(M_Settings.AircraftRoofMesh);
	M_RoofMeshComp->SetRelativeLocation(M_Settings.AircraftRoofMeshRelativeLocation);
	M_RoofMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M_RoofMeshComp->SetCanEverAffectNavigation(false);
	M_RoofMeshComp->AttachToComponent(M_BuildingMesh, FAttachmentTransformRules::KeepRelativeTransform);
	M_RoofMeshComp->RegisterComponent();

	M_RoofMeshComp->SetAnimInstanceClass(M_Settings.AircraftRoofAnimBPClass);
	M_RoofAnimInst = Cast<UAirBaseAnimInstance>(M_RoofMeshComp->GetAnimInstance());
	if (not GetIsValidRoofAnimInstance())
	{
		return;
	}

	M_RoofAnimInst->InitAirbaseAnimInstance(M_Sockets.Num());

	// Default all bays to Closed in the anim graph
	for (int32 i = 0; i < M_Sockets.Num(); ++i)
	{
		M_RoofAnimInst->SetAnimationSequenceForState(EAircraftSocketState::Closed, i);
		M_Sockets[i].SocketState = EAircraftSocketState::Closed;
	}
	M_PackingState = EAirbasePackingState::Unpacked_Building;


	// At end of UnpackAirbase(), after successful setup:
	// todo set repair strength.
	M_AirfieldRepairManager.InitAircraftRepair(/*RepairStrengthMlt=*/1.0f, this);
}

bool UAircraftOwnerComp::GetCanPackUp() const
{
	// Allowed to pack if nobody is physically inside (landed) — training + flying are OK.
	return not HasAnyAircraftInside();
}

void UAircraftOwnerComp::OnPackUpAirbaseStart(UMaterialInstance* NomadicConstructionMaterial)
{
	// start fresh to avoid dupes/stale pointers on repeated pack attempts
	M_PackingRecords.Empty();

	// set roof construction mats once, even if no aircraft are assigned
	OnPackUp_SetRoofToConstruction(NomadicConstructionMaterial);

	// Important set before telling aircraft to cancel their waiting for bay doors to open.
	// So we can ignore the request of the aircraft to close the doors when it cancels the waiting for doors.
	M_PackingState = EAirbasePackingState::PackingUp;

	for (int32 i = 0; i < M_Sockets.Num(); ++i)
	{
		auto& Rec = M_Sockets[i];
		if (not Rec.AssignedAircraft.IsValid())
		{
			continue;
		}

		FAircraftRecordPacking PackRec;
		const EAircraftLandingState LandedState = Rec.AssignedAircraft->GetLandedState();
		PackRec.bLandAircraftIfPackUpCancelled = (LandedState == EAircraftLandingState::VerticalLanding) ||
			(LandedState == EAircraftLandingState::Landed);

		PackRec.AssignedAircraft = Rec.AssignedAircraft;

		// remember where it was so we can restore assignment on cancel
		PackRec.SocketIndex = i;

		M_PackingRecords.Add(PackRec);

		// An aircraft waiting for bay to open will cancel the landing and call for the closing door anim; this will not
		// do anything as M_PackingState = EAirbasePackingState::PackingUp.
		Rec.AssignedAircraft->OnOwnerStartPackingUp(this);

		// Reset all pending, landed, pointer state.
		// Also unbind the death delegate.
		ClearSocketRecByIndex(i);
		// In case the door was not open we do not let the aircraft wait for the animation but take off immediately while
		// we are opening the door.
		if (Rec.SocketState == EAircraftSocketState::Closed || Rec.SocketState == EAircraftSocketState::Closing)
		{
			PlayDoorMontage(i, EAircraftSocketState::Opening, M_Settings.OpenAircraftSocketAnimTime);
		}
		PackRec.PreviousSocketState = Rec.SocketState;
	}

	// stop any running timers before nuking sockets
	if (UWorld* World = GetWorld())
	{
		for (auto& Rec : M_Sockets)
		{
			World->GetTimerManager().ClearTimer(Rec.AnimTimerHandle);
		}
	}


	M_Sockets.Empty();
}


void UAircraftOwnerComp::OnPackUpAirbaseCancelled()
{
	// Re-build sockets
	DiscoverLandingSockets();

	//  default all discovered sockets to Closed & sync the anim instance
	if (GetIsValidRoofAnimInstance())
	{
		for (int32 i = 0; i < M_Sockets.Num(); ++i)
		{
			M_Sockets[i].SocketState = EAircraftSocketState::Closed;
			M_RoofAnimInst->SetAnimationSequenceForState(EAircraftSocketState::Closed, i);
		}
	}

	for (const auto& EachRec : M_PackingRecords)
	{
		if (not EachRec.AssignedAircraft.IsValid() ||
			not RTSFunctionLibrary::RTSIsValid(EachRec.AssignedAircraft.Get()))
		{
			continue;
		}

		// Validate recorded index against newly discovered sockets
		if (not EnsureSocketIndexValid(EachRec.SocketIndex, TEXT("OnPackUpAirbaseCancelled")))
		{
			continue;
		}

		if (GetIsValidRoofAnimInstance())
		{
			// Resolve any transient state to its final logical state and DO NOT play a montage here.
			// We want the bay to be immediately usable and logically correct.
			EAircraftSocketState& SocketState = M_Sockets[EachRec.SocketIndex].SocketState;

			if (EachRec.PreviousSocketState == EAircraftSocketState::Closing)
			{
				SocketState = EAircraftSocketState::Closed;
				M_RoofAnimInst->SetAnimationSequenceForState(EAircraftSocketState::Closed, EachRec.SocketIndex);
			}
			else
			{
				// Opening or anything else -> ensure OPEN now
				SocketState = EAircraftSocketState::Open;
				M_RoofAnimInst->SetAnimationSequenceForState(EAircraftSocketState::Open, EachRec.SocketIndex);
			}
		}


		// Restore owner + map/delegates
		AssignAircraftToSocket(EachRec.AssignedAircraft.Get(), EachRec.SocketIndex);

		if (EachRec.bLandAircraftIfPackUpCancelled)
		{
			if (EachRec.AssignedAircraft->GetActiveCommandID() == EAbilityID::IdIdle)
			{
				EachRec.AssignedAircraft->ReturnToBase(true);
			}
			else
			{
				RTSFunctionLibrary::PrintString(
					TEXT("Aircraft was previously landed/landing but is not idle; cancel pack-up leaves it as-is."));
			}
		}
	}

	// Restore roof materials last
	OnPackupCancelled_RestoreRoofMaterials();

	//  we’ve consumed these records for this cancel; reset to avoid reuse/duplication
	M_PackingRecords.Empty();

	M_PackingState = EAirbasePackingState::Unpacked_Building;
}

void UAircraftOwnerComp::OnPackUpAirbaseComplete()
{
	if (not GetCanPackUp())
	{
		RTSFunctionLibrary::ReportError("PackupAirbase called but aircraft still inside on: " + GetOwner()->GetName());
		return;
	}
	if (not GetIsValidRoofSkelMesh())
	{
		return;
	}
	// Make sure to stop any running timers
	if (UWorld* World = GetWorld())
	{
		for (auto& Rec : M_Sockets)
		{
			World->GetTimerManager().ClearTimer(Rec.AnimTimerHandle);
		}
	}
	M_RoofMeshComp->DestroyComponent();
	M_RoofMeshComp = nullptr;
	M_RoofAnimInst = nullptr;
	M_PackingState = EAirbasePackingState::PackedUp_Vehicle;
	// Clear any packing records as now the airbase is fully packed up and this process can no longer be cancelled.
	M_PackingRecords.Empty();
}

bool UAircraftOwnerComp::IsAirBaseFull() const
{
	for (const auto& Rec : M_Sockets)
	{
		const bool bOccupied = Rec.AssignedAircraft.IsValid() || Rec.bM_ReservedByTraining;
		if (not bOccupied)
		{
			return false;
		}
	}
	return true;
}

bool UAircraftOwnerComp::OnAircraftTrainingStart(const EAircraftSubtype AircraftType)
{
	if (IsAirBaseFull())
	{
		return false;
	}
	const int32 Index = FindFirstFreeSocketIndex();
	if (Index == INDEX_NONE)
	{
		return false;
	}
	M_Sockets[Index].bM_ReservedByTraining = true;
	M_Sockets[Index].ReservedType = AircraftType;
	return true;
}

void UAircraftOwnerComp::OnAircraftTrainingCancelled(const EAircraftSubtype AircraftType)
{
	const int32 Index = FindFirstReservedForType(AircraftType);
	if (Index == INDEX_NONE)
	{
		return;
	}
	M_Sockets[Index].bM_ReservedByTraining = false;
}

bool UAircraftOwnerComp::OnAircraftTrained(AAircraftMaster* TrainedAircraft, const EAircraftSubtype AircraftType)
{
	if (not IsValid(TrainedAircraft))
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(this, "TrainedAircraft", "OnAircraftTrained");
		return false;
	}

	int32 SocketIndex = FindFirstReservedForType(AircraftType);
	if (SocketIndex == INDEX_NONE)
	{
		SocketIndex = FindFirstFreeSocketIndex();
	}

	if (SocketIndex == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError("OnAircraftTrained called but no free/Reserved socket found!");
		return false;
	}

	AssignAircraftToSocket(TrainedAircraft, SocketIndex);

	// Position the aircraft at the socket (landed)
	FVector Loc;
	if (GetSocketWorldLocation(SocketIndex, Loc))
	{
		TrainedAircraft->SetActorLocation(Loc);
	}
	M_Sockets[SocketIndex].bM_IsInside = true;
	// Notify for repairs.
	M_AirfieldRepairManager.OnAnyAircraftLanded();
	// Open the door so aircraft can VTO immediately if desired.
	RequestOpenIfNeeded(SocketIndex);

	return true;
}

bool UAircraftOwnerComp::OnAircraftChangesToThisOwner(AAircraftMaster* Aircraft)
{
	int32 SocketIndex = FindFirstFreeSocketIndex();
	if (SocketIndex == INDEX_NONE)
	{
		return false;
	}
	// Calls change owner on aircraft to set the new pointer.
	AssignAircraftToSocket(Aircraft, SocketIndex);
	return true;
}

void UAircraftOwnerComp::OnAircraftLanded(AAircraftMaster* Aircraft)
{
	const int32 SocketIndex = GetAssignedSocketIndex(Aircraft);
	if (SocketIndex == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError("OnAircraftLanded: Aircraft has no socket assignment on owner.");
		return;
	}
	auto& Rec = M_Sockets[SocketIndex];
	Rec.bM_IsInside = true;
	M_AirfieldRepairManager.OnAnyAircraftLanded();
}

bool UAircraftOwnerComp::OnAircraftRequestVTO(AAircraftMaster* Aircraft)
{
	if (not IsValid(Aircraft))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "Aircraft", "OnAircraftRequestVTO");
		return false;
	}
	const int32 Index = FindAssignedIndex(Aircraft);
	if (Index == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError("OnAircraftRequestVTO: Aircraft has no socket assignment on owner.");
		return false;
	}

	auto& Rec = M_Sockets[Index];
	Rec.bM_PendingVTO = true;
	RequestOpenIfNeeded(Index);
	// When open finishes, StartVTO is called from OnDoorOpened_GrantPending
	return true;
}

bool UAircraftOwnerComp::CancelPendingLandingFor(AAircraftMaster* Aircraft, bool bCloseDoor)
{
	if (!IsValid(Aircraft))
	{
		return false;
	}

	const int32 Index = FindAssignedIndex(Aircraft);
	if (Index == INDEX_NONE)
	{
		return false;
	}

	auto& Rec = M_Sockets[Index];
	Rec.bM_PendingLanding = false;

	if (bCloseDoor && M_PackingState != EAirbasePackingState::PackingUp)
	{
		RequestCloseIfNeeded(Index); // Play closing montage, clears/open-timer safely
	}
	return true;
}


bool UAircraftOwnerComp::IsAircraftAllowedToLandHere(AAircraftMaster* Aircraft)
{
	if (not IsValid(Aircraft))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "Aircraft", "IsAircraftAllowedToLandHere");
		return false;
	}
	if (not IsAirbaseUnPackedAsBuilding())
	{
		FRTSAircraftHelpers::AircraftDebug(
			"Aircraft : " + Aircraft->GetName() + " attempted to land at packed up airbase: " + GetOwner()->GetName(),
			FColor::Red);
		return false;
	}

	int32 Index = FindAssignedIndex(Aircraft);
	if (Index == INDEX_NONE)
	{
		// Not known yet; assign if space exists
		if (IsAirBaseFull())
		{
			RTSFunctionLibrary::ReportError("IsAircraftAllowedToLandHere: Airbase full, cannot accept aircraft.");
			return false;
		}
		Index = FindFirstFreeSocketIndex();
		if (Index == INDEX_NONE)
		{
			RTSFunctionLibrary::ReportError("IsAircraftAllowedToLandHere: could not find free socket.");
			return false;
		}
		AssignAircraftToSocket(Aircraft, Index);
	}

	// Allowed and assigned (possibly just now).
	return true;
}

bool UAircraftOwnerComp::RequestOpenBayDoorsForLanding(AAircraftMaster* Aircraft, bool& bOutDoorsAlreadyOpen)
{
	if (not IsValid(Aircraft))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "Aircraft", "RequestOpenBayDoors");
		return false;
	}

	const int32 Index = FindAssignedIndex(Aircraft);
	if (Index == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError("RequestOpenBayDoors: Aircraft has no socket assignment on owner.");
		return false;
	}

	auto& Rec = M_Sockets[Index];
	bOutDoorsAlreadyOpen = (Rec.SocketState == EAircraftSocketState::Open);
	Rec.bM_PendingLanding = true;
	RequestOpenIfNeeded(Index); // When open finishes, Notify_LandingBayOpen() is called on the aircraft.
	return true;
}

bool UAircraftOwnerComp::RequestOpenBayDoorsNoPendingAction(AAircraftMaster* Aircraft, bool& bOutDoorsAlreadyOpen,
                                                            float& OpeningAnimTime)
{
	if (not IsValid(Aircraft))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "Aircraft", "RequestOpenBayDoors");
		return false;
	}

	const int32 Index = FindAssignedIndex(Aircraft);
	if (Index == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError("RequestOpenBayDoors: Aircraft has no socket assignment on owner.");
		return false;
	}

	auto& Rec = M_Sockets[Index];
	bOutDoorsAlreadyOpen = (Rec.SocketState == EAircraftSocketState::Open);
	OpeningAnimTime = M_Settings.OpenAircraftSocketAnimTime;
	RequestOpenIfNeeded(Index); // When open finishes, no action is taken on the aircraft.
	return true;
}


int32 UAircraftOwnerComp::GetAssignedSocketIndex(const AAircraftMaster* Aircraft) const
{
	if (not IsValid(Aircraft))
	{
		return INDEX_NONE;
	}
	if (const int32* Found = M_AircraftToSocketIndex.Find(Aircraft))
	{
		return *Found;
	}
	return INDEX_NONE;
}

bool UAircraftOwnerComp::GetSocketWorldLocation(const int32 SocketIndex, FVector& OutWorldLocation) const
{
	if (not GetIsValidBuildingMesh())
	{
		return false;
	}
	if (not EnsureSocketIndexValid(SocketIndex, "GetSocketWorldLocation"))
	{
		return false;
	}
	const FName Name = M_Sockets[SocketIndex].SocketName;
	const FTransform T = M_BuildingMesh->GetSocketTransform(Name, ERelativeTransformSpace::RTS_World);
	OutWorldLocation = T.GetLocation();
	return true;
}

bool UAircraftOwnerComp::GetSocketWorldRotation(const int32 SocketIndex, FRotator& OutWorldRotation) const
{
	if (not GetIsValidBuildingMesh())
	{
		return false;
	}
	if (not EnsureSocketIndexValid(SocketIndex, "GetSocketWorldLocation"))
	{
		return false;
	}
	const FName Name = M_Sockets[SocketIndex].SocketName;
	const FTransform T = M_BuildingMesh->GetSocketTransform(Name, ERelativeTransformSpace::RTS_World);
	OutWorldRotation = T.GetRotation().Rotator();
	return true;
}

bool UAircraftOwnerComp::RemoveAircraftFromBuilding_CloseBayDoors(AAircraftMaster* Aircraft)
{
	int32 AircraftSocket = INDEX_NONE;
	bool bSuccesfullRemoval = false;
	for (auto EachSocket : M_Sockets)
	{
		if (EachSocket.AssignedAircraft == Aircraft)
		{
			bSuccesfullRemoval = ClearAssignmentForAircraft(Aircraft, AircraftSocket);
			break;
		}
	}
	if (bSuccesfullRemoval)
	{
		// Close the bay doors.
		RequestCloseIfNeeded(AircraftSocket);
		return true;
	}
	return false;
}

// -------------------- Discovery / assignment --------------------

void UAircraftOwnerComp::DiscoverLandingSockets()
{
	M_Sockets.Empty();

	auto CollectFromStatic = [this](UStaticMesh* SM)
	{
		for (const UStaticMeshSocket* Sock : SM->Sockets)
		{
			if (not Sock) continue;
			int32 Index = INDEX_NONE;
			if (ParseLandingIndex(Sock->SocketName, Index))
			{
				FSocketRecord Rec;
				Rec.SocketName = Sock->SocketName;
				M_Sockets.SetNum(FMath::Max(M_Sockets.Num(), Index + 1));
				M_Sockets[Index] = Rec;
			}
		}
	};

	auto CollectFromSkeletal = [this](USkeletalMesh* SK)
	{
		const int32 NumSockets = SK->NumSockets();
		for (uint8 i = 0; i < NumSockets; ++i)
		{
			const USkeletalMeshSocket* Sock = SK->GetSocketByIndex(i);
			if (not Sock) continue;
			int32 Index = INDEX_NONE;
			if (ParseLandingIndex(Sock->SocketName, Index))
			{
				FSocketRecord Rec;
				Rec.SocketName = Sock->SocketName;
				M_Sockets.SetNum(FMath::Max(M_Sockets.Num(), Index + 1));
				M_Sockets[Index] = Rec;
			}
		}
	};

	if (const UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(M_BuildingMesh))
	{
		if (IsValid(SMC->GetStaticMesh()))
		{
			CollectFromStatic(SMC->GetStaticMesh());
		}
	}
	else if (const USkeletalMeshComponent* SKC = Cast<USkeletalMeshComponent>(M_BuildingMesh))
	{
		if (IsValid(SKC->GetSkeletalMeshAsset()))
		{
			CollectFromSkeletal(SKC->GetSkeletalMeshAsset());
		}
	}

	// Compact out any default-constructed holes if indices were sparse
	TArray<FSocketRecord> Compact;
	for (auto& Rec : M_Sockets)
	{
		if (Rec.SocketName != NAME_None)
		{
			Compact.Add(Rec);
		}
	}
	M_Sockets = MoveTemp(Compact);
}

bool UAircraftOwnerComp::ParseLandingIndex(const FName& SocketName, int32& OutIndex)
{
	static const FString Prefix = TEXT("Landing");
	const FString Str = SocketName.ToString();
	if (not Str.StartsWith(Prefix))
	{
		return false;
	}
	// Accept "Landing_3" or "Landing3"
	int32 Underscore;
	if (Str.FindChar(TCHAR('_'), Underscore))
	{
		const FString Num = Str.Mid(Underscore + 1);
		OutIndex = FCString::Atoi(*Num);
		return OutIndex >= 0;
	}
	const FString Num = Str.RightChop(Prefix.Len());
	OutIndex = FCString::Atoi(*Num);
	return OutIndex >= 0;
}

int32 UAircraftOwnerComp::FindFirstFreeSocketIndex() const
{
	for (int32 i = 0; i < M_Sockets.Num(); ++i)
	{
		const auto& R = M_Sockets[i];
		if (not R.AssignedAircraft.IsValid() && not R.bM_ReservedByTraining)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

int32 UAircraftOwnerComp::FindFirstReservedForType(const EAircraftSubtype Type) const
{
	for (int32 i = 0; i < M_Sockets.Num(); ++i)
	{
		const auto& R = M_Sockets[i];
		if (R.bM_ReservedByTraining && R.ReservedType == Type)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

int32 UAircraftOwnerComp::FindAssignedIndex(const AAircraftMaster* Aircraft) const
{
	if (const int32* Found = M_AircraftToSocketIndex.Find(Aircraft))
	{
		return *Found;
	}
	return INDEX_NONE;
}

bool UAircraftOwnerComp::EnsureSocketIndexValid(const int32 Index, const FString& Context) const
{
	if (M_Sockets.IsValidIndex(Index))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Invalid socket index in " + Context + " on: " + GetOwner()->GetName());
	return false;
}

// -------------------- Door control --------------------

void UAircraftOwnerComp::RequestOpenIfNeeded(const int32 SocketIndex)
{
	if (not GetIsValidRoofAnimInstance())
	{
		return;
	}
	if (not EnsureSocketIndexValid(SocketIndex, "RequestOpenIfNeeded"))
	{
		return;
	}

	auto& Rec = M_Sockets[SocketIndex];
	if (Rec.SocketState == EAircraftSocketState::Open || Rec.SocketState == EAircraftSocketState::Opening)
	{
		// Already open/opening; fulfill immediately if opening finished later.
		if (Rec.SocketState == EAircraftSocketState::Open)
		{
			OnDoorOpened_GrantPending(SocketIndex);
		}
		return;
	}

	PlayDoorMontage(SocketIndex, EAircraftSocketState::Opening, M_Settings.OpenAircraftSocketAnimTime);
}

void UAircraftOwnerComp::RequestCloseIfNeeded(const int32 SocketIndex)
{
	if (not GetIsValidRoofAnimInstance())
	{
		return;
	}
	if (not EnsureSocketIndexValid(SocketIndex, "RequestCloseIfNeeded"))
	{
		return;
	}

	auto& Rec = M_Sockets[SocketIndex];
	if (Rec.SocketState == EAircraftSocketState::Closed || Rec.SocketState == EAircraftSocketState::Closing)
	{
		return;
	}

	PlayDoorMontage(SocketIndex, EAircraftSocketState::Closing, M_Settings.CloseAircraftSocketAnimTime);
}

void UAircraftOwnerComp::OnDoorOpened_GrantPending(const int32 SocketIndex)
{
	if (not EnsureSocketIndexValid(SocketIndex, "OnDoorOpened_GrantPending"))
	{
		return;
	}
	auto& Rec = M_Sockets[SocketIndex];

	// VTO?
	if (Rec.bM_PendingVTO && Rec.AssignedAircraft.IsValid())
	{
		Rec.bM_PendingVTO = false;
		Rec.bM_IsInside = false;
		// Tell aircraft to start the VTO (component orchestrated)
		Rec.AssignedAircraft->StartVto();
		M_AirfieldRepairManager.OnAnyAircraftVTO();
	}

	// Landing?
	if (Rec.bM_PendingLanding && Rec.AssignedAircraft.IsValid())
	{
		Rec.bM_PendingLanding = false;
		// Tell aircraft the bay is ready; it will call its landing routine and, on completion,
		// it is expected to inform owner (by simply being inside—we set flag on Notify start/finish).
		Rec.AssignedAircraft->StartVerticalLanding();
	}
}

void UAircraftOwnerComp::PlayDoorMontage(const int32 SocketIndex, const EAircraftSocketState NewState,
                                         const float PlayTime)
{
	if (not GetIsValidRoofAnimInstance())
	{
		return;
	}
	if (not EnsureSocketIndexValid(SocketIndex, "PlayDoorMontage"))
	{
		return;
	}

	auto& Rec = M_Sockets[SocketIndex];
	M_RoofAnimInst->PlayMontageForState(NewState, SocketIndex, PlayTime);
	Rec.SocketState = NewState;

	UWorld* World = GetWorld();
	// Arm timer to flip into the destination state and trigger grant
	if (not World)
	{
		return;
	}
	World->GetTimerManager().ClearTimer(Rec.AnimTimerHandle);

	const bool bToOpen = (NewState == EAircraftSocketState::Opening);
	const bool bToClose = (NewState == EAircraftSocketState::Closing);

	if (bToOpen)
	{
		// Already set the sequence for when the montage finishes.
		M_RoofAnimInst->SetAnimationSequenceForState(EAircraftSocketState::Open, SocketIndex);
		World->GetTimerManager().SetTimer(
			Rec.AnimTimerHandle,
			FTimerDelegate::CreateWeakLambda(this, [WeakAircraftOwnerComp = TWeakObjectPtr<UAircraftOwnerComp>(this), SocketIndex]()
			{
				if (not WeakAircraftOwnerComp.IsValid())
				{
					return;
				}

				UAircraftOwnerComp* StrongAircraftOwnerComp = WeakAircraftOwnerComp.Get();
				if (not StrongAircraftOwnerComp->M_Sockets.IsValidIndex(SocketIndex))
				{
					return;
				}
				StrongAircraftOwnerComp->M_Sockets[SocketIndex].SocketState = EAircraftSocketState::Open;
				StrongAircraftOwnerComp->OnDoorOpened_GrantPending(SocketIndex);
			}),
			FMath::Max(PlayTime, 0.01f), false);
	}
	else if (bToClose)
	{
		// Already set the sequence for when the montage finishes.
		M_RoofAnimInst->SetAnimationSequenceForState(EAircraftSocketState::Closed, SocketIndex);
		World->GetTimerManager().SetTimer(
			Rec.AnimTimerHandle,
			FTimerDelegate::CreateWeakLambda(this, [WeakAircraftOwnerComp = TWeakObjectPtr<UAircraftOwnerComp>(this), SocketIndex]()
			{
				if (not WeakAircraftOwnerComp.IsValid())
				{
					return;
				}

				UAircraftOwnerComp* StrongAircraftOwnerComp = WeakAircraftOwnerComp.Get();
				if (not StrongAircraftOwnerComp->M_Sockets.IsValidIndex(SocketIndex))
				{
					return;
				}
				StrongAircraftOwnerComp->M_Sockets[SocketIndex].SocketState = EAircraftSocketState::Closed;
			}),
			FMath::Max(PlayTime, 0.01f), false);
	}
}

// -------------------- State updates --------------------

void UAircraftOwnerComp::AssignAircraftToSocket(AAircraftMaster* Aircraft, const int32 SocketIndex)
{
	if (not IsValid(Aircraft))
	{
		return;
	}
	if (not EnsureSocketIndexValid(SocketIndex, "AssignAircraftToSocket"))
	{
		return;
	}

	// If this aircraft is already assigned on THIS owner but to a *different* socket, clear that old one.
	const int32 ExistingOnThisOwner = FindAssignedIndex(Aircraft);
	if (ExistingOnThisOwner != INDEX_NONE && ExistingOnThisOwner != SocketIndex)
	{
		ClearSocketRecByIndex(ExistingOnThisOwner);
	}

	// Assign here.
	auto& SocketRecord = M_Sockets[SocketIndex];
	SocketRecord.AssignedAircraft = Aircraft;
	SocketRecord.bM_ReservedByTraining = false;

	// Map: Aircraft -> SocketIndex
	M_AircraftToSocketIndex.Add(Aircraft, SocketIndex);

	// Tell aircraft it now belongs to us.
	Aircraft->SetOwnerAircraft(this);

	// Track death; capture the socket index so we can free it even if the aircraft pointer is invalid on death.
	BindDeath(Aircraft, SocketIndex);
}


bool UAircraftOwnerComp::ClearAssignmentForAircraft(AAircraftMaster* Aircraft, int32& OutSocketIndex)
{
	OutSocketIndex = INDEX_NONE;
	if (not IsValid(Aircraft))
	{
		return false;
	}
	const int32 Existing = FindAssignedIndex(Aircraft);
	if (Existing == INDEX_NONE)
	{
		return false;
	}
	OutSocketIndex = Existing;
	return ClearSocketRecByIndex(Existing);
}

bool UAircraftOwnerComp::ClearSocketRecByIndex(const int32 SocketIndex)
{
	if (!EnsureSocketIndexValid(SocketIndex, "ClearBySocketIndex"))
		return false;

	auto& Rec = M_Sockets[SocketIndex];

	if (Rec.AssignedAircraft.IsValid())
	{
		UnbindDeath(Rec.AssignedAircraft.Get());
		M_AircraftToSocketIndex.Remove(Rec.AssignedAircraft);
	}
	else
	{
		// Remove any map entry that pointed at this socket
		for (auto It = M_AircraftToSocketIndex.CreateIterator(); It; ++It)
		{
			if (It.Value() == SocketIndex)
			{
				It.RemoveCurrent();
				break;
			}
		}
	}

	Rec.AssignedAircraft.Reset();
	Rec.bM_IsInside = false;
	Rec.bM_ReservedByTraining = false;
	Rec.bM_PendingVTO = false;
	Rec.bM_PendingLanding = false;
	return true;
}


// -------------------- Delegates --------------------

void UAircraftOwnerComp::BindDeath(AAircraftMaster* Aircraft, int32 SocketIndex)
{
	if (!IsValid(Aircraft)) return;

	TWeakObjectPtr<UAircraftOwnerComp> WeakOwner(this);
	FDelegateHandle Handle = Aircraft->OnUnitDies.AddLambda([WeakOwner, SocketIndex]()
	{
		if (!WeakOwner.IsValid())
			return;
		WeakOwner->ClearSocketRecByIndex(SocketIndex);
		WeakOwner->M_AirfieldRepairManager.OnAircraftDies();
	});

	M_DeathDelegateHandles.Add(Aircraft, Handle);
}


void UAircraftOwnerComp::UnbindDeath(AAircraftMaster* Aircraft)
{
	if (!IsValid(Aircraft)) return;

	if (FDelegateHandle* Handle = M_DeathDelegateHandles.Find(Aircraft))
	{
		Aircraft->OnUnitDies.Remove(*Handle);
		M_DeathDelegateHandles.Remove(Aircraft);
	}
}


void UAircraftOwnerComp::OnBoundAircraftDied(TWeakObjectPtr<AAircraftMaster> WeakAircraft)
{
	if (not WeakAircraft.IsValid())
	{
		return;
	}
	int32 AircraftSocket = INDEX_NONE;
	ClearAssignmentForAircraft(WeakAircraft.Get(), AircraftSocket);
}

// -------------------- Errors / helpers --------------------

void UAircraftOwnerComp::ReportInvalidOwnerState(const FString& Context) const
{
	RTSFunctionLibrary::ReportError("Invalid state in " + Context + " on: " + GetOwner()->GetName());
}

bool UAircraftOwnerComp::HasAnyAircraftInside() const
{
	for (const auto& Rec : M_Sockets)
	{
		if (Rec.bM_IsInside && Rec.AssignedAircraft.IsValid())
		{
			return true;
		}
	}
	return false;
}

void UAircraftOwnerComp::DebugTick(const float DeltaTime)
{
	if (not GetOwner())
	{
		return;
	}
	const float XOffset = 300;
	const float ZOffset = 800;
	const FVector CompWorldLocation = GetOwner()->GetActorLocation();
	if (UWorld* World = GetWorld())
	{
		int32 i = 0;
		for (auto EachSocket : M_Sockets)
		{
			const int32 IMod3 = i % 3;
			const bool bIMod3Is2 = (IMod3 == 2) ? 1 : 0;
			const float Sign = bIMod3Is2 ? -1.0f : 1.0f;
			FString DebugStr = EachSocket.GetDebugStr();
			float AddX = IMod3 == 0 ? 0.0f : (XOffset * Sign);
			FVector DebugLocation = CompWorldLocation + FVector(AddX, 0.0f, ZOffset);
			DrawDebugString(World, DebugLocation, DebugStr, nullptr, FColor::White, DeltaTime, false);
		}
	}
}

void UAircraftOwnerComp::OnPackUp_SetRoofToConstruction(UMaterialInstance* NomadicConstructionMaterial)
{
	M_RoofMaterials.Empty();
	if (NomadicConstructionMaterial && GetIsValidRoofSkelMesh())
	{
		M_RoofMaterials = M_RoofMeshComp->GetMaterials();
		const int32 NumMats = M_RoofMaterials.Num();
		for (int32 i = 0; i < NumMats; ++i)
		{
			M_RoofMeshComp->SetMaterial(i, NomadicConstructionMaterial);
		}
	}
}

void UAircraftOwnerComp::OnPackupCancelled_RestoreRoofMaterials()
{
	const int32 NumMats = M_RoofMaterials.Num();
	if (NumMats > 0 && GetIsValidRoofSkelMesh())
	{
		for (int32 i = 0; i < NumMats; ++i)
		{
			if (not M_RoofMaterials.IsValidIndex(i) || not M_RoofMaterials[i])
			{
				RTSFunctionLibrary::ReportError("OnPackupCancelled_RestoreRoofMaterials: Invalid material index");
				continue;
			}
			M_RoofMeshComp->SetMaterial(i, M_RoofMaterials[i]);
		}
	}
}
