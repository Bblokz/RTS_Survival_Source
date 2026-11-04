#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Subsystems/FireSubsystem/ERTSFireType.h"
#include "RTS_Survival/Subsystems/FireSubsystem/FRTSFireEffectParams.h"
#include "TankEngineFireFuelLeakSettings.generated.h"

/**
 * @brief Burning fuel leaking out of a tank whose engine is on fire: a small flame low on the hull below the engine
 * and short-lived burning patches left on the ground behind it. Visual only; the patches deal no damage.
 * Every patch is a pooled fire with a short lifetime and each tank keeps only a few, so the cost stays bounded.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FTankEngineFireFuelLeakSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fuel Leak")
	bool bLeakFuel = true;

	// Height (cm) above the tank's pivot where the fuel drips out below the engine.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fuel Leak", meta=(EditCondition="bLeakFuel", ClampMin="0"))
	float LeakHeightAboveGround = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fuel Leak|Leak Fire", meta=(EditCondition="bLeakFuel"))
	ERTSFireType LeakFireType = ERTSFireType::SmallOptim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fuel Leak|Leak Fire", meta=(EditCondition="bLeakFuel"))
	FVector LeakFireScale = FVector(0.45);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fuel Leak|Leak Fire", meta=(EditCondition="bLeakFuel"))
	FRTSFireEffectParams LeakFireEffectParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fuel Leak|Ground Patches", meta=(EditCondition="bLeakFuel"))
	ERTSFireType PatchFireType = ERTSFireType::SmallOptim;

	// Seconds a burning patch stays on the ground after it dripped.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fuel Leak|Ground Patches",
		meta=(EditCondition="bLeakFuel", ClampMin="0.1"))
	float PatchLifeTime = 3.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fuel Leak|Ground Patches", meta=(EditCondition="bLeakFuel"))
	FVector PatchFireScale = FVector(0.35);

	// A moving tank drops a new patch each time it has driven this far (cm) from the last one.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fuel Leak|Ground Patches",
		meta=(EditCondition="bLeakFuel", ClampMin="10"))
	float PatchSpacingCm = 220.f;

	// A standing tank renews the patch under it this often, so a puddle keeps burning without stacking fires.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fuel Leak|Ground Patches",
		meta=(EditCondition="bLeakFuel", ClampMin="0.1"))
	float StandingPatchIntervalSeconds = 3.f;

	// Most patches one tank keeps burning; the oldest goes out when a new one drips.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fuel Leak|Ground Patches",
		meta=(EditCondition="bLeakFuel", ClampMin="1"))
	int32 MaxPatchesPerTank = 4;

	// Dark stain under each patch that outlasts the flames.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fuel Leak|Ground Patches", meta=(EditCondition="bLeakFuel"))
	bool bLeaveScorchMarks = true;

	// Min and max scorch mark size in cm.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fuel Leak|Ground Patches",
		meta=(EditCondition="bLeakFuel && bLeaveScorchMarks"))
	FVector2D ScorchMarkSize = FVector2D(90.0, 140.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fuel Leak|Ground Patches",
		meta=(EditCondition="bLeakFuel && bLeaveScorchMarks", ClampMin="0"))
	float ScorchMarkLifeTime = 12.f;
};
