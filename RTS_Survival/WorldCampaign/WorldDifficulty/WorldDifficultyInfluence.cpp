// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldDifficulty/WorldDifficultyInfluence.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Subsystems/RadiusSubsystem/RTSRadiusPoolSubsystem/RTSRadiusPoolSubsystem.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

int32 FWorldDifficultyInfluenceSettings::GetDifficultyPercent(const ERTSGameDifficulty GameDifficulty) const
{
	return PerDifficultyMlt.ApplyToPercentage(BasePercentage, GameDifficulty);
}

FRTSStrengthEstimationInfluenceReason FWorldDifficultyInfluenceSettings::BuildInfluenceReason(
	const ERTSGameDifficulty GameDifficulty) const
{
	return FRTSStrengthEstimationInfluenceReason(ReasonText, GetDifficultyPercent(GameDifficulty));
}

UWorldDifficultyInfluence::UWorldDifficultyInfluence()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWorldDifficultyInfluence::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HideAllRadii();

	Super::EndPlay(EndPlayReason);
}

int32 UWorldDifficultyInfluence::GetDifficultyInfluencePercent(const ERTSGameDifficulty GameDifficulty) const
{
	return M_Settings.GetDifficultyPercent(GameDifficulty);
}

FRTSStrengthEstimationInfluenceReason UWorldDifficultyInfluence::BuildInfluenceReason(
	const ERTSGameDifficulty GameDifficulty) const
{
	return M_Settings.BuildInfluenceReason(GameDifficulty);
}

void UWorldDifficultyInfluence::ShowHoverRadius()
{
	if (GetIsRadiusActive(M_SelectedRadiusId) || GetIsRadiusActive(M_HoverRadiusId))
	{
		return;
	}

	M_SelectedRadiusId = InvalidRadiusId;
	M_HoverRadiusId = InvalidRadiusId;
	M_HoverRadiusId = CreateRadius();
}

void UWorldDifficultyInfluence::HideHoverRadius()
{
	HideRadiusById(M_HoverRadiusId);
}

void UWorldDifficultyInfluence::ShowSelectedRadius()
{
	HideHoverRadius();
	if (GetIsRadiusActive(M_SelectedRadiusId))
	{
		return;
	}

	M_SelectedRadiusId = InvalidRadiusId;
	M_SelectedRadiusId = CreateRadius();
}

void UWorldDifficultyInfluence::HideSelectedRadius()
{
	HideRadiusById(M_SelectedRadiusId);
}

void UWorldDifficultyInfluence::HideAllRadii()
{
	HideHoverRadius();
	HideSelectedRadius();
}

void UWorldDifficultyInfluence::ShowHoverRadiiOnActor(AActor* Actor)
{
	if (not IsValid(Actor))
	{
		return;
	}

	TArray<UWorldDifficultyInfluence*> DifficultyInfluences;
	Actor->GetComponents<UWorldDifficultyInfluence>(DifficultyInfluences);
	for (UWorldDifficultyInfluence* DifficultyInfluence : DifficultyInfluences)
	{
		if (not IsValid(DifficultyInfluence))
		{
			continue;
		}

		DifficultyInfluence->ShowHoverRadius();
	}
}

void UWorldDifficultyInfluence::HideHoverRadiiOnActor(AActor* Actor)
{
	if (not IsValid(Actor))
	{
		return;
	}

	TArray<UWorldDifficultyInfluence*> DifficultyInfluences;
	Actor->GetComponents<UWorldDifficultyInfluence>(DifficultyInfluences);
	for (UWorldDifficultyInfluence* DifficultyInfluence : DifficultyInfluences)
	{
		if (not IsValid(DifficultyInfluence))
		{
			continue;
		}

		DifficultyInfluence->HideHoverRadius();
	}
}

void UWorldDifficultyInfluence::ShowSelectedRadiiOnActor(AActor* Actor)
{
	if (not IsValid(Actor))
	{
		return;
	}

	TArray<UWorldDifficultyInfluence*> DifficultyInfluences;
	Actor->GetComponents<UWorldDifficultyInfluence>(DifficultyInfluences);
	for (UWorldDifficultyInfluence* DifficultyInfluence : DifficultyInfluences)
	{
		if (not IsValid(DifficultyInfluence))
		{
			continue;
		}

		DifficultyInfluence->ShowSelectedRadius();
	}
}

void UWorldDifficultyInfluence::HideSelectedRadiiOnActor(AActor* Actor)
{
	if (not IsValid(Actor))
	{
		return;
	}

	TArray<UWorldDifficultyInfluence*> DifficultyInfluences;
	Actor->GetComponents<UWorldDifficultyInfluence>(DifficultyInfluences);
	for (UWorldDifficultyInfluence* DifficultyInfluence : DifficultyInfluences)
	{
		if (not IsValid(DifficultyInfluence))
		{
			continue;
		}

		DifficultyInfluence->HideSelectedRadius();
	}
}

bool UWorldDifficultyInfluence::GetIsValidRadiusPoolSubsystem()
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
			TEXT("UWorldDifficultyInfluence::GetIsValidRadiusPoolSubsystem"),
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
		TEXT("UWorldDifficultyInfluence::GetIsValidRadiusPoolSubsystem"),
		this);
	return false;
}

URTSRadiusPoolSubsystem* UWorldDifficultyInfluence::GetRadiusPoolSubsystem()
{
	if (not GetIsValidRadiusPoolSubsystem())
	{
		return nullptr;
	}

	return M_RadiusPoolSubsystem.Get();
}

int32 UWorldDifficultyInfluence::CreateRadius()
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

bool UWorldDifficultyInfluence::GetIsRadiusActive(const int32 RadiusId)
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

void UWorldDifficultyInfluence::HideRadiusById(int32& RadiusId)
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
