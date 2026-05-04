#include "EnemyDirectControlComponent.h"

#include "DrawDebugHelpers.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyFieldConstructionComponent/EnemyFieldConstructionComponent.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyFormationController/EnemyFormationController.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyNavigationAIComponent/EnemyNavigationAIComponent.h"
#include "RTS_Survival/Enemy/StrategicAI/Requests/StrategicAIRequests.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyDirectControlComponent/EnemyDirectControlConstants.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UEnemyDirectControlComponent::UEnemyDirectControlComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnemyDirectControlComponent::InitDirectControlComponent(AEnemyController* EnemyController)
{
	M_EnemyController = EnemyController;
}

void UEnemyDirectControlComponent::BeginPlay()
{
	Super::BeginPlay();
	StartDirectControlTickTimer();
}

void UEnemyDirectControlComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopDirectControlTickTimer();
	Super::EndPlay(EndPlayReason);
}

bool UEnemyDirectControlComponent::EnsureEnemyControllerIsValid() const
{
	if (M_EnemyController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_EnemyController",
		"UEnemyDirectControlComponent::EnsureEnemyControllerIsValid",
		this);
	return false;
}

UEnemyStrategicAIComponent* UEnemyDirectControlComponent::GetValidStrategicAIComponent() const
{
	if(not EnsureEnemyControllerIsValid())
	{
		return nullptr;
	}
	UEnemyStrategicAIComponent* StrategicAIComponent = M_EnemyController->GetEnemyStrategicAIComponent();
	if (IsValid(StrategicAIComponent))
	{
		return StrategicAIComponent;
	}
	RTSFunctionLibrary::ReportError("Enemy direct control component requires a valid strategic AI component.");
	return nullptr;
}

bool UEnemyDirectControlComponent::GetIsValidDirectControlUnitActor(const AActor* UnitActor) const
{
	if (not IsValid(UnitActor))
	{
		RTSFunctionLibrary::ReportError("Enemy direct control component received an invalid unit actor pointer.");
		return false;
	}

	return true;
}

void UEnemyDirectControlComponent::StartDirectControlTickTimer()
{
	if (not EnsureEnemyControllerIsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		M_DirectControlTickTimerHandle,
		this,
		&UEnemyDirectControlComponent::TickDirectControl,
		EnemyDirectControlConstants::DirectControlTickRateSeconds,
		true);
}

void UEnemyDirectControlComponent::StopDirectControlTickTimer()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	World->GetTimerManager().ClearTimer(M_DirectControlTickTimerHandle);
}

bool UEnemyDirectControlComponent::RegisterDirectControlUnit(AActor* UnitActor)
{
	if (not GetIsValidDirectControlUnitActor(UnitActor))
	{
		return false;
	}

	if (TryGetCommandsInterface(UnitActor) == nullptr)
	{
		RTSFunctionLibrary::ReportError(
			"Enemy direct control component can only register units implementing ICommands.");
		return false;
	}

	if (M_RegisteredIdleDirectControlUnits.Contains(UnitActor))
	{
		DebugReportRegisterDeregister("RegisterDirectControlUnit ignored duplicate unit: " + UnitActor->GetName());
		return false;
	}

	M_RegisteredIdleDirectControlUnits.Add(UnitActor);
	DebugReportRegisterDeregister("RegisterDirectControlUnit added unit: " + UnitActor->GetName());
	return true;
}

bool UEnemyDirectControlComponent::DeregisterDirectControlUnit(AActor* UnitActor)
{
	if (not GetIsValidDirectControlUnitActor(UnitActor))
	{
		return false;
	}

	const int32 RemovedCount = M_RegisteredIdleDirectControlUnits.Remove(UnitActor);
	if (RemovedCount <= 0)
	{
		DebugReportRegisterDeregister("DeregisterDirectControlUnit could not find unit: " + UnitActor->GetName());
		return false;
	}

	DebugReportRegisterDeregister("DeregisterDirectControlUnit removed unit: " + UnitActor->GetName());
	return true;
}

