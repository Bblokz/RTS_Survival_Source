// Copyright (C) 2025 Bas Blokzijl - All rights reserved.

#include "Cargo.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "RTS_Survival/GameUI/Healthbar/GarrisonHealthBar/W_GarrisonHealthBar.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/CargoMechanic/CargoSquad/CargoSquad.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"

TArray<FName> FCargoSocketOccupancy::GetEmptySockets() const
{
	TArray<FName> Empty;
	for (const FName& Sock : M_AllCargoSockets)
	{
		if (not M_SocketToUnit.Contains(Sock) || not IsValid(M_SocketToUnit[Sock]))
		{
			Empty.Add(Sock);
		}
	}
	return Empty;
}

UCargo::UCargo()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCargo::OnSquadCompletelyDiedInside(ASquadController* SquadController)
{
	if (not IsValid(SquadController))
	{
		return;
	}

	// 1) Free any seats occupied by this squad so future squads get clean seat allocation.
	ClearSeatOccupancyForSquad(SquadController);

	// 2) Release the UI slot without double-adjusting the seat counters.
	const int32 SlotIndex = GetSlotIndexForSquad(SquadController);
	if (SlotIndex != INDEX_NONE)
	{
		if (EnsureIsValidGarrisonWidget())
		{
			FTrainingOption UnitID;
			if (const URTSComponent* RTS = SquadController->FindComponentByClass<URTSComponent>())
			{
				UnitID = RTS->GetUnitTrainingOption();
			}
			else
			{
				RTSFunctionLibrary::ReportNullErrorComponent(
					SquadController, "RTSComponent", "UCargo::OnSquadCompletelyDiedInside");
			}

			// bIsOccupied = false, SeatsDelta = 0 -> do not subtract seats again;
			// OnUnitDiedWhileInside already kept the seats text in sync per death.
			M_GarrisonHpBarWidget->UpdateGarrisonSlot(SlotIndex, /*bIsOccupied=*/false, UnitID, /*SeatsDelta=*/0);
		}

		ReleaseSlotForSquad(SquadController);
	}

	// 3) Make sure vacancy no longer counts this squad.
	RegisterSquadAtVacancy(SquadController, /*bRegister=*/false);
}

void UCargo::OnOwnerDies(const ERTSDeathType DeathType)
{
	// Only the server should authoritatively kill actors.
	if (not GetOwner() || not GetOwner()->HasAuthority())
	{
		return;
	}

	const TArray<TObjectPtr<ASquadController>> InsideSquads = OnOwnerDies_GetInsideSquadsSnapshot();
	OnOwnerDies_KillAllInsideSquads(InsideSquads);
	OnOwnerDies_ResetState();
}

void UCargo::AdjustSeatsForSquad(ASquadController* Squad, const int32 SeatsDelta)
{
	if (SeatsDelta == 0 || not EnsureIsValidGarrisonWidget() || not IsValid(Squad))
	{
		return;
	}

	const int32 SlotIndex = GetSlotIndexForSquad(Squad);
	if (SlotIndex == INDEX_NONE)
	{
		return; // Not inside (or no slot claimed), nothing to show.
	}

	// Keep the slot occupied; only seat text changes by SeatsDelta (can be negative).
	FTrainingOption UnitID;
	if (const URTSComponent* RTS = Squad->FindComponentByClass<URTSComponent>())
	{
		UnitID = RTS->GetUnitTrainingOption();
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorComponent(Squad, "RTSComponent", "UCargo::AdjustSeatsForSquad");
	}

	M_GarrisonHpBarWidget->UpdateGarrisonSlot(SlotIndex, /*bIsOccupied=*/true, UnitID, SeatsDelta);
}

void UCargo::SetIsEnabled(const bool bEnable)
{
	bM_IsEnabled = bEnable;
	// do not ensure, if the widget is not loaded we propagate the bool later.
	if (M_GarrisonHpBarWidget.IsValid())
	{
		M_GarrisonHpBarWidget->OnGarrisonEnabled(bM_IsEnabled);
	}
	if (not bEnable)
	{
		MoveOutGarrisonedInfantryOnDisabled();
	}
}

