#include "CoverProviderComponent.h"

#include "Engine/World.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorldSubsystem.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorker.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

URTSCoverProviderComponent::URTSCoverProviderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URTSCoverProviderComponent::BeginPlay()
{
	Super::BeginPlay();
	RegisterCoverPoints();
}

void URTSCoverProviderComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterCoverPoints();
	Super::EndPlay(EndPlayReason);
}

void URTSCoverProviderComponent::RegisterCoverPoints()
{
	if (M_LocalCoverPoints.IsEmpty())
	{
		return;
	}

	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (not IsValid(Owner) || not IsValid(World))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("Cover provider owner or world"),
			TEXT("RegisterCoverPoints"),
			this);
		return;
	}

	URTSCoverFinderWorldSubsystem* CoverSubsystem = World->GetSubsystem<URTSCoverFinderWorldSubsystem>();
	if (not IsValid(CoverSubsystem))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("URTSCoverFinderWorldSubsystem"),
			TEXT("RegisterCoverPoints"),
			this);
		return;
	}

	M_RegistrationId = CoverSubsystem->RegisterAuthoredCoverProvider(BuildWorldCoverPoints(*Owner));
}

void URTSCoverProviderComponent::UnregisterCoverPoints()
{
	if (M_RegistrationId == 0)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		M_RegistrationId = 0;
		return;
	}

	URTSCoverFinderWorldSubsystem* CoverSubsystem = World->GetSubsystem<URTSCoverFinderWorldSubsystem>();
	if (not IsValid(CoverSubsystem))
	{
		M_RegistrationId = 0;
		return;
	}

	CoverSubsystem->UnregisterAuthoredCoverProvider(M_RegistrationId);
	M_RegistrationId = 0;
}

TArray<FRTSCoverPoint> URTSCoverProviderComponent::BuildWorldCoverPoints(const AActor& Owner) const
{
	TArray<FRTSCoverPoint> WorldCoverPoints;
	WorldCoverPoints.Reserve(M_LocalCoverPoints.Num());
	const FTransform& OwnerTransform = Owner.GetActorTransform();
	for (const FRTSLocalCoverPoint& LocalCoverPoint : M_LocalCoverPoints)
	{
		WorldCoverPoints.Add(FCoverFinderAlgorithms::BuildWorldAuthoredPoint(OwnerTransform, LocalCoverPoint));
	}
	return WorldCoverPoints;
}
