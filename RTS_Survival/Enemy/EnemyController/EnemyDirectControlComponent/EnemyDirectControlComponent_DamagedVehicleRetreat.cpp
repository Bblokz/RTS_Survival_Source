// Damaged vehicle retreat flow of UEnemyDirectControlComponent: badly damaged direct control vehicles reverse once
// to a base (preferring bases not in combat) while one hazmat squad is pulled off its orders to repair them.

#include "EnemyDirectControlComponent.h"

#include "DrawDebugHelpers.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Behaviours/BehaviourComp.h"
#include "RTS_Survival/Behaviours/Derived/BehaviourVehicleRetreat/VehicleRetreatBehaviour.h"
#include "RTS_Survival/Enemy/EnemyAISettings/EnemyAISettings.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyDirectControlComponent/EnemyDirectControlConstants.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyFieldConstructionComponent/EnemyFieldConstructionComponent.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyFormationController/EnemyFormationController.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyRetreatController/EnemyRetreatController.h"
#include "RTS_Survival/Enemy/StrategicAI/BlackboardIdleUnitEntry.h"
#include "RTS_Survival/Enemy/StrategicAI/Component/EnemyStrategicAIComponent.h"
#include "RTS_Survival/Enemy/StrategicAI/Requests/StrategicAIRequests.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace
{
	bool GetIsVehicleAlive(const ATankMaster* Vehicle)
	{
		if (not IsValid(Vehicle))
		{
			return false;
		}

		const UHealthComponent* HealthComponent = Vehicle->GetHealthComponent();
		return IsValid(HealthComponent) && HealthComponent->GetCurrentHealth() > 0.f;
	}

	bool GetIsHazmatEngineerSquad(const AActor* UnitActor)
	{
		const URTSComponent* RTSComponent = UnitActor->FindComponentByClass<URTSComponent>();
		if (not IsValid(RTSComponent) || RTSComponent->GetUnitType() != EAllUnitType::UNType_Squad)
		{
			return false;
		}

		return static_cast<ESquadSubtype>(RTSComponent->GetUnitSubType()) == ESquadSubtype::Squad_Rus_HazmatEngineers;
	}

	bool GetIsRetreatMovementCommand(const EAbilityID AbilityID)
	{
		return AbilityID == EAbilityID::IdReverseMove || AbilityID == EAbilityID::IdMove;
	}
}

void UEnemyDirectControlComponent::StartDamagedVehicleCheckTimer()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		M_DamagedVehicleState.CheckTimerHandle,
		this,
		&UEnemyDirectControlComponent::TickDamagedVehicleRetreats,
		EnemyAISettings::DamagedVehicleRetreat::CheckInterval,
		true);
}

void UEnemyDirectControlComponent::StopDamagedVehicleCheckTimer()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	World->GetTimerManager().ClearTimer(M_DamagedVehicleState.CheckTimerHandle);
}

void UEnemyDirectControlComponent::TickDamagedVehicleRetreats()
{
	TRACE_CPUPROFILER_EVENT_SCOPE("DirectControl_DamagedVehicleRetreats");
	PruneDamagedVehicleRoster();

	TArray<FDamagedVehicleRetreatEntry>& ActiveRetreats = M_DamagedVehicleState.ActiveRetreats;
	for (int32 EntryIndex = ActiveRetreats.Num() - 1; EntryIndex >= 0; --EntryIndex)
	{
		if (not UpdateDamagedVehicleRetreat(ActiveRetreats[EntryIndex]))
		{
			continue;
		}

		// Remove before releasing: re-registering the units is refused while their entry still exists.
		const FDamagedVehicleRetreatEntry FinishedEntry = ActiveRetreats[EntryIndex];
		ActiveRetreats.RemoveAtSwap(EntryIndex);
		FinishDamagedVehicleRetreat(FinishedEntry, GetIsVehicleAlive(FinishedEntry.Vehicle.Get()));
	}

	StartRetreatsForDamagedVehicles();
}