void UEnemyDirectControlComponent::ClearDirectControlUnits()
{
	M_RegisteredIdleDirectControlUnits.Empty();
	DebugReportRegisterDeregister("ClearDirectControlUnits removed all direct control units.");
}

TArray<AActor*> UEnemyDirectControlComponent::GetRegisteredDirectControlUnits() const
{
	TArray<AActor*> RegisteredActors;
	RegisteredActors.Reserve(M_RegisteredIdleDirectControlUnits.Num());

	for (const TWeakObjectPtr<AActor>& RegisteredUnit : M_RegisteredIdleDirectControlUnits)
	{
		if (not RegisteredUnit.IsValid())
		{
			continue;
		}

		RegisteredActors.Add(RegisteredUnit.Get());
	}

	return RegisteredActors;
}

void UEnemyDirectControlComponent::RegisterDirectControlUnits(const TArray<AActor*>& UnitActors)
{
	for (AActor* UnitActor : UnitActors)
	{
		RegisterDirectControlUnit(UnitActor);
	}
}

void UEnemyDirectControlComponent::DeregisterDirectControlUnits(const TArray<AActor*>& UnitActors)
{
	for (AActor* UnitActor : UnitActors)
	{
		DeregisterDirectControlUnit(UnitActor);
	}
}

void UEnemyDirectControlComponent::TickDirectControl()
{
	RemoveInvalidRegisteredUnits();
	DebugDrawRegisteredDirectControlUnits();
}

void UEnemyDirectControlComponent::RemoveInvalidRegisteredUnits()
{
	for (int32 UnitIndex = M_RegisteredIdleDirectControlUnits.Num() - 1; UnitIndex >= 0; --UnitIndex)
	{
		const TWeakObjectPtr<AActor> RegisteredUnit = M_RegisteredIdleDirectControlUnits[UnitIndex];
		if (RegisteredUnit.IsValid())
		{
			continue;
		}

		M_RegisteredIdleDirectControlUnits.RemoveAtSwap(UnitIndex);
	}
}

void UEnemyDirectControlComponent::DebugDrawRegisteredDirectControlUnits() const
{
	if constexpr (not DeveloperSettings::Debugging::GEnemyController_DirectControl_Compile_DebugSymbols)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	for (const TWeakObjectPtr<AActor>& RegisteredUnit : M_RegisteredIdleDirectControlUnits)
	{
		if (not RegisteredUnit.IsValid())
		{
			continue;
		}

		const FVector UnitLocation = RegisteredUnit->GetActorLocation();

		if constexpr (
			EnemyDirectControlConstants::Debugging::DrawRegisteredUnitLabel
			&& DeveloperSettings::Debugging::GEnemyController_DirectControl_Compile_DebugSymbols)
		{
			DrawDebugString(
				World,
				UnitLocation + FVector(0.f, 0.f, EnemyDirectControlConstants::DebugTextOffsetZ),
				TEXT("Direct Control"),
				nullptr,
				FColor::Red,
				EnemyDirectControlConstants::DebugDrawDurationSeconds,
				true);
		}

		if constexpr (
			EnemyDirectControlConstants::Debugging::DrawRegisteredUnitSphere
			&& DeveloperSettings::Debugging::GEnemyController_DirectControl_Compile_DebugSymbols)
		{
			DrawDebugSphere(
				World,
				UnitLocation + FVector(0.f, 0.f, EnemyDirectControlConstants::DebugSphereOffsetZ),
				EnemyDirectControlConstants::DebugSphereRadius,
				EnemyDirectControlConstants::DebugSphereSegments,
				FColor::Red,
				false,
				EnemyDirectControlConstants::DebugDrawDurationSeconds,
				0,
				EnemyDirectControlConstants::DebugSphereThickness);
		}
	}
}

void UEnemyDirectControlComponent::DebugReportRegisterDeregister(const FString& Message) const
{
	if constexpr (
		EnemyDirectControlConstants::Debugging::ReportRegisterDeregister
		&& DeveloperSettings::Debugging::GEnemyController_DirectControl_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(Message, FColor::Red);
	}
}

