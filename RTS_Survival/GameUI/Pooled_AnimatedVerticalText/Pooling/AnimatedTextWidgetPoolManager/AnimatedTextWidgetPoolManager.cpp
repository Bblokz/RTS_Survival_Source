#include "AnimatedTextWidgetPoolManager.h"

#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalText/AnimatedTextSettings/AnimatedTextSettings.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalText/Pooling/AnimatedTextPoolActor/AnimatedTextPoolActor.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"

bool UAnimatedTextWidgetPoolManager::ProjectWorldToScreen(const FVector& WorldLocation, FVector2D& OutScreen) const
{
	if (not GetIsValidWorld())
	{
		return false;
	}

	APlayerController* PC = M_World->GetFirstPlayerController();
	if (not IsValid(PC))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("AnimatedText: No PlayerController available for world-to-screen projection."));
		return false;
	}

	// Absolute screen coordinates (not viewport-relative), works with SetRenderTranslation.
	const bool bOK = PC->ProjectWorldLocationToScreen(WorldLocation, OutScreen, /*bPlayerViewportRelative*/ false);
	if (not bOK)
	{
		RTSFunctionLibrary::ReportError(TEXT("AnimatedText: ProjectWorldLocationToScreen failed."));
	}
	return bOK;
}

bool UAnimatedTextWidgetPoolManager::GetIsValidWorld() const
{
	if (IsValid(M_World))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(TEXT("UAnimatedTextWidgetPoolManager::M_World was not initialised."));
	return false;
}

void UAnimatedTextWidgetPoolManager::Init(UWorld* InWorld)
{
	if (not IsValid(InWorld))
	{
		RTSFunctionLibrary::ReportError(TEXT("UAnimatedTextWidgetPoolManager::Init - World is null."));
		return;
	}
	M_World = InWorld;

	const UAnimatedTextSettings* Settings = GetDefault<UAnimatedTextSettings>();
	if (Settings == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT(
			"AnimatedTextSettings missing. Ensure DefaultEngine.ini has [/Script/RTS_Survival.AnimatedTextSettings]."));
		return;
	}

	// Load widget classes from developer settings.
	UClass* RegularClass = Settings->WidgetClass.LoadSynchronous();
	if (RegularClass == nullptr || not RegularClass->IsChildOf(UW_RTSVerticalAnimatedText::StaticClass()))
	{
		RTSFunctionLibrary::ReportError(TEXT("Animated Text Widget Class must derive from UW_RTSVerticalAnimatedText."));
		return;
	}

	UClass* ResourceClass = Settings->ResourceWidgetClass.LoadSynchronous();
	if (ResourceClass == nullptr || not ResourceClass->IsChildOf(UW_RTSVerticalAnimatedText::StaticClass()))
	{
		RTSFunctionLibrary::ReportError(TEXT("Resource Widget Class must derive from UW_RTSVerticalAnimatedText."));
		return;
	}

	// Set debug names and classes.
	M_RegularPool.DebugName = FName(TEXT("Regular"));
	M_RegularPool.WidgetBPClass = RegularClass;

	M_ResourcePool.DebugName = FName(TEXT("Resource"));
	M_ResourcePool.WidgetBPClass = ResourceClass;

	// Create owner actors for both pools.
	Init_CreateOwnerActor(M_RegularPool, FName(TEXT("AnimatedTextPoolOwner_Regular")));
	Init_CreateOwnerActor(M_ResourcePool, FName(TEXT("AnimatedTextPoolOwner_Resource")));

	if (not IsValid(M_RegularPool.OwnerActor) || not IsValid(M_ResourcePool.OwnerActor))
	{
		return;
	}

	// Spawn both pools with their own sizes. For now use the same DefaultPoolSize for both.
	const int32 PoolSize = FMath::Max(1, Settings->DefaultPoolSize);
	Init_SpawnPool(M_RegularPool, PoolSize, RegularClass);
	Init_SpawnPool(M_ResourcePool, PoolSize, ResourceClass);
}

