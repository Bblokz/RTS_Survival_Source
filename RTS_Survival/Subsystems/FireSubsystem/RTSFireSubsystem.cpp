#include "RTSFireSubsystem.h"

#include "Engine/World.h"
#include "RTS_Survival/Subsystems/FireSubsystem/FirePoolSettings/RTSFirePoolSettings.h"
#include "RTS_Survival/Subsystems/FireSubsystem/RTSFireManager/RTSFireManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

bool URTSFireSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld))
	{
		return false;
	}

	return OuterWorld->IsGameWorld();
}

void URTSFireSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (not IsValid(GetWorld()))
	{
		RTSFunctionLibrary::ReportError(TEXT("URTSFireSubsystem::Initialize - World is invalid."));
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.ObjectFlags |= RF_Transient;

	M_FireManager = GetWorld()->SpawnActor<ARTSFireManager>(ARTSFireManager::StaticClass(), FTransform::Identity,
	                                                       SpawnParameters);
	if (not GetIsValidFireManager())
	{
		return;
	}

	const URTSFirePoolSettings* Settings = GetDefault<URTSFirePoolSettings>();
	if (not IsValid(Settings))
	{
		RTSFunctionLibrary::ReportError(TEXT("URTSFireSubsystem::Initialize - Fire pool settings are invalid."));
		return;
	}

	M_FireManager->InitFireManager(Settings);
}

void URTSFireSubsystem::Deinitialize()
{
	if (not IsValid(M_FireManager))
	{
		Super::Deinitialize();
		return;
	}

	M_FireManager->Destroy();
	M_FireManager = nullptr;
	Super::Deinitialize();
}

int32 URTSFireSubsystem::SpawnFireAtLocation(const ERTSFireType FireType,
                                             const float LifeTimeSeconds,
                                             const FVector& Location,
                                             const FVector& Scale)
{
	if (not GetIsValidFireManager())
	{
		return INDEX_NONE;
	}

	return M_FireManager->ActivateFireAtLocation(FireType, LifeTimeSeconds, Location, Scale);
}

int32 URTSFireSubsystem::SpawnFireAttached(AActor* AttachActor,
                                           const ERTSFireType FireType,
                                           const float LifeTimeSeconds,
                                           const FVector& AttachOffset,
                                           const FVector& Scale)
{
	if (not GetIsValidFireManager())
	{
		return INDEX_NONE;
	}

	return M_FireManager->ActivateFireAttached(AttachActor, FireType, LifeTimeSeconds, AttachOffset, Scale);
}

bool URTSFireSubsystem::StopFireByHandle(const int32 FireHandle)
{
	if (not GetIsValidFireManager())
	{
		return false;
	}

	return M_FireManager->StopFireByHandle(FireHandle);
}

bool URTSFireSubsystem::GetIsValidFireManager() const
{
	if (not IsValid(M_FireManager))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"M_FireManager",
			"URTSFireSubsystem::GetIsValidFireManager",
			this);
		return false;
	}

	return true;
}
