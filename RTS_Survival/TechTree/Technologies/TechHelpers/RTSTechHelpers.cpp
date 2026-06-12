// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "RTSTechHelpers.h"

#include "GameFramework/Actor.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/ArmorCalculation.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

bool RTSTechHelpers::ApplyArmorValueMultiplierToMatchingPlates(
	AActor* ActorToAdjust,
	const TArray<EArmorPlate>& ArmorPlatesToAdjust,
	const float ArmorValueMultiplier)
{
	if (not IsValid(ActorToAdjust))
	{
		return false;
	}

	UArmorCalculation* ArmorComponent = ActorToAdjust->FindComponentByClass<UArmorCalculation>();
	if (not IsValid(ArmorComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(
			ActorToAdjust,
			"ArmorComponent",
			"RTSTechHelpers::ApplyArmorValueMultiplierToMatchingPlates");
		return false;
	}

	ArmorComponent->ApplyArmorValueMultiplierToMatchingPlates(ArmorPlatesToAdjust, ArmorValueMultiplier);
	return true;
}

bool RTSTechHelpers::ApplyMaxHealthMultiplier(AActor* ActorToAdjust, const float HealthMultiplier)
{
	if (not IsValid(ActorToAdjust))
	{
		return false;
	}

	UHealthComponent* HealthComponent = ActorToAdjust->FindComponentByClass<UHealthComponent>();
	if (not IsValid(HealthComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(
			ActorToAdjust,
			"HealthComponent",
			"RTSTechHelpers::ApplyMaxHealthMultiplier");
		return false;
	}

	HealthComponent->SetMaxHealth(HealthComponent->GetMaxHealth() * HealthMultiplier);
	return true;
}
