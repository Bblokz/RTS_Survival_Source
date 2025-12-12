#include "AnimatedTextWorldSubsystem.h"

#include "Engine/World.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalText/Pooling/AnimatedTextWidgetPoolManager/AnimatedTextWidgetPoolManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UAnimatedTextWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (not IsValid(GetWorld()))
	{
		RTSFunctionLibrary::ReportError(TEXT("AnimatedTextWorldSubsystem::Initialize - World is null."));
		return;
	}

	M_PoolManager = NewObject<UAnimatedTextWidgetPoolManager>(this);
	if (not IsValid(M_PoolManager))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to create UAnimatedTextWidgetPoolManager."));
		return;
	}

	M_PoolManager->Init(GetWorld());
}

void UAnimatedTextWorldSubsystem::Deinitialize()
{
	if (IsValid(M_PoolManager))
	{
		M_PoolManager->Shutdown();
		M_PoolManager = nullptr;
	}
	Super::Deinitialize();
}

bool UAnimatedTextWorldSubsystem::GetIsValidPoolManager() const
{
	if (IsValid(M_PoolManager))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(TEXT("AnimatedTextWorldSubsystem: pool manager is not initialised."));
	return false;
}

void UAnimatedTextWorldSubsystem::Tick(float DeltaTime)
{
	if (not GetIsValidPoolManager())
	{
		return;
	}

	M_PoolManager->Tick(DeltaTime);
}

TStatId UAnimatedTextWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAnimatedTextWorldSubsystem, STATGROUP_Tickables);
}
