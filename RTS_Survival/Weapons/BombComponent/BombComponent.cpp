// Copyright (C) Bas Blokzijl - All rights reserved.


#include "BombComponent.h"

#include "BombActor/BombActor.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"


UBombComponent::UBombComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}


void UBombComponent::ForceReloadAllBombsInstantly()
{
	if (!EnsureBombInstancerMeshIsValid())
	{
		return;
	}

	for (FBombBayEntry& EachEntry : BombBayData.TBombEntries) // by reference
	{
		if (!EachEntry.EnsureEntryHasSocketSet(this))
		{
			continue;
		}

		EachEntry.SetBombArmed(true);

		const int32 InstanceIdx = EachEntry.GetBombInstancerIndex();
		if (InstanceIdx == INDEX_NONE || !M_BombInstances.IsValid())
		{
			continue;
		}

		M_BombInstances->SetInstanceHidden(InstanceIdx, /*bHide=*/false);
	}
	M_ActiveEntries = BombBayData.TBombEntries.Num();
	OnMagConsumed.Broadcast(M_ActiveEntries);
}

void UBombComponent::InitBombComponent(UMeshComponent* MeshComponent, const int8 OwningPlayer)
{
	const bool bIsMeshCompValid = IsValid(MeshComponent);
	const bool bIsOwningPlayerValid = OwningPlayer > 0;
	if (not bIsOwningPlayerValid || not bIsMeshCompValid)
	{
		const FString OwnerName = GetOwner() ? GetOwner()->GetName() : TEXT("NULL OWNER");
		RTSFunctionLibrary::ReportError("Failed to Init Bomb component as:"
			"\n IsMeshCompValid = " + FString::FromInt(bIsMeshCompValid) +
			"\n IsOwningPlayerValid = " + FString::FromInt(bIsOwningPlayerValid) +
			"\n OwnerName: " + OwnerName);
		return;
	}
	if (not BombBayData.EnsureBombBayIsProperlyInitialized(GetOwner()))
	{
		RTSFunctionLibrary::ReportError(
			"Bomb bay initialization failed as the defaults are not filled in completely/correctly!");
		return;
	}
	M_OwnerMeshComponent = MeshComponent;
	BombBayData.OwningPlayer = OwningPlayer;
	OnInit_InitWeaponData();

	// Create instancer and all instances accoording to the amount of entries set in the defaults.
	OnInit_SetupAllBombEntries();
	// Set the material from the bombbay data and provide the owning aircraft for the bomb tracking widet.
	InitBombTracker();
}

bool UBombComponent::ReloadSingleBomb()
{
	if (!EnsureBombInstancerMeshIsValid())
	{
		return false;
	}

	// Arm exactly one unarmed slot
	for (FBombBayEntry& EachEntry : BombBayData.TBombEntries)
	{
		if (!EachEntry.EnsureEntryHasSocketSet(this))
		{
			continue;
		}

		if (EachEntry.IsBombArmed())
		{
			continue;
		}

		// Pool-friendly: try to ensure the pooled actor exists; if not, repair once.
		if (!IsValid(EachEntry.GetBombActor()))
		{
			EnsureBombActorForEntry(EachEntry); // optional repair path, keeps pooling semantics
		}

		EachEntry.SetBombArmed(true);

		const int32 InstanceIdx = EachEntry.GetBombInstancerIndex();
		if (InstanceIdx != INDEX_NONE && M_BombInstances.IsValid())
		{
			M_BombInstances->SetInstanceHidden(InstanceIdx, /*bHide=*/false);
			++M_ActiveEntries;
			OnMagConsumed.Broadcast(M_ActiveEntries);
		}

		break; // only one per call
	}

	// Return true only if all socket-valid entries are now armed
	for (FBombBayEntry& EachEntry : BombBayData.TBombEntries)
	{
		if (EachEntry.EnsureEntryHasSocketSet(this) && !EachEntry.IsBombArmed())
		{
			return false;
		}
	}

	return true;
}