void UEnemyDirectControlComponent::AddUnitToDamagedVehicleRoster(AActor* UnitActor)
{
	if (ATankMaster* Vehicle = Cast<ATankMaster>(UnitActor))
	{
		M_DamagedVehicleState.DirectControlVehicles.AddUnique(Vehicle);
		return;
	}

	ASquadController* Squad = Cast<ASquadController>(UnitActor);
	if (IsValid(Squad) && GetIsHazmatEngineerSquad(Squad))
	{
		M_DamagedVehicleState.DirectControlHazmats.AddUnique(Squad);
	}
}

void UEnemyDirectControlComponent::RemoveUnitFromDamagedVehicleRoster(const AActor* UnitActor)
{
	M_DamagedVehicleState.DirectControlVehicles.RemoveAll(
		[UnitActor](const TWeakObjectPtr<ATankMaster>& Vehicle)
		{
			return Vehicle.Get() == UnitActor;
		});
	M_DamagedVehicleState.DirectControlHazmats.RemoveAll(
		[UnitActor](const TWeakObjectPtr<ASquadController>& Hazmat)
		{
			return Hazmat.Get() == UnitActor;
		});
}

void UEnemyDirectControlComponent::PruneDamagedVehicleRoster()
{
	M_DamagedVehicleState.DirectControlVehicles.RemoveAll(
		[](const TWeakObjectPtr<ATankMaster>& Vehicle)
		{
			return not GetIsVehicleAlive(Vehicle.Get());
		});
	M_DamagedVehicleState.DirectControlHazmats.RemoveAll(
		[](const TWeakObjectPtr<ASquadController>& Hazmat)
		{
			return not Hazmat.IsValid();
		});
}

bool UEnemyDirectControlComponent::GetIsUnitInDamagedVehicleRetreat(const AActor* UnitActor) const
{
	if (not IsValid(UnitActor))
	{
		return false;
	}

	for (const FDamagedVehicleRetreatEntry& Entry : M_DamagedVehicleState.ActiveRetreats)
	{
		if (Entry.Vehicle.Get() == UnitActor || Entry.RepairHazmat.Get() == UnitActor)
		{
			return true;
		}
	}

	return false;
}

void UEnemyDirectControlComponent::AppendDamagedVehicleRetreatUnitsToExclude(
	TArray<TWeakObjectPtr<AActor>>& OutUnitsToExclude) const
{
	for (const FDamagedVehicleRetreatEntry& Entry : M_DamagedVehicleState.ActiveRetreats)
	{
		if (Entry.Vehicle.IsValid())
		{
			OutUnitsToExclude.AddUnique(Entry.Vehicle.Get());
		}

		if (Entry.RepairHazmat.IsValid())
		{
			OutUnitsToExclude.AddUnique(Entry.RepairHazmat.Get());
		}
	}
}

void UEnemyDirectControlComponent::StartRetreatsForDamagedVehicles()
{
	// Collect first: starting a retreat edits formations, which can re-register units into the roster we iterate.
	TArray<ATankMaster*> VehiclesToRetreat;
	for (const TWeakObjectPtr<ATankMaster>& Vehicle : M_DamagedVehicleState.DirectControlVehicles)
	{
		if (GetShouldVehicleStartRetreat(Vehicle.Get()))
		{
			VehiclesToRetreat.Add(Vehicle.Get());
		}
	}

	for (ATankMaster* Vehicle : VehiclesToRetreat)
	{
		(void)TryStartDamagedVehicleRetreat(Vehicle);
	}
}

bool UEnemyDirectControlComponent::GetShouldVehicleStartRetreat(const ATankMaster* Vehicle) const
{
	if (not GetIsVehicleAlive(Vehicle))
	{
		return false;
	}

	// Cheapest check first; nearly every vehicle exits here.
	const float HealthRatio = Vehicle->GetHealthComponent()->GetHealthPercentage();
	if (HealthRatio > EnemyAISettings::DamagedVehicleRetreat::HealthRatioThresholdToRetreat)
	{
		return false;
	}

	return not GetIsUnitInDamagedVehicleRetreat(Vehicle) && not GetIsUnitInOtherRetreatSystem(Vehicle);
}

