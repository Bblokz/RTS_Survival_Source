#include "FAmmoHpBarTrackerState.h"

#include "Components/WidgetComponent.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/GameUI/Healthbar/AmmoHealthBar/UW_AmmoHealthBar.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

bool FAmmoHpBarTrackerState::EnsureActorToCheckIsValid() const
{
	if (not M_ActorToCheckForAmmoHpBarWidget.IsValid())
	{
		RTSFunctionLibrary::ReportError("actor to find widget on for tracking is not valid!!");
		return false;
	}
	return true;
}

bool FAmmoHpBarTrackerState::EnsureTrackWeaponIsValid() const
{
	if (not M_TrackWeapon.IsValid())
	{
		RTSFunctionLibrary::ReportError("Tracking weapon is invalid for AmmoTracker!");
		return false;
	}
	return true;
}

void FAmmoHpBarTrackerState::OnWeaponFound(UWeaponState* WeaponToTrack)
{
	M_TrackWeapon = WeaponToTrack;
	if (M_ActorToCheckForAmmoHpBarWidget.IsValid())
	{
		// Both weapon and actor are set;
		SetupTrackingLogic();
	}
}

void FAmmoHpBarTrackerState::SetupTrackingLogic()
{
	if (not EnsureActorToCheckIsValid())
	{
		return;
	}
	if (not EnsureTrackWeaponIsValid())
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
		Debug(TEXT("Ammo widget not ready; deferred binding."), FColor::Orange);
	}
}


bool FAmmoHpBarTrackerState::TryBindAmmoBarOnWidgetComponent(UWidgetComponent* WidgetComp)
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

	if (UW_AmmoHealthBar* AmmoBar = Cast<UW_AmmoHealthBar>(WidgetGeneral))
	{
		AmmoBar->SetTrackedWeapon(M_TrackWeapon.Get());
		AmmoBar->ConfigureAmmoIcon(AmmoTrackerInitSettings.AmmoIconMaterial,
		                           AmmoTrackerInitSettings.VerticalSliceUVRatio);
		return true;
	}

	// Still no widget object -> defer a single retry to next tick.
	if (not WidgetGeneral)
	{
		ScheduleDeferredAmmoBindNextTick();
	}

	return false;
}

void FAmmoHpBarTrackerState::ScheduleDeferredAmmoBindNextTick()
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
	const TWeakObjectPtr<UWeaponState> WeakWeapon = M_TrackWeapon;
	const FAmmoTrackerInitSettings Settings = AmmoTrackerInitSettings;

	FTimerDelegate RetryDel = FTimerDelegate::CreateLambda([WeakActor, WeakWeapon, Settings]()
	{
		if (not WeakActor.IsValid() || not WeakWeapon.IsValid())
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

			if (UW_AmmoHealthBar* AmmoBar2 = Cast<UW_AmmoHealthBar>(W))
			{
				AmmoBar2->SetTrackedWeapon(WeakWeapon.Get());
				AmmoBar2->ConfigureAmmoIcon(Settings.AmmoIconMaterial, Settings.VerticalSliceUVRatio);
				break;
			}
		}
	});

	World->GetTimerManager().SetTimerForNextTick(RetryDel);
}

bool FAmmoTrackerInitSettings::EnsureIsValid() const
{
	if (WeaponIndexToTrack == INDEX_NONE || VerticalSliceUVRatio < 0.0 || FMath::IsNearlyZero(VerticalSliceUVRatio) ||
		not AmmoIconMaterial)
	{
		const FString IndexName = FString::FromInt(WeaponIndexToTrack);
		const FString VerticalSliceUVRatioName = FString::SanitizeFloat(VerticalSliceUVRatio);
		const FString AmmoMatName = AmmoIconMaterial ? AmmoIconMaterial->GetName() : "NULL";
		RTSFunctionLibrary::ReportError("Invalid Ammo Tracker struct:"
			"\n" + IndexName +
			"\n" + VerticalSliceUVRatioName +
			"\n " + AmmoMatName);
		return false;
	}
	return true;
}

void FAmmoHpBarTrackerState::CheckIsWeaponToTrack(const int32 WeaponIndex, UWeaponState* AddedWeapon)
{
	if (!IsValid(AddedWeapon))
	{
		Debug("Invalid weapon provided to track", FColor::Red);
		return;
	}
	if (WeaponIndex == AmmoTrackerInitSettings.WeaponIndexToTrack)
	{
		Debug("Weapon to track found", FColor::Green);
		OnWeaponFound(AddedWeapon);
	}
}

void FAmmoHpBarTrackerState::SetActorWithAmmoWidget(AActor* HPAmmoWidgetActor)
{
	if (not IsValid(HPAmmoWidgetActor))
	{
		RTSFunctionLibrary::ReportError("Cannot setup ammo tracking as actor invalid");
		return;
	}
	M_ActorToCheckForAmmoHpBarWidget = HPAmmoWidgetActor;
	if (M_TrackWeapon.IsValid())
	{
		// Both weapon and actor are set; start tracking.
		SetupTrackingLogic();
	}
}

void FAmmoHpBarTrackerState::Debug(const FString& DebugMessage, const FColor Color) const
{
	if (DeveloperSettings::Debugging::GAmmoTracking_Compile_DebugSymbols)
	{
		const FString WeaponIndexName = FString::FromInt(AmmoTrackerInitSettings.WeaponIndexToTrack);
		const FString AmmoIconMaterialName = AmmoTrackerInitSettings.AmmoIconMaterial
			                                     ? AmmoTrackerInitSettings.AmmoIconMaterial->GetName()
			                                     : "NULL";
		RTSFunctionLibrary::PrintString(DebugMessage +
		                                "\n " + WeaponIndexName +
		                                "\n " + AmmoIconMaterialName, Color);
	}
}

void FAmmoHpBarTrackerState::VerifyTrackingActive()
{
	if (not EnsureTrackWeaponIsValid() || not EnsureActorToCheckIsValid())
	{
		return;
	}
	if (not M_TrackWeapon->OnMagConsumed.IsBound())
	{
		RTSFunctionLibrary::DisplayNotification(FText::FromString("Did not bind to mag consumed delegate!"
			"\n Weapon : " + M_TrackWeapon->GetName()));
		SetupTrackingLogic();
	}
}