void UBombComponent::ConfigureBombAmmoTracking(const FBombTrackerInitSettings& Settings, AActor* WidgetOwner)
{
	M_BombTrackerState.bIsUsingBombAmmoTracker = true;
	M_BombTrackerState.BombTrackerInitSettings = Settings;

	if (IsValid(WidgetOwner))
	{
		M_BombTrackerState.SetActorWithAmmoWidget(WidgetOwner);
	}

	// Order-agnostic: safe to call anytime.
	M_BombTrackerState.SetBombComponent(this);
	UWorld* World = GetWorld();
	if (World)
	{
		const TWeakObjectPtr<UBombComponent> WeakBombComponent(this);
		World->GetTimerManager().SetTimer(
			M_BombTrackerState.VerifyBindingTimer,
			FTimerDelegate::CreateLambda([WeakBombComponent]()
			{
				if (not WeakBombComponent.IsValid())
				{
					return;
				}

				WeakBombComponent->M_BombTrackerState.VerifyTrackingActive();
			}),
			M_BombTrackerState.VerifyBindingDelay, false);
	}
}

void UBombComponent::InitBombTracker()
{
	if (BombBayData.TBombEntries.IsEmpty())
	{
		return;
	}
	FBombTrackerInitSettings Settings;
	Settings.AmmoIconMaterial = BombBayData.BombIconMaterial;
	const float RatioPerBomb = 1.f / static_cast<float>(BombBayData.TBombEntries.Num());
	Settings.VerticalSliceUVRatio = RatioPerBomb;
	AActor* Owner = GetOwner();
	ConfigureBombAmmoTracking(Settings, Owner);
}


int32 UBombComponent::GetAmountBombsToReload(float& OutReloadTimePerBomb)
{
	OutReloadTimePerBomb = 0.0f;

	int32 MaxBombs = 0; // capacity via sockets
	int32 Missing = 0; // how many are currently unarmed

	for (FBombBayEntry& EachEntry : BombBayData.TBombEntries)
	{
		if (!EachEntry.EnsureEntryHasSocketSet(this))
		{
			continue;
		}

		++MaxBombs;

		if (!EachEntry.IsBombArmed())
		{
			++Missing;
		}
	}

	if (MaxBombs > 0)
	{
		const float TotalReloadSpeed = M_WeaponData.ReloadSpeed;
		if (TotalReloadSpeed > 0.f && FMath::IsFinite(TotalReloadSpeed))
		{
			OutReloadTimePerBomb = TotalReloadSpeed / static_cast<float>(MaxBombs);
		}
		else
		{
			OutReloadTimePerBomb = 0.f;
		}
	}

	return Missing;
}

void UBombComponent::OnBombKilledActor(AActor* ActorKilled)
{
}

void UBombComponent::StartThrowingBombs(AActor* TargetActor)
{
	M_TargetActor = TargetActor;
	StartBombs_InitTimer();
	// Instantly fire the first bombs.
	BombInterval();
}

void UBombComponent::StopThrowingBombs()
{
	KillBombTimer();
}

void UBombComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UBombComponent::StartBombs_InitTimer()
{
	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}
	TWeakObjectPtr<UBombComponent> WeakThis(this);
	auto BombIntervalLambda = [WeakThis]() -> void
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->BombInterval();
	};
	FTimerDelegate TimerDel;
	TimerDel.BindLambda(BombIntervalLambda);
	World->GetTimerManager().ClearTimer(BombBayData.M_BombingTimer);
	World->GetTimerManager().SetTimer(BombBayData.M_BombingTimer,
	                                  TimerDel, BombBayData.BombInterval, true);
}

void UBombComponent::KillBombTimer()
{
	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}
	World->GetTimerManager().ClearTimer(BombBayData.M_BombingTimer);
}

void UBombComponent::BombInterval()
{
	int32 RemainingToThrow = FMath::RandRange(BombBayData.BombsThrownPerInterval_Min,
	                                          BombBayData.BombsThrownPerInterval_Max);

	while (RemainingToThrow-- > 0)
	{
		FBombBayEntry* Entry = nullptr;
		if (not BombBayData.GetFirstArmedBomb(Entry))
		{
			KillBombTimer();
			return;
		}
		if (not Entry->EnsureEntryIsValid(this))
		{
			// Make sure we do not get this entry again.
			Entry->SetBombArmed(false);
			continue;
		}
		ThrowBombFromEntry(Entry);
		CreateLaunchSound();
	}
}