bool UCargo::GetIsEnabledAndVacant() const
{
	const bool bIsVacant = M_VacancyState.HasVacancy();
	return bM_IsEnabled && bIsVacant;
}

UPrimitiveComponent* UCargo::GetCargoMesh() const
{
	if (M_CargoMesh.IsValid())
	{
		return M_CargoMesh.Get();
	}
	return nullptr;
}

void UCargo::SetupCargo(UPrimitiveComponent* MeshComponent, const int32 MaxSupportedSquads,
                        const FVector CargoPositionsOffset, const EGarrisonSeatsTextType SeatTextType,
                        const bool bStartEnabled)
{
	M_CargoMesh = MeshComponent;
	bM_IsEnabled = bStartEnabled;
	if (not GetIsValidCargoMesh())
	{
		return;
	}
	M_Offset = CargoPositionsOffset;
	M_SeatTextType = SeatTextType;

	TArray<FName> AllSocketNames;
	CollectSocketNames(AllSocketNames);

	M_SocketOccupancy.M_AllCargoSockets = FilterSocketsContains(AllSocketNames, TEXT("cargo"));
	M_EntranceSockets = FilterSocketsContains(AllSocketNames, TEXT("entrance"));

	if (M_SocketOccupancy.M_AllCargoSockets.Num() == 0)
	{
		RTSFunctionLibrary::ReportError(
			"UCargo::SetupCargo -> No sockets containing 'cargo' found on mesh for: " +
			GetOwner()->GetName());
	}
	if (M_EntranceSockets.Num() == 0)
	{
		RTSFunctionLibrary::ReportError(
			"UCargo::SetupCargo -> No sockets containing 'entrance' found on mesh for: " +
			GetOwner()->GetName());
	}

	M_VacancyState.M_MaxSquadsSupported = FMath::Max(0, MaxSupportedSquads);
	M_VacancyState.M_CurrentSquads = 0;
	M_VacancyState.M_InsideSquads.Empty();
	RegisterCallBackToInitHealthComponentOnOwner();
}


