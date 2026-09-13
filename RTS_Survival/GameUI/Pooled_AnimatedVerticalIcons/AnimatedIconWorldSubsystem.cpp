#include "AnimatedIconWorldSubsystem.h"

#include "AnimatedIconWidgetPoolManager.h"
#include "Engine/World.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UAnimatedIconWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	M_PoolManager = NewObject<UAnimatedIconWidgetPoolManager>(this);
}

void UAnimatedIconWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (not GetIsValidPoolManager())
	{
		return;
	}
	// Widget prewarming needs a game world ready for actors and local player widgets.
	M_PoolManager->Init(&InWorld);
}

void UAnimatedIconWorldSubsystem::Deinitialize()
{
	if (GetIsValidPoolManager())
	{
		M_PoolManager->Shutdown();
	}
	M_PoolManager = nullptr;
	Super::Deinitialize();
}

void UAnimatedIconWorldSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (not GetIsValidPoolManager())
	{
		return;
	}
	M_PoolManager->Tick();
}

bool UAnimatedIconWorldSubsystem::IsTickable() const
{
	if (IsTemplate() || not IsInitialized())
	{
		return false;
	}
	return GetIsValidPoolManager() && M_PoolManager->IsReady() && M_PoolManager->GetActiveIconCount() > 0;
}

bool UAnimatedIconWorldSubsystem::IsTickableInEditor() const
{
	return false;
}

TStatId UAnimatedIconWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAnimatedIconWorldSubsystem, STATGROUP_Tickables);
}

bool UAnimatedIconWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

UAnimatedIconWidgetPoolManager* UAnimatedIconWorldSubsystem::GetAnimatedIconWidgetPoolManager() const
{
	return GetIsValidPoolManager() ? M_PoolManager.Get() : nullptr;
}

bool UAnimatedIconWorldSubsystem::GetIsValidPoolManager() const
{
	if (IsValid(M_PoolManager))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, TEXT("M_PoolManager"),
		TEXT("GetIsValidPoolManager"), this);
	return false;
}
