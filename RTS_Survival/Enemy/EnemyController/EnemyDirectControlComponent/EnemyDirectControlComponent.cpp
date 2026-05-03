#include "EnemyDirectControlComponent.h"

#include "DrawDebugHelpers.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyFormationController/EnemyFormationController.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyNavigationAIComponent/EnemyNavigationAIComponent.h"
#include "RTS_Survival/Enemy/StrategicAI/Requests/StrategicAIRequests.h"
#include "RTS_Survival/Units/Squads/SquadController.h"
#include "RTS_Survival/Units/Tanks/AITankMaster.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyDirectControlComponent/EnemyDirectControlConstants.h"
#include "RTS_Survival/Interfaces/Commands.h"
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

	if (M_RegisteredDirectControlUnits.Contains(UnitActor))
	{
		DebugReportRegisterDeregister("RegisterDirectControlUnit ignored duplicate unit: " + UnitActor->GetName());
		return false;
	}

	M_RegisteredDirectControlUnits.Add(UnitActor);
	DebugReportRegisterDeregister("RegisterDirectControlUnit added unit: " + UnitActor->GetName());
	return true;
}

bool UEnemyDirectControlComponent::DeregisterDirectControlUnit(AActor* UnitActor)
{
	if (not GetIsValidDirectControlUnitActor(UnitActor))
	{
		return false;
	}

	const int32 RemovedCount = M_RegisteredDirectControlUnits.Remove(UnitActor);
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
	M_RegisteredDirectControlUnits.Empty();
	DebugReportRegisterDeregister("ClearDirectControlUnits removed all direct control units.");
}

TArray<AActor*> UEnemyDirectControlComponent::GetRegisteredDirectControlUnits() const
{
	TArray<AActor*> RegisteredActors;
	RegisteredActors.Reserve(M_RegisteredDirectControlUnits.Num());

	for (const TWeakObjectPtr<AActor>& RegisteredUnit : M_RegisteredDirectControlUnits)
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
	for (int32 UnitIndex = M_RegisteredDirectControlUnits.Num() - 1; UnitIndex >= 0; --UnitIndex)
	{
		const TWeakObjectPtr<AActor> RegisteredUnit = M_RegisteredDirectControlUnits[UnitIndex];
		if (RegisteredUnit.IsValid())
		{
			continue;
		}

		M_RegisteredDirectControlUnits.RemoveAtSwap(UnitIndex);
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

	for (const TWeakObjectPtr<AActor>& RegisteredUnit : M_RegisteredDirectControlUnits)
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


void UEnemyDirectControlComponent::HandleRetreatGroupResult(const FResultAlliedTanksToRetreat& RetreatResult)
{
	M_RetreatCache.M_LastRequestID = RetreatResult.RequestID;
	M_RetreatCache.M_CachedRetreatGroups.Empty();

	M_RetreatCache.M_CachedRetreatGroups.Add(RetreatResult.Group1);
	M_RetreatCache.M_CachedRetreatGroups.Add(RetreatResult.Group2);
	M_RetreatCache.M_CachedRetreatGroups.Add(RetreatResult.Group3);

	for (const FDamagedTanksRetreatGroup& RetreatGroup : M_RetreatCache.M_CachedRetreatGroups)
	{
		HandleRetreatGroup(RetreatGroup);
	}
}

void UEnemyDirectControlComponent::HandleRetreatGroup(const FDamagedTanksRetreatGroup& RetreatGroup)
{
	if (RetreatGroup.DamagedTanks.IsEmpty())
	{
		return;
	}

	RegisterRetreatGroupUnits(RetreatGroup);
	RemoveRetreatGroupUnitsFromFormations(RetreatGroup);
	IssueRetreatOrdersToDamagedTanks(RetreatGroup);
	IssueHazmatSupportOrders(RetreatGroup);
}

void UEnemyDirectControlComponent::RegisterRetreatGroupUnits(const FDamagedTanksRetreatGroup& RetreatGroup)
{
	for (const TWeakObjectPtr<AActor>& DamagedTank : RetreatGroup.DamagedTanks)
	{
		if (not DamagedTank.IsValid())
		{
			continue;
		}

		RegisterDirectControlUnit(DamagedTank.Get());
	}

	for (const FWeakActorLocations& HazmatData : RetreatGroup.HazmatsWithFormationLocations)
	{
		if (not HazmatData.Actor.IsValid())
		{
			continue;
		}

		RegisterDirectControlUnit(HazmatData.Actor.Get());
	}
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

	FormationController->RemoveUnitsFromAnyFormation(NoSquads, DamagedTanks);
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
		if (RetreatGroup.DamagedTanks.IsValidIndex(DamagedTankIndex) && RetreatGroup.DamagedTanks[DamagedTankIndex].IsValid())
		{
			DamagedTankTarget = RetreatGroup.DamagedTanks[DamagedTankIndex].Get();
		}

		HazmatCommands->RepairActor(DamagedTankTarget, false);
		DamagedTankIndex = (DamagedTankIndex + 1) % FMath::Max(RetreatGroup.DamagedTanks.Num(), 1);
	}
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

bool UEnemyDirectControlComponent::TryGetProjectedLocation(const FVector& OriginalLocation, FVector& OutProjectedLocation) const
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

bool UEnemyDirectControlComponent::EnsureFormationControllerIsValid(UEnemyFormationController*& OutFormationController) const
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
