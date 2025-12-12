#include "RTSTimedProgressBarManager.h"

#include "Components/WidgetComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "RTS_Survival/GameUI/Pooled_TimedProgressBars/Pooling/RTSTimedProgressBarPoolActor.h"
#include "RTS_Survival/GameUI/Pooled_TimedProgressBars/Settings/RTSTimedProgressBarSettings.h"
#include "RTS_Survival/GameUI/Pooled_TimedProgressBars/WidgetTimedProgressBar/W_RTSTimedProgressBar.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

bool URTSTimedProgressBarManager::GetIsValidWorld() const
{
	if (IsValid(M_World))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(TEXT("URTSTimedProgressBarManager::M_World was not initialised."));
	return false;
}

bool URTSTimedProgressBarManager::GetIsValidPoolOwnerActor() const
{
	if (IsValid(M_PoolOwnerActor))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(TEXT("URTSTimedProgressBarManager::M_PoolOwnerActor was not created."));
	return false;
}

void URTSTimedProgressBarManager::Init(UWorld* InWorld)
{
	if (!IsValid(InWorld))
	{
		RTSFunctionLibrary::ReportError(TEXT("URTSTimedProgressBarManager::Init - World is null."));
		return;
	}
	M_World = InWorld;

	const URTSTimedProgressBarSettings* Settings = GetDefault<URTSTimedProgressBarSettings>();
	if (Settings == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT(
			"RTSTimedProgressBarSettings missing. Ensure DefaultEngine.ini has [/Script/RTS_Survival.RTSTimedProgressBarSettings]."));
		return;
	}

	UClass* WidgetBPClass = Settings->WidgetClass.LoadSynchronous();
	if (WidgetBPClass == nullptr)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Timed Progress Bar Widget Class is not set in Project Settings → Game → Timed Progress Bar."));
		return;
	}
	if (!WidgetBPClass->IsChildOf(UW_RTSTimedProgressBar::StaticClass()))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Timed Progress Bar Widget Class must derive from UW_RTSTimedProgressBar."));
		return;
	}

	Init_CreateOwnerActor();
	if (!GetIsValidPoolOwnerActor())
	{
		return;
	}

	const int32 PoolSize = FMath::Max(1, Settings->DefaultPoolSize);
	Init_SpawnPool(PoolSize, WidgetBPClass);
}

void URTSTimedProgressBarManager::Shutdown()
{
	for (FRTSTimedProgressBarInstance& Instance : M_Instances)
	{
		if (IsValid(Instance.Component))
		{
			Instance.Component->DestroyComponent();
		}
	}
	M_Instances.Empty();
	M_FreeList.Reset();
	M_ActiveList.Reset();

	if (IsValid(M_PoolOwnerActor))
	{
		M_PoolOwnerActor->Destroy();
		M_PoolOwnerActor = nullptr;
	}
	M_World = nullptr;
	M_NextActivationID = 1;
}

void URTSTimedProgressBarManager::Tick(const float DeltaTime)
{
	if (!GetIsValidWorld())
	{
		return;
	}
	if (M_ActiveList.Num() == 0)
	{
		return;
	}

	const float Now = M_World->GetTimeSeconds();

	// Iterate backwards to allow swap-removal on completion.
	for (int32 a = M_ActiveList.Num() - 1; a >= 0; --a)
	{
		const int32 Index = M_ActiveList[a];
		FRTSTimedProgressBarInstance& Instance = M_Instances[Index];

		const bool bStillActive = AnimateInstance(Instance, Now);
		if (!bStillActive)
		{
			ResetInstance(Instance);
			M_ActiveList.RemoveAtSwap(a, 1, EAllowShrinking::No);
			M_FreeList.Add(Index);
		}
	}
}

void URTSTimedProgressBarManager::Init_CreateOwnerActor()
{
	if (!GetIsValidWorld())
	{
		return;
	}
	if (IsValid(M_PoolOwnerActor))
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.Name = TEXT("RTSTimedProgressBarPoolOwner");
	Params.ObjectFlags = RF_Transient;

	M_PoolOwnerActor = M_World->SpawnActor<ARTSTimedProgressBarPoolActor>(
		ARTSTimedProgressBarPoolActor::StaticClass(), FTransform::Identity, Params);

	if (!IsValid(M_PoolOwnerActor))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to spawn ARTSTimedProgressBarPoolActor."));
	}
}