ICommands* UEnemyDirectControlComponent::TryGetCommandsInterface(AActor* UnitActor) const
{
	if (not GetIsValidDirectControlUnitActor(UnitActor))
	{
		return nullptr;
	}

	if (not UnitActor->GetClass()->ImplementsInterface(UCommands::StaticClass()))
	{
		return nullptr;
	}

	return Cast<ICommands>(UnitActor);
}

TArray<TWeakObjectPtr<AActor>> UEnemyDirectControlComponent::GetRetreatGroupUnitsToExclude() const
{
	TArray<TWeakObjectPtr<AActor>> UnitsToExclude;
	for (const FDamagedTanksRetreatGroup& RetreatGroup : M_RetreatCache.M_CachedRetreatGroups)
	{
		AppendRetreatGroupUnitsToExclude(RetreatGroup, UnitsToExclude);
	}

	return UnitsToExclude;
}

void UEnemyDirectControlComponent::AppendRetreatGroupUnitsToExclude(
	const FDamagedTanksRetreatGroup& RetreatGroup,
	TArray<TWeakObjectPtr<AActor>>& OutUnitsToExclude) const
{
	for (const TWeakObjectPtr<AActor>& DamagedTank : RetreatGroup.DamagedTanks)
	{
		if (not DamagedTank.IsValid())
		{
			continue;
		}

		OutUnitsToExclude.AddUnique(DamagedTank);
	}

	for (const FWeakActorLocations& HazmatData : RetreatGroup.HazmatsWithFormationLocations)
	{
		if (not HazmatData.Actor.IsValid())
		{
			continue;
		}

		OutUnitsToExclude.AddUnique(HazmatData.Actor);
	}
}


void UEnemyDirectControlComponent::HandleAsyncRetreatGroupResult(const FResultAlliedTanksToRetreat& RetreatResult)
{
	M_RetreatCache.M_LastRequestID = RetreatResult.RequestID;

	M_RetreatCache.M_CachedRetreatGroups.Add(RetreatResult.Group1);
	M_RetreatCache.M_CachedRetreatGroups.Add(RetreatResult.Group2);
	M_RetreatCache.M_CachedRetreatGroups.Add(RetreatResult.Group3);

	TArray<FDamagedTanksRetreatGroup> NewGroups = { RetreatResult.Group1, RetreatResult.Group2, RetreatResult.Group3 };

	for (FDamagedTanksRetreatGroup& RetreatGroup : NewGroups)
	{
		OnRetreatGroupFound(RetreatGroup);
	}
}

void UEnemyDirectControlComponent::OnRetreatGroupFound(FDamagedTanksRetreatGroup& RetreatGroup)
{
	if (RetreatGroup.DamagedTanks.IsEmpty())
	{
		return;
	}

	RemoveRetreatGroupUnitsFromFormations(RetreatGroup);
	IgnoreHazmatsInFieldConstruction(RetreatGroup);
	IssueRetreatOrdersToDamagedTanks(RetreatGroup);
	if(IsRetreatGroupDamagedTanksOnly(RetreatGroup))
	{
		MarkRetreatGroupTanksOnlyRetreating(RetreatGroup);
		return;
	}
	
	IssueHazmatSupportOrders(RetreatGroup);
	MarkRetreatGroupHazmatsRepairing(RetreatGroup);
}


void UEnemyDirectControlComponent::RemoveRetreatGroupUnitsFromFormations(const FDamagedTanksRetreatGroup& RetreatGroup)
{
	UEnemyFormationController* FormationController = nullptr;
	if (not EnsureFormationControllerIsValid(FormationController))
	{
		return;
	}

	TArray<ATankMaster*> DamagedTanks;
	TArray<ASquadController*> NoSquads;
	for (const TWeakObjectPtr<AActor>& DamagedTank : RetreatGroup.DamagedTanks)
	{
		ATankMaster* TankMaster = Cast<ATankMaster>(DamagedTank.Get());
		if (not IsValid(TankMaster))
		{
			continue;
		}

		DamagedTanks.Add(TankMaster);
	}

	for (const FWeakActorLocations& HazmatData : RetreatGroup.HazmatsWithFormationLocations)
	{
		ATankMaster* TankMaster = Cast<ATankMaster>(HazmatData.Actor.Get());
		if (IsValid(TankMaster))
		{
			DamagedTanks.AddUnique(TankMaster);
		}
	}

	TArray<AActor*> RemovedUnits = FormationController->RemoveUnitsFromAnyFormation(NoSquads, DamagedTanks);
	if constexpr (DeveloperSettings::Debugging::GEnemyController_DirectControl_Compile_DebugSymbols &&
		EnemyDirectControlConstants::Debugging::DebugRemoveDirectControlUnitFromFormations)
	{
		Debug_RetreatUnitsRemovedFromFormations(RemovedUnits);
	}
}