bool UCargo::GetIsValidCargoMesh() const
{
	if (M_CargoMesh.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(GetOwner(), "M_CargoMesh",
	                                                      "UCargo::GetIsValidCargoMesh");
	return false;
}

FVector UCargo::GetClosestEntranceWorldLocation(const FVector& FromWorldLocation) const
{
	if (not GetIsValidCargoMesh() || M_EntranceSockets.Num() == 0)
	{
		return GetOwner() ? GetOwner()->GetActorLocation() : FromWorldLocation;
	}
	float BestDistSq = TNumericLimits<float>::Max();
	FVector BestLocation = GetOwner()->GetActorLocation();

	for (const FName& Sock : M_EntranceSockets)
	{
		const FTransform T = GetSocketWorldTransform(Sock);
		const float D = FVector::DistSquared(T.GetLocation(), FromWorldLocation);
		if (D < BestDistSq)
		{
			BestDistSq = D;
			BestLocation = T.GetLocation();
		}
	}
	return BestLocation;
}

bool UCargo::RequestCanEnterCargo(ASquadController* SquadController, TMap<ASquadUnit*, FName>& OutAssignments)
{
	if (!SquadController || !GetIsValidCargoMesh() || !M_VacancyState.HasVacancy())
		return false;

	const TArray<ASquadUnit*> Units = SquadController->GetSquadUnitsChecked();
	OutAssignments.Reset();

	TArray<FName> Empty = M_SocketOccupancy.GetEmptySockets();
	if (Empty.Num() < Units.Num())
		return false; // not enough seats for the whole squad

	// seat everyone 1:1
	for (int32 i = 0; i < Units.Num(); ++i)
	{
		ASquadUnit* Unit = Units[i];
		if (!IsValid(Unit)) continue;
		const FName Sock = Empty[i];
		OutAssignments.Add(Unit, Sock);
		M_SocketOccupancy.M_SocketToUnit.Add(Sock, Unit);
	}

	RegisterSquadAtVacancy(SquadController, true);
	return true;
}

void UCargo::OnSquadEntered(ASquadController* SquadController)
{
	if (not IsValid(SquadController))
	{
		return;
	}
	const int32 SlotIndex = AssignSlotForSquad(SquadController);
	if (SlotIndex != INDEX_NONE)
	{
		UpdateGarrisonWidgetForSquad(SquadController, SlotIndex, /*bIsEntering=*/true);
	}
}

void UCargo::OnSquadExited(ASquadController* SquadController, const TArray<ASquadUnit*>& Units)
{
	// Find the UI slot for this squad (if any) and update the widget first.
	const int32 SlotIndex = GetSlotIndexForSquad(SquadController);
	if (SlotIndex != INDEX_NONE)
	{
		// This subtracts the number of seats currently occupied by this squad.
		UpdateGarrisonWidgetForSquad(SquadController, SlotIndex, /*bIsEntering=*/false);

		// Free the UI slot so others can use it.
		ReleaseSlotForSquad(SquadController);
	}

	// Now free seat socket occupancy (so UpdateGarrisonWidgetForSquad could count them correctly above).
	M_SocketOccupancy.ClearUnits(Units);

	// And unregister the squad from vacancy state.
	RegisterSquadAtVacancy(SquadController, false);
}

FTransform UCargo::GetSocketWorldTransform(const FName& SocketName) const
{
	if (!GetIsValidCargoMesh()) return FTransform::Identity;

	if (const USkeletalMeshComponent* Skel = Cast<USkeletalMeshComponent>(M_CargoMesh.Get()))
		return Skel->GetSocketTransform(SocketName, RTS_World);

	if (const UStaticMeshComponent* Stat = Cast<UStaticMeshComponent>(M_CargoMesh.Get()))
		return Stat->GetSocketTransform(SocketName, RTS_World);

	return GetOwner() ? GetOwner()->GetActorTransform() : FTransform::Identity;
}

void UCargo::BeginPlay()
{
	Super::BeginPlay();

	BeginPlay_SetOwnerInterface();
}

void UCargo::ClearSeatOccupancyForSquad(ASquadController* Squad)
{
	if (not IsValid(Squad))
	{
		return;
	}

	TArray<ASquadUnit*> UnitsToClear;
	for (const TPair<FName, TObjectPtr<ASquadUnit>>& SocketAndUnit : M_SocketOccupancy.M_SocketToUnit)
	{
		ASquadUnit* const Occupant = SocketAndUnit.Value.Get();
		if (not IsValid(Occupant))
		{
			continue;
		}
		if (Occupant->GetSquadControllerChecked() == Squad)
		{
			UnitsToClear.Add(Occupant);
		}
	}

	if (UnitsToClear.Num() == 0)
	{
		return;
	}

	M_SocketOccupancy.ClearUnits(UnitsToClear);
}


void UCargo::OnHealthBarWidgetInit(UHealthComponent* InitializedComponent)
{
	if (not IsValid(InitializedComponent) || not InitializedComponent->GetHealthBarWidgetComp())
	{
		return;
	}
	UW_GarrisonHealthBar* GarrisonWidget = Cast<UW_GarrisonHealthBar>(
		InitializedComponent->GetHealthBarWidgetComp()->GetUserWidgetObject());
	if (not IsValid(GarrisonWidget))
	{
		OnHealthBarWidgetInit_NoValidWidget();
		return;
	}
	M_GarrisonHpBarWidget = GarrisonWidget;
	// Set data of how many squads, seats and seat text.
	InitGarrisonWidget();
}

void UCargo::CollectSocketNames(TArray<FName>& OutAllSockets) const
{
	OutAllSockets.Reset();

	if (not GetIsValidCargoMesh())
	{
		return;
	}
	if (const USkeletalMeshComponent* Skel = Cast<USkeletalMeshComponent>(M_CargoMesh.Get()))
	{
		OutAllSockets = Skel->GetAllSocketNames();
		return;
	}
	if (const UStaticMeshComponent* Stat = Cast<UStaticMeshComponent>(M_CargoMesh.Get()))
	{
		if (const UStaticMesh* SM = Stat->GetStaticMesh())
		{
			for (const UStaticMeshSocket* S : SM->Sockets)
			{
				if (S) { OutAllSockets.Add(S->SocketName); }
			}
		}
	}
}

TArray<FName> UCargo::FilterSocketsContains(const TArray<FName>& In, const FString& Substring)
{
	TArray<FName> Out;
	const FString Lower = Substring.ToLower();

	for (const FName& N : In)
	{
		const FString S = N.ToString().ToLower();
		if (S.Contains(Lower))
		{
			Out.Add(N);
		}
	}
	return Out;
}

void UCargo::RegisterCallBackToInitHealthComponentOnOwner()
{
	AActor* const OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	UHealthComponent* const HealthComp = OwnerActor->FindComponentByClass<UHealthComponent>();
	if (!IsValid(HealthComp))
	{
		return;
	}

	// Uses the templated overload: void (T::*)(UHealthComponent*), T* Owner
	HealthComp->M_HealthBarWidgetCallbacks.CallbackOnHealthBarReady(&UCargo::OnHealthBarWidgetInit, this);
}

void UCargo::OnHealthBarWidgetInit_NoValidWidget() const
{
	const FString OwnerName = GetOwner() ? GetOwner()->GetName() : "NoOwner";
	const FString Name = GetName();
	RTSFunctionLibrary::ReportError("The cargo component attempted to get a valid garrison widget from the initialized"
		"health component on the owner: " + OwnerName +
		" but the cast to UW_GarrisonHealthBar failed. Cargo component name: " + Name);
}

// ---------------- Helpers (private) ----------------

bool UCargo::EnsureIsValidGarrisonWidget() const
{
	if (not M_GarrisonHpBarWidget.IsValid())
	{
		// RTSFunctionLibrary::ReportError("No valid garrison widget in cargo component on owner: " +
		// 	(GetOwner() ? GetOwner()->GetName() : "NoOwner"));
		return false;
	}
	return true;
}

void UCargo::InitGarrisonWidget()
{
	if (not EnsureIsValidGarrisonWidget())
	{
		return;
	}

	const int32 TotalSeats = M_SocketOccupancy.M_AllCargoSockets.Num();
	int32 SlotsForWidget = M_VacancyState.M_MaxSquadsSupported;
	if (SlotsForWidget > 3)
	{
		// In this case we let the seats logic drive occupancy instead of slots.
		SlotsForWidget = 0;
		// special case: instruct widget to hide all garrison boxes, will later be overwritten at setup; this is
		// to init the UI with only the seat text.
		M_VacancyState.M_MaxSquadsSupported = 16;
	}

	M_GarrisonHpBarWidget->SetupGarrison(
		SlotsForWidget,
		TotalSeats,
		M_SeatTextType);
	// update in case the flag was changed while the widget UI was still async loading.
	M_GarrisonHpBarWidget->OnGarrisonEnabled(bM_IsEnabled);
}

int32 UCargo::AssignSlotForSquad(ASquadController* Squad)
{
	SweepInvalidSquadSlots();
	if (not IsValid(Squad) || M_VacancyState.M_MaxSquadsSupported <= 0)
	{
		return INDEX_NONE;
	}
	if (const int32* const Found = M_SquadToGarrisonSlot.Find(Squad))
	{
		return *Found; // already assigned
	}

	// Pick the lowest free index.
	for (int32 i = 0; i < M_VacancyState.M_MaxSquadsSupported; ++i)
	{
		if (GetIsSlotIndexInUse(i))
		{
			continue;
		}
		M_SquadToGarrisonSlot.Add(Squad, i);
		return i;
	}
	return INDEX_NONE;
}

void UCargo::ReleaseSlotForSquad(ASquadController* Squad)
{
	if (not IsValid(Squad))
	{
		return;
	}
	M_SquadToGarrisonSlot.Remove(Squad);
}

int32 UCargo::GetSlotIndexForSquad(ASquadController* Squad) const
{
	if (not IsValid(Squad))
	{
		return INDEX_NONE;
	}
	if (const int32* Found = M_SquadToGarrisonSlot.Find(Squad))
	{
		return *Found;
	}
	return INDEX_NONE;
}

bool UCargo::GetIsSlotIndexInUse(const int32 SlotIndex) const
{
	for (const auto& Pair : M_SquadToGarrisonSlot)
	{
		if (!Pair.Key.IsValid())
		{
			continue;
		}
		if (Pair.Value == SlotIndex)
		{
			return true;
		}
	}
	return false;
}


void UCargo::SweepInvalidSquadSlots()
{
	TArray<TWeakObjectPtr<ASquadController>> ToRemove;
	for (const auto& Pair : M_SquadToGarrisonSlot)
	{
		if (!Pair.Key.IsValid())
		{
			ToRemove.Add(Pair.Key);
		}
	}
	for (const auto& K : ToRemove)
	{
		M_SquadToGarrisonSlot.Remove(K);
	}
}

void UCargo::RegisterSquadAtVacancy(ASquadController* Squad, const bool bRegister)
{
	if (bRegister)
	{
		const int32 OldCount = M_VacancyState.M_CurrentSquads;

		M_VacancyState.RegisterSquad(Squad);

		// If this is the first squad entering, start the shuffle timer
		if (OldCount == 0 && M_VacancyState.M_CurrentSquads == 1)
		{
			StartSeatShuffleTimer();
		}

		if (GetIsValidCargoOwner())
		{
			M_CargoOwner->OnSquadRegistered(Squad);
		}
		return;
	}

	const int32 OldCount = M_VacancyState.M_CurrentSquads;

	M_VacancyState.UnregisterSquad(Squad);

	// If this was the last squad leaving, stop the shuffle timer.
	if (OldCount > 0 && M_VacancyState.M_CurrentSquads == 0)
	{
		StopSeatShuffleTimer();
	}

	// Only fire OnCargoEmpty when transitioning from non-empty to empty.
	if (OldCount > 0 && M_VacancyState.M_CurrentSquads == 0 && GetIsValidCargoOwner())
	{
		M_CargoOwner->OnCargoEmpty();
	}
}

void UCargo::BeginPlay_SetOwnerInterface()
{
	AActor* OwnerActor = GetOwner();
	if (not OwnerActor)
	{
		return;
	}
	ICargoOwner* Interface = Cast<ICargoOwner>(OwnerActor);
	if (Interface)
	{
		M_CargoOwner.SetObject(OwnerActor);
		M_CargoOwner.SetInterface(Interface);
	}
}

void UCargo::BeginPlay_InitSeatShuffleTimer()
{
	AActor* OwnerActor = GetOwner();
	if (not OwnerActor || not OwnerActor->HasAuthority())
	{
		return;
	}
	if (M_SeatShuffleIntervalSeconds <= 0.f)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}

	TWeakObjectPtr<UCargo> WeakThis(this);

	FTimerDelegate SeatShuffleDelegate;
	SeatShuffleDelegate.BindLambda(
		[WeakThis]()
		{
			if (not WeakThis.IsValid())
			{
				return;
			}
			WeakThis->TickSeatReassignment();
		});

	FTimerHandle SeatShuffleTimerHandle;
	World->GetTimerManager().SetTimer(
		SeatShuffleTimerHandle,
		SeatShuffleDelegate,
		M_SeatShuffleIntervalSeconds,
		true);
}