void UAnimatedTextWidgetPoolManager::Shutdown()
{
	// Destroy components of both pools
	for (FAnimatedTextInstance& Instance : M_RegularPool.Instances)
	{
		if (IsValid(Instance.Component))
		{
			Instance.Component->DestroyComponent();
		}
	}
	for (FAnimatedTextInstance& Instance : M_ResourcePool.Instances)
	{
		if (IsValid(Instance.Component))
		{
			Instance.Component->DestroyComponent();
		}
	}

	M_RegularPool.Instances.Empty();
	M_ResourcePool.Instances.Empty();

	M_RegularPool.FreeList.Reset();
	M_RegularPool.ActiveList.Reset();
	M_ResourcePool.FreeList.Reset();
	M_ResourcePool.ActiveList.Reset();

	if (IsValid(M_RegularPool.OwnerActor))
	{
		M_RegularPool.OwnerActor->Destroy();
		M_RegularPool.OwnerActor = nullptr;
	}
	if (IsValid(M_ResourcePool.OwnerActor))
	{
		M_ResourcePool.OwnerActor->Destroy();
		M_ResourcePool.OwnerActor = nullptr;
	}

	M_World = nullptr;
}

void UAnimatedTextWidgetPoolManager::Tick(const float DeltaTime)
{
	if (not GetIsValidWorld())
	{
		return;
	}

	const auto TickOnePool = [this](FAnimatedTextPool& Pool)
	{
		if (Pool.ActiveList.Num() == 0)
		{
			return;
		}

		const float Now = M_World->GetTimeSeconds();

		for (int32 a = Pool.ActiveList.Num() - 1; a >= 0; --a)
		{
			const int32 Index = Pool.ActiveList[a];
			FAnimatedTextInstance& Instance = Pool.Instances[Index];

			const bool bStillActive = AnimateInstance(Instance, Now);
			if (not bStillActive)
			{
				ResetInstance(Pool, Instance);

				// Move index from Active -> Free
				Pool.ActiveList.RemoveAtSwap(a, 1, EAllowShrinking::No);
				Pool.FreeList.Add(Index);
			}
		}
	};

	TickOnePool(M_RegularPool);
	TickOnePool(M_ResourcePool);
}

void UAnimatedTextWidgetPoolManager::Init_CreateOwnerActor(FAnimatedTextPool& Pool, const FName OwnerName)
{
	if (not GetIsValidWorld())
	{
		return;
	}

	if (IsValid(Pool.OwnerActor))
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.Name = OwnerName;
	Params.ObjectFlags = RF_Transient;

	Pool.OwnerActor = M_World->SpawnActor<AAnimatedTextPoolActor>(AAnimatedTextPoolActor::StaticClass(),
	                                                               FTransform::Identity, Params);
	if (not IsValid(Pool.OwnerActor))
	{
		RTSFunctionLibrary::ReportError(
			FString::Printf(TEXT("Failed to spawn AAnimatedTextPoolActor for pool '%s'."), *Pool.DebugName.ToString()));
	}
}

