// Copyright (C) Bas Blokzijl - All rights reserved.

#include "HealthLevelDecalComponent.h"

#include "Components/DecalComponent.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace HealthLevelDecalConstants
{
	// Decals project along local X, while placement sockets conventionally expose their surface normal on local Z.
	const FRotator SocketToDecalRelativeRotation(-90.f, 0.f, 0.f);

	constexpr EHealthLevel DamageLevelsInDescendingOrder[] =
	{
		EHealthLevel::Level_75Percent,
		EHealthLevel::Level_66Percent,
		EHealthLevel::Level_50Percent,
		EHealthLevel::Level_33Percent,
		EHealthLevel::Level_25Percent
	};
}

UHealthLevelDecalComponent::UHealthLevelDecalComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UHealthLevelDecalComponent::InitializeHealthLevelDecals(UMeshComponent* InMeshComponent)
{
	ClearSpawnedDecals();
	M_EligibleSocketNames.Reset();
	M_MeshComponent = InMeshComponent;

	if (not GetIsValidMeshComponent())
	{
		return false;
	}

	if (InMeshComponent->GetOwner() != GetOwner())
	{
		RTSFunctionLibrary::ReportError(
			"The health level decal mesh must belong to the component owner."
			"\n At function: UHealthLevelDecalComponent::InitializeHealthLevelDecals");
		M_MeshComponent.Reset();
		return false;
	}

	CacheEligibleSocketNames();
	if (M_EligibleSocketNames.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(
			"No mesh sockets passed the health level decal socket filters."
			"\n At function: UHealthLevelDecalComponent::InitializeHealthLevelDecals");
		return false;
	}

	if (GetDamageLevelIndex(M_StartDecalDamageLevel) == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(
			"Start Decal Damage Level must be 75 percent or lower."
			"\n At function: UHealthLevelDecalComponent::InitializeHealthLevelDecals");
		return false;
	}

	if (not GetHasUsableDecalEntry())
	{
		RTSFunctionLibrary::ReportError(
			"Health level decals require a valid material entry with a positive weight."
			"\n At function: UHealthLevelDecalComponent::InitializeHealthLevelDecals");
		return false;
	}

	return true;
}

void UHealthLevelDecalComponent::HandleHealthLevelChanged(
	const EHealthLevel HealthLevel,
	const bool bIsHealing)
{
	if (bIsHealing)
	{
		ClearSpawnedDecals();
		return;
	}

	if (not GetIsValidMeshComponent())
	{
		return;
	}

	const int32 DesiredDecalCount = GetDesiredDecalCount(HealthLevel);
	if (DesiredDecalCount <= 0)
	{
		return;
	}

	AddDecalsUpToDesiredCount(DesiredDecalCount);
}

void UHealthLevelDecalComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearSpawnedDecals();
	Super::EndPlay(EndPlayReason);
}

bool UHealthLevelDecalComponent::GetIsValidMeshComponent() const
{
	if (M_MeshComponent.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_MeshComponent",
		"GetIsValidMeshComponent",
		this);
	return false;
}

void UHealthLevelDecalComponent::CacheEligibleSocketNames()
{
	M_EligibleSocketNames.Reset();
	if (not GetIsValidMeshComponent())
	{
		return;
	}

	const TArray<FName> SocketNames = M_MeshComponent->GetAllSocketNames();
	for (const FName SocketName : SocketNames)
	{
		if (DoesSocketPassFilters(SocketName))
		{
			M_EligibleSocketNames.AddUnique(SocketName);
		}
	}
}

bool UHealthLevelDecalComponent::DoesSocketPassFilters(const FName SocketName) const
{
	const FString SocketNameString = SocketName.ToString();
	if (not M_SocketNameContains.IsEmpty()
		&& not SocketNameString.Contains(M_SocketNameContains, ESearchCase::IgnoreCase))
	{
		return false;
	}

	if (not M_SocketNamePrefix.IsEmpty()
		&& not SocketNameString.StartsWith(M_SocketNamePrefix, ESearchCase::IgnoreCase))
	{
		return false;
	}

	return true;
}