void URTSTimedProgressBarManager::Init_SpawnPool(const int32 PoolSize, UClass* WidgetBPClass)
{
	if (!GetIsValidPoolOwnerActor())
	{
		return;
	}

	M_Instances.SetNum(PoolSize, EAllowShrinking::No);
	M_FreeList.Reset();
	M_FreeList.Reserve(PoolSize);
	M_ActiveList.Reset();
	M_ActiveList.Reserve(PoolSize);

	for (int32 i = 0; i < PoolSize; ++i)
	{
		UWidgetComponent* Comp = NewObject<UWidgetComponent>(M_PoolOwnerActor);
		if (!IsValid(Comp))
		{
			RTSFunctionLibrary::ReportError(
				FString::Printf(TEXT("RTS ProgressBar: Failed to create UWidgetComponent for pool index %d."), i));
			continue;
		}

		// Configure BEFORE RegisterComponent
		Comp->SetWidgetSpace(EWidgetSpace::Screen);
		Comp->SetDrawAtDesiredSize(true);
		Comp->SetTwoSided(false);
		Comp->SetPivot(FVector2D(0.5f, 0.5f));
		Comp->SetVisibility(false, true);
		Comp->SetHiddenInGame(true);
		Comp->SetWidgetClass(WidgetBPClass);
		Comp->InitWidget();

		// Attach then register (default to pool owner root)
		Comp->AttachToComponent(M_PoolOwnerActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		Comp->RegisterComponent();

		UUserWidget* CreatedWidget = Comp->GetUserWidgetObject();
		UW_RTSTimedProgressBar* WidgetTyped = Cast<UW_RTSTimedProgressBar>(CreatedWidget);
		if (!WidgetTyped)
		{
			RTSFunctionLibrary::ReportError(
				FString::Printf(TEXT("Pool index %d - created widget is not UW_RTSTimedProgressBar."), i));
			Comp->DestroyComponent();
			continue;
		}

		WidgetTyped->SetDormant(true);

		FRTSTimedProgressBarInstance NewInst;
		NewInst.Component = Comp;
		NewInst.Widget = WidgetTyped;
		NewInst.bActive = false;
		NewInst.bAttached = false;
		NewInst.bUsePercentageText = false;
		NewInst.StartTime = -1.0f;
		NewInst.StartRatio = 0.0f;
		NewInst.TimeTillFull = 0.0f;
		NewInst.InvDuration = 0.0f;
		NewInst.BarType = ERTSProgressBarType::Default;
		NewInst.WorldAnchor = FVector::ZeroVector;
		NewInst.AttachedParent = nullptr;
		NewInst.AttachOffset = FVector::ZeroVector;
		NewInst.ActivationID = 0;

		M_Instances[i] = NewInst;
		M_FreeList.Add(i);
	}
}

int32 URTSTimedProgressBarManager::FindOldestActiveIndex() const
{
	int32 OldestIndex = INDEX_NONE;
	float OldestTime = TNumericLimits<float>::Max();

	for (const int32 Index : M_ActiveList)
	{
		const FRTSTimedProgressBarInstance& Inst = M_Instances[Index];
		if (!Inst.bActive || Inst.StartTime < 0.0f)
		{
			continue;
		}
		if (Inst.StartTime < OldestTime)
		{
			OldestTime = Inst.StartTime;
			OldestIndex = Index;
		}
	}
	return OldestIndex;
}

int URTSTimedProgressBarManager::ActivateTimedProgressBar(float RatioStart,
                                                          float TimeTillFull,
                                                          bool bUsePercentageText,
                                                          ERTSProgressBarType BarType,
                                                          const bool bUseDescriptionText,
                                                          const FString& InText, const FVector& Location,
                                                          const float ScaleMlt)
{
	if (M_Instances.Num() <= 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("Timed progress bar pool is empty. Did Init() fail?"));
		return 0;
	}

	// Try to allocate from Free list
	int32 Index = INDEX_NONE;
	bool bReusedOldest = false;
	if (M_FreeList.Num() > 0)
	{
		Index = M_FreeList.Pop(EAllowShrinking::No);
	}
	else
	{
		Index = FindOldestActiveIndex();
		bReusedOldest = (Index != INDEX_NONE);
		if (Index == INDEX_NONE)
		{
			RTSFunctionLibrary::ReportError(TEXT("Failed to find any instance in the progress bar pool."));
			return 0;
		}
		RTSFunctionLibrary::ReportWarning(TEXT("Progress bar pool exhausted; reusing the oldest active widget."));
		ResetInstance(M_Instances[Index]);
	}

	FRTSTimedProgressBarInstance& Inst = M_Instances[Index];
	if (!IsValid(Inst.Component) || !IsValid(Inst.Widget))
	{
		RTSFunctionLibrary::ReportError(TEXT("Corrupt pool entry (missing component or widget)."));
		return 0;
	}

	// Ensure it's attached to the pool owner (non-attached mode)
	if (Inst.bAttached || Inst.Component->GetAttachParent() != M_PoolOwnerActor->GetRootComponent())
	{
		Inst.Component->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		Inst.Component->AttachToComponent(M_PoolOwnerActor->GetRootComponent(),
		                                  FAttachmentTransformRules::KeepRelativeTransform);
	}
	Inst.bAttached = false;
	Inst.AttachedParent = nullptr;
	Inst.AttachOffset = FVector::ZeroVector;

	// Clamp & prep
	const float ClampedStart = FMath::Clamp(RatioStart, 0.0f, 1.0f);
	TimeTillFull = FMath::Max(0.0f, TimeTillFull);

	Inst.WorldAnchor = Location;
	Inst.Component->SetWorldLocation(Location);

	// Make visible before activation.
	Inst.Component->SetHiddenInGame(false);
	Inst.Component->SetVisibility(true, true);

	// Bookkeeping
	const float Now = M_World ? M_World->GetTimeSeconds() : 0.0f;

	Inst.StartTime = Now;
	Inst.TimeTillFull = TimeTillFull;
	Inst.InvDuration = (TimeTillFull > 0.0f) ? (1.0f / TimeTillFull) : 0.0f;
	Inst.StartRatio = ClampedStart;
	Inst.bUsePercentageText = bUsePercentageText;
	Inst.BarType = BarType;
	Inst.bActive = true;

	// Assign a unique activation id
	const uint64 NewID = M_NextActivationID++;
	Inst.ActivationID = NewID;

	// === NEW: apply render scale ==============================
	const float SafeScale = FMath::Max(ScaleMlt, 0.001f);
	const FVector2D NewScale = FVector2D(1.f, 1.f) * SafeScale; // multiply scalar -> 2D scale
	Inst.Widget->SetRenderScale(NewScale);
	// ==========================================================

	// Activate visuals
	Inst.Widget->ActivateProgressBar(Inst.StartRatio, Inst.bUsePercentageText, Inst.BarType, bUseDescriptionText,
	                                 InText);

	// If coming from Free list, add to Active
	if (!bReusedOldest)
	{
		M_ActiveList.Add(Index);
	}

	// Handle instant completion
	if (Inst.TimeTillFull <= 0.0f || Inst.StartRatio >= 1.0f)
	{
		Inst.Widget->UpdateProgress(1.0f, Inst.bUsePercentageText);
		ResetInstance(Inst);
		const int32 FoundIdx = M_ActiveList.Find(Index);
		if (FoundIdx != INDEX_NONE)
		{
			M_ActiveList.RemoveAtSwap(FoundIdx, 1, EAllowShrinking::No);
		}
		M_FreeList.Add(Index);
	}

	return static_cast<int>(NewID);
}


