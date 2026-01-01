// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/Resistances/Resistances.h"
#include "Armor.generated.h"

UENUM(Blueprintable)
enum class EArmorPlate : uint8
{
	Plate_Front,
	Plate_FrontUpperGlacis,
	Plate_FrontLowerGlacis,
	Plate_SideLeft,
	Plate_SideRight,
	Plate_SideLowerLeft,
	Plate_SideLowerRight,
	Plate_Rear,
	Plate_RearLowerGlacis,
	Plate_RearUpperGlacis,
	Turret_Front,
	Turret_SideLeft,
	Turret_SideRight,
	Turret_Rear,
	Turret_SidesAndRear,
	Turret_Cupola,
	Turret_Mantlet
	
};

enum class EArmorPlateDamageType : uint8
{
	DamageFront = 0,
	DamageSides = 1,
	DamageRear = 2,
}

static EArmorPlateDamageType GetDamageTypeFromPlate(const EArmorPlate PlateHit)
{
	switch (PlateHit)
	{
	case EArmorPlate::Plate_Front:
	case EArmorPlate::Plate_FrontUpperGlacis:
	case EArmorPlate::Plate_FrontLowerGlacis:
	return EArmorPlateDamageType::DamageFront;
	
	case EArmorPlate::Plate_SideLeft:
	case EArmorPlate::Plate_SideRight:
	case EArmorPlate::Plate_SideLowerLeft:
	case EArmorPlate::Plate_SideLowerRight:
	return EArmorPlateDamageType::DamageSides;

	
	case EArmorPlate::Plate_Rear:
	case EArmorPlate::Plate_RearLowerGlacis:
	case EArmorPlate::Plate_RearUpperGlacis:
	return EArmorPlateDamageType::DamageRear;
	
	case EArmorPlate::Turret_Front:
	case EArmorPlate::Turret_Cupola:
	case EArmorPlate::Turret_Mantle:
	return EArmorPlateDamageType::DamageFront;
	
	case EArmorPlate::Turret_SideLeft:
	case EArmorPlate::Turret_SideRight:
	return EArmorPlateDamageType::DamageSides;
	
	case EArmorPlate::Turret_Rear:
	return EArmorPlateDamageType::DamageRear;
	
	case EArmorPlate::Turret_SidesAndRear:
	return EArmorPlateDamageType::DamageSides;
	default:
		break;
	}
	return EArmorPlateDamageType::DamageFront;
}

static float Global_GetResistanceMultiplierFromPlate(
	const FDamageMltPerSide& ResistancePerSide, const EArmorPlate Side)
{
	switch (Side) {
	case EArmorPlate::Plate_Front:
		return ResistancePerSide.FrontMlt;
	case EArmorPlate::Plate_FrontUpperGlacis:
		return ResistancePerSide.FrontMlt;
	case EArmorPlate::Plate_FrontLowerGlacis:
		return ResistancePerSide.FrontMlt;
	case EArmorPlate::Plate_SideLeft:
		return ResistancePerSide.SidesMlt;
	case EArmorPlate::Plate_SideRight:
		return ResistancePerSide.SidesMlt;
	case EArmorPlate::Plate_SideLowerLeft:
		return ResistancePerSide.SidesMlt;
	case EArmorPlate::Plate_SideLowerRight:
		return ResistancePerSide.SidesMlt;
	case EArmorPlate::Plate_Rear:
		return ResistancePerSide.RearMlt;
	case EArmorPlate::Plate_RearLowerGlacis:
		return ResistancePerSide.RearMlt;
	case EArmorPlate::Plate_RearUpperGlacis:
		return ResistancePerSide.RearMlt;
	case EArmorPlate::Turret_Front:
		return ResistancePerSide.FrontMlt;
	case EArmorPlate::Turret_SideLeft:
		return ResistancePerSide.SidesMlt;
	case EArmorPlate::Turret_SideRight:
		return ResistancePerSide.SidesMlt;
	case EArmorPlate::Turret_Rear:
		return ResistancePerSide.RearMlt;
	case EArmorPlate::Turret_SidesAndRear:
		return ResistancePerSide.SidesMlt;
	case EArmorPlate::Turret_Cupola:
		return ResistancePerSide.FrontMlt;
	case EArmorPlate::Turret_Mantlet:
		return ResistancePerSide.FrontMlt;
	}
	return ResistancePerSide.FrontMlt;
};


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UArmor : public UBoxComponent
{
	GENERATED_BODY()

public:
	UArmor();

	float GetArmorAtAngle(const float Angle, float& OutArmorPenAtAngle)const;
	inline float GetRawArmorValue()const {return ArmorValue;};
	inline void SetRawArmorValue(const float NewArmorValue) {ArmorValue = NewArmorValue;};

	FString GetArmorPlateName()const {return GetArmorPlateStringName();};
	EArmorPlate GetArmorPlateType()const {return ArmorType;};

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float ArmorValue = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	EArmorPlate ArmorType = EArmorPlate::Plate_Front;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;


private:
	FString GetArmorPlateStringName() const;

	// Static members for the parabola coefficients
	static const float M_PenParabola_A;
	static const float M_PenParabola_B;
	static const float M_PenParabola_C;

};
