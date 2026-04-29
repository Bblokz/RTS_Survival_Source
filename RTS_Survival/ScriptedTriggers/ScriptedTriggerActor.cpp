#include "ScriptedTriggerActor.h"

#include "Engine/World.h"
#include "RTS_Survival/Missions/TriggerAreas/TriggerArea.h"
#include "RTS_Survival/Player/PortraitManager/PortraitManager.h"
#include "RTS_Survival/ScriptedTriggers/Subsystem/ScriptedTriggerAreaSubsystem.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

AScriptedTriggerActor::AScriptedTriggerActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AScriptedTriggerActor::BeginPlay()
{
	Super::BeginPlay();

	LoadTriggerAreaClasses();
}

void AScriptedTriggerActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveAllTriggerAreas();
	bM_IsProcessingTriggerAreaCallbacks = false;
	FlushPendingTriggerAreaChanges();

	Super::EndPlay(EndPlayReason);
}

ATriggerArea* AScriptedTriggerActor::CreateTriggerAreaSphere(const FVector& Location,
                                                             const FRotator& Rotation,
                                                             const FVector& Scale,
                                                             const ETriggerOverlapLogic TriggerOverlapLogic,
                                                             const float DelayBetweenCallbacks,
                                                             const int32 MaxCallbacks,
                                                             const int32 TriggerId)
{
	return CreateTriggerArea(
		EScriptedTriggerAreaShape::Sphere,
		Location,
		Rotation,
		Scale,
		TriggerOverlapLogic,
		DelayBetweenCallbacks,
		MaxCallbacks,
		TriggerId);
}

ATriggerArea* AScriptedTriggerActor::CreateTriggerAreaRectangle(const FVector& Location,
                                                                const FRotator& Rotation,
                                                                const FVector& Scale,
                                                                const ETriggerOverlapLogic TriggerOverlapLogic,
                                                                const float DelayBetweenCallbacks,
                                                                const int32 MaxCallbacks,
                                                                const int32 TriggerId)
{
	return CreateTriggerArea(
		EScriptedTriggerAreaShape::Rectangle,
		Location,
		Rotation,
		Scale,
		TriggerOverlapLogic,
		DelayBetweenCallbacks,
		MaxCallbacks,
		TriggerId);
}

void AScriptedTriggerActor::RemoveTriggerAreasById(const int32 TriggerId)
{
	MarkTriggerAreasForRemovalById(TriggerId);
	FlushPendingTriggerAreaChanges();
}

void AScriptedTriggerActor::RemoveAllTriggerAreas()
{
	MarkAllTriggerAreasForRemoval();
	FlushPendingTriggerAreaChanges();
}

void AScriptedTriggerActor::PlayPortrait(const ERTSPortraitTypes PortraitType, USoundBase* VoiceLine) const
{
	UPlayerPortraitManager* PlayerPortraitManager = FRTS_Statics::GetPlayerPortraitManager(this);
	if (not IsValid(PlayerPortraitManager))
	{
		RTSFunctionLibrary::ReportError(
			"ScriptedTriggerActor attempted to play portrait but player portrait manager is not valid!"
			"\n Actor : " + GetName());
		return;
	}

	PlayerPortraitManager->PlayPortrait(PortraitType, VoiceLine);
}

void AScriptedTriggerActor::OnTriggerAreaCallback(AActor* OverlappingActor,
                                                  const int32 TriggerId,
                                                  ATriggerArea* TriggerVolume)
{
	BP_OnTriggerAreaCallback(OverlappingActor, TriggerId, TriggerVolume);
}

ATriggerArea* AScriptedTriggerActor::CreateTriggerArea(const EScriptedTriggerAreaShape TriggerAreaShape,
                                                       const FVector& Location,
                                                       const FRotator& Rotation,
                                                       const FVector& Scale,
                                                       const ETriggerOverlapLogic TriggerOverlapLogic,
                                                       const float DelayBetweenCallbacks,
                                                       const int32 MaxCallbacks,
                                                       const int32 TriggerId)
{
	if (not GetIsValidWorldContext() || not LoadTriggerAreaClasses() || not GetHasValidTriggerAreaClass(TriggerAreaShape))
	{
		return nullptr;
	}

	const TSubclassOf<ATriggerArea> TriggerAreaClass = TriggerAreaShape == EScriptedTriggerAreaShape::Sphere
		                                                   ? M_TriggerAreaSphereClass
		                                                   : M_TriggerAreaRectangleClass;

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.Owner = this;

	ATriggerArea* TriggerArea = GetWorld()->SpawnActor<ATriggerArea>(TriggerAreaClass, Location, Rotation, SpawnParameters);
	if (not IsValid(TriggerArea))
	{
		RTSFunctionLibrary::ReportError("ScriptedTriggerActor failed to spawn TriggerArea actor.");
		return nullptr;
	}

	TriggerArea->ApplyScaleToCollision(Scale);
	TriggerArea->SetupOverlapLogic(TriggerOverlapLogic);
	RegisterTriggerArea(TriggerArea, TriggerId, DelayBetweenCallbacks, MaxCallbacks);
	FlushPendingTriggerAreaChanges();
	return TriggerArea;
}