bool UHealthLevelDecalComponent::GetHasUsableDecalEntry() const
{
	for (const FHealthLevelDecalEntry& DecalEntry : M_DecalEntries)
	{
		if (IsValid(DecalEntry.DecalMaterial) && FMath::IsFinite(DecalEntry.Weight) && DecalEntry.Weight > 0.f)
		{
			return true;
		}
	}

	return false;
}

int32 UHealthLevelDecalComponent::GetDesiredDecalCount(const EHealthLevel HealthLevel) const
{
	const int32 StartLevelIndex = GetDamageLevelIndex(M_StartDecalDamageLevel);
	const int32 CurrentLevelIndex = GetDamageLevelIndex(HealthLevel);
	if (StartLevelIndex == INDEX_NONE || CurrentLevelIndex < StartLevelIndex)
	{
		return 0;
	}

	const int64 InitialAmount = FMath::Max(0, M_AmountOfDecalsAtStartDamageLevel);
	const int64 AddedPerLevel = FMath::Max(0, M_AddedAmountOfDecalsPerSequentialHealthLevel);
	const int64 SequentialLevelsReached = CurrentLevelIndex - StartLevelIndex;
	const int64 DesiredAmount = InitialAmount + SequentialLevelsReached * AddedPerLevel;
	return static_cast<int32>(FMath::Min<int64>(DesiredAmount, M_EligibleSocketNames.Num()));
}

int32 UHealthLevelDecalComponent::GetDamageLevelIndex(const EHealthLevel HealthLevel)
{
	for (int32 DamageLevelIndex = 0;
		DamageLevelIndex < UE_ARRAY_COUNT(HealthLevelDecalConstants::DamageLevelsInDescendingOrder);
		++DamageLevelIndex)
	{
		if (HealthLevelDecalConstants::DamageLevelsInDescendingOrder[DamageLevelIndex] == HealthLevel)
		{
			return DamageLevelIndex;
		}
	}

	return INDEX_NONE;
}

void UHealthLevelDecalComponent::AddDecalsUpToDesiredCount(const int32 DesiredDecalCount)
{
	RemoveInvalidSpawnedDecals();
	int32 AmountToAdd = DesiredDecalCount - M_SpawnedDecals.Num();
	if (AmountToAdd <= 0)
	{
		return;
	}

	TArray<FName> AvailableSocketNames = GetAvailableSocketNames();
	AmountToAdd = FMath::Min(AmountToAdd, AvailableSocketNames.Num());
	for (int32 DecalNumber = 0; DecalNumber < AmountToAdd; ++DecalNumber)
	{
		const int32 DecalEntryIndex = PickWeightedDecalEntryIndex();
		if (not M_DecalEntries.IsValidIndex(DecalEntryIndex))
		{
			return;
		}

		const int32 SocketIndex = FMath::RandRange(0, AvailableSocketNames.Num() - 1);
		const FName SocketName = AvailableSocketNames[SocketIndex];
		if (SpawnDecalAtSocket(SocketName, M_DecalEntries[DecalEntryIndex]))
		{
			AvailableSocketNames.RemoveAtSwap(SocketIndex, 1, EAllowShrinking::No);
		}
	}
}

TArray<FName> UHealthLevelDecalComponent::GetAvailableSocketNames() const
{
	TArray<FName> AvailableSocketNames;
	const int32 MaximumAvailableSocketCount = FMath::Max(
		0,
		M_EligibleSocketNames.Num() - M_UsedSocketNames.Num());
	AvailableSocketNames.Reserve(MaximumAvailableSocketCount);
	for (const FName SocketName : M_EligibleSocketNames)
	{
		if (not M_UsedSocketNames.Contains(SocketName))
		{
			AvailableSocketNames.Add(SocketName);
		}
	}

	return AvailableSocketNames;
}

