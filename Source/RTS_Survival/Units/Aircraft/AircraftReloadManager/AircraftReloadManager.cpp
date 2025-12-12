#include "AircraftReloadManager.h"

#include "RTS_Survival/GameUI/Pooled_TimedProgressBars/Pooling/WorldSubSystem/RTSTimedProgressBarWorldSubsystem.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void FAircraftReloadManager::OnAircraftLanded(AAircraftMaster* Aircraft)
{
	if (!EnsureAircraftIsValid(Aircraft)) return;

	M_OwnerAircraft = Aircraft;
	UWorld* World = Aircraft->GetWorld();
	if (!World) return;

	GetReloadTasks(Aircraft);

	// (optional) early out if nothing to do, before touching UI
	if (M_ReloadTasks.IsEmpty())
	{
		return;
	}

	StartReloadProgressBar(World, Aircraft->GetActorLocation());
	StartReloadTimer(World);
}


void FAircraftReloadManager::OnAircraftTakeOff(const AAircraftMaster* Aircraft)
{
	if (UWorld* World = Aircraft ? Aircraft->GetWorld() : nullptr)
	{
		StopReloadProgressBar(World);
		Reset(World);
	}
}

void FAircraftReloadManager::OnAircraftDied(const AAircraftMaster* Aircraft)
{
	if (UWorld* World = Aircraft ? Aircraft->GetWorld() : nullptr)
	{
		Reset(World);
	}
}

float FAircraftReloadManager::GetTotalReloadTimeRemaining() const
{
	float TotalTime = 0.f;
	for (int32 i = M_CurrentTaskIndex; i < M_ReloadTasks.Num(); ++i)
	{
		TotalTime += M_ReloadTasks[i].TimeToReload;
	}
	return TotalTime;
}


void FAircraftReloadManager::GetReloadTasks(const AAircraftMaster* Aircraft)
{
	M_ReloadTasks.Reset();
	M_CurrentTaskIndex = 0;

	if (not EnsureAircraftIsValid(Aircraft))
	{
		return;
	}
	if (IsValid(Aircraft->GetBombComponent()))
	{
		UBombComponent* BombComp = Aircraft->GetBombComponent();
		float ReloadTimePerBomb = 0.f;
		const int32 BombsToReload = BombComp->GetAmountBombsToReload(ReloadTimePerBomb);
		for (int32 i = 0; i < BombsToReload; ++i)
		{
			FReloadWeaponTask NewTask;
			NewTask.BombCompToReload = BombComp;
			NewTask.TimeToReload = ReloadTimePerBomb;
			M_ReloadTasks.Add(NewTask);
		}
	}
	for (auto EachWeapon : Aircraft->GetAllAircraftWeapons())
	{
		if (not IsValid(EachWeapon))
		{
			continue;
		}
		if (EachWeapon->IsWeaponFullyLoaded())
		{
			continue;
		}
		FReloadWeaponTask NewTask;
		NewTask.WeaponToReload = EachWeapon;
		NewTask.TimeToReload = EachWeapon->GetRawWeaponData().ReloadSpeed;
		M_ReloadTasks.Add(NewTask);
	}
}

bool FAircraftReloadManager::EnsureAircraftIsValid(const AAircraftMaster* Aircraft) const
{
	if (not IsValid(Aircraft))
	{
		RTSFunctionLibrary::ReportError(TEXT("FAircraftReloadManager::EnsureAircraftIsValid - Aircraft is null."));
		return false;
	}
	return true;
}

bool FAircraftReloadManager::EnsureTaskIsValid(const FReloadWeaponTask& Task) const
{
	if (!Task.WeaponToReload.IsValid() && !Task.BombCompToReload.IsValid())
	{
		RTSFunctionLibrary::ReportError(TEXT("FAircraftReloadManager::EnsureTaskIsValid - Task.WeaponToReload is null"
			"and BombCompToRealod is null!"));
		return false;
	}
	return true;
}

void FAircraftReloadManager::ReloadWeapon(const FReloadWeaponTask& Task) const
{
	if (not EnsureTaskIsValid(Task))
	{
		return;
	}
	if (Task.WeaponToReload.IsValid())
	{
		Task.WeaponToReload->ForceInstantReload();
		PlayAircraftWeaponReloadSound(false);
	}
	if (Task.BombCompToReload.IsValid())
	{
		Task.BombCompToReload->ReloadSingleBomb();
		PlayAircraftWeaponReloadSound(true);
	}
}