bool AScriptedTriggerActor::LoadTriggerAreaClasses()
{
	const bool bHasRectangleClass = M_TriggerAreaRectangleClass != nullptr;
	const bool bHasSphereClass = M_TriggerAreaSphereClass != nullptr;
	if (bHasRectangleClass && bHasSphereClass)
	{
		return true;
	}

	if (not GetIsValidWorldContext())
	{
		return false;
	}

	UScriptedTriggerAreaSubsystem* TriggerAreaSubsystem = GetWorld()->GetSubsystem<UScriptedTriggerAreaSubsystem>();
	if (not IsValid(TriggerAreaSubsystem))
	{
		RTSFunctionLibrary::ReportError(
			"ScriptedTriggerActor could not access UScriptedTriggerAreaSubsystem while loading trigger area classes.");
		return false;
	}

	M_TriggerAreaRectangleClass = TriggerAreaSubsystem->GetRectangleTriggerAreaClass();
	M_TriggerAreaSphereClass = TriggerAreaSubsystem->GetSphereTriggerAreaClass();

	if (M_TriggerAreaRectangleClass != nullptr || M_TriggerAreaSphereClass != nullptr)
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(
		"ScriptedTriggerActor has no trigger area classes loaded from scripted trigger subsystem.");
	return false;
}

bool AScriptedTriggerActor::GetHasValidTriggerAreaClass(const EScriptedTriggerAreaShape TriggerAreaShape) const
{
	const bool bIsSphere = TriggerAreaShape == EScriptedTriggerAreaShape::Sphere;
	const bool bHasClass = bIsSphere ? M_TriggerAreaSphereClass != nullptr : M_TriggerAreaRectangleClass != nullptr;
	if (bHasClass)
	{
		return true;
	}

	const FString ShapeName = bIsSphere ? TEXT("Sphere") : TEXT("Rectangle");
	RTSFunctionLibrary::ReportError(
		"ScriptedTriggerActor requested trigger area class is missing for shape: " + ShapeName);
	return false;
}

bool AScriptedTriggerActor::GetIsValidWorldContext() const
{
	if (IsValid(GetWorld()))
	{
		return true;
	}

	RTSFunctionLibrary::ReportError("ScriptedTriggerActor cannot access world context.");
	return false;
}

void AScriptedTriggerActor::RegisterTriggerArea(ATriggerArea* TriggerArea,
                                                const int32 TriggerId,
                                                const float DelayBetweenCallbacks,
                                                const int32 MaxCallbacks)
{
	if (not IsValid(TriggerArea))
	{
		RTSFunctionLibrary::ReportError("ScriptedTriggerActor received an invalid TriggerArea during registration.");
		return;
	}

	FScriptedTriggerAreaRegistration NewRegistration;
	NewRegistration.M_TriggerArea = TriggerArea;
	NewRegistration.M_TriggerId = TriggerId;
	NewRegistration.M_DelayBetweenCallbacks = FMath::Max(0.0f, DelayBetweenCallbacks);
	NewRegistration.M_LastCallbackTime = -1.0f;
	NewRegistration.M_MaxCallbacks = MaxCallbacks;
	NewRegistration.M_CallbackCount = 0;
	NewRegistration.bM_PendingRemoval = false;

	if (bM_IsProcessingTriggerAreaCallbacks)
	{
		M_PendingRegisteredTriggerAreas.Add(NewRegistration);
	}
	else
	{
		M_RegisteredTriggerAreas.Add(NewRegistration);
	}

	TriggerArea->OnTriggerAreaOverlap.RemoveDynamic(this, &AScriptedTriggerActor::OnTriggerAreaOverlap);
	TriggerArea->OnTriggerAreaOverlap.AddDynamic(this, &AScriptedTriggerActor::OnTriggerAreaOverlap);
}

void AScriptedTriggerActor::MarkTriggerAreasForRemovalById(const int32 TriggerId)
{
	MarkTriggerAreasForRemovalById(M_RegisteredTriggerAreas, TriggerId);
	MarkTriggerAreasForRemovalById(M_PendingRegisteredTriggerAreas, TriggerId);
}

void AScriptedTriggerActor::MarkTriggerAreasForRemovalById(TArray<FScriptedTriggerAreaRegistration>& TriggerAreaRegistrations,
                                                           const int32 TriggerId)
{
	for (FScriptedTriggerAreaRegistration& TriggerAreaRegistration : TriggerAreaRegistrations)
	{
		if (TriggerAreaRegistration.M_TriggerId != TriggerId)
		{
			continue;
		}

		TriggerAreaRegistration.bM_PendingRemoval = true;
	}
}

void AScriptedTriggerActor::MarkAllTriggerAreasForRemoval()
{
	MarkAllTriggerAreasForRemoval(M_RegisteredTriggerAreas);
	MarkAllTriggerAreasForRemoval(M_PendingRegisteredTriggerAreas);
}

