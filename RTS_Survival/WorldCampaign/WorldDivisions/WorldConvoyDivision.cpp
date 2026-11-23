// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldConvoyDivision.h"

#include "Components/SphereComponent.h"

namespace WorldConvoyDivisionConstants
{
	constexpr float ConvoyCollisionRadius = 300.f;
}

AWorldConvoyDivision::AWorldConvoyDivision()
{
	M_ConvoyCollision = CreateDefaultSubobject<USphereComponent>(TEXT("ConvoyCollision"));
	M_ConvoyCollision->InitSphereRadius(WorldConvoyDivisionConstants::ConvoyCollisionRadius);
	M_ConvoyCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	M_ConvoyCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	M_ConvoyCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	SetRootComponent(M_ConvoyCollision);
}

void AWorldConvoyDivision::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	M_RemainingTruckCount = FMath::Max(1, M_StartingTruckCount);
}

void AWorldConvoyDivision::ApplyWorldDivisionDamage(const int32 DamageEntryCount)
{
	if (DamageEntryCount <= 0 || M_RemainingTruckCount <= 0)
	{
		return;
	}

	M_RemainingTruckCount -= FMath::Min(DamageEntryCount, M_RemainingTruckCount);
	RecalculateCurrentStrengthFromEntries(M_RemainingTruckCount, FMath::Max(1, M_StartingTruckCount));
	BroadcastDivisionStrengthChanged();
}

void AWorldConvoyDivision::RestoreSubtypeSaveData(const FWorldDivisionSaveData& DivisionSaveData)
{
	const int32 StartingTruckCount = FMath::Max(1, M_StartingTruckCount);
	const int32 MaxStrength = GetMaxStrengthPercentage();
	const int32 CurrentStrength = GetCurrentStrengthPercentage();
	if (MaxStrength <= 0 || CurrentStrength >= MaxStrength)
	{
		M_RemainingTruckCount = StartingTruckCount;
		return;
	}

	const float StrengthAlpha = static_cast<float>(FMath::Max(0, CurrentStrength)) / static_cast<float>(MaxStrength);
	const int32 MinimumTrucks = CurrentStrength > 0 ? 1 : 0;
	M_RemainingTruckCount = FMath::Clamp(FMath::RoundToInt(StartingTruckCount * StrengthAlpha), MinimumTrucks,
	                                     StartingTruckCount);
}