void UCargo::TickSeatReassignment()
{
        if (not CanShuffleSeats())
        {
                return;
        }

        const int32 TotalSockets = M_SocketOccupancy.M_AllCargoSockets.Num();
        const int32 FirstEmptyIndex = FindFirstEmptySeatIndexAndClean(TotalSockets);
        if (FirstEmptyIndex == INDEX_NONE)
        {
                return;
        }

        ASquadUnit* UnitToMove = nullptr;
        int32 SourceIndex = INDEX_NONE;
        if (not TryFindNextUnitToMove(FirstEmptyIndex, UnitToMove, SourceIndex))
        {
                return;
        }

        const FName& SourceSocketName = M_SocketOccupancy.M_AllCargoSockets[SourceIndex];
        const FName& TargetSocketName = M_SocketOccupancy.M_AllCargoSockets[FirstEmptyIndex];

        if (SourceSocketName == TargetSocketName)
        {
                return;
        }

        UpdateSeatOccupancyAfterMove(SourceSocketName, TargetSocketName, UnitToMove);
        TryRequestSeatMoveOnSquad(UnitToMove, TargetSocketName);
}

bool UCargo::CanShuffleSeats() const
{
        if (not bM_IsEnabled)
        {
                return false;
        }
        if (M_VacancyState.M_InsideSquads.Num() == 0)
        {
                return false;
        }

        if (M_SocketOccupancy.M_AllCargoSockets.Num() <= 1 || M_SocketOccupancy.M_SocketToUnit.Num() == 0)
        {
                return false;
        }

        return true;
}

