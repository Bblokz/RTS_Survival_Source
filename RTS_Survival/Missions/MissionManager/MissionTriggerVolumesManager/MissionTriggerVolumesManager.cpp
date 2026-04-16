#include "MissionTriggerVolumesManager.h"

#include "RTS_Survival/Missions/MissionClasses/MissionBase/MissionBase.h"
#include "RTS_Survival/Missions/TriggerAreas/Subsystem/MissionTriggerAreaSubsystem.h"
#include "RTS_Survival/Missions/TriggerAreas/TriggerArea.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UMissionTriggerVolumesManager::UMissionTriggerVolumesManager()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMissionTriggerVolumesManager::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_LoadTriggerAreaClasses();
}

ATriggerArea* UMissionTriggerVolumesManager::CreateTriggerAreaForMission(UMissionBase* Mission,
                                                                          const EMissionTriggerAreaShape TriggerAreaShape,
                                                                          const FVector& Location,
                                                                          const FRotator& Rotation,
                                                                          const FVector& Scale,
                                                                          const ETriggerOverlapLogic TriggerOverlapLogic,
                                                                          const float DelayBetweenCallbacks,
                                                                          const int32 MaxCallbacks,
                                                                          const int32 TriggerId)
{
	if (not GetIsValidMission(Mission) || not GetIsValidWorldContext() || not GetHasValidTriggerAreaClass(TriggerAreaShape))
	{
		return nullptr;
	}

	const TSubclassOf<ATriggerArea> TriggerAreaClass = TriggerAreaShape == EMissionTriggerAreaShape::Sphere
		                                                     ? M_TriggerAreaSphereClass
		                                                     : M_TriggerAreaRectangleClass;

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.Owner = GetOwner();

	ATriggerArea* TriggerArea = GetWorld()->SpawnActor<ATriggerArea>(TriggerAreaClass, Location, Rotation, SpawnParameters);
	if (not IsValid(TriggerArea))
	{
		RTSFunctionLibrary::ReportError("MissionTriggerVolumesManager failed to spawn TriggerArea actor.");
		return nullptr;
	}

	TriggerArea->ApplyScaleToCollision(Scale);
	TriggerArea->SetupOverlapLogic(TriggerOverlapLogic);
	RegisterTriggerArea(Mission, TriggerArea, TriggerId, DelayBetweenCallbacks, MaxCallbacks);
	return TriggerArea;
}

void UMissionTriggerVolumesManager::RemoveTriggerAreasForMissionById(UMissionBase* Mission, const int32 TriggerId)
{
	if (not GetIsValidMission(Mission))
	{
		return;
	}

	for (int32 RegistrationIndex = M_RegisteredTriggerAreas.Num() - 1; RegistrationIndex >= 0; --RegistrationIndex)
	{
		const FMissionTriggerAreaRegistration& Registration = M_RegisteredTriggerAreas[RegistrationIndex];
		if (Registration.M_Mission.Get() != Mission || Registration.M_TriggerId != TriggerId)
		{
			continue;
		}

		DestroyTriggerAreaAndRemoveRegistration(RegistrationIndex);
	}

	RemoveDestroyedEntries();
}

void UMissionTriggerVolumesManager::RemoveAllTriggerAreasForMission(UMissionBase* Mission)
{
	if (not GetIsValidMission(Mission))
	{
		return;
	}

	for (int32 RegistrationIndex = M_RegisteredTriggerAreas.Num() - 1; RegistrationIndex >= 0; --RegistrationIndex)
	{
		const FMissionTriggerAreaRegistration& Registration = M_RegisteredTriggerAreas[RegistrationIndex];
		if (Registration.M_Mission.Get() != Mission)
		{
			continue;
		}

		DestroyTriggerAreaAndRemoveRegistration(RegistrationIndex);
	}

	RemoveDestroyedEntries();
}

bool UMissionTriggerVolumesManager::BeginPlay_LoadTriggerAreaClasses()
{
	if (not GetIsValidWorldContext())
	{
		return false;
	}

	UMissionTriggerAreaSubsystem* TriggerAreaSubsystem = GetWorld()->GetSubsystem<UMissionTriggerAreaSubsystem>();
	if (not IsValid(TriggerAreaSubsystem))
	{
		RTSFunctionLibrary::ReportError(
			"MissionTriggerVolumesManager could not access UMissionTriggerAreaSubsystem in BeginPlay.");
		return false;
	}

	M_TriggerAreaRectangleClass = TriggerAreaSubsystem->GetRectangleTriggerAreaClass();
	M_TriggerAreaSphereClass = TriggerAreaSubsystem->GetSphereTriggerAreaClass();

	const bool bHasRectangleClass = M_TriggerAreaRectangleClass != nullptr;
	const bool bHasSphereClass = M_TriggerAreaSphereClass != nullptr;
	if (bHasRectangleClass && bHasSphereClass)
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(
		"MissionTriggerVolumesManager has missing TriggerArea classes loaded from subsystem.");
	return false;
}

bool UMissionTriggerVolumesManager::GetHasValidTriggerAreaClass(const EMissionTriggerAreaShape TriggerAreaShape) const
{
	const bool bIsSphere = TriggerAreaShape == EMissionTriggerAreaShape::Sphere;
	const bool bHasClass = bIsSphere ? M_TriggerAreaSphereClass != nullptr : M_TriggerAreaRectangleClass != nullptr;
	if (bHasClass)
	{
		return true;
	}

	const FString ShapeName = bIsSphere ? TEXT("Sphere") : TEXT("Rectangle");
	RTSFunctionLibrary::ReportError(
		"MissionTriggerVolumesManager requested trigger area class is missing for shape: " + ShapeName);
	return false;
}

