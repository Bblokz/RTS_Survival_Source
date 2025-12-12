#pragma once

#include "CoreMinimal.h"

#include "Resistances.generated.h"

USTRUCT(BlueprintType, Blueprintable)
struct FDamageMltPerSide
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FrontMlt = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SidesMlt = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RearMlt  = 1.f;

	// Keep aggregate-friendly defaults, and add a constexpr convenience ctor.
	constexpr FDamageMltPerSide() = default;
	constexpr FDamageMltPerSide(float Front, float Sides, float Rear)
		: FrontMlt(Front), SidesMlt(Sides), RearMlt(Rear) {}

	// Handy helper
	static constexpr FDamageMltPerSide Uniform(float v) { return {v,v,v}; }
};

USTRUCT(BlueprintType, Blueprintable)
struct FLaserRadiationDamageMlt
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDamageMltPerSide LaserMltPerPart{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDamageMltPerSide RadiationMltPerPart{};

	// Default + convenience ctor, both constexpr
	constexpr FLaserRadiationDamageMlt() = default;
	constexpr FLaserRadiationDamageMlt(FDamageMltPerSide Laser, FDamageMltPerSide Radiation)
		: LaserMltPerPart(Laser), RadiationMltPerPart(Radiation) {}
};
