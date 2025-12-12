#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "FAmmoHpBarTrackerState.generated.h"

class UWeaponState;

// Weaponowner-> postInit-> 
// Logic flow of setup: UWeaponState::Init -> weaponowner -> OnWeaponadded 
// ->Set Actor with AmmoHpBarWidget. (ActorToCheckForAmmoHpBarWidget)
// THEN
// -> FAmmoTrackerState -> checkWeapon -> Set UWeapon if index matches. 
// -> Find widget on previously set actor.
// ->  UW_AmmoHealthBar::SetupAmmoTrackingLogic
USTRUCT(BlueprintType)
struct FAmmoTrackerInitSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 WeaponIndexToTrack  =INDEX_NONE;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float VerticalSliceUVRatio = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UMaterialInterface* AmmoIconMaterial = nullptr;

	bool EnsureIsValid()const;
	
};

// Make sure to set AmmoTrackerInitSettings before begin play.
// Call CheckIsWeaponToTrack if bIsUsingWeaponAmmoTracker to check whether we track a newly added weapon.
// todo there is a strange error where the ammo delegate gets unbound for spawned-during-gameplay turrets.
// todo see void ACPPTurretsMaster::BeginPlay() for a temporary workaround.
USTRUCT()
struct FAmmoHpBarTrackerState
{
	GENERATED_BODY()

	bool bIsUsingWeaponAmmoTracker = false;
	// Needs to be set at the postinit of the UObject implementing it so
	// the settings are ready once weapons get added in begin play.
	FAmmoTrackerInitSettings AmmoTrackerInitSettings;

	// Call with any new weapon; verifies 
	void CheckIsWeaponToTrack(const int32 WeaponIndex, UWeaponState* AddedWeapon);
	// Call with actor owning the widget.
	void SetActorWithAmmoWidget(AActor* HPAmmoWidgetActor);

	// Will rebind if delegate was deferred.
	void VerifyTrackingActive();

	FTimerHandle VerifyBindingTimer;

	float VerifyBindingDelay = 3.f;
	
private:
	bool EnsureActorToCheckIsValid()const;
	bool EnsureTrackWeaponIsValid()const;
	void OnWeaponFound(UWeaponState* WeaponToTrack);
	// Called once both the weapon and the actor are set.
	void SetupTrackingLogic();

	/**
     * @brief Try to locate/build the widget on a widget component and bind the ammo bar.
     * @param WidgetComp Candidate widget component (may be invalid).
     * @return true if binding succeeded immediately, false otherwise (may schedule a deferred bind).
     */
    bool TryBindAmmoBarOnWidgetComponent(UWidgetComponent* WidgetComp);
    
    /**
     * @brief Schedule a one-shot retry on next tick to bind once the widget exists/built.
     */
    void ScheduleDeferredAmmoBindNextTick();

	void Debug(const FString& DebugMessage, const FColor Color)const;
	// Needs to be set before weapons are added.
	TWeakObjectPtr<AActor> M_ActorToCheckForAmmoHpBarWidget = nullptr;

	TWeakObjectPtr<UWeaponState> M_TrackWeapon;


};