void AScriptedTriggerActor::MarkAllTriggerAreasForRemoval(TArray<FScriptedTriggerAreaRegistration>& TriggerAreaRegistrations)
{
	for (FScriptedTriggerAreaRegistration& TriggerAreaRegistration : TriggerAreaRegistrations)
	{
		TriggerAreaRegistration.bM_PendingRemoval = true;
	}
}

void AScriptedTriggerActor::FlushPendingTriggerAreaChanges()
{
	if (bM_IsProcessingTriggerAreaCallbacks)
	{
		return;
	}

	FlushPendingTriggerAreaChanges(M_RegisteredTriggerAreas);
	FlushPendingTriggerAreaChanges(M_PendingRegisteredTriggerAreas);

	if (M_PendingRegisteredTriggerAreas.Num() == 0)
	{
		return;
	}

	M_RegisteredTriggerAreas.Append(M_PendingRegisteredTriggerAreas);
	M_PendingRegisteredTriggerAreas.Reset();
}

void AScriptedTriggerActor::FlushPendingTriggerAreaChanges(TArray<FScriptedTriggerAreaRegistration>& TriggerAreaRegistrations)
{
	for (int32 RegistrationIndex = TriggerAreaRegistrations.Num() - 1; RegistrationIndex >= 0; --RegistrationIndex)
	{
		const FScriptedTriggerAreaRegistration& TriggerAreaRegistration = TriggerAreaRegistrations[RegistrationIndex];
		if (TriggerAreaRegistration.M_TriggerArea.IsValid() && not TriggerAreaRegistration.bM_PendingRemoval)
		{
			continue;
		}

		ATriggerArea* TriggerArea = TriggerAreaRegistration.M_TriggerArea.Get();
		if (IsValid(TriggerArea))
		{
			TriggerArea->OnTriggerAreaOverlap.RemoveDynamic(this, &AScriptedTriggerActor::OnTriggerAreaOverlap);
			TriggerArea->Destroy();
		}

		TriggerAreaRegistrations.RemoveAtSwap(RegistrationIndex);
	}
}

void AScriptedTriggerActor::ProcessTriggerAreaRegistrationOverlap(FScriptedTriggerAreaRegistration& TriggerAreaRegistration,
                                                                  AActor* OverlappingActor,
                                                                  ATriggerArea* TriggerArea,
                                                                  const float CurrentTimeSeconds)
{
	const bool bHasLimitedCallbacks = TriggerAreaRegistration.M_MaxCallbacks >= 0;
	if (bHasLimitedCallbacks && TriggerAreaRegistration.M_MaxCallbacks == 0)
	{
		TriggerAreaRegistration.bM_PendingRemoval = true;
		return;
	}

	const bool bHasPreviousCallbackTime = TriggerAreaRegistration.M_LastCallbackTime >= 0.0f;
	if (bHasPreviousCallbackTime
		&& CurrentTimeSeconds - TriggerAreaRegistration.M_LastCallbackTime < TriggerAreaRegistration.M_DelayBetweenCallbacks)
	{
		return;
	}

	TriggerAreaRegistration.M_LastCallbackTime = CurrentTimeSeconds;
	TriggerAreaRegistration.M_CallbackCount++;
	OnTriggerAreaCallback(OverlappingActor, TriggerAreaRegistration.M_TriggerId, TriggerArea);

	const bool bReachedCallbackLimit = bHasLimitedCallbacks
		&& TriggerAreaRegistration.M_CallbackCount >= TriggerAreaRegistration.M_MaxCallbacks;
	if (bReachedCallbackLimit)
	{
		TriggerAreaRegistration.bM_PendingRemoval = true;
	}
}

void AScriptedTriggerActor::OnTriggerAreaOverlap(AActor* OverlappingActor, ATriggerArea* TriggerArea)
{
	if (not IsValid(OverlappingActor) || not IsValid(TriggerArea))
	{
		return;
	}

	if (not GetIsValidWorldContext())
	{
		return;
	}

	const float CurrentTimeSeconds = GetWorld()->GetTimeSeconds();

	bM_IsProcessingTriggerAreaCallbacks = true;

	for (int32 RegistrationIndex = M_RegisteredTriggerAreas.Num() - 1; RegistrationIndex >= 0; --RegistrationIndex)
	{
		FScriptedTriggerAreaRegistration& TriggerAreaRegistration = M_RegisteredTriggerAreas[RegistrationIndex];
		if (TriggerAreaRegistration.bM_PendingRemoval || TriggerAreaRegistration.M_TriggerArea.Get() != TriggerArea)
		{
			continue;
		}

		ProcessTriggerAreaRegistrationOverlap(
			TriggerAreaRegistration,
			OverlappingActor,
			TriggerArea,
			CurrentTimeSeconds);
	}

	bM_IsProcessingTriggerAreaCallbacks = false;
	FlushPendingTriggerAreaChanges();
}
