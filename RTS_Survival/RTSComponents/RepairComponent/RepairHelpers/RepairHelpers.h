#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Squads/SquadControllerHpComp/USquadHealthComponent.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"

namespace FRTSRepairHelpers
{
	static float GetUnitRepairRadius(bool& OutIsValidRadius, AActor* UnitToRepair)
	{
		OutIsValidRadius = false;
		if (not IsValid(UnitToRepair))
		{
			return 0.f;
		}
		ICommands* UnitAsCommands = Cast<ICommands>(UnitToRepair);
		if (not UnitAsCommands)
		{
			return 0.f;
		}
		const float UnitIcCommandsRepairRadius = UnitAsCommands->GetUnitRepairRadius();
		OutIsValidRadius = UnitIcCommandsRepairRadius > 0.f;
		return UnitIcCommandsRepairRadius;
	};


	static void Debug_Repair(const FString& DebugMessage)
	{
		if constexpr (DeveloperSettings::Debugging::GRepair_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString(DebugMessage);
		}
	};

	static bool GetIsUnitValidForRepairs(AActor* UnitToRepair)
	{
		if (not RTSFunctionLibrary::RTSIsValid(UnitToRepair))
		{
			return false;
		}
		const bool bIsTankDerived = UnitToRepair->IsA(ATankMaster::StaticClass());
		const bool bIsBuildingDerived = UnitToRepair->IsA(ABuildingExpansion::StaticClass());
		if (not bIsTankDerived && not bIsBuildingDerived)
		{
			Debug_Repair("Unit is not tank or building; cannot repair!");
			return false;
		}
		const UHealthComponent* HealthComponent = UnitToRepair->FindComponentByClass<UHealthComponent>();
		if (not IsValid(HealthComponent))
		{
			return false;
		}
		const bool IsNearlyFullHealth = HealthComponent->GetHealthPercentage() >= 0.99f;
		return not IsNearlyFullHealth;
	};

	static bool GetIsUnitValidForHealing(AActor* UnitToHeal)
	{
		if (not RTSFunctionLibrary::RTSIsValid(UnitToHeal))
		{
			return false;
		}
		ASquadController* UnitAsSquadController = Cast<ASquadController>(UnitToHeal);
		if(UnitAsSquadController)
		{
			if(USquadHealthComponent* SquadHealth = UnitAsSquadController->FindComponentByClass<USquadHealthComponent>())
			{
				return SquadHealth->GetHealthPercentage() < 1.f;	
			}
			return false;
		}
		if(ASquadUnit* UnitAsSquadUnit = Cast<ASquadUnit>(UnitToHeal))
		{
			if(USquadHealthComponent* SquadHealth = UnitAsSquadUnit->FindComponentByClass<USquadHealthComponent>())
			{
				return SquadHealth->GetHealthPercentage() < 1.f;	
			}
			return false;
		}
		return false;
	}


	static TArray<FVector> GetRepairSquadOffsets(
		const int32 AmountSquadUnits,
		AActor* ActorToRepair,
		bool& bOutAreOffsetsValid)
	{
		TArray<FVector> Offsets;

		// sanity check
		if (AmountSquadUnits <= 0 || ActorToRepair == nullptr)
		{
			bOutAreOffsetsValid = false;
			return Offsets;
		}

		const float RepairRadius = GetUnitRepairRadius(bOutAreOffsetsValid, ActorToRepair);
		if (!bOutAreOffsetsValid)
		{
			Debug_Repair("Failed to get the unit repair radius and so will fail to get repair squad offsets.");
			return {};
		}

		// full circle and step between units
		constexpr float TwoPi = 2.0f * PI;
		const float AngleDelta = TwoPi / static_cast<float>(AmountSquadUnits);

		// rotates the entire formation by 15° each time this function runs
		static float AccumulatedOffset = 0.0f;
		constexpr float OffsetStep = FMath::DegreesToRadians(15.0f);
		AccumulatedOffset = FMath::Fmod(AccumulatedOffset + OffsetStep, TwoPi);

		// build offsets around the circle, shifted by AccumulatedOffset
		for (int32 i = 0; i < AmountSquadUnits; ++i)
		{
			const float Angle = AccumulatedOffset + AngleDelta * static_cast<float>(i);
			const float X = FMath::Cos(Angle) * RepairRadius;
			const float Y = FMath::Sin(Angle) * RepairRadius;
			Offsets.Add(FVector(X, Y, 0.0f));
		}

		bOutAreOffsetsValid = true;
		return Offsets;
	}
}