bool UEnemyDirectControlComponent::GetIsUnitInOtherRetreatSystem(const AActor* UnitActor) const
{
	for (const FDamagedTanksRetreatGroup& RetreatGroup : M_RetreatCache.M_CachedRetreatGroups)
	{
		if (RetreatGroup.DamagedTanks.Contains(UnitActor))
		{
			return true;
		}

		const bool bIsGroupHazmat = RetreatGroup.HazmatsWithFormationLocations.ContainsByPredicate(
			[UnitActor](const FWeakActorLocations& HazmatData)
			{
				return HazmatData.Actor.Get() == UnitActor;
			});
		if (bIsGroupHazmat)
		{
			return true;
		}
	}

	if (not EnsureEnemyControllerIsValid())
	{
		return false;
	}

	const UEnemyRetreatController* RetreatController = M_EnemyController->GetEnemyRetreatController();
	return IsValid(RetreatController) && RetreatController->GetIsUnitRetreating(UnitActor);
}

bool UEnemyDirectControlComponent::TryStartDamagedVehicleRetreat(ATankMaster* Vehicle)
{
	FVector RetreatLocation = FVector::ZeroVector;
	if (not TryGetDamagedVehicleRetreatLocation(Vehicle->GetActorLocation(), RetreatLocation))
	{
		// No known base yet; the vehicle keeps its orders and is checked again next tick.
		return false;
	}

	// The only retreat order this vehicle gets; arrival is detected by polling, never by re-ordering.
	const bool bIsRetreatOrdered = IssueRetreatOrderToVehicle(Vehicle, RetreatLocation);

	FDamagedVehicleRetreatEntry NewEntry;
	NewEntry.Vehicle = Vehicle;
	NewEntry.RetreatLocation = RetreatLocation;
	NewEntry.Phase = bIsRetreatOrdered
		                 ? EDamagedVehicleRetreatPhase::Reversing
		                 : EDamagedVehicleRetreatPhase::AwaitingRepair;
	// Add the entry before pulling the vehicle from its formation so any re-registration it triggers is refused.
	const int32 EntryIndex = M_DamagedVehicleState.ActiveRetreats.Add(NewEntry);
	RemoveUnitFromStrategicAIOrders(Vehicle);
	if (bIsRetreatOrdered)
	{
		SetVehicleRetreatBehaviourActive(Vehicle, true);
	}

	TryAssignRepairHazmat(M_DamagedVehicleState.ActiveRetreats[EntryIndex]);
	Debug_DamagedVehicleRetreat(Vehicle, bIsRetreatOrdered
		                                     ? TEXT("Damaged: retreating to base")
		                                     : TEXT("Damaged: could not order retreat, waiting for repair"));
	return true;
}

bool UEnemyDirectControlComponent::TryGetDamagedVehicleRetreatLocation(
	const FVector& VehicleLocation,
	FVector& OutRetreatLocation) const
{
	const UEnemyStrategicAIComponent* StrategicAIComponent = GetValidStrategicAIComponent();
	if (not IsValid(StrategicAIComponent))
	{
		return false;
	}

	const FStrategicAIBlackboard& Blackboard = StrategicAIComponent->GetStrategicAIBlackboard();
	const FVector* ClosestCalmBase = nullptr;
	const FVector* ClosestAnyBase = nullptr;
	float ClosestCalmDistanceSquared = TNumericLimits<float>::Max();
	float ClosestAnyDistanceSquared = TNumericLimits<float>::Max();
	for (const FEnemyBasePointCoreBuildings& BasePoint : Blackboard.EnemyBasePoints)
	{
		if (BasePoint.BaseLocation.IsNearlyZero())
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared2D(VehicleLocation, BasePoint.BaseLocation);
		if (DistanceSquared < ClosestAnyDistanceSquared)
		{
			ClosestAnyDistanceSquared = DistanceSquared;
			ClosestAnyBase = &BasePoint.BaseLocation;
		}

		if (DistanceSquared < ClosestCalmDistanceSquared && not GetIsBaseInCombat(BasePoint.BaseLocation, Blackboard))
		{
			ClosestCalmDistanceSquared = DistanceSquared;
			ClosestCalmBase = &BasePoint.BaseLocation;
		}
	}

	const FVector* ChosenBase = ClosestCalmBase != nullptr ? ClosestCalmBase : ClosestAnyBase;
	if (ChosenBase == nullptr)
	{
		return false;
	}

	// Never stand off farther than the vehicle already is, or it would reverse away from the base.
	const FVector BaseToVehicle = (VehicleLocation - *ChosenBase).GetSafeNormal2D();
	const float StandOffDistance = FMath::Min(
		EnemyAISettings::DamagedVehicleRetreat::RetreatStandOffDistanceFromBase,
		FVector::Dist2D(VehicleLocation, *ChosenBase));
	const FVector StandOffLocation = *ChosenBase + BaseToVehicle * StandOffDistance;
	if (TryGetProjectedLocation(StandOffLocation, OutRetreatLocation))
	{
		return true;
	}

	return TryGetProjectedLocation(*ChosenBase, OutRetreatLocation);
}

