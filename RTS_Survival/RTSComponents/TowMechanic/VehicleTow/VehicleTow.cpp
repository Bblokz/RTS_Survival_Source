#include "VehicleTow.h"

#include "RTS_Survival/RTSComponents/TowMechanic/TowedActor/TowedActor.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UVehicleTowComponent::UVehicleTowComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVehicleTowComponent::InitTowMesh(USceneComponent* TowMeshComponent)
{
	M_TowMeshComponent = TowMeshComponent;
}

bool UVehicleTowComponent::IsTowFree() const
{
	return not GetIsValidTowedActor() || not GetIsValidTowedActorComp() || M_CurrentTowSubtype == ETowActorAbilitySubtypes::None;
}

bool UVehicleTowComponent::GetIsValidTowMeshComponent() const
{
	if (M_TowMeshComponent.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		GetOwner(),
		"M_TowMeshComponent",
		"UVehicleTowComponent::GetIsValidTowMeshComponent",
		GetOwner());
	return false;
}

bool UVehicleTowComponent::GetIsValidTowedActor() const
{
	if (M_TowedActor.IsValid())
	{
		return true;
	}

	return false;
}

bool UVehicleTowComponent::GetIsValidTowedActorComp() const
{
	if (M_TowedActorComp.IsValid())
	{
		return true;
	}

	return false;
}

void UVehicleTowComponent::SetTowRelationship(AActor* TowedActor, UTowedActorComponent* TowedActorComp,
                                              const ETowActorAbilitySubtypes TowSubtype)
{
	M_TowedActor = TowedActor;
	M_TowedActorComp = TowedActorComp;
	M_CurrentTowSubtype = TowSubtype;
}

void UVehicleTowComponent::ClearTowRelationship()
{
	M_TowedActor = nullptr;
	M_TowedActorComp = nullptr;
	M_CurrentTowSubtype = ETowActorAbilitySubtypes::None;
}

USceneComponent* UVehicleTowComponent::GetTowMeshComponent() const
{
	if (M_TowMeshComponent.IsValid())
	{
		return M_TowMeshComponent.Get();
	}

	return nullptr;
}

AActor* UVehicleTowComponent::GetTowedActor() const
{
	if (M_TowedActor.IsValid())
	{
		return M_TowedActor.Get();
	}

	return nullptr;
}

UTowedActorComponent* UVehicleTowComponent::GetTowedActorComp() const
{
	if (M_TowedActorComp.IsValid())
	{
		return M_TowedActorComp.Get();
	}

	return nullptr;
}
