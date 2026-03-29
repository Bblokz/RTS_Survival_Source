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
	return not GetIsValidTowedActor() || not GetIsValidTowedActorComp() || M_CurrentTowSubtype ==
		ETowedActorTarget::None;
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
                                              const ETowedActorTarget TowSubtype)
{
	M_TowedActor = TowedActor;
	M_TowedActorComp = TowedActorComp;
	M_CurrentTowSubtype = TowSubtype;
}

void UVehicleTowComponent::SwapAbilityToDetachTow()
{
	if (not GetIsValidICommands())
	{
		return;
	}
	FUnitAbilityEntry NewAbility;
	NewAbility.AbilityId = EAbilityID::IdDetachTow;
	NewAbility.CustomType = static_cast<int32>(ETowAbilityType::VehicleHook);
	M_OwnerCommandsInterface->SwapAbility(EAbilityID::IdTowActor, NewAbility);
}

void UVehicleTowComponent::SwapAbilityToTow()
{
	if (not GetIsValidICommands())
	{
		return;
	}
	FUnitAbilityEntry NewAbility;
	NewAbility.AbilityId = EAbilityID::IdTowActor;
	NewAbility.CustomType = static_cast<int32>(ETowAbilityType::VehicleHook);
	M_OwnerCommandsInterface->SwapAbility(EAbilityID::IdDetachTow, NewAbility);
}

void UVehicleTowComponent::ClearTowRelationship()
{
	M_TowedActor = nullptr;
	M_TowedActorComp = nullptr;
	M_CurrentTowSubtype = ETowedActorTarget::None;
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

void UVehicleTowComponent::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_SetupCommandsInterface();
	if (UWorld* World = GetWorld())
	{
		TWeakObjectPtr<UVehicleTowComponent> WeakThis(this);
		auto SetupAbility = [WeakThis]()-> void
		{
			if (not WeakThis.IsValid())
			{
				return;
			}
			WeakThis->AddAbilityToCommands();
		};
		// Prevent race condition with tank master beginplay initialisation.
		FTimerDelegate Del;
		Del.BindLambda(SetupAbility);
		World->GetTimerManager().SetTimerForNextTick(Del);
	}
}

void UVehicleTowComponent::BeginPlay_SetupCommandsInterface()
{
	if (not GetOwner())
	{
		return;
	}
	ICommands* CommandsInterface = Cast<ICommands>(GetOwner());
	M_OwnerCommandsInterface.SetInterface(CommandsInterface);
	M_OwnerCommandsInterface.SetObject(GetOwner());
	(void)GetIsValidICommands();
}

bool UVehicleTowComponent::GetIsValidICommands()
{
	if (not IsValid(M_OwnerCommandsInterface.GetObject()))
	{
		if (not GetOwner())
		{
			return false;
		}
		ICommands* CommandsInterface = Cast<ICommands>(GetOwner());
		M_OwnerCommandsInterface.SetInterface(CommandsInterface);
		M_OwnerCommandsInterface.SetObject(GetOwner());
		if (not IsValid(M_OwnerCommandsInterface->_getUObject()))
		{
			const FString OwnerName = GetOwner() ? GetOwner()->GetName() : "NULL";
			RTSFunctionLibrary::ReportError(
				"The tow vehicle comp does not have access to a valid ICommands interface! Owner: " + OwnerName +
				"\n vehicle tow;  " + GetName());
			return false;;
		}
		return true;
	}
	return true;
}

void UVehicleTowComponent::AddAbilityToCommands()
{
	if (not GetIsValidICommands())
	{
		return;
	}
	FUnitAbilityEntry NewAbility;
	NewAbility.AbilityId = EAbilityID::IdTowActor;
	NewAbility.CustomType = static_cast<int32>(ETowAbilityType::VehicleHook);
	M_OwnerCommandsInterface->AddAbility(NewAbility, M_TowSettings.PreferredAbilityIndex);
}
