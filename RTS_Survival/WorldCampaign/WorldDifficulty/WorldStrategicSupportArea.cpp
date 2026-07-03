// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldDifficulty/WorldStrategicSupportArea.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Subsystems/RadiusSubsystem/RTSRadiusPoolSubsystem/RTSRadiusPoolSubsystem.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UWorldStrategicSupportArea::UWorldStrategicSupportArea()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWorldStrategicSupportArea::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HideAllRadii();

	Super::EndPlay(EndPlayReason);
}

void UWorldStrategicSupportArea::SetCachedStrategicSupportPercentage(const int32 StrengthPercentage)
{
	M_CachedStrategicSupportPercentage = StrengthPercentage;
}

void UWorldStrategicSupportArea::ShowHoverRadius()
{
	if (GetIsRadiusActive(M_SelectedRadiusId) || GetIsRadiusActive(M_HoverRadiusId))
	{
		return;
	}

	M_SelectedRadiusId = InvalidRadiusId;
	M_HoverRadiusId = InvalidRadiusId;
	M_HoverRadiusId = CreateRadius();
}

void UWorldStrategicSupportArea::HideHoverRadius()
{
	HideRadiusById(M_HoverRadiusId);
}

void UWorldStrategicSupportArea::ShowSelectedRadius()
{
	HideHoverRadius();
	if (GetIsRadiusActive(M_SelectedRadiusId))
	{
		return;
	}

	M_SelectedRadiusId = InvalidRadiusId;
	M_SelectedRadiusId = CreateRadius();
}

void UWorldStrategicSupportArea::HideSelectedRadius()
{
	HideRadiusById(M_SelectedRadiusId);
}

void UWorldStrategicSupportArea::HideAllRadii()
{
	HideHoverRadius();
	HideSelectedRadius();
}

void UWorldStrategicSupportArea::ShowHoverRadiiOnActor(AActor* Actor)
{
	if (not IsValid(Actor))
	{
		return;
	}

	TArray<UWorldStrategicSupportArea*> DifficultyInfluences;
	Actor->GetComponents<UWorldStrategicSupportArea>(DifficultyInfluences);
	for (UWorldStrategicSupportArea* DifficultyInfluence : DifficultyInfluences)
	{
		if (not IsValid(DifficultyInfluence))
		{
			continue;
		}

		DifficultyInfluence->ShowHoverRadius();
	}
}

void UWorldStrategicSupportArea::HideHoverRadiiOnActor(AActor* Actor)
{
	if (not IsValid(Actor))
	{
		return;
	}

	TArray<UWorldStrategicSupportArea*> DifficultyInfluences;
	Actor->GetComponents<UWorldStrategicSupportArea>(DifficultyInfluences);
	for (UWorldStrategicSupportArea* DifficultyInfluence : DifficultyInfluences)
	{
		if (not IsValid(DifficultyInfluence))
		{
			continue;
		}

		DifficultyInfluence->HideHoverRadius();
	}
}

void UWorldStrategicSupportArea::ShowSelectedRadiiOnActor(AActor* Actor)
{
	if (not IsValid(Actor))
	{
		return;
	}

	TArray<UWorldStrategicSupportArea*> DifficultyInfluences;
	Actor->GetComponents<UWorldStrategicSupportArea>(DifficultyInfluences);
	for (UWorldStrategicSupportArea* DifficultyInfluence : DifficultyInfluences)
	{
		if (not IsValid(DifficultyInfluence))
		{
			continue;
		}

		DifficultyInfluence->ShowSelectedRadius();
	}
}

void UWorldStrategicSupportArea::HideSelectedRadiiOnActor(AActor* Actor)
{
	if (not IsValid(Actor))
	{
		return;
	}

	TArray<UWorldStrategicSupportArea*> DifficultyInfluences;
	Actor->GetComponents<UWorldStrategicSupportArea>(DifficultyInfluences);
	for (UWorldStrategicSupportArea* DifficultyInfluence : DifficultyInfluences)
	{
		if (not IsValid(DifficultyInfluence))
		{
			continue;
		}

		DifficultyInfluence->HideSelectedRadius();
	}
}

bool UWorldStrategicSupportArea::GetIsValidRadiusPoolSubsystem()
{
	if (M_RadiusPoolSubsystem.IsValid())
	{
		return true;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("World"),
			TEXT("UWorldStrategicSupportArea::GetIsValidRadiusPoolSubsystem"),
			this);
		return false;
	}

	M_RadiusPoolSubsystem = World->GetSubsystem<URTSRadiusPoolSubsystem>();
	if (M_RadiusPoolSubsystem.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_RadiusPoolSubsystem"),
		TEXT("UWorldStrategicSupportArea::GetIsValidRadiusPoolSubsystem"),
		this);
	return false;
}

URTSRadiusPoolSubsystem* UWorldStrategicSupportArea::GetRadiusPoolSubsystem()
{
	if (not GetIsValidRadiusPoolSubsystem())
	{
		return nullptr;
	}

	return M_RadiusPoolSubsystem.Get();
}

int32 UWorldStrategicSupportArea::CreateRadius()
{
	if (M_Settings.XYRadius <= 0.f)
	{
		return InvalidRadiusId;
	}

	AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return InvalidRadiusId;
	}

	URTSRadiusPoolSubsystem* RadiusPoolSubsystem = GetRadiusPoolSubsystem();
	if (not IsValid(RadiusPoolSubsystem))
	{
		return InvalidRadiusId;
	}

	const int32 RadiusId = RadiusPoolSubsystem->CreateRTSRadius(
		Owner->GetActorLocation(),
		M_Settings.XYRadius,
		M_Settings.RadiusType);
	if (RadiusId <= 0)
	{
		return InvalidRadiusId;
	}

	RadiusPoolSubsystem->AttachRTSRadiusToActorYawOnly(RadiusId, Owner, FVector::ZeroVector);
	return RadiusId;
}

bool UWorldStrategicSupportArea::GetIsRadiusActive(const int32 RadiusId)
{
	if (RadiusId <= 0)
	{
		return false;
	}

	URTSRadiusPoolSubsystem* RadiusPoolSubsystem = GetRadiusPoolSubsystem();
	if (not IsValid(RadiusPoolSubsystem))
	{
		return false;
	}

	return RadiusPoolSubsystem->GetIsRTSRadiusIdActive(RadiusId);
}

void UWorldStrategicSupportArea::HideRadiusById(int32& RadiusId)
{
	if (RadiusId <= 0)
	{
		RadiusId = InvalidRadiusId;
		return;
	}

	URTSRadiusPoolSubsystem* RadiusPoolSubsystem = GetRadiusPoolSubsystem();
	if (not IsValid(RadiusPoolSubsystem))
	{
		RadiusId = InvalidRadiusId;
		return;
	}

	if (not RadiusPoolSubsystem->GetIsRTSRadiusIdActive(RadiusId))
	{
		RadiusId = InvalidRadiusId;
		return;
	}

	RadiusPoolSubsystem->HideRTSRadiusById(RadiusId);
	RadiusId = InvalidRadiusId;
}