bool UEnemyDirectControlComponent::GetIsBaseInCombat(
	const FVector& BaseLocation,
	const FStrategicAIBlackboard& Blackboard) const
{
	constexpr float MatchToleranceSquared = FMath::Square(
		EnemyAISettings::DamagedVehicleRetreat::BaseCombatMatchTolerance);
	for (const FPlayerAttackLocationEvaluation& BaseInCombat : Blackboard.CurrentEnemyBasesInCombat.LocationsUnderAttack)
	{
		if (FVector::DistSquared2D(BaseInCombat.LocationUnderAttack, BaseLocation) <= MatchToleranceSquared)
		{
			return true;
		}
	}

	return false;
}

bool UEnemyDirectControlComponent::IssueRetreatOrderToVehicle(ATankMaster* Vehicle, const FVector& RetreatLocation) const
{
	if (Vehicle->ReverseUnitToLocation(RetreatLocation, true) == ECommandQueueError::NoError)
	{
		return true;
	}

	// Vehicles without reverse on their command card still drive back to the base.
	return Vehicle->MoveToLocation(RetreatLocation, true, FRotator::ZeroRotator) == ECommandQueueError::NoError;
}

void UEnemyDirectControlComponent::RemoveUnitFromStrategicAIOrders(AActor* UnitActor)
{
	if (UEnemyStrategicAIComponent* StrategicAIComponent = GetValidStrategicAIComponent())
	{
		FBlackboardIdleUnitEntry::RemoveByUnitActor(
			StrategicAIComponent->GetEditableStrategicAIBlackboard().IdleDirectControlUnits,
			UnitActor);
	}

	UEnemyFormationController* FormationController = nullptr;
	if (not EnsureFormationControllerIsValid(FormationController))
	{
		return;
	}

	TArray<ASquadController*> Squads;
	TArray<ATankMaster*> Vehicles;
	if (ASquadController* Squad = Cast<ASquadController>(UnitActor))
	{
		Squads.Add(Squad);
	}
	else if (ATankMaster* Vehicle = Cast<ATankMaster>(UnitActor))
	{
		Vehicles.Add(Vehicle);
	}

	(void)FormationController->RemoveUnitsFromAnyFormation(Squads, Vehicles);
}

void UEnemyDirectControlComponent::SetVehicleRetreatBehaviourActive(ATankMaster* Vehicle, const bool bActive) const
{
	if (not IsValid(Vehicle))
	{
		return;
	}

	UBehaviourComp* BehaviourComp = Vehicle->FindComponentByClass<UBehaviourComp>();
	if (not IsValid(BehaviourComp))
	{
		return;
	}

	if (bActive)
	{
		BehaviourComp->AddBehaviour(UVehicleRetreatBehaviour::StaticClass());
		return;
	}

	BehaviourComp->RemoveBehaviour(UVehicleRetreatBehaviour::StaticClass());
}

bool UEnemyDirectControlComponent::UpdateDamagedVehicleRetreat(FDamagedVehicleRetreatEntry& Entry)
{
	const ATankMaster* Vehicle = Entry.Vehicle.Get();
	if (not GetIsVehicleAlive(Vehicle))
	{
		return true;
	}

	if (not Vehicle->GetHealthComponent()->GetHasDamageToRepair())
	{
		return true;
	}

	UpdateRetreatPhase(Entry);
	UpdateRepairHazmat(Entry);
	return false;
}