int URTSTimedProgressBarManager::ActivateTimedProgressBarAnchored(USceneComponent* AnchorComponent,
                                                                  const FVector& AttachOffset,
                                                                  float RatioStart,
                                                                  float TimeTillFull,
                                                                  bool bUsePercentageText,
                                                                  ERTSProgressBarType BarType,
                                                                  const bool bUseDescriptionText,
                                                                  const FString& InText,
                                                                  const float ScaleMlt)
{
	if (M_Instances.Num() <= 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("Timed progress bar pool is empty. Did Init() fail?"));
		return 0;
	}
	if (!IsValid(AnchorComponent))
	{
		RTSFunctionLibrary::ReportError(TEXT("ActivateTimedProgressBarAnchored: AnchorComponent is invalid."));
		return 0;
	}

	// Try to allocate from Free list
	int32 Index = INDEX_NONE;
	bool bReusedOldest = false;
	if (M_FreeList.Num() > 0)
	{
		Index = M_FreeList.Pop(EAllowShrinking::No);
	}
	else
	{
		Index = FindOldestActiveIndex();
		bReusedOldest = (Index != INDEX_NONE);
		if (Index == INDEX_NONE)
		{
			RTSFunctionLibrary::ReportError(TEXT("Failed to find any instance in the progress bar pool."));
			return 0;
		}
		RTSFunctionLibrary::ReportWarning(TEXT("Progress bar pool exhausted; reusing the oldest active widget."));
		ResetInstance(M_Instances[Index]);
	}

	FRTSTimedProgressBarInstance& Inst = M_Instances[Index];
	if (!IsValid(Inst.Component) || !IsValid(Inst.Widget))
	{
		RTSFunctionLibrary::ReportError(TEXT("Corrupt pool entry (missing component or widget)."));
		return 0;
	}

	// Attach to provided component with given offset
	Inst.Component->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	Inst.Component->AttachToComponent(AnchorComponent, FAttachmentTransformRules::KeepRelativeTransform);
	Inst.Component->SetRelativeLocation(AttachOffset);

	Inst.bAttached = true;
	Inst.AttachedParent = AnchorComponent;
	Inst.AttachOffset = AttachOffset;

	// Clamp & prep
	const float ClampedStart = FMath::Clamp(RatioStart, 0.0f, 1.0f);
	TimeTillFull = FMath::Max(0.0f, TimeTillFull);

	Inst.WorldAnchor = FVector::ZeroVector; // unused while attached

	// Make visible before activation.
	Inst.Component->SetHiddenInGame(false);
	Inst.Component->SetVisibility(true, true);

	// Bookkeeping
	const float Now = M_World ? M_World->GetTimeSeconds() : 0.0f;

	Inst.StartTime = Now;
	Inst.TimeTillFull = TimeTillFull;
	Inst.InvDuration = (TimeTillFull > 0.0f) ? (1.0f / TimeTillFull) : 0.0f;
	Inst.StartRatio = ClampedStart;
	Inst.bUsePercentageText = bUsePercentageText;
	Inst.BarType = BarType;
	Inst.bActive = true;

	const uint64 NewID = M_NextActivationID++;
	Inst.ActivationID = NewID;

	// === NEW: apply render scale ==============================
	const float SafeScale = FMath::Max(ScaleMlt, 0.001f);
	const FVector2D NewScale = FVector2D(1.f, 1.f) * SafeScale; // multiply scalar -> 2D scale
	Inst.Widget->SetRenderScale(NewScale);
	// ==========================================================

	Inst.Widget->ActivateProgressBar(Inst.StartRatio, Inst.bUsePercentageText, Inst.BarType, bUseDescriptionText, InText);

	if (!bReusedOldest)
	{
		M_ActiveList.Add(Index);
	}

	// Handle instant completion
	if (Inst.TimeTillFull <= 0.0f || Inst.StartRatio >= 1.0f)
	{
		Inst.Widget->UpdateProgress(1.0f, Inst.bUsePercentageText);
		ResetInstance(Inst);
		const int32 FoundIdx = M_ActiveList.Find(Index);
		if (FoundIdx != INDEX_NONE)
		{
			M_ActiveList.RemoveAtSwap(FoundIdx, 1, EAllowShrinking::No);
		}
		M_FreeList.Add(Index);
	}

	return static_cast<int>(NewID);
}