void UAnimatedTextWidgetPoolManager::Init_SpawnPool(FAnimatedTextPool& Pool, const int32 PoolSize, UClass* WidgetBPClass)
{
	if (not IsValid(Pool.OwnerActor))
	{
		RTSFunctionLibrary::ReportError(
			FString::Printf(TEXT("Init_SpawnPool: OwnerActor invalid for pool '%s'."), *Pool.DebugName.ToString()));
		return;
	}

	Pool.Instances.SetNum(PoolSize, EAllowShrinking::No);
	Pool.FreeList.Reset();
	Pool.FreeList.Reserve(PoolSize);
	Pool.ActiveList.Reset();
	Pool.ActiveList.Reserve(PoolSize);

	for (int32 i = 0; i < PoolSize; ++i)
	{
		UWidgetComponent* Comp = NewObject<UWidgetComponent>(Pool.OwnerActor);
		if (not IsValid(Comp))
		{
			RTSFunctionLibrary::ReportError(
				FString::Printf(TEXT("Failed to create UWidgetComponent for pool '%s' index %d."),
				                *Pool.DebugName.ToString(), i));
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

		// Attach then register (RegisterComponent will create the widget)
		Comp->AttachToComponent(Pool.OwnerActor->GetRootComponent(),
		                        FAttachmentTransformRules::KeepRelativeTransform);
		Comp->RegisterComponent();

		// After registration the widget should be created
		UUserWidget* CreatedWidget = Comp->GetUserWidgetObject();
		UW_RTSVerticalAnimatedText* WidgetTyped = Cast<UW_RTSVerticalAnimatedText>(CreatedWidget);
		if (WidgetTyped == nullptr)
		{
			RTSFunctionLibrary::ReportError(
				FString::Printf(
					TEXT("Pool '%s' index %d - created widget is not UW_RTSVerticalAnimatedText (or not created)."),
					*Pool.DebugName.ToString(), i));
			Comp->DestroyComponent();
			continue;
		}

		WidgetTyped->SetDormant(true);

		FAnimatedTextInstance NewInst;
		NewInst.Component = Comp;
		NewInst.Widget = WidgetTyped;
		NewInst.bActive = false;
		NewInst.bAttachedToActor = false;
		NewInst.StartWorldLocation = FVector::ZeroVector;
		NewInst.AttachOffset = FVector::ZeroVector;
		NewInst.ActivatedAtSeconds = -1.0f;

		Pool.Instances[i] = NewInst;
		Pool.FreeList.Add(i);
	}
}

int32 UAnimatedTextWidgetPoolManager::FindOldestActiveIndex(const FAnimatedTextPool& Pool) const
{
	int32 OldestIndex = INDEX_NONE;
	float OldestTime = TNumericLimits<float>::Max();

	for (const int32 Index : Pool.ActiveList)
	{
		const FAnimatedTextInstance& Inst = Pool.Instances[Index];
		if (not Inst.bActive || Inst.ActivatedAtSeconds < 0.0f)
		{
			continue;
		}
		if (Inst.ActivatedAtSeconds < OldestTime)
		{
			OldestTime = Inst.ActivatedAtSeconds;
			OldestIndex = Index;
		}
	}

	return OldestIndex;
}

bool UAnimatedTextWidgetPoolManager::AnimateInstance(FAnimatedTextInstance& Instance, const float NowSeconds) const
{
	// We control lifecycle; in shipping you can omit these checks for hot path
	if (not ensure(Instance.Widget && Instance.Component))
	{
		return false;
	}

	if (Instance.bAttachedToActor && not Instance.AttachedActor.IsValid())
	{
		return false;
	}

	const float Elapsed = NowSeconds - Instance.StartT;

	// Opacity: visible until VisibleEndT, then linear fade until FadeEndT
	float Opacity = 1.0f;
	if (NowSeconds >= Instance.VisibleEndT)
	{
		const float FadeElapsed = NowSeconds - Instance.VisibleEndT;
		Opacity = 1.0f - FMath::Clamp(FadeElapsed * Instance.InvFade, 0.0f, 1.0f);
	}
	Instance.Widget->SetRenderOpacity(Opacity);

	// Vertical movement over total lifetime [0..Total]
	const float Norm = FMath::Clamp(Elapsed * Instance.InvTotal, 0.0f, 1.0f);
	const float TargetZ = Instance.Settings.DeltaZ * Norm;
	const float DeltaZ = TargetZ - Instance.LastAppliedZ;
	if (not FMath::IsNearlyZero(DeltaZ))
	{
		Instance.Component->AddWorldOffset(FVector(0.0f, 0.0f, DeltaZ));
		Instance.LastAppliedZ = TargetZ;
	}

	// Still alive while before FadeEndT
	return (NowSeconds < Instance.FadeEndT);
}

void UAnimatedTextWidgetPoolManager::ResetInstance(FAnimatedTextPool& Pool, FAnimatedTextInstance& Instance) const
{
	if (IsValid(Instance.Component))
	{
		Instance.Component->SetVisibility(false, true);
		Instance.Component->SetHiddenInGame(true);

		USceneComponent* OwnerRoot = IsValid(Pool.OwnerActor) ? Pool.OwnerActor->GetRootComponent() : nullptr;
		if (IsValid(OwnerRoot))
		{
			Instance.Component->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			Instance.Component->AttachToComponent(OwnerRoot, FAttachmentTransformRules::KeepRelativeTransform);
			Instance.Component->SetRelativeLocation(FVector::ZeroVector);
		}
	}

	if (IsValid(Instance.Widget))
	{
		Instance.Widget->SetDormant(true);
	}

	Instance.AttachedActor.Reset();
	Instance.AttachOffset = FVector::ZeroVector;
	Instance.bAttachedToActor = false;
	Instance.LastAppliedZ = 0.0f;
	Instance.StartWorldLocation = FVector::ZeroVector;
	Instance.bActive = false;
	Instance.ActivatedAtSeconds = -1.0f;
}

FString UAnimatedTextWidgetPoolManager::GetSingleResourceText(const ERTSResourceType ResType,
                                                              const int32 AddOrSubtractAmount) const
{
	FString Addition = (AddOrSubtractAmount >= 0) ? TEXT("+") : TEXT("-");
	Addition.AppendInt(FMath::Abs(AddOrSubtractAmount));

	const ERTSResourceRichText TextType = (AddOrSubtractAmount >= 0)
		                                      ? ERTSResourceRichText::Blueprint
		                                      : ERTSResourceRichText::Red;

	Addition = FRTSRichTextConverter::MakeStringRTSResource(Addition, TextType);

	// Obtain the resource image.
	const FString ResourceImage = FRTSRichTextConverter::GetResourceRichImage(ResType);

	return Addition + TEXT(" ") + ResourceImage;
}

FString UAnimatedTextWidgetPoolManager::GetDoubleResourceText(
	const FRTSVerticalDoubleResourceTextSettings& ResourceSettings) const
{
	return GetSingleResourceText(ResourceSettings.ResourceType1, ResourceSettings.AddOrSubtractAmount1)
		+ TEXT("   ")
		+ GetSingleResourceText(ResourceSettings.ResourceType2, ResourceSettings.AddOrSubtractAmount2);
}

bool UAnimatedTextWidgetPoolManager::ShowAnimatedTextInPool(FAnimatedTextPool& Pool,
                                                            const FString& InText,
                                                            const FVector& InWorldStartLocation,
                                                            const bool bInAutoWrap,
                                                            const float InWrapAt,
                                                            const TEnumAsByte<ETextJustify::Type> InJustification,
                                                            const FRTSVerticalAnimTextSettings& InSettings,
                                                            AActor* InAttachActor,
                                                            const FVector& InAttachOffset)
{
	if (Pool.Instances.Num() <= 0)
	{
		RTSFunctionLibrary::ReportError(
			FString::Printf(TEXT("Animated text pool '%s' is empty. Did Init() fail?"), *Pool.DebugName.ToString()));
		return false;
	}

	// Try to allocate from Free list
	int32 Index = INDEX_NONE;
	bool bReusedOldest = false;

	if (Pool.FreeList.Num() > 0)
	{
		Index = Pool.FreeList.Pop(EAllowShrinking::No);
	}
	else
	{
		// Pool exhausted: reuse oldest active
		Index = FindOldestActiveIndex(Pool);
		bReusedOldest = (Index != INDEX_NONE);
		if (Index == INDEX_NONE)
		{
			RTSFunctionLibrary::ReportError(
				FString::Printf(TEXT("Failed to find any instance in the animated text pool '%s'."),
				                *Pool.DebugName.ToString()));
			return false;
		}

		RTSFunctionLibrary::ReportWarning(
			FString::Printf(TEXT("Animated text pool '%s' exhausted; reusing the oldest active widget."),
			                *Pool.DebugName.ToString()));

		// Visual reset but keep it logically in ActiveList
		ResetInstance(Pool, Pool.Instances[Index]);
	}

	FAnimatedTextInstance& Inst = Pool.Instances[Index];
	if (not IsValid(Inst.Component) || not IsValid(Inst.Widget))
	{
		RTSFunctionLibrary::ReportError(
			FString::Printf(TEXT("Corrupt pool entry (missing component or widget) in pool '%s'."),
			                *Pool.DebugName.ToString()));
		return false;
	}

	Inst.AttachedActor.Reset();
	Inst.AttachOffset = FVector::ZeroVector;
	Inst.bAttachedToActor = false;

	// Prepare world-space anchor and settings.
	FVector StartWorldLocation = InWorldStartLocation;
	const bool bAttachToActor = IsValid(InAttachActor);
	if (bAttachToActor)
	{
		USceneComponent* AttachRoot = InAttachActor->GetRootComponent();
		if (not IsValid(AttachRoot))
		{
			RTSFunctionLibrary::ReportError(
				FString::Printf(TEXT("Animated text pool '%s' cannot attach: actor has no valid root component."),
				                *Pool.DebugName.ToString()));
			return false;
		}

		Inst.Component->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		Inst.Component->AttachToComponent(AttachRoot, FAttachmentTransformRules::KeepRelativeTransform);
		StartWorldLocation = InAttachActor->GetActorLocation() + InAttachOffset;

		Inst.AttachedActor = InAttachActor;
		Inst.AttachOffset = InAttachOffset;
		Inst.bAttachedToActor = true;
	}
	else if (IsValid(Pool.OwnerActor))
	{
		USceneComponent* OwnerRoot = Pool.OwnerActor->GetRootComponent();
		if (IsValid(OwnerRoot))
		{
			Inst.Component->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			Inst.Component->AttachToComponent(
				OwnerRoot,
				FAttachmentTransformRules::KeepWorldTransform);
		}
	}

	Inst.StartWorldLocation = StartWorldLocation;
	Inst.Settings = InSettings;

	// Position the component at the requested world location
	if (bAttachToActor)
	{
		Inst.Component->SetRelativeLocation(InAttachOffset);
	}
	else
	{
		Inst.Component->SetWorldLocation(StartWorldLocation);
	}

	// Make it visible before activation.
	Inst.Component->SetHiddenInGame(false);
	Inst.Component->SetVisibility(true, true);

	// Reset any residual screen-space offset; we animate via world movement only.
	Inst.Widget->ApplyInitialScreenOffset(FVector2D::ZeroVector);
	Inst.Widget->ActivateAnimatedText(InText, bInAutoWrap, InWrapAt, InJustification, InSettings);

	// Bookkeeping
	Inst.bActive = true;

	const float Now = M_World ? M_World->GetTimeSeconds() : 0.0f;
	Inst.ActivatedAtSeconds = Now;

	const float Visible = FMath::Max(0.0f, Inst.Settings.VisibleDuration);
	const float Fade = FMath::Max(0.0f, Inst.Settings.FadeOutDuration);
	const float Total = Visible + Fade;

	Inst.StartT = Now;
	Inst.VisibleEndT = Now + Visible;
	Inst.FadeEndT = Now + Total;
	Inst.InvTotal = (Total > 0.0f) ? (1.0f / Total) : 0.0f;
	Inst.InvFade = (Fade > 0.0f) ? (1.0f / Fade) : 0.0f;
	Inst.LastAppliedZ = 0.0f;

	// If coming from Free list, it wasn't in Active; add it now
	if (not bReusedOldest)
	{
		Pool.ActiveList.Add(Index);
	}

	return true;
}

bool UAnimatedTextWidgetPoolManager::ShowAnimatedText(const FString& InText,
                                                      const FVector& InWorldStartLocation,
                                                      const bool bInAutoWrap,
                                                      const float InWrapAt,
                                                      const TEnumAsByte<ETextJustify::Type> InJustification,
                                                      const FRTSVerticalAnimTextSettings& InSettings)
{
	return ShowAnimatedTextInPool(M_RegularPool, InText, InWorldStartLocation, bInAutoWrap, InWrapAt,
	                              InJustification, InSettings);
}

bool UAnimatedTextWidgetPoolManager::ShowAnimatedTextAttachedToActor(const FString& InText,
                                                                     AActor* InAttachActor,
                                                                     const FVector& InAttachOffset,
                                                                     const bool bInAutoWrap,
                                                                     const float InWrapAt,
                                                                     const TEnumAsByte<ETextJustify::Type> InJustification,
                                                                     const FRTSVerticalAnimTextSettings& InSettings)
{
	if (not IsValid(InAttachActor))
	{
		return false;
	}

	if (not IsValid(InAttachActor->GetRootComponent()))
	{
		return false;
	}

	const FVector StartLocation = InAttachActor->GetActorLocation() + InAttachOffset;

	return ShowAnimatedTextInPool(M_RegularPool, InText, StartLocation, bInAutoWrap, InWrapAt,
	                              InJustification, InSettings, InAttachActor, InAttachOffset);
}

bool UAnimatedTextWidgetPoolManager::ShowSingleAnimatedResourceText(
	FRTSVerticalSingleResourceTextSettings ResourceSettings,
	const FVector& InWorldStartLocation,
	const bool bInAutoWrap,
	const float InWrapAt,
	const TEnumAsByte<ETextJustify::Type> InJustification,
	const FRTSVerticalAnimTextSettings& InSettings)
{
	// Zero changes (0) would render as "+0" or "-0" → treat as "no text".
	if (ResourceSettings.AddOrSubtractAmount == 0)
	{
		return false;
	}

	const FString TextToDisplay = GetSingleResourceText(ResourceSettings.ResourceType,
	                                                    ResourceSettings.AddOrSubtractAmount);

	return ShowAnimatedTextInPool(M_ResourcePool, TextToDisplay, InWorldStartLocation, bInAutoWrap, InWrapAt,
	                              InJustification, InSettings);
}

bool UAnimatedTextWidgetPoolManager::ShowDoubleAnimatedResourceText(
	FRTSVerticalDoubleResourceTextSettings ResourceSettings,
	const FVector& InWorldStartLocation,
	const bool bInAutoWrap,
	const float InWrapAt,
	const TEnumAsByte<ETextJustify::Type> InJustification,
	const FRTSVerticalAnimTextSettings& InSettings)
{
	// If both are zero, nothing to show. If one is zero, still show the other.
	if (ResourceSettings.AddOrSubtractAmount1 == 0 && ResourceSettings.AddOrSubtractAmount2 == 0)
	{
		return false;
	}

	const FString TextToDisplay = GetDoubleResourceText(ResourceSettings);

	return ShowAnimatedTextInPool(M_ResourcePool, TextToDisplay, InWorldStartLocation, bInAutoWrap, InWrapAt,
	                              InJustification, InSettings);
}
