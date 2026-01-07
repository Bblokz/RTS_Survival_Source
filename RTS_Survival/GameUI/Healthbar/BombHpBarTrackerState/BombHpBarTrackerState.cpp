// Copyright (C) Bas Blokzijl

#include "BombHpBarTrackerState.h"

#include "UW_BombHealthBar.h"
#include "Components/WidgetComponent.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/GameUI/Healthbar/AmmoHealthBar/UW_AmmoHealthBar.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/BombComponent/BombComponent.h"

bool FBombTrackerInitSettings::EnsureIsValid() const
{
	const bool bBadRatio = VerticalSliceUVRatio <= 0.0f || FMath::IsNearlyZero(VerticalSliceUVRatio);
	if (bBadRatio || not AmmoIconMaterial)
	{
		const FString RatioStr = FString::SanitizeFloat(VerticalSliceUVRatio);
		const FString MatStr = AmmoIconMaterial ? AmmoIconMaterial->GetName() : TEXT("NULL");
		RTSFunctionLibrary::ReportError(
			TEXT("Invalid Bomb Tracker struct:")
			TEXT("\n VerticalSliceUVRatio=") + RatioStr +
			TEXT("\n AmmoIconMaterial=") + MatStr);
		return false;
	}
	return true;
}

void FBombHpBarTrackerState::SetBombComponent(UBombComponent* InBombComponent)
{
	if (not IsValid(InBombComponent))
	{
		RTSFunctionLibrary::ReportError(TEXT("Cannot set bomb tracker: BombComponent invalid."));
		return;
	}

	M_TrackBombComponent = InBombComponent;

	if (M_ActorToCheckForAmmoHpBarWidget.IsValid())
	{
		SetupTrackingLogic();
	}
}

void FBombHpBarTrackerState::SetActorWithAmmoWidget(AActor* HPAmmoWidgetActor)
{
	if (not IsValid(HPAmmoWidgetActor))
	{
		RTSFunctionLibrary::ReportError(TEXT("Cannot setup bomb tracking: Widget actor invalid."));
		return;
	}

	M_ActorToCheckForAmmoHpBarWidget = HPAmmoWidgetActor;

	if (M_TrackBombComponent.IsValid())
	{
		SetupTrackingLogic();
	}
}

bool FBombHpBarTrackerState::EnsureActorToCheckIsValid() const
{
	if (M_ActorToCheckForAmmoHpBarWidget.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(TEXT("Bomb tracker: Actor to find widget on is not valid."));
	return false;
}

bool FBombHpBarTrackerState::EnsureTrackBombComponentIsValid() const
{
	if (M_TrackBombComponent.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(TEXT("Bomb tracker: Tracked UBombComponent is invalid."));
	return false;
}

void FBombHpBarTrackerState::SetupTrackingLogic()
{
	if (not bIsUsingBombAmmoTracker)
	{
		return;
	}

	if (not EnsureActorToCheckIsValid())
	{
		return;
	}

	if (not EnsureTrackBombComponentIsValid())
	{
		return;
	}

	if (not BombTrackerInitSettings.EnsureIsValid())
	{
		return;
	}

	TInlineComponentArray<UWidgetComponent*> WidgetComps;
	M_ActorToCheckForAmmoHpBarWidget->GetComponents(WidgetComps);

	bool bBound = false;
	for (UWidgetComponent* WidgetComp : WidgetComps)
	{
		if (TryBindAmmoBarOnWidgetComponent(WidgetComp))
		{
			bBound = true;
			break;
		}
	}

	if (not bBound)
	{
		Debug(TEXT("Bomb widget not ready; deferred binding."), FColor::Orange);
	}
}

bool FBombHpBarTrackerState::TryBindAmmoBarOnWidgetComponent(UWidgetComponent* WidgetComp)
{
	if (not IsValid(WidgetComp))
	{
		return false;
	}

	UUserWidget* WidgetGeneral = WidgetComp->GetUserWidgetObject();
	if (not WidgetGeneral)
	{
		WidgetComp->InitWidget();
		WidgetGeneral = WidgetComp->GetUserWidgetObject();
	}

	if (UW_BombHealthBar* BombBar = Cast<UW_BombHealthBar>(WidgetGeneral))
	{
		BombBar->SetTrackedBombComponent(M_TrackBombComponent.Get());
		BombBar->ConfigureAmmoIcon(BombTrackerInitSettings.AmmoIconMaterial,
		                           BombTrackerInitSettings.VerticalSliceUVRatio);
		return true;
	}

	if (not WidgetGeneral)
	{
		ScheduleDeferredAmmoBindNextTick();
	}

	return false;
}


void FBombHpBarTrackerState::ScheduleDeferredAmmoBindNextTick()
{
	if (not M_ActorToCheckForAmmoHpBarWidget.IsValid())
	{
		return;
	}

	UWorld* World = M_ActorToCheckForAmmoHpBarWidget->GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	const TWeakObjectPtr<AActor> WeakActor = M_ActorToCheckForAmmoHpBarWidget;
	const TWeakObjectPtr<UBombComponent> WeakBombComp = M_TrackBombComponent;
	const FBombTrackerInitSettings Settings = BombTrackerInitSettings;

	FTimerDelegate RetryDel = FTimerDelegate::CreateLambda([WeakActor, WeakBombComp, Settings]()
	{
		if (not WeakActor.IsValid() || not WeakBombComp.IsValid())
		{
			return;
		}

		TInlineComponentArray<UWidgetComponent*> RetryComps;
		WeakActor->GetComponents(RetryComps);

		for (UWidgetComponent* WC : RetryComps)
		{
			if (not IsValid(WC))
			{
				continue;
			}

			UUserWidget* W = WC->GetUserWidgetObject();
			if (not W)
			{
				WC->InitWidget();
				W = WC->GetUserWidgetObject();
			}

			if (UW_BombHealthBar* BombBar2 = Cast<UW_BombHealthBar>(W))
			{
				BombBar2->SetTrackedBombComponent(WeakBombComp.Get());
				BombBar2->ConfigureAmmoIcon(Settings.AmmoIconMaterial, Settings.VerticalSliceUVRatio);
				break;
			}
		}
	});

	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, RetryDel,3, false);
}

void FBombHpBarTrackerState::VerifyTrackingActive()
{
	if (not EnsureTrackBombComponentIsValid() || not EnsureActorToCheckIsValid())
	{
		return;
	}

	// If the widget was recreated or the bomb component changed, the delegate may no longer be bound.
	// We can at least detect the "no listeners" case and try to re-bind.
	if (not M_TrackBombComponent->OnMagConsumed.IsBound())
	{
		RTSFunctionLibrary::DisplayNotification(
			FText::FromString(TEXT("Bomb tracker: OnMagConsumed not bound; attempting to bind.")));
		SetupTrackingLogic();
	}
}

void FBombHpBarTrackerState::Debug(const FString& DebugMessage, const FColor Color) const
{
	if constexpr (DeveloperSettings::Debugging::GAmmoTracking_Compile_DebugSymbols)
	{
		const FString AmmoIconMaterialName = BombTrackerInitSettings.AmmoIconMaterial
			                                     ? BombTrackerInitSettings.AmmoIconMaterial->GetName()
			                                     : TEXT("NULL");
		RTSFunctionLibrary::PrintString(
			DebugMessage + TEXT("\n ") + AmmoIconMaterialName, Color);
	}
}
