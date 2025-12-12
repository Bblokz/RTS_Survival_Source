#pragma once

#include "CoreMinimal.h"

#include "AircraftReloadManager.generated.h"

class UBombComponent;
class AAircraftMaster;
class UWeaponState;

// forward declare
class UWorld;

USTRUCT()
struct FReloadWeaponTask
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<class UWeaponState> WeaponToReload;
	UPROPERTY()
	TWeakObjectPtr<class UBombComponent> BombCompToReload;

	UPROPERTY()
	float TimeToReload = 0.f;
};

USTRUCT(BlueprintType)
struct FAircraftReloadManager
{
	GENERATED_BODY()

public:
	void OnAircraftLanded(class AAircraftMaster* Aircraft);
	void OnAircraftTakeOff(const class AAircraftMaster* Aircraft);
	void OnAircraftDied(const class AAircraftMaster* Aircraft);
	float GetTotalReloadTimeRemaining() const;

	// Keep only tweakables exposed
	UPROPERTY(EditDefaultsOnly, Category="Reload Manager")
	float ReloadBarOffset = 200.f;

	UPROPERTY(EditDefaultsOnly, Category="Reload Manager")
	float ReloadBarScaleMlt = 0.66f;

	UPROPERTY(EditDefaultsOnly, Category="Reload Manager|Sound")
	USoundBase* AircraftWeaponReloadedSound = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Reload Manager|Sound")
	USoundBase* AircraftBombReloadedSound = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Reload Manager|Sound")
	USoundAttenuation* AircraftReloadSoundAttenuation = nullptr;
	UPROPERTY(EditDefaultsOnly, Category="Reload Manager|Sound")
	USoundConcurrency* AircraftReloadSoundConcurrency = nullptr;

private:
	void GetReloadTasks(const class AAircraftMaster* Aircraft);
	bool EnsureAircraftIsValid(const class AAircraftMaster* Aircraft) const;
	bool EnsureTaskIsValid(const FReloadWeaponTask& Task) const;
	void ReloadWeapon(const FReloadWeaponTask& Task) const;

	// IMPORTANT: make GC/serializer aware of where the weak refs live
	UPROPERTY(Transient)
	TArray<FReloadWeaponTask> M_ReloadTasks;
	UPROPERTY(Transient)
	int32 M_CurrentTaskIndex = 0;

	// Timer handle does not need UPROPERTY
	FTimerHandle M_ReloadTimerHandle;

	void Reset(UWorld* World);
	void StartReloadTimer(UWorld* World);

	UPROPERTY(Transient)
	int32 M_ProgressBarID = -1;
	void StartReloadProgressBar(UWorld* World, const FVector& Location);
	void StopReloadProgressBar(UWorld* World);

	void OnReloadTaskInterval();
	void PlayAircraftWeaponReloadSound(const bool bIsBomb) const;
	void PlayAircraftBombReloadSound();

	// Cache & owner guards
	UPROPERTY(Transient)
	TWeakObjectPtr<UWorld> M_CachedWorld;
	UPROPERTY(Transient)
	TWeakObjectPtr<AAircraftMaster> M_OwnerAircraft;
};
