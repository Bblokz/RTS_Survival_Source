// Copyright (C) Bas Blokzijl

#include "ResourceConverterComponent.h"

#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalText/Pooling/AnimatedTextWidgetPoolManager/AnimatedTextWidgetPoolManager.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalText/Pooling/WorldSubSystem/AnimatedTextWorldSubsystem.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalText/RTSVerticalAnimatedText/RTSVerticalAnimatedText.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"


// ===== ctor / UE lifecycle =====

class UAnimatedTextWorldSubsystem;

UResourceConverterComponent::UResourceConverterComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UResourceConverterComponent::BeginPlay()
{
	Super::BeginPlay();
	// Nothing implicit; InitResourceConverter must be called from BP.
}

void UResourceConverterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopResourceTimer();
	Super::EndPlay(EndPlayReason);
}


void UResourceConverterComponent::InitResourceConverter(const FResourceConverterSettings& InSettings)
{
	// Cache settings first.
	M_Settings = InSettings;

	// Cache RTS component on the owner.
	AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		RTSFunctionLibrary::ReportError(TEXT("ResourceConverter: Owner is invalid in InitResourceConverter."));
		return;
	}

	M_RTSComponent = Owner->FindComponentByClass<URTSComponent>();
	if (not GetIsValidRTSComponent())
	{
		return; // error logged in helper
	}

	// Only proceed for player 1 per requirement.
	const uint8 PlayerId = M_RTSComponent->GetOwningPlayer();
	if (PlayerId != 1)
	{
		// Do not start a timer; silently inactive for non-player-1 units.
		return;
	}
	if (not Init_SetupResourceAndPoolManagers())
	{
		return;
	}

	// Validate tick rate.
	if (M_Settings.Tick.TickRate <= 0.f)
	{
		RTSFunctionLibrary::ReportError(TEXT("ResourceConverter: TickRate must be > 0."));
		return;
	}

	// If too many entries, log once up front (we’ll still apply all; UI will clamp to two).
	if (M_Settings.Tick.ResourceDeltas.Num() > 2)
	{
		RTSFunctionLibrary::ReportError(TEXT("ResourceConverter: ResourceDeltas contains more than two entries; "
			"vertical text can only display up to two. Extras will not be shown in text."));
	}
	if (M_Settings.Tick.bStartEnabled)
	{
		SetResourceConversionEnabled(true);
	}
}

void UResourceConverterComponent::SetResourceConversionEnabled(const bool bEnabled)
{
	if (bEnabled == bM_Enabled)
	{
		return;
	}
	bM_Enabled = bEnabled;
	if (not bM_Enabled)
	{
		StopResourceTimer();
		return;
	}
	(void)StartResourceTimer();
}

// ===== timer control =====

bool UResourceConverterComponent::StartResourceTimer()
{
	if (not bM_Enabled)
	{
		return false;
	}
	if (not GetWorld())
	{
		RTSFunctionLibrary::ReportError(TEXT("ResourceConverter: GetWorld() returned null in StartResourceTimer."));
		return false;
	}
	if (M_Settings.Tick.TickRate <= 0.f)
	{
		return false;
	}

	// Clear old timer and set a new one.
	StopResourceTimer();

	TWeakObjectPtr<UResourceConverterComponent> WeakThis(this);
	FTimerDelegate Del;
	Del.BindLambda([WeakThis]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}

		UResourceConverterComponent* StrongThis = WeakThis.Get();
		StrongThis->OnResourceTick();
	});

	GetWorld()->GetTimerManager().SetTimer(
		M_TickTimerHandle,
		Del,
		M_Settings.Tick.TickRate,
		/*bLoop=*/true);

	return true;
}

void UResourceConverterComponent::StopResourceTimer()
{
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(M_TickTimerHandle);
	}
}

// ===== core tick =====

