#pragma once

#include "CoreMinimal.h"
#include "Engine/DamageEvents.h"

#include "RTSDamageTypes.generated.h"

UENUM(BlueprintType)
enum class ERTSDamageType : uint8
{
	None,
	Kinetic,
	Fire,
	Laser,
	Radiation
};

UCLASS()
class URTSDamageType: public UDamageType 
{
	GENERATED_BODY()

public:
	ERTSDamageType RTSDamageType = ERTSDamageType::None;
	
	
};
UCLASS()
class UKineticDamageType : public URTSDamageType
{
	GENERATED_BODY()
public:
	UKineticDamageType(){ RTSDamageType= ERTSDamageType::Kinetic;}
	
};

UCLASS()
class UFireDamageType: public URTSDamageType
{
	GENERATED_BODY()
public:
	UFireDamageType(){ RTSDamageType= ERTSDamageType::Fire;}
	
};

UCLASS()
class ULaserDamageType: public URTSDamageType
{
	GENERATED_BODY()
public:
	ULaserDamageType(){ RTSDamageType= ERTSDamageType::Laser;}
	
};

UCLASS()
class URadiationDamageType: public URTSDamageType
{
	GENERATED_BODY()
public:
	URadiationDamageType(){ RTSDamageType= ERTSDamageType::Radiation;}
	
};