void UEnemyDirectControlComponent::UpdateRetreatPhase(FDamagedVehicleRetreatEntry& Entry) const
{
	if (Entry.Phase != EDamagedVehicleRetreatPhase::Reversing)
	{
		return;
	}

	ATankMaster* Vehicle = Entry.Vehicle.Get();
	constexpr float ReachedToleranceSquared = FMath::Square(
		EnemyAISettings::DamagedVehicleRetreat::RetreatLocationReachedTolerance);
	const bool bHasReachedRetreatLocation =
		FVector::DistSquared2D(Vehicle->GetActorLocation(), Entry.RetreatLocation) <= ReachedToleranceSquared;
	// A retreat that stopped early (blocked path, interrupted) counts as arrived; it is never re-ordered.
	if (not bHasReachedRetreatLocation && GetIsRetreatMovementCommand(Vehicle->GetActiveCommandID()))
	{
		return;
	}

	Entry.Phase = EDamagedVehicleRetreatPhase::AwaitingRepair;
	SetVehicleRetreatBehaviourActive(Vehicle, false);
	Debug_DamagedVehicleRetreat(Vehicle, TEXT("Retreat finished, waiting for repair"));
}

void UEnemyDirectControlComponent::UpdateRepairHazmat(FDamagedVehicleRetreatEntry& Entry)
{
	ASquadController* Hazmat = Entry.RepairHazmat.Get();
	if (not IsValid(Hazmat))
	{
		Entry.RepairHazmat.Reset();
		TryAssignRepairHazmat(Entry);
		return;
	}

	// Hazmats chase a reversing vehicle; only re-order once the vehicle stands still so orders are not spammed.
	if (GetIsHazmatRepairing(Hazmat) || Entry.Phase == EDamagedVehicleRetreatPhase::Reversing)
	{
		return;
	}

	if (Entry.RepairOrdersToHazmat >= EnemyAISettings::DamagedVehicleRetreat::MaxRepairOrdersPerHazmat)
	{
		DropRepairHazmat(Entry);
		TryAssignRepairHazmat(Entry);
		return;
	}

	if (Hazmat->RepairActor(Entry.Vehicle.Get(), true) == ECommandQueueError::NoError)
	{
		++Entry.RepairOrdersToHazmat;
		return;
	}

	DropRepairHazmat(Entry);
}

void UEnemyDirectControlComponent::TryAssignRepairHazmat(FDamagedVehicleRetreatEntry& Entry)
{
	ATankMaster* Vehicle = Entry.Vehicle.Get();
	if (Entry.RepairHazmat.IsValid() || not IsValid(Vehicle))
	{
		return;
	}

	ASquadController* Hazmat = FindClosestAvailableRepairHazmat(
		Vehicle->GetActorLocation(),
		Entry.FailedRepairHazmat.Get());
	if (not IsValid(Hazmat))
	{
		return;
	}

	if (Hazmat->RepairActor(Vehicle, true) != ECommandQueueError::NoError)
	{
		// Rejected before the hazmat's queue was touched; try another hazmat next tick.
		Entry.FailedRepairHazmat = Hazmat;
		return;
	}

	// Set the hazmat on the entry before pulling it from its formation so re-registration is refused.
	Entry.RepairHazmat = Hazmat;
	Entry.RepairOrdersToHazmat = 1;
	RemoveUnitFromStrategicAIOrders(Hazmat);
	Debug_DamagedVehicleRetreat(Hazmat, TEXT("Assigned to repair retreating vehicle"));
}

void UEnemyDirectControlComponent::DropRepairHazmat(FDamagedVehicleRetreatEntry& Entry)
{
	ASquadController* Hazmat = Entry.RepairHazmat.Get();
	Entry.FailedRepairHazmat = Hazmat;
	Entry.RepairHazmat.Reset();
	Entry.RepairOrdersToHazmat = 0;
	ReleaseUnitToDirectControl(Hazmat);
}