int32 UHealthLevelDecalComponent::PickWeightedDecalEntryIndex() const
{
	float TotalWeight = 0.f;
	int32 LastUsableIndex = INDEX_NONE;
	for (int32 DecalEntryIndex = 0; DecalEntryIndex < M_DecalEntries.Num(); ++DecalEntryIndex)
	{
		const FHealthLevelDecalEntry& DecalEntry = M_DecalEntries[DecalEntryIndex];
		if (not IsValid(DecalEntry.DecalMaterial)
			|| not FMath::IsFinite(DecalEntry.Weight)
			|| DecalEntry.Weight <= 0.f)
		{
			continue;
		}

		TotalWeight += DecalEntry.Weight;
		LastUsableIndex = DecalEntryIndex;
	}

	if (LastUsableIndex == INDEX_NONE || not FMath::IsFinite(TotalWeight) || TotalWeight <= 0.f)
	{
		return INDEX_NONE;
	}

	const float SelectionWeight = FMath::FRandRange(0.f, TotalWeight);
	float AccumulatedWeight = 0.f;
	for (int32 DecalEntryIndex = 0; DecalEntryIndex < M_DecalEntries.Num(); ++DecalEntryIndex)
	{
		const FHealthLevelDecalEntry& DecalEntry = M_DecalEntries[DecalEntryIndex];
		if (not IsValid(DecalEntry.DecalMaterial)
			|| not FMath::IsFinite(DecalEntry.Weight)
			|| DecalEntry.Weight <= 0.f)
		{
			continue;
		}

		AccumulatedWeight += DecalEntry.Weight;
		if (SelectionWeight <= AccumulatedWeight)
		{
			return DecalEntryIndex;
		}
	}

	return LastUsableIndex;
}

bool UHealthLevelDecalComponent::SpawnDecalAtSocket(
	const FName SocketName,
	const FHealthLevelDecalEntry& DecalEntry)
{
	if (not GetIsValidMeshComponent() || not M_MeshComponent->DoesSocketExist(SocketName))
	{
		return false;
	}

	AActor* Owner = GetOwner();
	if (not IsValid(Owner) || not IsValid(DecalEntry.DecalMaterial))
	{
		return false;
	}

	UDecalComponent* DecalComponent = NewObject<UDecalComponent>(Owner, NAME_None, RF_Transient);
	if (not IsValid(DecalComponent))
	{
		RTSFunctionLibrary::ReportError(
			"Failed to create a health level decal component."
			"\n At function: UHealthLevelDecalComponent::SpawnDecalAtSocket");
		return false;
	}

	const float MinSize = FMath::Max(0.f, DecalEntry.MinSize);
	const float MaxSize = FMath::Max(MinSize, DecalEntry.MaxSize);
	const float RandomSize = FMath::FRandRange(MinSize, MaxSize);
	const float ScaledSize = RandomSize * FMath::Max(0.f, M_OverallSizeMultiplier);

	DecalComponent->DecalSize = FVector(ScaledSize);
	DecalComponent->SetDecalMaterial(DecalEntry.DecalMaterial);
	Owner->AddInstanceComponent(DecalComponent);
	DecalComponent->SetupAttachment(M_MeshComponent.Get(), SocketName);
	DecalComponent->SetRelativeLocation(FVector::ZeroVector);
	DecalComponent->SetRelativeRotation(HealthLevelDecalConstants::SocketToDecalRelativeRotation);
	DecalComponent->RegisterComponent();

	M_UsedSocketNames.Add(SocketName);
	M_SpawnedDecals.Add({SocketName, DecalComponent});
	return true;
}

void UHealthLevelDecalComponent::RemoveInvalidSpawnedDecals()
{
	for (int32 DecalIndex = M_SpawnedDecals.Num() - 1; DecalIndex >= 0; --DecalIndex)
	{
		if (M_SpawnedDecals[DecalIndex].DecalComponent.IsValid())
		{
			continue;
		}

		M_UsedSocketNames.Remove(M_SpawnedDecals[DecalIndex].SocketName);
		M_SpawnedDecals.RemoveAtSwap(DecalIndex, 1, EAllowShrinking::No);
	}
}

void UHealthLevelDecalComponent::ClearSpawnedDecals()
{
	for (FSpawnedHealthLevelDecal& SpawnedDecal : M_SpawnedDecals)
	{
		if (UDecalComponent* DecalComponent = SpawnedDecal.DecalComponent.Get())
		{
			DecalComponent->DestroyComponent();
		}
	}

	M_SpawnedDecals.Reset();
	M_UsedSocketNames.Reset();
}