bool UMissionTriggerVolumesManager::GetIsValidMission(const UMissionBase* Mission) const
{
	if (IsValid(Mission))
	{
		return true;
	}

	RTSFunctionLibrary::ReportError("MissionTriggerVolumesManager received invalid mission reference.");
	return false;
}

bool UMissionTriggerVolumesManager::GetIsValidWorldContext() const
{
	if (IsValid(GetWorld()))
	{
		return true;
	}

	RTSFunctionLibrary::ReportError("MissionTriggerVolumesManager cannot access world context.");
	return false;
}

void UMissionTriggerVolumesManager::RegisterTriggerArea(UMissionBase* Mission,
                                                        ATriggerArea* TriggerArea,
                                                        const int32 TriggerId,
                                                        const float DelayBetweenCallbacks,
                                                        const int32 MaxCallbacks)
{
	if (not GetIsValidMission(Mission) || not IsValid(TriggerArea))
	{
		return;
	}

	FMissionTriggerAreaRegistration NewRegistration;
	NewRegistration.M_Mission = Mission;
	NewRegistration.M_TriggerArea = TriggerArea;
	NewRegistration.M_TriggerId = TriggerId;
	NewRegistration.M_DelayBetweenCallbacks = FMath::Max(0.0f, DelayBetweenCallbacks);
	NewRegistration.M_LastCallbackTime = -1.0f;
	NewRegistration.M_MaxCallbacks = MaxCallbacks;
	NewRegistration.M_CallbackCount = 0;
	M_RegisteredTriggerAreas.Add(NewRegistration);

	TriggerArea->OnTriggerAreaOverlap.RemoveDynamic(this, &UMissionTriggerVolumesManager::OnTriggerAreaOverlap);
	TriggerArea->OnTriggerAreaOverlap.AddDynamic(this, &UMissionTriggerVolumesManager::OnTriggerAreaOverlap);
}

void UMissionTriggerVolumesManager::RemoveDestroyedEntries()
{
	for (int32 RegistrationIndex = M_RegisteredTriggerAreas.Num() - 1; RegistrationIndex >= 0; --RegistrationIndex)
	{
		const FMissionTriggerAreaRegistration& Registration = M_RegisteredTriggerAreas[RegistrationIndex];
		if (Registration.M_TriggerArea.IsValid() && Registration.M_Mission.IsValid())
		{
			continue;
		}

		ATriggerArea* TriggerArea = Registration.M_TriggerArea.Get();
		if (IsValid(TriggerArea))
		{
			TriggerArea->OnTriggerAreaOverlap.RemoveDynamic(this, &UMissionTriggerVolumesManager::OnTriggerAreaOverlap);
			TriggerArea->Destroy();
		}

		M_RegisteredTriggerAreas.RemoveAtSwap(RegistrationIndex);
	}
}

void UMissionTriggerVolumesManager::DestroyTriggerAreaAndRemoveRegistration(const int32 RegistrationIndex)
{
	if (not M_RegisteredTriggerAreas.IsValidIndex(RegistrationIndex))
	{
		return;
	}

	ATriggerArea* TriggerArea = M_RegisteredTriggerAreas[RegistrationIndex].M_TriggerArea.Get();
	if (IsValid(TriggerArea))
	{
		TriggerArea->OnTriggerAreaOverlap.RemoveDynamic(this, &UMissionTriggerVolumesManager::OnTriggerAreaOverlap);
		TriggerArea->Destroy();
	}

	M_RegisteredTriggerAreas.RemoveAtSwap(RegistrationIndex);
}

void UMissionTriggerVolumesManager::OnTriggerAreaOverlap(AActor* OverlappingActor, ATriggerArea* TriggerArea)
{
	if (not IsValid(OverlappingActor) || not IsValid(TriggerArea) || not GetIsValidWorldContext())
	{
		return;
	}

	for (int32 RegistrationIndex = M_RegisteredTriggerAreas.Num() - 1; RegistrationIndex >= 0; --RegistrationIndex)
	{
		FMissionTriggerAreaRegistration& Registration = M_RegisteredTriggerAreas[RegistrationIndex];
		if (Registration.M_TriggerArea.Get() != TriggerArea)
		{
			continue;
		}

		UMissionBase* Mission = Registration.M_Mission.Get();
		if (not GetIsValidMission(Mission))
		{
			DestroyTriggerAreaAndRemoveRegistration(RegistrationIndex);
			continue;
		}

		const bool bHasLimitedCallbacks = Registration.M_MaxCallbacks >= 0;
		if (bHasLimitedCallbacks && Registration.M_MaxCallbacks == 0)
		{
			DestroyTriggerAreaAndRemoveRegistration(RegistrationIndex);
			continue;
		}

		const float CurrentTimeSeconds = GetWorld()->GetTimeSeconds();
		const bool bHasPreviousCallbackTime = Registration.M_LastCallbackTime >= 0.0f;
		if (bHasPreviousCallbackTime && CurrentTimeSeconds - Registration.M_LastCallbackTime < Registration.M_DelayBetweenCallbacks)
		{
			continue;
		}

		Registration.M_LastCallbackTime = CurrentTimeSeconds;
		Registration.M_CallbackCount++;
		Mission->OnTriggerAreaCallback(OverlappingActor, Registration.M_TriggerId, TriggerArea);

		const bool bReachedCallbackLimit = bHasLimitedCallbacks && Registration.M_CallbackCount >= Registration.M_MaxCallbacks;
		if (bReachedCallbackLimit)
		{
			DestroyTriggerAreaAndRemoveRegistration(RegistrationIndex);
		}
	}

	RemoveDestroyedEntries();
}