void FAircraftReloadManager::Reset(UWorld* World)
{
	if (World)
	{
		World->GetTimerManager().ClearTimer(M_ReloadTimerHandle);
	}
	M_ReloadTasks.Reset();
	M_CurrentTaskIndex = 0;
	M_CachedWorld.Reset();
	M_OwnerAircraft.Reset();
}

void FAircraftReloadManager::StartReloadTimer(UWorld* World)
{
	if (not World)
	{
		return;
	}
	if (M_CurrentTaskIndex >= M_ReloadTasks.Num())
	{
		World->GetTimerManager().ClearTimer(M_ReloadTimerHandle);
		return;
	}

	const FReloadWeaponTask& NextTask = M_ReloadTasks[M_CurrentTaskIndex];
	const float Delay = FMath::Max(0.f, NextTask.TimeToReload);

	M_CachedWorld = World;

	FTimerDelegate TimerDel;
	TimerDel.BindRaw(this, &FAircraftReloadManager::OnReloadTaskInterval);

	World->GetTimerManager().ClearTimer(M_ReloadTimerHandle);
	World->GetTimerManager().SetTimer(M_ReloadTimerHandle, TimerDel, Delay, /*bLoop=*/false);
}


void FAircraftReloadManager::StartReloadProgressBar(UWorld* World, const FVector& Location)
{
	if (not World)
	{
		return;
	}
	URTSTimedProgressBarWorldSubsystem* Subsystem = World->GetSubsystem<URTSTimedProgressBarWorldSubsystem>();
	if (not Subsystem)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("FAircraftReloadManager::StartReloadProgressBar - WorldSubsystem is null."));
		return;
	}
	const float TotalReloadTime = GetTotalReloadTimeRemaining();
	if (TotalReloadTime <= 0.f)
	{
		return;
	}
	FVector AdjustedLocation = Location;
	AdjustedLocation.Z += ReloadBarOffset;
	M_ProgressBarID = Subsystem->ActivateTimedProgressBar(
		0.f, TotalReloadTime, /*bUsePercentageText=*/true, ERTSProgressBarType::Default,
		/*bUseDescriptionText=*/true, "<Text_Armor>Reloading</>", AdjustedLocation, ReloadBarScaleMlt);
}

void FAircraftReloadManager::StopReloadProgressBar(UWorld* World)
{
	if (not World)
	{
		return;
	}
	if (M_ProgressBarID >= 0)
	{
		URTSTimedProgressBarWorldSubsystem* Subsystem = World->GetSubsystem<URTSTimedProgressBarWorldSubsystem>();
		if (Subsystem)
		{
			Subsystem->ForceProgressBarDormant(M_ProgressBarID);
		}
		M_ProgressBarID = -1;
	}
}

void FAircraftReloadManager::OnReloadTaskInterval()
{
	if (!M_OwnerAircraft.IsValid())
	{
		// owner vanished mid-sequence; stop safely
		if (UWorld* W = M_CachedWorld.Get())
			W->GetTimerManager().ClearTimer(M_ReloadTimerHandle);
		M_CachedWorld.Reset();
		return;
	}
	if (M_CurrentTaskIndex < M_ReloadTasks.Num())
	{
		ReloadWeapon(M_ReloadTasks[M_CurrentTaskIndex]);
		++M_CurrentTaskIndex;
	}

	if (M_CurrentTaskIndex >= M_ReloadTasks.Num())
	{
		if (UWorld* World = M_CachedWorld.Get())
		{
			World->GetTimerManager().ClearTimer(M_ReloadTimerHandle);
		}
		M_CachedWorld.Reset();
		return;
	}

	if (UWorld* World = M_CachedWorld.Get())
	{
		StartReloadTimer(World);
	}
}

void FAircraftReloadManager::PlayAircraftWeaponReloadSound(const bool bIsBomb = false) const
{
	if(not M_CachedWorld.IsValid() || not M_OwnerAircraft.IsValid())
	{
		return;
	}
	USoundBase* SoundToPlay = bIsBomb ? AircraftBombReloadedSound : AircraftWeaponReloadedSound;
	UWorld* World = M_CachedWorld.Get();
	
        UGameplayStatics::PlaySoundAtLocation(World, SoundToPlay, M_OwnerAircraft->GetActorLocation(),
                                              FRotator::ZeroRotator, 1, 1, 0,
                                             AircraftReloadSoundAttenuation,AircraftReloadSoundConcurrency);
	
}

void FAircraftReloadManager::PlayAircraftBombReloadSound()
{
}
