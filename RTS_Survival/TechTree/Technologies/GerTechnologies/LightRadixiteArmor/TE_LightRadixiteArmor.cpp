// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "TE_LightRadixiteArmor.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/RTSComponents/ArmorComponent/Armor.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

TArray<ETankSubtype> UTE_LightRadixiteArmor::GetTanksToApplyTo_Internal() const
{
	TArray<ETankSubtype> CombinedSubtypes = TanksToApplyTo;
	for (const ETankSubtype TankSubtype : Panzer38TSubtypes)
	{
		CombinedSubtypes.AddUnique(TankSubtype);
	}
	for (const ETankSubtype TankSubtype : PanzerIISubtypes)
	{
		CombinedSubtypes.AddUnique(TankSubtype);
	}
	for (const ETankSubtype TankSubtype : Sdkfz140Subtypes)
	{
		CombinedSubtypes.AddUnique(TankSubtype);
	}
	return CombinedSubtypes;
}

void UTE_LightRadixiteArmor::ApplyOnTank_Internal(ATankMaster* Tank)
{
	if (not IsValid(Tank))
	{
		return;
	}

	const URTSComponent* RTSComponent = Tank->GetRTSComponent();
	if (not IsValid(RTSComponent))
	{
		return;
	}

	UStaticMesh* MeshToApply = GetMeshToApply(RTSComponent->GetSubtypeAsTankSubtype());
	if (not IsValid(MeshToApply))
	{
		RTSFunctionLibrary::ReportError("Failed to get radixite mesh to apply for " + Tank->GetName());
		return;
	}

	UMeshComponent* TankMesh = Tank->GetTankMesh();
	if (not IsValid(TankMesh))
	{
		RTSFunctionLibrary::ReportError("Invalid or null TankMesh in " + Tank->GetName());
		return;
	}

	UStaticMeshComponent* RadixiteArmorMeshComp = NewObject<UStaticMeshComponent>(Tank);
	if (not IsValid(RadixiteArmorMeshComp))
	{
		RTSFunctionLibrary::ReportError("Failed to create RadixiteArmorMeshComp for " + Tank->GetName());
		return;
	}

	RadixiteArmorMeshComp->SetStaticMesh(MeshToApply);
	RadixiteArmorMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RadixiteArmorMeshComp->AttachToComponent(TankMesh, FAttachmentTransformRules::KeepRelativeTransform);
	RadixiteArmorMeshComp->RegisterComponent();
	RadixiteArmorMeshComp->SetRelativeTransform(FTransform::Identity);
	ImproveArmor(Tank);

	if constexpr (DeveloperSettings::Debugging::GTechTree_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Applying light Radixite armor to " + Tank->GetName());
	}
}

UStaticMesh* UTE_LightRadixiteArmor::GetMeshToApply(const ETankSubtype TankSubtype) const
{
	if (Panzer38TSubtypes.Contains(TankSubtype))
	{
		return Panzer38TRadixiteMesh;
	}
	if (PanzerIISubtypes.Contains(TankSubtype))
	{
		return PanzerIIRadixiteMesh;
	}
	if (Sdkfz140Subtypes.Contains(TankSubtype))
	{
		return Sdkfz140RadixiteMesh;
	}
	return nullptr;
}

void UTE_LightRadixiteArmor::ImproveArmor(ATankMaster* ValidTank) const
{
	if (not IsValid(ValidTank))
	{
		return;
	}

	for (UArmor* EachArmor : ValidTank->GetTankArmor())
	{
		if (IsValid(EachArmor) && IsArmorTypeToAdjust(EachArmor->GetArmorPlateType()))
		{
			EachArmor->SetRawArmorValue(EachArmor->GetRawArmorValue() * ArmorValueMlt);
		}
	}
}

bool UTE_LightRadixiteArmor::IsArmorTypeToAdjust(const EArmorPlate ArmorPlate)
{
	return ArmorPlate == EArmorPlate::Plate_Front ||
		ArmorPlate == EArmorPlate::Plate_SideLeft ||
		ArmorPlate == EArmorPlate::Plate_SideRight ||
		ArmorPlate == EArmorPlate::Turret_Front ||
		ArmorPlate == EArmorPlate::Turret_Mantlet ||
		ArmorPlate == EArmorPlate::Plate_FrontLowerGlacis ||
		ArmorPlate == EArmorPlate::Plate_FrontUpperGlacis ||
		ArmorPlate == EArmorPlate::Plate_SideLowerLeft ||
		ArmorPlate == EArmorPlate::Plate_SideLowerRight;
}
