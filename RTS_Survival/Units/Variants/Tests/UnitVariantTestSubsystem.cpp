// Copyright (C) Bas Blokzijl - All rights reserved.

#include "UnitVariantTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/PlayerUnitVariantManager/PlayerUnitVariantManager.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Variants/UnitVariantDataAsset.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "Slate/SlateBrushAsset.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSUnitVariantTest, Log, All);

namespace UnitVariantTestPrivate
{
	// The map boots behind the start-game widget and the profile loads the spawner overrides right after.
	constexpr float StartDelaySeconds = 3.f;
	constexpr float SpawnTimeoutSeconds = 60.f;
	constexpr int32 VariantSpawnID = 1;
	constexpr int32 BaseSpawnID = 2;
	constexpr float SpawnSpacingCm = 900.f;
	// A GC requested with ForceGarbageCollection runs at the end of the frame; check a few frames later.
	constexpr int32 FramesAfterGarbageCollection = 5;
	constexpr float GroundTraceHalfHeightCm = 50000.f;
	const TCHAR* const HullComponentName = TEXT("Hull");

	FString GetHullMeshName(const AActor* Tank)
	{
		TArray<UStaticMeshComponent*> MeshComponents;
		Tank->GetComponents(MeshComponents);
		for (const UStaticMeshComponent* MeshComponent : MeshComponents)
		{
			if (MeshComponent->GetName() != HullComponentName || not IsValid(MeshComponent->GetStaticMesh()))
			{
				continue;
			}
			return MeshComponent->GetStaticMesh()->GetName();
		}
		return TEXT("<no hull>");
	}

	EUnitVariant GetSpawnedUnitVariant(const AActor* Tank)
	{
		const URTSComponent* RTSComponent = Tank->FindComponentByClass<URTSComponent>();
		return IsValid(RTSComponent) ? RTSComponent->GetUnitVariant() : EUnitVariant::Base;
	}
}

bool UUnitVariantTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), TEXT("UnitVariantValidate"));
}

TStatId UUnitVariantTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UUnitVariantTestSubsystem, STATGROUP_Tickables);
}

void UUnitVariantTestSubsystem::Tick(const float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (M_Phase == EUnitVariantTestPhase::Inactive || not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	if (M_PhaseStartSeconds < 0.f)
	{
		int32 TankSubtypeValue = static_cast<int32>(ETankSubtype::Tank_PzIV_G);
		FParse::Value(FCommandLine::Get(), TEXT("UnitVariantTank="), TankSubtypeValue);
		M_TestedTank = FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(TankSubtypeValue));
		M_PhaseStartSeconds = World->GetTimeSeconds();
		UE_LOG(LogRTSUnitVariantTest, Display, TEXT("RTS_UNIT_VARIANT_TEST started map=%s tank=%s"),
		       *World->GetName(), *M_TestedTank.GetTrainingName());
	}
	switch (M_Phase)
	{
	case EUnitVariantTestPhase::Inactive:
		return;
	case EUnitVariantTestPhase::WaitingForStart:
		TickWaitingForStart();
		return;
	case EUnitVariantTestPhase::WaitingForGarbageCollection:
		TickWaitingForGarbageCollection();
		return;
	case EUnitVariantTestPhase::WaitingForVariantSpawn:
		TickWaitingForVariantSpawn();
		return;
	case EUnitVariantTestPhase::WaitingForBaseSpawn:
		TickWaitingForBaseSpawn();
		return;
	}
}

void UUnitVariantTestSubsystem::Check(const bool bCondition, const FString& Description, const FString& Details)
{
	M_FailedCheckCount += bCondition ? 0 : 1;
	UE_LOG(LogRTSUnitVariantTest, Display, TEXT("RTS_UNIT_VARIANT_TEST check=%s %s %s"),
	       bCondition ? TEXT("PASS") : TEXT("FAIL"), *Description, *Details);
}

void UUnitVariantTestSubsystem::EnterPhase(const EUnitVariantTestPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.f;
}

float UUnitVariantTestSubsystem::GetPhaseSeconds() const
{
	return IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() - M_PhaseStartSeconds : 0.f;
}