bool URTSTimedProgressBarManager::UpdateProgressBar(uint64 InActivationID, const FVector& NewWorldLocation)
{
	if (InActivationID == 0 || M_ActiveList.Num() == 0)
	{
		return false;
	}

	for (int32 a = M_ActiveList.Num() - 1; a >= 0; --a)
	{
		const int32 Index = M_ActiveList[a];
		FRTSTimedProgressBarInstance& Inst = M_Instances[Index];
		if (Inst.bActive && Inst.ActivationID == InActivationID)
		{
			if (Inst.bAttached)
			{
				RTSFunctionLibrary::ReportError(
					TEXT(
						"UpdateProgressBar: This progress bar is attached; updating location directly is not allowed."));
				return false;
			}

			Inst.WorldAnchor = NewWorldLocation;
			if (IsValid(Inst.Component))
			{
				Inst.Component->SetWorldLocation(NewWorldLocation);
			}
			return true;
		}
	}
	return false;
}

void URTSTimedProgressBarManager::ForceProgressBarDormant(uint64 InActivationID)
{
	if (InActivationID == 0 || M_ActiveList.Num() == 0)
	{
		return;
	}

	for (int32 a = M_ActiveList.Num() - 1; a >= 0; --a)
	{
		const int32 Index = M_ActiveList[a];
		FRTSTimedProgressBarInstance& Inst = M_Instances[Index];
		if (Inst.bActive && Inst.ActivationID == InActivationID)
		{
			ResetInstance(Inst);
			M_ActiveList.RemoveAtSwap(a, 1, EAllowShrinking::No);
			M_FreeList.Add(Index);
			return;
		}
	}
}