ASquadController* UEnemyDirectControlComponent::FindClosestAvailableRepairHazmat(
	const FVector& VehicleLocation,
	const ASquadController* ExcludedHazmat) const
{
	ASquadController* ClosestHazmat = nullptr;
	float ClosestDistanceSquared = TNumericLimits<float>::Max();
	for (const TWeakObjectPtr<ASquadController>& HazmatPtr : M_DamagedVehicleState.DirectControlHazmats)
	{
		ASquadController* Hazmat = HazmatPtr.Get();
		if (not GetIsHazmatAvailableForRepair(Hazmat, ExcludedHazmat))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared2D(Hazmat->GetOwnerLocation(), VehicleLocation);
		if (DistanceSquared < ClosestDistanceSquared)
		{
			ClosestDistanceSquared = DistanceSquared;
			ClosestHazmat = Hazmat;
		}
	}

	return ClosestHazmat;
}

bool UEnemyDirectControlComponent::GetIsHazmatAvailableForRepair(
	ASquadController* Hazmat,
	const ASquadController* ExcludedHazmat) const
{
	if (not IsValid(Hazmat) || Hazmat == ExcludedHazmat || not Hazmat->GetIsSquadFullyLoadedAndInitialized())
	{
		return false;
	}

	if (Hazmat->GetHasCommandInQueue(EAbilityID::IdFieldConstruction))
	{
		return false;
	}

	if (GetIsUnitInDamagedVehicleRetreat(Hazmat) || GetIsUnitInOtherRetreatSystem(Hazmat))
	{
		return false;
	}

	if (not EnsureEnemyControllerIsValid())
	{
		return false;
	}

	const UEnemyFieldConstructionComponent* FieldConstructionComponent =
		M_EnemyController->GetEnemyFieldConstructionComponent();
	return not IsValid(FieldConstructionComponent)
		|| not FieldConstructionComponent->GetIsSquadAssignedToFieldConstruction(Hazmat);
}

bool UEnemyDirectControlComponent::GetIsHazmatRepairing(ASquadController* Hazmat) const
{
	return Hazmat->GetActiveCommandID() == EAbilityID::IdRepair || Hazmat->GetHasCommandInQueue(EAbilityID::IdRepair);
}

void UEnemyDirectControlComponent::FinishDamagedVehicleRetreat(
	const FDamagedVehicleRetreatEntry& Entry,
	const bool bVehicleRepaired)
{
	ATankMaster* Vehicle = Entry.Vehicle.Get();
	if (Entry.Phase == EDamagedVehicleRetreatPhase::Reversing)
	{
		SetVehicleRetreatBehaviourActive(Vehicle, false);
	}

	ReleaseUnitToDirectControl(Entry.RepairHazmat.Get());
	if (bVehicleRepaired)
	{
		ReleaseUnitToDirectControl(Vehicle);
	}

	Debug_DamagedVehicleRetreat(Vehicle, bVehicleRepaired
		                                     ? TEXT("Repaired: back under direct control")
		                                     : TEXT("Destroyed: hazmat released"));
}

void UEnemyDirectControlComponent::ReleaseUnitToDirectControl(AActor* UnitActor)
{
	if (not IsValid(UnitActor))
	{
		return;
	}

	(void)RegisterDirectControlUnit(UnitActor);
}

void UEnemyDirectControlComponent::Debug_DamagedVehicleRetreat(const AActor* UnitActor, const FString& Message) const
{
	if constexpr (DeveloperSettings::Debugging::GEnemyController_DirectControl_Compile_DebugSymbols &&
		EnemyDirectControlConstants::Debugging::DebugDamagedVehicleRetreat)
	{
		if (not IsValid(UnitActor))
		{
			return;
		}

		using namespace EnemyDirectControlConstants::Debugging;
		const FVector Location = UnitActor->GetActorLocation() + FVector(0, 0, RemoveFromFormationZOffset);
		DrawDebugString(GetWorld(), Location, Message, nullptr, DamagedVehicleRetreatColor,
		                RemoveFromLocationDebugTime, false, 1);
	}
}
