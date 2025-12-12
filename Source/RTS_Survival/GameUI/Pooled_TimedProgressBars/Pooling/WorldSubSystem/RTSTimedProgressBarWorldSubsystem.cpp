#include "RTSTimedProgressBarWorldSubsystem.h"

#include "Engine/World.h"
#include "RTS_Survival/GameUI/Pooled_TimedProgressBars/Pooling/Manager/RTSTimedProgressBarManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void URTSTimedProgressBarWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (!IsValid(GetWorld()))
	{
		RTSFunctionLibrary::ReportError(TEXT("RTSTimedProgressBarWorldSubsystem::Initialize - World is null."));
		return;
	}

	M_PoolManager = NewObject<URTSTimedProgressBarManager>(this);
	if (!IsValid(M_PoolManager))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to create URTSTimedProgressBarManager."));
		return;
	}
	M_PoolManager->Init(GetWorld());
}

void URTSTimedProgressBarWorldSubsystem::Deinitialize()
{
	if (IsValid(M_PoolManager))
	{
		M_PoolManager->Shutdown();
		M_PoolManager = nullptr;
	}
	Super::Deinitialize();
}

bool URTSTimedProgressBarWorldSubsystem::GetIsValidPoolManager() const
{
	if (IsValid(M_PoolManager))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(TEXT("RTSTimedProgressBarWorldSubsystem: pool manager is not initialised."));
	return false;
}

void URTSTimedProgressBarWorldSubsystem::Tick(float DeltaTime)
{
	if (!GetIsValidPoolManager())
	{
		return;
	}
	M_PoolManager->Tick(DeltaTime);
}

TStatId URTSTimedProgressBarWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URTSTimedProgressBarWorldSubsystem, STATGROUP_Tickables);
}

int URTSTimedProgressBarWorldSubsystem::ActivateTimedProgressBar(float RatioStart,
                                                                 float TimeTillFull,
                                                                 bool bUsePercentageText,
                                                                 ERTSProgressBarType BarType,
                                                                 const bool bUseDescriptionText,
                                                                 const FString& InText,
                                                                 const FVector& Location,
                                                                 const float ScaleMlt)
{
	if (!GetIsValidPoolManager())
	{
		return 0;
	}
	return M_PoolManager->ActivateTimedProgressBar(RatioStart, TimeTillFull, bUsePercentageText, BarType,
	                                               bUseDescriptionText, InText, Location, ScaleMlt);
}


int URTSTimedProgressBarWorldSubsystem::ActivateTimedProgressBarAnchored(USceneComponent* AnchorComponent,
                                                                         const FVector AttachOffset,
                                                                         float RatioStart,
                                                                         float TimeTillFull,
                                                                         bool bUsePercentageText,
                                                                         ERTSProgressBarType BarType,
                                                                         const bool bUseDescriptionText,
                                                                         const FString& InText,
                                                                         const float ScaleMlt)
{
	if (!GetIsValidPoolManager())
	{
		return 0;
	}
	return M_PoolManager->ActivateTimedProgressBarAnchored(
		AnchorComponent, AttachOffset, RatioStart, TimeTillFull, bUsePercentageText, BarType,
		bUseDescriptionText, InText, ScaleMlt);
}


bool URTSTimedProgressBarWorldSubsystem::UpdateProgressBar(int ActivationID, const FVector& NewWorldLocation)
{
	if (!GetIsValidPoolManager())
	{
		return false;
	}
	return M_PoolManager->UpdateProgressBar(ActivationID, NewWorldLocation);
}

void URTSTimedProgressBarWorldSubsystem::ForceProgressBarDormant(int ActivationID)
{
	if (!GetIsValidPoolManager())
	{
		return;
	}
	M_PoolManager->ForceProgressBarDormant(ActivationID);
}