int32 UCargo::FindFirstEmptySeatIndexAndClean(const int32 TotalSockets)
{
        for (int32 SocketIndex = 0; SocketIndex < TotalSockets; ++SocketIndex)
        {
                const FName& SocketName = M_SocketOccupancy.M_AllCargoSockets[SocketIndex];

                TObjectPtr<ASquadUnit>* const OccupantPtr = M_SocketOccupancy.M_SocketToUnit.Find(SocketName);
                ASquadUnit* OccupantUnit = nullptr;
                if (OccupantPtr)
                {
                        OccupantUnit = OccupantPtr->Get();
                }

                if (not OccupantPtr || not IsValid(OccupantUnit))
                {
                        if (OccupantPtr && not IsValid(OccupantUnit))
                        {
                                M_SocketOccupancy.M_SocketToUnit.Remove(SocketName);
                        }
                        return SocketIndex;
                }
        }

        return INDEX_NONE;
}

bool UCargo::TryFindNextUnitToMove(const int32 EmptySeatIndex, ASquadUnit*& OutUnitToMove, int32& OutSourceIndex)
{
        OutUnitToMove = nullptr;
        OutSourceIndex = INDEX_NONE;

        const int32 TotalSockets = M_SocketOccupancy.M_AllCargoSockets.Num();
        if (TotalSockets == 0)
        {
                return false;
        }

        const int32 StartIndex = FMath::Max(M_SocketOccupancy.M_NextRoundRobinIndex, EmptySeatIndex + 1) % TotalSockets;

        for (int32 Offset = 0; Offset < TotalSockets; ++Offset)
        {
                const int32 SocketIndex = (StartIndex + Offset) % TotalSockets;
                if (SocketIndex == EmptySeatIndex)
                {
                        continue;
                }

                const FName& SocketName = M_SocketOccupancy.M_AllCargoSockets[SocketIndex];
                TObjectPtr<ASquadUnit>* const OccupantPtr = M_SocketOccupancy.M_SocketToUnit.Find(SocketName);
                if (not OccupantPtr)
                {
                        continue;
                }

                ASquadUnit* const OccupantUnit = OccupantPtr->Get();
                if (not IsValid(OccupantUnit))
                {
                        M_SocketOccupancy.M_SocketToUnit.Remove(SocketName);
                        continue;
                }

                OutUnitToMove = OccupantUnit;
                OutSourceIndex = SocketIndex;
                M_SocketOccupancy.M_NextRoundRobinIndex = (SocketIndex + 1) % TotalSockets;
                return true;
        }

        M_SocketOccupancy.M_NextRoundRobinIndex = StartIndex;
        return false;
}

