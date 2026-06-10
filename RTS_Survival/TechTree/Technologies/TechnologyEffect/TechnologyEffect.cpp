#include "TechnologyEffect.h"

#include "RTS_Survival/Behaviours/BehaviourComp.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"

TArray<ENomadicSubtype> UTechnologyEffect::GetNomadicsToApplyTo() const
{
	return GetNomadicsToApplyTo_Internal();
}

TArray<ETankSubtype> UTechnologyEffect::GetTanksToApplyTo() const
{
	return GetTanksToApplyTo_Internal();
}

TArray<ESquadSubtype> UTechnologyEffect::GetSquadsToApplyTo() const
{
	return GetSquadsToApplyTo_Internal();
}

TArray<EBuildingExpansionType> UTechnologyEffect::GetBuildingExpansionsToApplyTo() const
{
	return GetBuildingExpansionsToApplyTo_Internal();
}

TArray<EAircraftSubtype> UTechnologyEffect::GetAircraftToApplyTo() const
{
	return GetAircraftToApplyTo_Internal();
}

void UTechnologyEffect::ApplyOnTank(ATankMaster* Tank)
{
	if (not ShouldApplyToActor(Tank))
	{
		return;
	}

	ApplyOnTank_Internal(Tank);
	TryAddBehaviourToActor(Tank);
	OnTechAppliedToActor(Tank);
}

void UTechnologyEffect::ApplyOnNomadic(ANomadicVehicle* Nomadic)
{
	if (not ShouldApplyToActor(Nomadic))
	{
		return;
	}

	ApplyOnNomadic_Internal(Nomadic);
	TryAddBehaviourToActor(Nomadic);
	OnTechAppliedToActor(Nomadic);
}

void UTechnologyEffect::ApplyOnSquad(ASquadController* Squad)
{
	if (not ShouldApplyToActor(Squad))
	{
		return;
	}

	ApplyOnSquad_Internal(Squad);
	TryAddBehaviourToActor(Squad);
	OnTechAppliedToActor(Squad);
}

void UTechnologyEffect::ApplyOnBuildingExpansion(ABuildingExpansion* BuildingExpansion)
{
	if (not ShouldApplyToActor(BuildingExpansion))
	{
		return;
	}

	ApplyOnBuildingExpansion_Internal(BuildingExpansion);
	TryAddBehaviourToActor(BuildingExpansion);
	OnTechAppliedToActor(BuildingExpansion);
}

void UTechnologyEffect::ApplyOnAircraft(AAircraftMaster* Aircraft)
{
	if (not ShouldApplyToActor(Aircraft))
	{
		return;
	}

	ApplyOnAircraft_Internal(Aircraft);
	TryAddBehaviourToActor(Aircraft);
	OnTechAppliedToActor(Aircraft);
}

TArray<ENomadicSubtype> UTechnologyEffect::GetNomadicsToApplyTo_Internal() const
{
	return NomadicsToApplyTo;
}

TArray<ETankSubtype> UTechnologyEffect::GetTanksToApplyTo_Internal() const
{
	return TanksToApplyTo;
}

TArray<ESquadSubtype> UTechnologyEffect::GetSquadsToApplyTo_Internal() const
{
	return SquadsToApplyTo;
}

TArray<EBuildingExpansionType> UTechnologyEffect::GetBuildingExpansionsToApplyTo_Internal() const
{
	return BuildingExpansionsToApplyTo;
}

TArray<EAircraftSubtype> UTechnologyEffect::GetAircraftToApplyTo_Internal() const
{
	return AircraftToApplyTo;
}

void UTechnologyEffect::ApplyOnTank_Internal(ATankMaster* Tank)
{
}

void UTechnologyEffect::ApplyOnNomadic_Internal(ANomadicVehicle* Nomadic)
{
}

void UTechnologyEffect::ApplyOnSquad_Internal(ASquadController* Squad)
{
}

void UTechnologyEffect::ApplyOnBuildingExpansion_Internal(ABuildingExpansion* BuildingExpansion)
{
}

void UTechnologyEffect::ApplyOnAircraft_Internal(AAircraftMaster* Aircraft)
{
}

void UTechnologyEffect::OnTechAppliedToActor(AActor* Actor)
{
	if (not IsValid(Actor))
	{
		return;
	}

	URTSComponent* RTSComponent = Actor->FindComponentByClass<URTSComponent>();
	if (not IsValid(RTSComponent))
	{
		return;
	}

	RTSComponent->AddAppliedTechnology(Technology);
}

bool UTechnologyEffect::ShouldApplyToActor(AActor* Actor) const
{
	if (not IsValid(Actor))
	{
		return false;
	}

	const URTSComponent* RTSComponent = Actor->FindComponentByClass<URTSComponent>();
	if (not IsValid(RTSComponent))
	{
		return false;
	}

	return not RTSComponent->HasTechnologyApplied(Technology);
}

void UTechnologyEffect::TryAddBehaviourToActor(AActor* Actor) const
{
	if (not BehaviourClassToApply || not IsValid(Actor))
	{
		return;
	}

	UBehaviourComp* BehaviourComponent = Actor->FindComponentByClass<UBehaviourComp>();
	if (not IsValid(BehaviourComponent))
	{
		return;
	}

	BehaviourComponent->AddBehaviour(BehaviourClassToApply);
}