void UResourceConverterComponent::OnResourceTick()
{
	if (not bM_Enabled)
	{
		return;
	}
	if (not GetIsValidRTSComponent() || not GetIsValidPlayerResourceManager())
	{
		return;
	}

	const uint8 PlayerId = M_RTSComponent->GetOwningPlayer();
	if (PlayerId != 1)
	{
		return;
	}

	// 1) Build a positive "cost" map from our negative deltas.
	TMap<ERTSResourceType, int32> Cost;
	for (const TPair<ERTSResourceType, int32>& Pair : M_Settings.Tick.ResourceDeltas)
	{
		if (Pair.Value < 0)
		{
			Cost.Add(Pair.Key, FMath::Abs(Pair.Value));
		}
	}

	// 2) Ask PRM if we can pay this tick's cost.
	UPlayerResourceManager* const PRM = M_PlayerResourceManager.Get();
	if (not IsValid(PRM))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("M_PlayerResourceManager"),
		                                                      TEXT("OnResourceTick"), GetOwner());
		return;
	}

	const EPlayerError CanPay = PRM->GetCanPayForCost(Cost);
	if (CanPay != EPlayerError::Error_None)
	{
		// Cannot afford → skip this tick (no text/sfx).
		return;
	}

	// 3) Apply all deltas. If application fails, skip text/sfx.
	if (not TryApplyAllDeltasAtomic(PlayerId, M_Settings.Tick.ResourceDeltas))
	{
		return;
	}

	// 4) Visuals & SFX on success.
	ShowResourceTextAtOwner(M_Settings.Tick.ResourceDeltas);
	PlayTickSoundAtOwner();
}

// ===== validity helpers =====

bool UResourceConverterComponent::GetIsValidRTSComponent() const
{
	if (IsValid(M_RTSComponent))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("M_RTSComponent"),
	                                                      TEXT("GetIsValidRTSComponent"), GetOwner());
	return false;
}

bool UResourceConverterComponent::GetIsValidPlayerResourceManager() const
{
	if (M_PlayerResourceManager.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("M_PlayerResourceManager"),
	                                                      TEXT("GetIsValidPlayerResourceManager"), GetOwner());
	return false;
}

bool UResourceConverterComponent::GetIsValidAnimatedTextManager() const
{
	if (M_AnimatedTextManager.IsValid())
	{
		return true;
	}
	// This is optional; don’t spam errors every tick. Report once here, then we’ll just skip showing text.
	RTSFunctionLibrary::ReportWarning(TEXT("ResourceConverter: AnimatedTextManager is not set/valid. "
		"No vertical text will be shown."));
	return false;
}


bool UResourceConverterComponent::HasEnough(const uint8 Player, const ERTSResourceType Res,
                                            const int32 RequiredAbs) const
{
	// Delegate to GetCanPayForCost using a single-entry positive cost map.
	if (not M_PlayerResourceManager.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("M_PlayerResourceManager"),
		                                                      TEXT("HasEnough"), GetOwner());
		return false;
	}

	TMap<ERTSResourceType, int32> SingleCost;
	SingleCost.Add(Res, FMath::Max(0, RequiredAbs));
	return M_PlayerResourceManager.Get()->GetCanPayForCost(SingleCost) == EPlayerError::Error_None;
}

bool UResourceConverterComponent::ApplyDelta(const uint8 /*Player*/, const ERTSResourceType Res,
                                             const int32 Delta) const
{
	// Use PRM->AddResource for both positive (gain) and negative (cost) deltas.
	if (not M_PlayerResourceManager.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("M_PlayerResourceManager"),
		                                                      TEXT("ApplyDelta"), GetOwner());
		return false;
	}
	return M_PlayerResourceManager.Get()->AddResource(Res, Delta);
}

bool UResourceConverterComponent::TryApplyAllDeltasAtomic(
	const uint8 /*Player*/,
	const TMap<ERTSResourceType, int32>& Deltas) const
{
	if (not M_PlayerResourceManager.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("M_PlayerResourceManager"),
		                                                      TEXT("TryApplyAllDeltasAtomic"), GetOwner());
		return false;
	}

	// Deterministic order.
	TArray<ERTSResourceType> Keys;
	Deltas.GetKeys(Keys);
	Keys.Sort([](const ERTSResourceType& A, const ERTSResourceType& B)
	{
		return static_cast<uint8>(A) < static_cast<uint8>(B);
	});

	// Apply all via AddResource (negative deltas subtract).
	for (const ERTSResourceType Key : Keys)
	{
		const int32 Delta = Deltas[Key];
		if (not M_PlayerResourceManager.Get()->AddResource(Key, Delta))
		{
			RTSFunctionLibrary::ReportError(TEXT("ResourceConverter: AddResource failed while applying deltas."));
			return false;
		}
	}
	return true;
}

// ===== UI / Audio =====