void UEnemyDirectControlComponent::IgnoreHazmatsInFieldConstruction(FDamagedTanksRetreatGroup& RetreatGroup) const
{
	if (not EnsureEnemyControllerIsValid())
	{
		return;
	}

	UEnemyFieldConstructionComponent* EnemyFieldConstructionComponent = M_EnemyController->
		GetEnemyFieldConstructionComponent();
	if (not IsValid(EnemyFieldConstructionComponent))
	{
		return;
	}

	const TArray<AActor*> HazmatUnitsToIgnore = EnemyFieldConstructionComponent->
		GetHazmatUnitsAlreadyAssignedToFieldConstruction(RetreatGroup.HazmatsWithFormationLocations);
	if (HazmatUnitsToIgnore.IsEmpty())
	{
		return;
	}

	RetreatGroup.HazmatsWithFormationLocations.RemoveAll(
		[&HazmatUnitsToIgnore](const FWeakActorLocations& HazmatData)
		{
			return HazmatUnitsToIgnore.Contains(HazmatData.Actor.Get());
		});

	if constexpr (DeveloperSettings::Debugging::GEnemyController_DirectControl_Compile_DebugSymbols &&
		EnemyDirectControlConstants::Debugging::DebugIgnoredFieldconstructionHazmats)
	{
		Debug_IgnoredFieldconstructionHazmats(HazmatUnitsToIgnore);
	}
}

bool UEnemyDirectControlComponent::IsRetreatGroupDamagedTanksOnly(const FDamagedTanksRetreatGroup& RetreatGroup) const
{
	return RetreatGroup.HazmatsWithFormationLocations.IsEmpty();
}

void UEnemyDirectControlComponent::MarkRetreatGroupTanksOnlyRetreating(FDamagedTanksRetreatGroup& RetreatGroup) const
{
	RetreatGroup.M_CurrentRetreatGroupState = EEnemyRetreatGroupState::TanksOnlyRetreating;
}

void UEnemyDirectControlComponent::IssueRetreatOrdersToDamagedTanks(const FDamagedTanksRetreatGroup& RetreatGroup)
{
	for (const TWeakObjectPtr<AActor>& DamagedTank : RetreatGroup.DamagedTanks)
	{
		AActor* DamagedTankActor = DamagedTank.Get();
		ICommands* Commands = TryGetCommandsInterface(DamagedTankActor);
		if (Commands == nullptr)
		{
			continue;
		}

		FVector ProjectedLocation = FVector::ZeroVector;
		if (not TryGetProjectedLocation(DamagedTankActor->GetActorLocation(), ProjectedLocation))
		{
			continue;
		}

		Commands->ReverseUnitToLocation(ProjectedLocation, true);
	}
}