void UCargo::UpdateSeatOccupancyAfterMove(const FName& SourceSocketName, const FName& TargetSocketName,
                                          ASquadUnit* UnitToMove)
{
        M_SocketOccupancy.M_SocketToUnit.Remove(SourceSocketName);
        M_SocketOccupancy.M_SocketToUnit.Add(TargetSocketName, UnitToMove);
}

bool UCargo::TryRequestSeatMoveOnSquad(ASquadUnit* UnitToMove, const FName& TargetSocketName)
{
        TObjectPtr<ASquadController> SquadController = UnitToMove->GetSquadControllerChecked();
        if (not IsValid(SquadController))
        {
                return false;
        }

        UCargoSquad* CargoSquad = SquadController->FindComponentByClass<UCargoSquad>();
        if (not IsValid(CargoSquad))
        {
                RTSFunctionLibrary::ReportNullErrorComponent(
                        SquadController.Get(), "CargoSquad", "UCargo::TickSeatReassignment");
                return false;
        }

        CargoSquad->HandleSeatReassignmentFromCargo(this, UnitToMove, TargetSocketName);
        return true;
}


void UCargo::UpdateGarrisonWidgetForSquad(ASquadController* Squad, const int32 SlotIndex, const bool bIsEntering)
{
	// Validate widget first.
	if (not EnsureIsValidGarrisonWidget())
	{
		return;
	}

	// Validate squad.
	if (not IsValid(Squad))
	{
		const FString OwnerName = GetOwner() ? GetOwner()->GetName() : "NoOwner";
		RTSFunctionLibrary::ReportError(
			"UCargo::UpdateGarrisonWidgetForSquad -> Invalid Squad (Owner: " + OwnerName + ")");
		return;
	}

	// Validate SlotIndex against the configured max squad slots.
	if (SlotIndex < 0 || SlotIndex >= M_VacancyState.M_MaxSquadsSupported)
	{
		const FString OwnerName = GetOwner() ? GetOwner()->GetName() : "NoOwner";
		RTSFunctionLibrary::ReportError(
			"UCargo::UpdateGarrisonWidgetForSquad -> SlotIndex out of range for Owner: " + OwnerName +
			" | Index: " + FString::FromInt(SlotIndex) +
			" | MaxSlots: " + FString::FromInt(M_VacancyState.M_MaxSquadsSupported));
		return;
	}

	// --- Get the TrainingOption ID of the squad (who is occupying this slot) ---
	FTrainingOption UnitID;
	const URTSComponent* const SquadRTS = Squad->FindComponentByClass<URTSComponent>();
	if (not IsValid(SquadRTS))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(Squad, "RTSComponent", "UCargo::UpdateGarrisonWidgetForSquad");
	}
	else
	{
		UnitID = SquadRTS->GetUnitTrainingOption();
	}

	// --- Compute seats delta by counting how many of THIS squad's units are currently seated ---
	int32 SeatsDelta = 0;
	for (const TPair<FName, TObjectPtr<ASquadUnit>>& SocketAndUnit : M_SocketOccupancy.M_SocketToUnit)
	{
		const ASquadUnit* const Occupant = SocketAndUnit.Value.Get();
		if (IsValid(Occupant) && Occupant->GetSquadControllerChecked() == Squad)
		{
			SeatsDelta++;
		}
	}


	// Fallbacks:
	// - If we're entering and (due to call order) sockets aren't filled yet, use current alive unit count as best-effort.
	// - If we're leaving and caller already cleared sockets, use current alive unit count (clamped to total seats).
	if (SeatsDelta <= 0)
	{
		const int32 AliveNow = Squad->GetSquadUnitsChecked().Num();
		if (bIsEntering)
		{
			SeatsDelta = AliveNow;
		}
		else
		{
			SeatsDelta = FMath::Min(AliveNow, M_SocketOccupancy.M_AllCargoSockets.Num());
		}
	}

	// Finally, push to the widget.
	// - bIsOccupied = bIsEntering (true => add seats; false => subtract seats)
	// - SeatsTakenOrBecameVacant = SeatsDelta
	M_GarrisonHpBarWidget->UpdateGarrisonSlot(SlotIndex, bIsEntering, UnitID, SeatsDelta);
}