void UBombComponent::ThrowBombFromEntry(FBombBayEntry* Entry)
{
	if (!Entry)
	{
		return;
	}

	ABombActor* Bomb = Entry->GetBombActor();
	if (!IsValid(Bomb))
	{
		// Repair the pool entry on demand (rare path)
		Bomb = const_cast<UBombComponent*>(this)->EnsureBombActorForEntry(*Entry);
		if (!IsValid(Bomb))
		{
			return;
		}
	}

	if (!M_BombInstances.IsValid())
	{
		return;
	}

	FTransform LaunchTransform;
	if (!M_BombInstances->GetInstanceTransform(Entry->GetBombInstancerIndex(), LaunchTransform, /*bWorldSpace=*/true))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Failed to get the bomb instance transform for index ")
			+ FString::FromInt(Entry->GetBombInstancerIndex()));
		Entry->SetBombArmed(false);
		return;
	}

	M_BombInstances->SetInstanceHidden(Entry->GetBombInstancerIndex(), /*bHide=*/true);
	Bomb->ActivateBomb(LaunchTransform, M_TargetActor);
	Entry->SetBombArmed(false);
	--M_ActiveEntries;
	OnMagConsumed.Broadcast(M_ActiveEntries);
}


bool UBombComponent::EnsureOwnerMeshComponentIsValid() const
{
	if (not M_OwnerMeshComponent.IsValid())
	{
		const FString OwnerName = GetOwner() ? GetOwner()->GetName() : TEXT("NULL OWNER");
		RTSFunctionLibrary::ReportError("No valid mesh component for Bombay: " + GetName() +
			"\n With Owner: " + OwnerName
			+ "\n forgot to set it with InitBombComponent?");
		return false;
	}
	return true;
}

bool UBombComponent::CreateBombInstancerOnOwnerMesh()
{
	if (not M_OwnerMeshComponent.IsValid() || not BombBayData.BombMesh)
	{
		return false;
	}
	M_BombInstances = NewObject<URTSHidableInstancedStaticMeshComponent>(GetOwner(), TEXT("BombInstancer"));
	if (not M_BombInstances.IsValid())
	{
		RTSFunctionLibrary::ReportError("Failed to create Bomb Instancer for BombBay: " + GetName());
		return false;
	}
	M_BombInstances->AttachToComponent(M_OwnerMeshComponent.Get(), FAttachmentTransformRules::KeepRelativeTransform);
	M_BombInstances->SetStaticMesh(BombBayData.BombMesh);
	M_BombInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M_BombInstances->RegisterComponent();
	return true;
}

bool UBombComponent::EnsureBombInstancerMeshIsValid() const
{
	if (not M_BombInstances.IsValid())
	{
		FString OwnerName = GetOwner() ? GetOwner()->GetName() : TEXT("NULL OWNER");
		RTSFunctionLibrary::ReportError("No valid Bomb Instancer for BombBay: " + GetName() +
			"\n With Owner: " + OwnerName);
		return false;
	}
	return true;
}

void UBombComponent::OnInit_InitWeaponData()
{
	if (BombBayData.WeaponName == EWeaponName::DEFAULT_WEAPON)
	{
		const FString OwnerName = GetOwner() ? GetOwner()->GetName() : TEXT("NULL OWNER");
		RTSFunctionLibrary::ReportError("BombComponent: No weapon data set for BombBayData, "
			"please set the WeaponName in the BombBayData to a valid weapon name."
			"\n BombBayOwner: " + OwnerName);
		return;
	}
	ACPPGameState* RTSGameState = FRTS_Statics::GetGameState(this);
	if (not RTSGameState)
	{
		RTSFunctionLibrary::ReportError("Failed to get game state for bomb bay!");
		return;
	}
	const FWeaponData* NewData = RTSGameState->GetWeaponDataOfPlayer(BombBayData.OwningPlayer, BombBayData.WeaponName);
	if (not NewData)
	{
		RTSFunctionLibrary::ReportError("Failed to get weapon data for bomb bay component!");
		return;
	}
	M_WeaponData.CopyWeaponDataValues(NewData);
}

