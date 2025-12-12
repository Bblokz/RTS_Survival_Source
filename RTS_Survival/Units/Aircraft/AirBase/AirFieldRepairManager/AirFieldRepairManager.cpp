// Copyright ...

#include "AirfieldRepairManager.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Units/Aircraft/AirBase/AircraftOwnerComp/AircraftOwnerComp.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

// We only need the socket record details in this .cpp

// Developer settings follow the same usage pattern as in URepairComponent
// (if your project requires an include for DeveloperSettings, add it here).
// Using: DeveloperSettings::GameBalance::Repair::RepairTickInterval
//        DeveloperSettings::GameBalance::Repair::HpRepairedPerTick

void FAirfieldRepairManager::InitAircraftRepair(const float RepairStrengthMlt, UAircraftOwnerComp* Owner)
{
	M_RepairStrengthMlt = FMath::Max(RepairStrengthMlt, 0.0f);
	M_Owner = Owner;

	// Guard for bad init (rule 0.5)
	(void)GetIsValidOwner();
}

void FAirfieldRepairManager::OnAnyAircraftLanded()
{
	if (bM_IsTicking)
	{
		return;
	}
	StartRepairTick();
}

void FAirfieldRepairManager::OnAnyAircraftVTO()
{
	if (not bM_IsTicking)
	{
		return;
	}
	StopRepairTick();
}

void FAirfieldRepairManager::OnAirfieldDies()
{
	if (not bM_IsTicking)
	{
		return;
	}
	StopRepairTick();
}

void FAirfieldRepairManager::OnAircraftDies()
{
	if (not bM_IsTicking)
	{
		return;
	}
	if (not HasAnyLanded())
	{
		StopRepairTick();
	}
}

// ---------- Private ----------

void FAirfieldRepairManager::StartRepairTick()
{
	if (bM_IsTicking)
	{
		return;
	}
	if (not GetIsValidOwner())
	{
		return;
	}
	if (not HasAnyLanded())
	{
		// Defensive: do not start if there is nothing to repair.
		return;
	}

	UWorld* World = M_Owner->GetWorld();
	if (not World)
	{
		RTSFunctionLibrary::ReportError(TEXT("AirfieldRepairManager: missing world on StartRepairTick"));
		return;
	}

	const float Rate = DeveloperSettings::GameBalance::Repair::RepairTickInterval;

	// Bind safely: timer will stop with Owner destruction.
	TWeakObjectPtr<UAircraftOwnerComp> WeakOwner = M_Owner;
	World->GetTimerManager().SetTimer(
		M_RepairTickHandle,
		FTimerDelegate::CreateLambda([WeakOwner]()
		{
			if (not WeakOwner.IsValid())
			{
				return;
			}
			// Access the manager through the owner; struct lives inside it.
			WeakOwner->M_AirfieldRepairManager.OnRepairTick();
		}),
		FMath::Max(Rate, 0.01f),
		true);

	bM_IsTicking = true;
}

void FAirfieldRepairManager::StopRepairTick()
{
	if (not bM_IsTicking)
	{
		return;
	}
	if (not GetIsValidOwner())
	{
		bM_IsTicking = false;
		return;
	}
	if (UWorld* World = M_Owner->GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_RepairTickHandle);
	}
	bM_IsTicking = false;
}

void FAirfieldRepairManager::OnRepairTick()
{
	if (not GetIsValidOwner())
	{
		StopRepairTick();
		return;
	}
	FRTSAircraftHelpers::AircraftDebug("Repair Tick!", FColor::Magenta);

	if (not HasAnyLanded())
	{
		RTSFunctionLibrary::ReportError(
			TEXT("AirfieldRepairManager ticked but found no landed aircraft. Expected StopRepairTick beforehand."));
		StopRepairTick();
		return;
	}

	HealAllLandedOnce();
}

bool FAirfieldRepairManager::GetIsValidOwner() const
{
	if (M_Owner.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		nullptr, TEXT("M_Owner"), TEXT("FAirfieldRepairManager::GetIsValidOwner"), nullptr);
	return false;
}

bool FAirfieldRepairManager::HasAnyLanded() const
{
	if (not GetIsValidOwner())
	{
		return false;
	}

	// Access private sockets: we are declared as friend inside UAircraftOwnerComp.
	for (const FSocketRecord& Rec : M_Owner->M_Sockets)
	{
		if (Rec.bM_IsInside && Rec.AssignedAircraft.IsValid())
		{
			return true;
		}
	}
	return false;
}

void FAirfieldRepairManager::HealAllLandedOnce() const
{
	// Per aircraft repair, consistent with URepairComponent logic.
	const float RepairAmount = DeveloperSettings::GameBalance::Repair::HpRepairedPerTick * M_RepairStrengthMlt;

	for (const FSocketRecord& Rec : M_Owner->M_Sockets)
	{
		if (not Rec.bM_IsInside || not Rec.AssignedAircraft.IsValid())
		{
			continue;
		}

		AAircraftMaster* Aircraft = Rec.AssignedAircraft.Get();
		if (not RTSFunctionLibrary::RTSIsValid(Aircraft))
		{
			continue;
		}

		// Find health and heal
		if (UActorComponent* HealthCompRaw = Aircraft->FindComponentByClass(UActorComponent::StaticClass()))
		{
			// Prefer a direct typed find if available in your codebase:
			auto* HealthComponent = Aircraft->FindComponentByClass<class UHealthComponent>();
			if (not IsValid(HealthComponent))
			{
				RTSFunctionLibrary::ReportError(
					TEXT("AirfieldRepairManager: Assigned aircraft has no UHealthComponent"));
				continue;
			}
			if (HealthComponent->GetHasDamageToRepair())
			{
				(void)HealthComponent->Heal(RepairAmount);
			}
		}
		else
		{
			RTSFunctionLibrary::ReportError(
				TEXT("AirfieldRepairManager: Failed to find Health component on aircraft."));
		}
	}
}