bool UCargo::GetIsValidCargoOwner() const
{
	if (not M_CargoOwner.GetInterface() || not IsValid(M_CargoOwner.GetObject()))
	{
		// Fail silently as not all cargo component users use the interface.
		return false;
	}
	return true;
}

TArray<TObjectPtr<ASquadController>> UCargo::OnOwnerDies_GetInsideSquadsSnapshot() const
{
	return M_VacancyState.M_InsideSquads.Array();
}

void UCargo::OnOwnerDies_KillAllInsideSquads(const TArray<TObjectPtr<ASquadController>>& InsideSquads)
{
	for (const TObjectPtr<ASquadController>& SquadPtr : InsideSquads)
	{
		ASquadController* Squad = SquadPtr.Get();
		if (not IsValid(Squad))
		{
			continue;
		}
		OnOwnerDies_HandleSquad(Squad);
	}
}

void UCargo::OnOwnerDies_HandleSquad(ASquadController* Squad)
{
	// Get all units from this squad and clear any socket occupancy for them.
	const TArray<ASquadUnit*> Units = Squad->GetSquadUnitsChecked();
	M_SocketOccupancy.ClearUnits(Units);

	for (ASquadUnit* Unit : Units)
	{
		if (not IsValid(Unit))
		{
			continue;
		}
		OnOwnerDies_DetachIfAttachedToOwner(Unit);
		OnOwnerDies_KillUnit(Unit);
	}

	// Mark the squad as no longer inside.
	RegisterSquadAtVacancy(Squad, false);
}