void UResourceConverterComponent::ShowResourceTextAtOwner(const TMap<ERTSResourceType, int32>& AppliedDeltas) const
{
	// Optional manager; if missing, skip quietly (we already logged a warning once).
	if (not M_AnimatedTextManager.IsValid())
	{
		return;
	}

	const AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return;
	}

	// Build a display map (clamped to two entries for Resource*Text).
	TMap<ERTSResourceType, int32> DisplayMap = AppliedDeltas;
	ClampMapForDisplay(DisplayMap);

	TArray<ERTSResourceType> Keys;
	DisplayMap.GetKeys(Keys);

	const FVector WorldLoc = Owner->GetActorLocation() + M_Settings.VerticalText.TextOffset;

	// Single or double resource text using your pooled manager API.  
	if (Keys.Num() == 1)
	{
		FRTSVerticalSingleResourceTextSettings S;
		S.ResourceType = Keys[0];
		S.AddOrSubtractAmount = DisplayMap[Keys[0]];

		(void)M_AnimatedTextManager->ShowSingleAnimatedResourceText(
			S,
			WorldLoc,
			/*bAutoWrap*/ M_Settings.VerticalText.bAutoWrap,
			M_Settings.VerticalText.InWrapAt,
			M_Settings.VerticalText.InJustification,
			M_Settings.VerticalText.InSettings // timings/delta Z  
		);
	}
	else if (Keys.Num() >= 2)
	{
		FRTSVerticalDoubleResourceTextSettings D;
		D.ResourceType1 = Keys[0];
		D.AddOrSubtractAmount1 = DisplayMap[Keys[0]];
		D.ResourceType2 = Keys[1];
		D.AddOrSubtractAmount2 = DisplayMap[Keys[1]];

		(void)M_AnimatedTextManager->ShowDoubleAnimatedResourceText(
			D,
			WorldLoc,
			/*bAutoWrap*/ M_Settings.VerticalText.bAutoWrap,
			M_Settings.VerticalText.InWrapAt,
			M_Settings.VerticalText.InJustification,
			M_Settings.VerticalText.InSettings
		);
	}
}

void UResourceConverterComponent::PlayTickSoundAtOwner() const
{
	if (not IsValid(M_Settings.OnTickSound))
	{
		return;
	}
	const AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return;
	}
	UGameplayStatics::PlaySoundAtLocation(
		this,
		M_Settings.OnTickSound,
		Owner->GetActorLocation() + M_Settings.VerticalText.TextOffset,
		1.f, 1.f, 0.f,
		M_Settings.OnTickAttenuation,
		M_Settings.OnTickConcurrency
	);
}

// ===== utilities =====

void UResourceConverterComponent::ClampMapForDisplay(TMap<ERTSResourceType, int32>& InOutMap)
{
	if (InOutMap.Num() <= 2)
	{
		return;
	}

	// Deterministic: keep two lowest enum keys.
	TArray<ERTSResourceType> Keys;
	InOutMap.GetKeys(Keys);
	Keys.Sort([](const ERTSResourceType& A, const ERTSResourceType& B)
	{
		return static_cast<uint8>(A) < static_cast<uint8>(B);
	});

	// Preserve first two; remove the rest.
	for (int32 i = 2; i < Keys.Num(); ++i)
	{
		InOutMap.Remove(Keys[i]);
	}
}

bool UResourceConverterComponent::Init_SetupResourceAndPoolManagers()
{
	M_PlayerResourceManager = FRTS_Statics::GetPlayerResourceManager(this);
	const bool bSuccess = GetIsValidPlayerResourceManager();
	const bool bAnimTextSuccess = GetAnimTextMgrFromWorld();
	return bSuccess && bAnimTextSuccess;
}

bool UResourceConverterComponent::GetAnimTextMgrFromWorld()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("ResourceConverter: GetWorld() invalid while loading AnimatedTextManager."));
		return false;
	}

	// Retrieve the subsystem that owns the animated text pool.
	UAnimatedTextWorldSubsystem* const Subsystem = World->GetSubsystem<UAnimatedTextWorldSubsystem>();
	if (not IsValid(Subsystem))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("ResourceConverter: Failed to get AnimatedTextWorldSubsystem from world."));
		return false;
	}

	UAnimatedTextWidgetPoolManager* const PoolManager = Subsystem->GetAnimatedTextWidgetPoolManager();
	if (not IsValid(PoolManager))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("ResourceConverter: AnimatedTextWidgetPoolManager is invalid on subsystem."));
		return false;
	}

	M_AnimatedTextManager = PoolManager;
	return true;
}