void UBombComponent::OnInit_SetupAllBombEntries()
{
	if (not EnsureOwnerMeshComponentIsValid() || not BombBayData.EnsureBombBayIsProperlyInitialized(GetOwner()))
	{
		return;
	}
	// Make sure we have the instancer set up on the owner mesh.
	if (not CreateBombInstancerOnOwnerMesh())
	{
		return;
	}
	for (auto& EachEntry : BombBayData.TBombEntries)
	{
		if (not EachEntry.EnsureEntryHasSocketSet(GetOwner()))
		{
			continue;
		}
		FTransform SocketCompXform = M_OwnerMeshComponent->GetSocketTransform(EachEntry.SocketName, RTS_Component);
		FVector LocCompSpace = SocketCompXform.GetLocation() + BombBayData.LocalOffsetPerBomb;
		FQuat RotCompSpace = SocketCompXform.GetRotation();

		FTransform OwnerMeshTransform = M_OwnerMeshComponent->GetComponentTransform();
		FVector WorldLoc = OwnerMeshTransform.TransformPosition(LocCompSpace);
		FQuat WorldRot = OwnerMeshTransform.TransformRotation(RotCompSpace);

		FTransform InstWorldXform(WorldRot, WorldLoc);
		const int32 InstanceRocketIndex = M_BombInstances->AddInstance(InstWorldXform, true);
		if (InstanceRocketIndex != INDEX_NONE)
		{
			EachEntry.SetBombArmed(true);
			EachEntry.SetBombInstancerIndex(InstanceRocketIndex);
			EachEntry.SetBombActor(CreateNewBomb(WorldLoc, WorldRot.Rotator()));
			EachEntry.GetBombActor()->InitBombActor(
				M_WeaponData, BombBayData.OwningPlayer,
				BombBayData.BombMesh, this,
				BombBayData.GravityForceMlt,
				BombBayData.BombStartingSpeed,
				BombBayData.BombFXSettings);
		}
	}
	OnMagConsumed.Broadcast(BombBayData.TBombEntries.Num());
	M_ActiveEntries = BombBayData.TBombEntries.Num();
}

void UBombComponent::CreateLaunchSound() const
{
	const UWorld* World = GetWorld();
	if (not BombBayData.BombFXSettings.LaunchSound || not World)
	{
		return;
	}
	const FVector Location = M_OwnerMeshComponent->GetComponentLocation();
	const FRotator Rotation = M_OwnerMeshComponent->GetComponentRotation();
	UGameplayStatics::PlaySoundAtLocation(World, BombBayData.BombFXSettings.LaunchSound, Location, Rotation, 1, 1,
	                                      0,
	                                      BombBayData.BombFXSettings.M_LaunchAttenuation,
	                                      BombBayData.BombFXSettings.M_LaunchConcurrency);
}

auto UBombComponent::CreateNewBomb(const FVector& Location, const FRotator& Rotation) const -> ABombActor*
{
	if (UWorld* World = GetWorld())
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		return World->SpawnActor<ABombActor>(Location, Rotation, SpawnParams);
	}

	return nullptr;
}

ABombActor* UBombComponent::EnsureBombActorForEntry(FBombBayEntry& Entry)
{
	if (IsValid(Entry.GetBombActor()))
	{
		return Entry.GetBombActor();
	}

	if (!EnsureBombInstancerMeshIsValid())
	{
		return nullptr;
	}
	RTSFunctionLibrary::ReportError("A bomb actor was missing for a bomb entry, repairing on demand."
		"\n See Function EnsureBombActorForEntry() in BombComponent.cpp"
		"\n BombEntry Socket: " + Entry.SocketName.ToString());

	FTransform InstWorldXf;
	if (!M_BombInstances->GetInstanceTransform(Entry.GetBombInstancerIndex(), InstWorldXf, /*bWorldSpace=*/true))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Failed to get instance transform while (re)spawning bomb actor. Index: ")
			+ FString::FromInt(Entry.GetBombInstancerIndex()));
		return nullptr;
	}

	ABombActor* NewBomb = CreateNewBomb(InstWorldXf.GetLocation(), InstWorldXf.Rotator());
	if (!NewBomb)
	{
		return nullptr;
	}

	NewBomb->InitBombActor(
		M_WeaponData,
		BombBayData.OwningPlayer,
		BombBayData.BombMesh,
		this,
		BombBayData.GravityForceMlt,
		BombBayData.BombStartingSpeed,
		BombBayData.BombFXSettings);

	Entry.SetBombActor(NewBomb);
	return NewBomb;
}