bool URTSTimedProgressBarManager::AnimateInstance(FRTSTimedProgressBarInstance& Instance, const float NowSeconds) const
{
	if (!ensure(Instance.Widget && Instance.Component))
	{
		return false;
	}

	// If attached, ensure the parent is still valid; if not, end it.
	if (Instance.bAttached)
	{
		if (!Instance.AttachedParent.IsValid())
		{
			RTSFunctionLibrary::ReportError(
				TEXT("Timed Progress Bar: AnchorComponent became invalid; forcing dormancy."));
			return false;
		}
		// Let attachment drive its placement; no manual SetWorldLocation here.
	}
	else
	{
		// Non-attached mode: keep the component at our world anchor (cheap, idempotent).
		Instance.Component->SetWorldLocation(Instance.WorldAnchor);
	}

	// Fill math with precomputed reciprocal
	float Ratio = Instance.StartRatio;
	if (Instance.TimeTillFull > 0.0f)
	{
		const float Elapsed = NowSeconds - Instance.StartTime;
		const float Add = Elapsed * Instance.InvDuration * (1.0f - Instance.StartRatio);
		Ratio = FMath::Clamp(Instance.StartRatio + Add, 0.0f, 1.0f);
	}
	else
	{
		Ratio = 1.0f;
	}

	Instance.Widget->UpdateProgress(Ratio, Instance.bUsePercentageText);

	// End condition: fully filled
	return (Ratio < 1.0f);
}

void URTSTimedProgressBarManager::ResetInstance(FRTSTimedProgressBarInstance& Instance)
{
	// Hide visuals & mark dormant
	if (IsValid(Instance.Component))
	{
		Instance.Component->SetVisibility(false, true);
		Instance.Component->SetHiddenInGame(true);
	}
	if (IsValid(Instance.Widget))
	{
		Instance.Widget->SetDormant(true);
	}

	// If attached, detach and re-attach to pool owner
	if (Instance.bAttached)
	{
		if (IsValid(Instance.Component))
		{
			Instance.Component->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			if (IsValid(M_PoolOwnerActor) && IsValid(M_PoolOwnerActor->GetRootComponent()))
			{
				Instance.Component->AttachToComponent(M_PoolOwnerActor->GetRootComponent(),
				                                      FAttachmentTransformRules::KeepRelativeTransform);
				Instance.Component->SetRelativeLocation(FVector::ZeroVector);
			}
		}
		Instance.bAttached = false;
		Instance.AttachedParent = nullptr;
		Instance.AttachOffset = FVector::ZeroVector;
	}

	// Reset bookkeeping
	Instance.bActive = false;
	Instance.StartTime = -1.0f;
	Instance.TimeTillFull = 0.0f;
	Instance.InvDuration = 0.0f;
	Instance.StartRatio = 0.0f;
	Instance.WorldAnchor = FVector::ZeroVector;
	Instance.ActivationID = 0;
}
