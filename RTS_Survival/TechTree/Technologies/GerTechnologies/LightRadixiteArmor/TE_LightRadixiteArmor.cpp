// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "TE_LightRadixiteArmor.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/RTSComponents/ArmorComponent/Armor.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UTE_LightRadixiteArmor::ApplyTechnologyEffect(const UObject* WorldContextObject)
{
	RTSFunctionLibrary::PrintString("apply light radixite armor effect");
	Super::ApplyTechnologyEffect(WorldContextObject);
}

TSet<TSubclassOf<AActor>> UTE_LightRadixiteArmor::GetTargetActorClasses() const
{
	return {Panzer38TClass, PanzerIIClass, Sdkfz140Class};
}

void UTE_LightRadixiteArmor::OnApplyEffectToActor(AActor* ValidActor)
{
	FString ErrorMessage;

	ATankMaster* Tank = Cast<ATankMaster>(ValidActor);
	if (!Tank)
	{
		ErrorMessage = "ValidActor is not an ATankMaster: " + (ValidActor ? ValidActor->GetName() : "Null ValidActor") +
			"\n";
	}
	else if (!Panzer38TRadixiteMesh || !PanzerIIRadixiteMesh)
	{
		ErrorMessage = "one of the radixite meshes is null in UTE_LightRadixiteArmor\n";
	}
	else
	{
		if constexpr (DeveloperSettings::Debugging::GTechTree_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString("Applying light Radixite armor to " + Tank->GetName());
		}

		UMeshComponent* TankMesh = Tank->GetTankMesh();
		if (!IsValid(TankMesh))
		{
			ErrorMessage = "Invalid or null TankMesh in " + Tank->GetName() + "\n";
		}
		else
		{
			UStaticMeshComponent* RadixiteArmorMeshComp = NewObject<UStaticMeshComponent>(Tank);
			if (!RadixiteArmorMeshComp)
			{
				ErrorMessage = "Failed to create RadixiteArmorMeshComp for " + Tank->GetName() + "\n";
			}
			else
			{
				// Set and configure the Radixite armor mesh
				UStaticMesh* MeshToApply = GetMeshToApply(Tank);
				if (!MeshToApply)
				{
					ErrorMessage = "Failed to get mesh to apply for " + Tank->GetName() + "\n";
				}
				else
				{
					RadixiteArmorMeshComp->SetStaticMesh(MeshToApply);
					RadixiteArmorMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					RadixiteArmorMeshComp->
						AttachToComponent(TankMesh, FAttachmentTransformRules::KeepRelativeTransform);
					RadixiteArmorMeshComp->RegisterComponent();
					RadixiteArmorMeshComp->SetRelativeTransform(FTransform::Identity);
					ImproveArmor(Tank);
				}
			}
		}
	}

	// Print the accumulated error message, if any
	if (!ErrorMessage.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(ErrorMessage);
	}
}

UStaticMesh* UTE_LightRadixiteArmor::GetMeshToApply(AActor* ValidActor) const
{
	if (ValidActor->IsA(Panzer38TClass))
	{
		return Panzer38TRadixiteMesh;
	}
	if (ValidActor->IsA(PanzerIIClass))
	{
		return PanzerIIRadixiteMesh;
	}
	return Sdkfz140RadixiteMesh;
}

void UTE_LightRadixiteArmor::ImproveArmor(ATankMaster* ValidTank)
{
	for (auto EachArmor : ValidTank->GetTankArmor())
	{
		if (IsValid(EachArmor) && IsArmorTypeToAdjust(EachArmor->GetArmorPlateType()))
		{
			EachArmor->SetRawArmorValue(EachArmor->GetRawArmorValue() * ArmorValueMlt);
		}
	}
}

bool UTE_LightRadixiteArmor::IsArmorTypeToAdjust(const EArmorPlate ArmorPlate)
{
	if (ArmorPlate == EArmorPlate::Plate_Front || ArmorPlate == EArmorPlate::Plate_SideLeft || ArmorPlate ==
		EArmorPlate::Plate_SideRight || ArmorPlate == EArmorPlate::Turret_Front || ArmorPlate ==
		EArmorPlate::Turret_Mantlet
		|| ArmorPlate == EArmorPlate::Plate_FrontLowerGlacis || ArmorPlate == EArmorPlate::Plate_FrontUpperGlacis ||
		ArmorPlate == EArmorPlate::Plate_SideLowerLeft || ArmorPlate == EArmorPlate::Plate_SideLowerRight)
	{
		return true;
	}
	return false;
}