void UUnitVariantTestSubsystem::Finish()
{
	M_Phase = EUnitVariantTestPhase::Inactive;
	UE_LOG(LogRTSUnitVariantTest, Display, TEXT("RTS_UNIT_VARIANT_TEST RESULT %s failed_checks=%d"),
	       M_FailedCheckCount == 0 ? TEXT("PASS") : TEXT("FAIL"), M_FailedCheckCount);
	FPlatformMisc::RequestExit(false, TEXT("UnitVariantValidate"));
}

void UUnitVariantTestSubsystem::TickWaitingForStart()
{
	ACPPController* Controller = GetController();
	UPlayerStartGameControl* StartGameControl = IsValid(Controller)
		                                            ? Controller->FindComponentByClass<UPlayerStartGameControl>()
		                                            : nullptr;
	if (IsValid(StartGameControl) && StartGameControl->GetIsWaitingForPlayerToStart())
	{
		StartGameControl->PlayerStartedGame();
	}
	if (IsValid(Controller) && Controller->GetIsGameOnPause())
	{
		Controller->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
	if (GetPhaseSeconds() < UnitVariantTestPrivate::StartDelaySeconds)
	{
		return;
	}
	UPlayerUnitVariantManager* VariantManager = GetVariantManager();
	const ARTSAsyncSpawner* AsyncSpawner = GetAsyncSpawner();
	if (not VariantManager || not AsyncSpawner)
	{
		Check(false, TEXT("manager_and_spawner_found"));
		Finish();
		return;
	}
	const UUnitVariantDataAsset* VariantDataAsset = VariantManager->GetVariantDataAsset();
	M_VariantClass = VariantDataAsset->FindVariantClass(M_TestedTank, EUnitVariant::VariantA);
	Check(VariantDataAsset->GetPathName().Contains(TEXT("DA_UnitVariants")), TEXT("settings_pick_data_asset"),
	      VariantDataAsset->GetPathName());
	// A forced variant left in the asset would hide the base class; start from Base.
	Check(VariantManager->SetSelectedVariant(M_TestedTank, EUnitVariant::Base), TEXT("select_base_first"));
	M_BaseClass = AsyncSpawner->GetTrainingOptionClass(M_TestedTank);
	if (M_VariantClass.IsNull() || M_BaseClass.IsNull())
	{
		Check(false, TEXT("variant_and_base_class_known"), FString::Printf(TEXT("variant=%s base=%s"),
			      *M_VariantClass.ToString(), *M_BaseClass.ToString()));
		Finish();
		return;
	}
	Check(M_VariantClass != M_BaseClass, TEXT("variant_class_differs_from_base"),
	      FString::Printf(TEXT("variant=%s base=%s"), *M_VariantClass.ToString(), *M_BaseClass.ToString()));
	StartVariantPhase();
}

void UUnitVariantTestSubsystem::StartVariantPhase()
{
	UPlayerUnitVariantManager* VariantManager = GetVariantManager();
	const ARTSAsyncSpawner* AsyncSpawner = GetAsyncSpawner();
	Check(VariantManager->SetSelectedVariant(M_TestedTank, EUnitVariant::VariantA), TEXT("select_variant_a"));
	const TSoftClassPtr<AActor> MappedClass = AsyncSpawner->GetTrainingOptionClass(M_TestedTank);
	Check(MappedClass == M_VariantClass, TEXT("spawner_maps_variant_class"), MappedClass.ToString());

	CheckTrainingPresentation(TEXT("presentation_variant_a"));
	// The brush asset reaches the menu only through the data asset; a full GC must not take it. Not CollectGarbage
	// here: collecting in the middle of the world tick trips Niagara's data channel tick.
	GEngine->ForceGarbageCollection(true);
	M_GarbageCollectionFramesLeft = UnitVariantTestPrivate::FramesAfterGarbageCollection;
	EnterPhase(EUnitVariantTestPhase::WaitingForGarbageCollection);
}

void UUnitVariantTestSubsystem::TickWaitingForGarbageCollection()
{
	if (--M_GarbageCollectionFramesLeft > 0)
	{
		return;
	}
	CheckTrainingPresentation(TEXT("presentation_after_gc"));
	EnterPhase(EUnitVariantTestPhase::WaitingForVariantSpawn);
	Check(RequestTankSpawn(UnitVariantTestPrivate::VariantSpawnID), TEXT("variant_spawn_requested"));
}

void UUnitVariantTestSubsystem::CheckTrainingPresentation(const FString& CheckName)
{
	const FUnitVariantTrainingPresentation Presentation = GetVariantManager()->GetTrainingPresentation(M_TestedTank);
	const USlateBrushAsset* ImageBrush = Presentation.ImageBrush.Get();
	const bool bHasTextureBrush = Presentation.bOverrideImageBrush && IsValid(ImageBrush)
		&& IsValid(Cast<UTexture2D>(ImageBrush->Brush.GetResourceObject()));
	Check(Presentation.Variant == EUnitVariant::VariantA && bHasTextureBrush && Presentation.bOverrideTitle
	      && Presentation.bOverrideDescription && Presentation.bOverrideTooltip, CheckName,
	      FString::Printf(TEXT("brush=%s title=%s tooltip=%s"), IsValid(ImageBrush) ? *ImageBrush->GetName() : TEXT("none"),
	                      *Presentation.Title.ToString(), *Presentation.Tooltip.ToString()));
}

void UUnitVariantTestSubsystem::TickWaitingForVariantSpawn()
{
	if (not bM_HasVariantSpawnReturned)
	{
		if (GetPhaseSeconds() > UnitVariantTestPrivate::SpawnTimeoutSeconds)
		{
			Check(false, TEXT("variant_spawned_in_time"));
			Finish();
		}
		return;
	}
	CheckSpawnedVariant(M_SpawnedVariant.Get());
	StartBasePhase();
}

void UUnitVariantTestSubsystem::CheckSpawnedVariant(AActor* SpawnedTank)
{
	if (not IsValid(SpawnedTank))
	{
		Check(false, TEXT("variant_spawned"));
		return;
	}
	Check(SpawnedTank->GetClass() == M_VariantClass.Get(), TEXT("spawned_variant_class"),
	      SpawnedTank->GetClass()->GetName());
	Check(UnitVariantTestPrivate::GetSpawnedUnitVariant(SpawnedTank) == EUnitVariant::VariantA,
	      TEXT("spawned_variant_rts_component_variant"));
	M_VariantHullMeshName = UnitVariantTestPrivate::GetHullMeshName(SpawnedTank);
	UE_LOG(LogRTSUnitVariantTest, Display, TEXT("RTS_UNIT_VARIANT_TEST variant_hull=%s"), *M_VariantHullMeshName);
}

void UUnitVariantTestSubsystem::StartBasePhase()
{
	UPlayerUnitVariantManager* VariantManager = GetVariantManager();
	Check(VariantManager->SetSelectedVariant(M_TestedTank, EUnitVariant::Base), TEXT("select_base_again"));
	const TSoftClassPtr<AActor> MappedClass = GetAsyncSpawner()->GetTrainingOptionClass(M_TestedTank);
	Check(MappedClass == M_BaseClass, TEXT("spawner_restores_base_class"), MappedClass.ToString());
	const FUnitVariantTrainingPresentation Presentation = VariantManager->GetTrainingPresentation(M_TestedTank);
	Check(not Presentation.bOverrideImageBrush && not Presentation.bOverrideTitle, TEXT("presentation_base"));

	EnterPhase(EUnitVariantTestPhase::WaitingForBaseSpawn);
	Check(RequestTankSpawn(UnitVariantTestPrivate::BaseSpawnID), TEXT("base_spawn_requested"));
}

void UUnitVariantTestSubsystem::TickWaitingForBaseSpawn()
{
	if (not bM_HasBaseSpawnReturned)
	{
		if (GetPhaseSeconds() > UnitVariantTestPrivate::SpawnTimeoutSeconds)
		{
			Check(false, TEXT("base_spawned_in_time"));
			Finish();
		}
		return;
	}
	CheckSpawnedBase(M_SpawnedBase.Get());
	Finish();
}

void UUnitVariantTestSubsystem::CheckSpawnedBase(AActor* SpawnedTank)
{
	if (not IsValid(SpawnedTank))
	{
		Check(false, TEXT("base_spawned"));
		return;
	}
	Check(SpawnedTank->GetClass() == M_BaseClass.Get(), TEXT("spawned_base_class"), SpawnedTank->GetClass()->GetName());
	Check(UnitVariantTestPrivate::GetSpawnedUnitVariant(SpawnedTank) == EUnitVariant::Base,
	      TEXT("spawned_base_rts_component_variant"));
	const FString BaseHullMeshName = UnitVariantTestPrivate::GetHullMeshName(SpawnedTank);
	Check(BaseHullMeshName != M_VariantHullMeshName, TEXT("variant_hull_differs_from_base"),
	      FString::Printf(TEXT("variant=%s base=%s"), *M_VariantHullMeshName, *BaseHullMeshName));
	const AActor* SpawnedVariant = M_SpawnedVariant.Get();
	Check(IsValid(SpawnedVariant) && SpawnedVariant->GetClass() == M_VariantClass.Get(),
	      TEXT("existing_variant_keeps_its_class"));
}

bool UUnitVariantTestSubsystem::RequestTankSpawn(const int32 SpawnID)
{
	ARTSAsyncSpawner* AsyncSpawner = GetAsyncSpawner();
	const ACPPController* Controller = GetController();
	UWorld* World = GetWorld();
	if (not AsyncSpawner || not IsValid(Controller) || not IsValid(Controller->GetPawn()) || not IsValid(World))
	{
		return false;
	}
	FVector SpawnLocation = Controller->GetPawn()->GetActorLocation()
		+ FVector(UnitVariantTestPrivate::SpawnSpacingCm * SpawnID, 0.f, 0.f);
	FHitResult GroundHit;
	const FVector TraceOffset(0.f, 0.f, UnitVariantTestPrivate::GroundTraceHalfHeightCm);
	if (World->LineTraceSingleByChannel(GroundHit, SpawnLocation + TraceOffset, SpawnLocation - TraceOffset,
	                                    ECC_Visibility))
	{
		SpawnLocation = GroundHit.ImpactPoint;
	}
	TWeakObjectPtr<UUnitVariantTestSubsystem> WeakThis(this);
	return AsyncSpawner->AsyncSpawnOptionAtLocation(
		M_TestedTank, SpawnLocation, TWeakObjectPtr<UObject>(this), SpawnID,
		[WeakThis](const FTrainingOption&, AActor* SpawnedActor, const int32 ID)
		{
			if (not WeakThis.IsValid())
			{
				return;
			}
			WeakThis->OnTankSpawned(SpawnedActor, ID);
		}, FRotator::ZeroRotator);
}

void UUnitVariantTestSubsystem::OnTankSpawned(AActor* SpawnedActor, const int32 SpawnID)
{
	if (SpawnID == UnitVariantTestPrivate::VariantSpawnID)
	{
		M_SpawnedVariant = SpawnedActor;
		bM_HasVariantSpawnReturned = true;
		return;
	}
	M_SpawnedBase = SpawnedActor;
	bM_HasBaseSpawnReturned = true;
}

ACPPController* UUnitVariantTestSubsystem::GetController() const
{
	return FRTS_Statics::GetRTSController(GetWorld());
}

ARTSAsyncSpawner* UUnitVariantTestSubsystem::GetAsyncSpawner() const
{
	return FRTS_Statics::GetAsyncSpawner(GetWorld());
}

UPlayerUnitVariantManager* UUnitVariantTestSubsystem::GetVariantManager() const
{
	return FRTS_Statics::GetPlayerUnitVariantManager(GetWorld());
}

#else

bool UUnitVariantTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UUnitVariantTestSubsystem::Tick(const float DeltaTime)
{
}

TStatId UUnitVariantTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UUnitVariantTestSubsystem, STATGROUP_Tickables);
}

#endif