void UCargo::OnOwnerDies_KillUnit(ASquadUnit* Unit)
{
	// Route through the normal damage pipeline so death events/replication fire.
	UGameplayStatics::ApplyDamage(
		Unit,
		TNumericLimits<float>::Max(), // big enough to guarantee kill
		/* EventInstigator */ nullptr,
		/* DamageCauser   */ GetOwner(),
		/* DamageType     */ nullptr
	);
}

void UCargo::OnOwnerDies_DetachIfAttachedToOwner(ASquadUnit* Unit)
{
	if (Unit->GetAttachParentActor() == GetOwner())
	{
		Unit->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
}

void UCargo::MoveOutGarrisonedInfantryOnDisabled()
{
	if (not GetOwner())
	{
		return;
	}

	// Nothing to do if nobody is inside.
	if (M_VacancyState.M_InsideSquads.Num() == 0)
	{
		return;
	}

	// Take a snapshot; exiting will mutate M_InsideSquads.
	const TArray<TObjectPtr<ASquadController>> InsideSquadsSnapshot = M_VacancyState.M_InsideSquads.Array();

	for (const TObjectPtr<ASquadController>& SquadPtr : InsideSquadsSnapshot)
	{
		ASquadController* const SquadController = SquadPtr.Get();
		if (not IsValid(SquadController))
		{
			continue;
		}

		// Use the per-squad cargo component to handle all exit logic.
		UCargoSquad* const CargoSquad = SquadController->FindComponentByClass<UCargoSquad>();
		if (IsValid(CargoSquad))
		{
			// Immediate exit; also swap Exit->Enter in the UI.
			CargoSquad->ExitCargoImmediate(/*bSwapAbilities=*/true);
			continue;
		}

		// Fallback (should be rare): log and clean up our own state to avoid leaks.
		RTSFunctionLibrary::ReportNullErrorComponent(
			SquadController, "CargoSquad", "UCargo::MoveOutGarrisonedInfantryOnDisabled");

		const TArray<ASquadUnit*> Units = SquadController->GetSquadUnitsChecked();
		OnSquadExited(SquadController, Units);
	}
}

void UCargo::StartSeatShuffleTimer()
{
	if (M_SeatShuffleIntervalSeconds <= 0.f)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}

	if (World->GetTimerManager().IsTimerActive(M_SeatShuffleTimerHandle))
	{
		return;
	}

	TWeakObjectPtr<UCargo> WeakThis(this);

	FTimerDelegate SeatShuffleDelegate;
	SeatShuffleDelegate.BindLambda(
		[WeakThis]()
		{
			if (not WeakThis.IsValid())
			{
				return;
			}
			WeakThis->TickSeatReassignment();
		});

	World->GetTimerManager().SetTimer(
		M_SeatShuffleTimerHandle,
		SeatShuffleDelegate,
		M_SeatShuffleIntervalSeconds,
		true);
}

void UCargo::StopSeatShuffleTimer()
{
	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}

	if (World->GetTimerManager().IsTimerActive(M_SeatShuffleTimerHandle))
	{
		World->GetTimerManager().ClearTimer(M_SeatShuffleTimerHandle);
	}
}

void UCargo::OnOwnerDies_ResetState()
{
	M_SocketOccupancy.M_SocketToUnit.Reset();
	M_VacancyState.M_InsideSquads.Empty();
	M_VacancyState.M_CurrentSquads = 0;
}