void UEnemyDirectControlComponent::IssueHazmatSupportOrders(const FDamagedTanksRetreatGroup& RetreatGroup)
{
	AActor* FirstDamagedTank = GetFirstValidDamagedTank(RetreatGroup);
	if (not IsValid(FirstDamagedTank))
	{
		return;
	}

	int32 DamagedTankIndex = 0;
	for (const FWeakActorLocations& HazmatData : RetreatGroup.HazmatsWithFormationLocations)
	{
		ICommands* HazmatCommands = TryGetCommandsInterface(HazmatData.Actor.Get());
		if (HazmatCommands == nullptr || HazmatData.Locations.IsEmpty())
		{
			continue;
		}

		FVector ProjectedLocation = FVector::ZeroVector;
		if (not TryGetProjectedLocation(HazmatData.Locations[0], ProjectedLocation))
		{
			continue;
		}

		HazmatCommands->MoveToLocation(ProjectedLocation, true, FRotator::ZeroRotator, false);

		AActor* DamagedTankTarget = FirstDamagedTank;
		if (RetreatGroup.DamagedTanks.IsValidIndex(DamagedTankIndex) && RetreatGroup.DamagedTanks[DamagedTankIndex].
			IsValid())
		{
			DamagedTankTarget = RetreatGroup.DamagedTanks[DamagedTankIndex].Get();
		}

		HazmatCommands->RepairActor(DamagedTankTarget, false);
		DamagedTankIndex = (DamagedTankIndex + 1) % FMath::Max(RetreatGroup.DamagedTanks.Num(), 1);
	}
}

void UEnemyDirectControlComponent::MarkRetreatGroupHazmatsRepairing(FDamagedTanksRetreatGroup& RetreatGroup) const
{
	RetreatGroup.M_CurrentRetreatGroupState = EEnemyRetreatGroupState::HazmatsMoveAndRepairTanks;
}

AActor* UEnemyDirectControlComponent::GetFirstValidDamagedTank(const FDamagedTanksRetreatGroup& RetreatGroup) const
{
	for (const TWeakObjectPtr<AActor>& DamagedTank : RetreatGroup.DamagedTanks)
	{
		if (DamagedTank.IsValid())
		{
			return DamagedTank.Get();
		}
	}

	return nullptr;
}

bool UEnemyDirectControlComponent::TryGetProjectedLocation(const FVector& OriginalLocation,
                                                           FVector& OutProjectedLocation) const
{
	OutProjectedLocation = OriginalLocation;

	if (not EnsureEnemyControllerIsValid())
	{
		return false;
	}

	UEnemyNavigationAIComponent* EnemyNavigationAIComponent = M_EnemyController->GetEnemyNavigationAIComponent();
	if (not IsValid(EnemyNavigationAIComponent))
	{
		return false;
	}

	return EnemyNavigationAIComponent->GetNavigablePoint(OriginalLocation, 1.f, OutProjectedLocation);
}

bool UEnemyDirectControlComponent::EnsureFormationControllerIsValid(
	UEnemyFormationController*& OutFormationController) const
{
	OutFormationController = nullptr;

	if (not EnsureEnemyControllerIsValid())
	{
		return false;
	}

	OutFormationController = M_EnemyController->GetEnemyFormationController();
	if (IsValid(OutFormationController))
	{
		return true;
	}

	RTSFunctionLibrary::ReportError("Enemy direct control component requires a valid enemy formation controller.");
	return false;
}

void UEnemyDirectControlComponent::Debug_RetreatUnitsRemovedFromFormations(const TArray<AActor*>& RemovedUnits) const
{
	for (const auto EachUnit : RemovedUnits)
	{
		if (not IsValid(EachUnit))
		{
			continue;
		}
		using namespace EnemyDirectControlConstants::Debugging;
		const FVector Location = EachUnit->GetActorLocation() + FVector(0, 0, RemoveFromFormationZOffset);

		DrawDebugString(GetWorld(), Location, "REMOVED from formations for retreat", nullptr,
		                RemoveFromFormationColor, RemoveFromLocationDebugTime, false, 1);
	}
}

void UEnemyDirectControlComponent::Debug_IgnoredFieldconstructionHazmats(const TArray<AActor*>& IgnoredHazmatUnits) const
{
	for (const AActor* IgnoredHazmatUnit : IgnoredHazmatUnits)
	{
		if (not IsValid(IgnoredHazmatUnit))
		{
			continue;
		}

		using namespace EnemyDirectControlConstants::Debugging;
		const FVector Location = IgnoredHazmatUnit->GetActorLocation() + FVector(0, 0, RemoveFromFormationZOffset);

		DrawDebugString(GetWorld(), Location, "IGNORED for retreat (field construction)", nullptr,
		                RemoveFromFormationColor, RemoveFromLocationDebugTime, false, 1);
	}
}
