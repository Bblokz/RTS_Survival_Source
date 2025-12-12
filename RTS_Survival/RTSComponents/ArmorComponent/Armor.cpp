// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "Armor.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"

const float UArmor::M_PenParabola_A = -1.576f;
const float UArmor::M_PenParabola_B = 2.96f;
const float UArmor::M_PenParabola_C = 1.0f;

UArmor::UArmor()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
	bCanEverAffectNavigation = false;
}

float UArmor::GetArmorAtAngle(const float Angle, float& OutArmorPenAtAngle) const
{
	// Note that we use the absolute value of the cosine here as we do not assume that the angle is not normalized to 0 - 90
	const float CosAngle = FMath::Abs(FMath::Cos(Angle));
	// See DeveloperSettings.h for explanation.
	constexpr float Decay = DeveloperSettings::GameBalance::Weapons::PenetrationExponentialDecayFactor;
	const float ReductionFactor = FMath::Exp(-Decay * (1.0f - CosAngle));
	OutArmorPenAtAngle *= ReductionFactor;
	return ArmorValue / CosAngle;
}


void UArmor::BeginPlay()
{
	Super::BeginPlay();
	bCanEverAffectNavigation = false;
	SetComponentTickEnabled(false);
	if (AActor* MyOwner = GetOwner())
	{
		URTSComponent* RTSComponent = MyOwner->FindComponentByClass<URTSComponent>();
		if (IsValid(RTSComponent))
		{
			if(RTSComponent->GetOwningPlayer() == 1)
			{
				FRTS_CollisionSetup::SetupPlayerArmorMeshCollision(this);
			}
			else
			{
				FRTS_CollisionSetup::SetupEnemyArmorMeshCollision(this);
			}
		}
		else
		{
			RTSFunctionLibrary::ReportError("cannot access RTS component for armor component."
				"\n Owner of component: " + MyOwner->GetName() + " does not have a RTS component."
				"function: UArmor::BeginPlay()"
				"\n Setting up collision as enemy armor part by default...");
			FRTS_CollisionSetup::SetupEnemyArmorMeshCollision(this);
		}
	}
}


void UArmor::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RTSFunctionLibrary::PrintString("Hello!");
}

FString UArmor::GetArmorPlateStringName() const
{
	switch (ArmorType)
	{
	case EArmorPlate::Plate_Front:
		return FString("Front Plate");
	case EArmorPlate::Plate_FrontUpperGlacis:
		return FString("Front Upper Glacis Plate");
	case EArmorPlate::Plate_FrontLowerGlacis:
		return FString("Front Lower Glacis Plate");
	case EArmorPlate::Plate_SideLeft:
		return FString("Left Side Plate");
	case EArmorPlate::Plate_SideRight:
		return FString("Right Side Plate");
	case EArmorPlate::Plate_SideLowerLeft:
		return FString("Lower Left Side Plate");
	case EArmorPlate::Plate_SideLowerRight:
		return FString("Lower Right Side Plate");
	case EArmorPlate::Plate_Rear:
		return FString("Rear Plate");
	case EArmorPlate::Plate_RearLowerGlacis:
		return FString("Rear Lower Glacis Plate");
	case EArmorPlate::Plate_RearUpperGlacis:
		return FString("Rear Upper Glacis Plate");
	case EArmorPlate::Turret_Front:
		return FString("Front Turret Plate");
	case EArmorPlate::Turret_SideLeft:
		return FString("Left Side Turret Plate");
	case EArmorPlate::Turret_SideRight:
		return FString("Right Side Turret Plate");
	case EArmorPlate::Turret_Rear:
		return FString("Rear Turret Plate");
	case EArmorPlate::Turret_SidesAndRear:
		return FString("Turret sides + rear");
	case EArmorPlate::Turret_Cupola:
		return FString("Cupola");
	default:
		return FString("Unknown Armor Plate");
	}
}
