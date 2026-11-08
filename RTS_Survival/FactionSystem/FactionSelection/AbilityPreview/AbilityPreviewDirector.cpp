// Copyright (C) Bas Blokzijl - All rights reserved.

#include "AbilityPreviewDirector.h"

#include "AbilityPreviewTargets.h"
#include "AIController.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/RTSGlobalAbilitySettings.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/RadixiteNuke/RadixiteNuke.h"
#include "RTS_Survival/Units/Aircraft/GliderDelivery/GliderTankDelivery.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/SpiesInfiltration/GA_SpiesInfiltration.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSAbilityPreview, Log, All);

namespace AbilityPreviewDirectorConstants
{
	// Camera framing over the centre (actor space): about 64 m out and 30 degrees down, the RTS camera's angle.
	const FVector CameraOffset(-5500.f, 0.f, 3200.f);
	constexpr float CameraFieldOfView = 62.f;
	constexpr float GroundTraceHalfHeight = 100000.f;

	// Class name parts of actors the preview never destroys: pools and managers spawn lazily on first use and must
	// outlive the preview (their pooled members are reused by later previews and the hangar).
	const TCHAR* const KeptClassNameParts[] = {
		TEXT("Pool"), TEXT("Projectile"), TEXT("Manager"), TEXT("Subsystem"), TEXT("Spawner"), TEXT("Camera"),
		TEXT("AbilityPreviewDirector")
	};

	// Settings structs of the aircraft run abilities and their run length: private to the ability classes, read and
	// changed on the preview's copy through reflection so the mission code stays as it is.
	const TCHAR* const StrafeSettingsName = TEXT("M_AircraftStrafingSettings");
	const TCHAR* const BombingSettingsName = TEXT("M_AircraftBombingSettings");
	const TCHAR* const StrafeLengthName = TEXT("StrafeLength");
	const TCHAR* const CarpetLengthName = TEXT("CarpetBombingLength");
	const TCHAR* const TotalStrafeTimeName = TEXT("TotalStrafeTime");

	/** A float inside one of the ability's settings structs; null when the ability has no such setting. */
	float* FindAbilitySettingsFloat(UGlobalAbility& Ability, const TCHAR* SettingsName, const TCHAR* FloatName)
	{
		const FStructProperty* const SettingsProperty = FindFProperty<FStructProperty>(Ability.GetClass(), SettingsName);
		if (SettingsProperty == nullptr)
		{
			return nullptr;
		}
		const FFloatProperty* const FloatProperty = FindFProperty<FFloatProperty>(SettingsProperty->Struct, FloatName);
		if (FloatProperty == nullptr)
		{
			return nullptr;
		}
		return FloatProperty->ContainerPtrToValuePtr<float>(SettingsProperty->ContainerPtrToValuePtr<void>(&Ability));
	}

	/** Length of the aircraft's strafing run; 0 for abilities without one. */
	float GetStrafeRunLength(UGlobalAbility& Ability)
	{
		const float* const StrafeLength = FindAbilitySettingsFloat(Ability, StrafeSettingsName, StrafeLengthName);
		return StrafeLength != nullptr ? *StrafeLength : 0.f;
	}

	/** Length of the aircraft's carpet bombing run; 0 for abilities without one. */
	float GetCarpetRunLength(UGlobalAbility& Ability)
	{
		const float* const CarpetLength = FindAbilitySettingsFloat(Ability, BombingSettingsName, CarpetLengthName);
		return CarpetLength != nullptr ? *CarpetLength : 0.f;
	}
}

AAbilityPreviewDirector::AAbilityPreviewDirector()
{
	PrimaryActorTick.bCanEverTick = false;

	M_PreviewCentre = CreateDefaultSubobject<USceneComponent>(TEXT("PreviewCentre"));
	SetRootComponent(M_PreviewCentre);

	M_PreviewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("PreviewCamera"));
	M_PreviewCamera->SetupAttachment(M_PreviewCentre);
	M_PreviewCamera->SetRelativeLocation(AbilityPreviewDirectorConstants::CameraOffset);
	const FVector ToCentre = -AbilityPreviewDirectorConstants::CameraOffset;
	M_PreviewCamera->SetRelativeRotation(ToCentre.Rotation());
	M_PreviewCamera->SetFieldOfView(AbilityPreviewDirectorConstants::CameraFieldOfView);
	M_PreviewCamera->bConstrainAspectRatio = false;

	M_PreviewAbilityManager = CreateDefaultSubobject<UGlobalAbilitiesManager>(TEXT("PreviewAbilityManager"));
	M_PreviewTargets = CreateDefaultSubobject<UAbilityPreviewTargetsComponent>(TEXT("PreviewTargets"));
}

void AAbilityPreviewDirector::BeginPlay()
{
	Super::BeginPlay();
	// Tracks spawns for the whole level: late spawns of a stopped preview (an async squad load) are cleaned up too.
	BeginTrackingSpawns();
}

void AAbilityPreviewDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopPreview();
	if (UWorld* World = GetWorld(); IsValid(World) && M_ActorSpawnedHandle.IsValid())
	{
		World->RemoveOnActorSpawnedHandler(M_ActorSpawnedHandle);
		M_ActorSpawnedHandle.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

bool AAbilityPreviewDirector::LoadAbilityDataAsset()
{
	if (IsValid(M_AbilityDataAsset))
	{
		return true;
	}
	const URTSGlobalAbilitySettings* const GlobalAbilitySettings = GetDefault<URTSGlobalAbilitySettings>();
	if (IsValid(GlobalAbilitySettings))
	{
		M_AbilityDataAsset = GlobalAbilitySettings->GlobalAbilityDataAsset.LoadSynchronous();
	}
	if (IsValid(M_AbilityDataAsset))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(
		"The ability preview could not load the global ability data asset (GlobalAbilitySettings)."
		"\nAt function: AAbilityPreviewDirector::LoadAbilityDataAsset");
	return false;
}

bool AAbilityPreviewDirector::StartPreview(const EGlobalAbility Ability)
{
	StopPreview();
	if (not LoadAbilityDataAsset())
	{
		return false;
	}
	// The commander screen lists abilities whose template is still being authored: no error for those.
	if (not IsValid(M_AbilityDataAsset->FindAbilityTemplate(Ability)))
	{
		UE_LOG(LogRTSAbilityPreview, Warning, TEXT("RTS_ABILITY_PREVIEW no template for %s"),
		       *UEnum::GetValueAsString(Ability));
		return false;
	}
	UGlobalAbility* const AbilityCopy = M_AbilityDataAsset->GetGLobalAbilityForTypeAsCopyFromTemplate(Ability);
	if (not IsValid(AbilityCopy))
	{
		return false;
	}

	const FVector GroundCentre = GetGroundPoint(GetActorLocation());
	const FVector ApproachDirection = GetActorTransform().TransformVectorNoScale(M_AircraftApproachDirection);
	M_PreviewAbilityManager->SetupForAbilityPreview(GroundCentre, ApproachDirection, M_AircraftSpawnDistance,
	                                                GroundCentre.Z + M_AircraftHeightAboveGround,
	                                                M_AircraftSpawnLineHalfLength);

	ShortenForPreview(*AbilityCopy);
	M_Ability = Ability;
	M_PreviewAbility = AbilityCopy;
	M_PreviewAbility->OnGlobalAbilityEnded.AddUObject(this, &AAbilityPreviewDirector::HandleAbilityEnded);
	// No player controller: the preview has no aim, panel or 2D feedback (the manager is a preview manager).
	M_PreviewAbility->InitGlobalAbility(1, M_PreviewAbilityManager, nullptr);

	bM_IsPreviewRunning = true;
	// Content errors of the dropped units (they fire in missions too) are logged but open no modal dialog in the
	// editor while the player watches a preview.
	RTSFunctionLibrary::PushModalDialogSuppression();
	bM_IsPreviewRunning_SuppressingDialogs = true;
	bM_HasExecuted = false;
	bM_HasAbilityFinished = false;
	M_CloseReason.Reset();
	M_PreviewStartTime = FPlatformTime::Seconds();
	ApplyPreviewLook();
	// After the preview counts as running: the targets are tracked spawns, cleaned up with the rest.
	SpawnPreviewTargets(*AbilityCopy, GroundCentre, ApproachDirection);

	UWorld* const World = GetWorld();
	if (IsValid(World))
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.SetTimer(M_ExecuteTimerHandle, this, &AAbilityPreviewDirector::ExecutePreviewAbility,
		                      FMath::Max(M_ExecuteDelaySeconds, 0.01f), false);
		TimerManager.SetTimer(M_MaxDurationTimerHandle, this, &AAbilityPreviewDirector::HandleMaxDurationReached,
		                      M_MaxPreviewSeconds, false);
	}
	UE_LOG(LogRTSAbilityPreview, Display, TEXT("RTS_ABILITY_PREVIEW START %s class=%s centre=%s"),
	       *UEnum::GetValueAsString(Ability), *AbilityCopy->GetClass()->GetName(), *GroundCentre.ToCompactString());
	return true;
}

void AAbilityPreviewDirector::ShortenForPreview(UGlobalAbility& AbilityCopy) const
{
	using namespace AbilityPreviewDirectorConstants;
	// The strafe settings are private to UGA_AircraftStrafing; the copy is changed through reflection so the mission
	// code stays as it is.
	float* const TotalStrafeTime = FindAbilitySettingsFloat(AbilityCopy, StrafeSettingsName, TotalStrafeTimeName);
	if (TotalStrafeTime == nullptr)
	{
		return;
	}
	if (*TotalStrafeTime > M_MaxPreviewStrafeSeconds)
	{
		UE_LOG(LogRTSAbilityPreview, Display, TEXT("RTS_ABILITY_PREVIEW strafe time %.0f s shortened to %.0f s"),
		       *TotalStrafeTime, M_MaxPreviewStrafeSeconds);
		*TotalStrafeTime = M_MaxPreviewStrafeSeconds;
	}
}

void AAbilityPreviewDirector::ExecutePreviewAbility()
{
	if (not bM_IsPreviewRunning || not IsValid(M_PreviewAbility))
	{
		return;
	}
	const FGlobalAbilityAimSettings& AimSettings = M_PreviewAbility->GetAimSettings();
	bM_HasExecuted = true;
	if (bM_ExecutesAlongStrip)
	{
		M_PreviewAbility->ExecuteAbilityAlongLine(M_StripStart, M_StripEnd);
	}
	else
	{
		M_PreviewAbility->ExecuteAbilityAtLocation(M_ExecuteLocation);
	}
	// The preview has no player audio controller: play the ability's own execute feedback here.
	const FGlobalAbilitySoundSettings& SoundSettings = M_PreviewAbility->GetSoundSettings();
	if (IsValid(SoundSettings.AnnouncerSoundOnActivate))
	{
		UGameplayStatics::PlaySound2D(this, SoundSettings.AnnouncerSoundOnActivate);
	}
	if (IsValid(SoundSettings.Sound2DOnExecute))
	{
		UGameplayStatics::PlaySound2D(this, SoundSettings.Sound2DOnExecute);
	}
	UE_LOG(LogRTSAbilityPreview, Display, TEXT("RTS_ABILITY_PREVIEW EXECUTED %s shape=%d t=%.2f"),
	       *UEnum::GetValueAsString(M_Ability), static_cast<int32>(AimSettings.AreaAim.Shape),
	       GetSecondsSincePreviewStart());

	// Instant abilities never report an end; give them a moment on screen, then treat them as finished.
	if (M_PreviewAbility->IsA<UGA_SpiesInfiltration>())
	{
		if (UWorld* const World = GetWorld(); IsValid(World))
		{
			TWeakObjectPtr<AAbilityPreviewDirector> WeakThis(this);
			World->GetTimerManager().SetTimer(M_LingerTimerHandle, FTimerDelegate::CreateWeakLambda(this, [WeakThis]()
			{
				if (WeakThis.IsValid())
				{
					WeakThis->MarkAbilityFinished(TEXT("instant"));
				}
			}), FMath::Max(M_SecondsUntilInstantAbilityFinished, 0.01f), false);
		}
	}
}

bool AAbilityPreviewDirector::PlanPreviewAttack(UGlobalAbility& AbilityCopy, const FVector& GroundCentre,
                                                const FVector& ApproachDirection, FVector& OutAttackStart,
                                                FVector& OutAttackEnd)
{
	M_ExecuteLocation = GroundCentre;
	bM_ExecutesAlongStrip = false;
	const FGlobalAbilityAimSettings& AimSettings = AbilityCopy.GetAimSettings();
	if (AimSettings.AreaAim.Shape == EGlobalAbilityAimShape::Strip)
	{
		GetStripAcrossView(GroundCentre, GetPreviewStripLength(AimSettings), M_StripStart, M_StripEnd);
		bM_ExecutesAlongStrip = true;
		OutAttackStart = M_StripStart;
		OutAttackEnd = M_StripEnd;
		return true;
	}
	const FVector TowardsAircraft = ApproachDirection.GetSafeNormal2D();
	if (TowardsAircraft.IsNearlyZero())
	{
		return false;
	}
	const float StrafeLength = AbilityPreviewDirectorConstants::GetStrafeRunLength(AbilityCopy);
	if (StrafeLength > 0.f)
	{
		// Every strafing run fires from the target outwards, the first along the approach and later ones wherever the
		// aircraft comes from: the targets stand around the target, mostly along the first run.
		const float ShownLength = FMath::Min(StrafeLength, M_MaxStripLength);
		OutAttackStart = GetGroundPoint(GroundCentre + TowardsAircraft * (M_StrafeTargetsBeforeStart * ShownLength));
		OutAttackEnd = GetGroundPoint(GroundCentre - TowardsAircraft * (M_StrafeTargetsAlongRun * ShownLength));
		return true;
	}
	const float CarpetLength = AbilityPreviewDirectorConstants::GetCarpetRunLength(AbilityCopy);
	if (CarpetLength <= 0.f)
	{
		return false;
	}
	// The preview's carpet runs straight on from the target for its length (UGA_AircraftBombing): starting it half a
	// run towards the aircraft puts the middle of the carpet in the middle of the view.
	M_ExecuteLocation = GetGroundPoint(GroundCentre + TowardsAircraft * (0.5f * CarpetLength));
	const float ShownLength = FMath::Min(CarpetLength, M_MaxStripLength);
	OutAttackStart = GetGroundPoint(GroundCentre + TowardsAircraft * (0.5f * ShownLength));
	OutAttackEnd = GetGroundPoint(GroundCentre - TowardsAircraft * (0.5f * ShownLength));
	return true;
}

float AAbilityPreviewDirector::GetPreviewStripLength(const FGlobalAbilityAimSettings& AimSettings) const
{
	const float StripLength = FMath::Min(
		0.5f * (AimSettings.AreaAim.MinStripLength + AimSettings.AreaAim.MaxStripLength), M_MaxStripLength);
	return FMath::Max(StripLength, AimSettings.AreaAim.MinStripLength);
}

void AAbilityPreviewDirector::SpawnPreviewTargets(UGlobalAbility& AbilityCopy, const FVector& GroundCentre,
                                                  const FVector& ApproachDirection)
{
	FVector AttackStart;
	FVector AttackEnd;
	const bool bAttacksALine = PlanPreviewAttack(AbilityCopy, GroundCentre, ApproachDirection, AttackStart, AttackEnd);
	if (not bAttacksALine || not IsValid(M_PreviewTargets))
	{
		return;
	}
	const EAbilityPreviewTargetKind TargetKind = M_PreviewTargets->GetTargetKindForAbility(M_Ability, AbilityCopy);
	if (TargetKind == EAbilityPreviewTargetKind::None)
	{
		return;
	}
	M_PreviewTargets->SpawnTargets(TargetKind, AttackStart, AttackEnd);
}

FVector AAbilityPreviewDirector::GetGroundPoint(const FVector& Location) const
{
	UWorld* const World = GetWorld();
	if (not IsValid(World))
	{
		return Location;
	}
	const FVector TraceStart = Location + FVector(0.f, 0.f, AbilityPreviewDirectorConstants::GroundTraceHalfHeight);
	const FVector TraceEnd = Location - FVector(0.f, 0.f, AbilityPreviewDirectorConstants::GroundTraceHalfHeight);
	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AbilityPreviewGround), false, this);
	FHitResult Hit;
	if (World->LineTraceSingleByObjectType(Hit, TraceStart, TraceEnd, ObjectQuery, QueryParams))
	{
		return Hit.ImpactPoint;
	}
	return Location;
}

void AAbilityPreviewDirector::GetStripAcrossView(const FVector& GroundCentre, const float StripLength,
                                                 FVector& OutStart, FVector& OutEnd) const
{
	// Left to right across the screen, so the whole run crosses the view.
	FVector Right = IsValid(M_PreviewCamera) ? M_PreviewCamera->GetRightVector() : GetActorRightVector();
	Right.Z = 0.f;
	Right = Right.GetSafeNormal();
	if (Right.IsNearlyZero())
	{
		Right = FVector::RightVector;
	}
	OutStart = GetGroundPoint(GroundCentre - Right * (0.5f * StripLength));
	OutEnd = GetGroundPoint(GroundCentre + Right * (0.5f * StripLength));
}

void AAbilityPreviewDirector::HandleAbilityEnded(UGlobalAbility* EndedAbility)
{
	if (EndedAbility != M_PreviewAbility || not bM_IsPreviewRunning)
	{
		return;
	}
	// The nuke and V2 abilities end as soon as the warhead is on its way; the preview waits for the blast.
	if (GetHasWarheadInFlight())
	{
		if (UWorld* const World = GetWorld(); IsValid(World))
		{
			constexpr float WarheadCheckInterval = 0.1f;
			World->GetTimerManager().SetTimer(M_WarheadWaitTimerHandle, this,
			                                  &AAbilityPreviewDirector::CheckWarheadsLanded, WarheadCheckInterval,
			                                  true);
			return;
		}
	}
	// Glider drops end when the tank drives out of the nose: hide the glider and show the tank first.
	if (AGliderTankDelivery* const Delivery = FindDeliveredGlider())
	{
		StartGliderFinale(*Delivery);
		return;
	}
	MarkAbilityFinished(TEXT("ability_ended"));
}

AGliderTankDelivery* AAbilityPreviewDirector::FindDeliveredGlider() const
{
	for (const TWeakObjectPtr<AActor>& SpawnedActor : M_SpawnedActors)
	{
		AGliderTankDelivery* const Delivery = Cast<AGliderTankDelivery>(SpawnedActor.Get());
		if (IsValid(Delivery) && IsValid(Delivery->GetDeliveredTank()))
		{
			return Delivery;
		}
	}
	return nullptr;
}

void AAbilityPreviewDirector::StartGliderFinale(AGliderTankDelivery& Delivery)
{
	UWorld* const World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	M_GliderFinale.Phase = EGliderFinalePhase::WaitingForTankOut;
	M_GliderFinale.Delivery = &Delivery;
	M_GliderFinale.Tank = Delivery.GetDeliveredTank();
	M_GliderFinale.PhaseStartSeconds = World->GetTimeSeconds();
	// About 60 updates a second, for a smooth pan; the director itself does not tick.
	constexpr float FinaleUpdateInterval = 1.f / 60.f;
	World->GetTimerManager().SetTimer(M_GliderFinaleTimerHandle, this, &AAbilityPreviewDirector::TickGliderFinale,
	                                  FinaleUpdateInterval, true);
}

void AAbilityPreviewDirector::TickGliderFinale()
{
	const UWorld* const World = GetWorld();
	if (not IsValid(World) || not IsValid(M_PreviewCamera))
	{
		return;
	}
	const double PhaseSeconds = World->GetTimeSeconds() - M_GliderFinale.PhaseStartSeconds;
	if (M_GliderFinale.Phase == EGliderFinalePhase::WaitingForTankOut)
	{
		const AGliderTankDelivery* const Delivery = M_GliderFinale.Delivery.Get();
		const ATankMaster* const Tank = M_GliderFinale.Tank.Get();
		const bool bTankIsOut = not IsValid(Delivery) || not IsValid(Tank)
			|| FVector::Dist2D(Tank->GetActorLocation(), Delivery->GetActorLocation()) >= M_GliderHideTankDistance;
		if (bTankIsOut || PhaseSeconds >= M_GliderHideTimeoutSeconds)
		{
			HideGliderAndStartPan(M_GliderFinale.Delivery.Get());
		}
		return;
	}
	const float PanAlpha = FMath::Clamp(static_cast<float>(PhaseSeconds) / M_GliderCameraPanSeconds, 0.f, 1.f);
	constexpr float PanEaseExponent = 2.f;
	// Follows the tank while it still drives: where it really stops ends up in the middle of the view.
	if (const ATankMaster* const Tank = M_GliderFinale.Tank.Get(); IsValid(Tank))
	{
		M_GliderFinale.PanTargetRelative = GetCameraRelativeLocationFocusing(Tank->GetActorLocation());
	}
	M_PreviewCamera->SetRelativeLocation(FMath::InterpEaseInOut(
		M_GliderFinale.PanStartRelative, M_GliderFinale.PanTargetRelative, PanAlpha, PanEaseExponent));
	if (PanAlpha < 1.f)
	{
		return;
	}
	if (UWorld* const MutableWorld = GetWorld(); IsValid(MutableWorld))
	{
		MutableWorld->GetTimerManager().ClearTimer(M_GliderFinaleTimerHandle);
	}
	M_GliderFinale.Phase = EGliderFinalePhase::None;
	MarkAbilityFinished(TEXT("glider_pan"));
}

void AAbilityPreviewDirector::HideGliderAndStartPan(AGliderTankDelivery* Delivery)
{
	FVector PanFocus = GetGroundPoint(GetActorLocation());
	if (IsValid(Delivery))
	{
		PanFocus = Delivery->GetTankExitLocation();
		// Hidden, not destroyed: the delivery still runs its own linger and sink timers, and the cleanup takes it.
		Delivery->SetActorHiddenInGame(true);
		Delivery->SetActorEnableCollision(false);
	}
	if (const ATankMaster* const Tank = M_GliderFinale.Tank.Get(); IsValid(Tank))
	{
		PanFocus = Tank->GetActorLocation();
	}
	M_GliderFinale.PanStartRelative = M_PreviewCamera->GetRelativeLocation();
	M_GliderFinale.PanTargetRelative = GetCameraRelativeLocationFocusing(PanFocus);
	M_GliderFinale.Phase = EGliderFinalePhase::Panning;
	M_GliderFinale.PhaseStartSeconds = GetWorld()->GetTimeSeconds();
	UE_LOG(LogRTSAbilityPreview, Display, TEXT("RTS_ABILITY_PREVIEW GLIDER_HIDDEN t=%.2f"), GetSecondsSincePreviewStart());
}

FVector AAbilityPreviewDirector::GetCameraRelativeLocationFocusing(const FVector& Focus) const
{
	// A pan along the camera's right axis only, until the focus is in the middle of the view.
	FVector CameraRight = IsValid(M_PreviewCamera) ? M_PreviewCamera->GetRightVector() : GetActorRightVector();
	CameraRight.Z = 0.f;
	CameraRight = CameraRight.GetSafeNormal();
	const float SideOffset = FVector::DotProduct(Focus - GetGroundPoint(GetActorLocation()), CameraRight);
	return AbilityPreviewDirectorConstants::CameraOffset
		+ GetActorTransform().InverseTransformVectorNoScale(CameraRight * SideOffset);
}

void AAbilityPreviewDirector::ResetGliderFinale()
{
	if (UWorld* const World = GetWorld(); IsValid(World))
	{
		World->GetTimerManager().ClearTimer(M_GliderFinaleTimerHandle);
	}
	M_GliderFinale = FGliderFinaleState();
	if (IsValid(M_PreviewCamera))
	{
		M_PreviewCamera->SetRelativeLocation(AbilityPreviewDirectorConstants::CameraOffset);
	}
}

bool AAbilityPreviewDirector::GetHasWarheadInFlight() const
{
	for (const TWeakObjectPtr<AActor>& SpawnedActor : M_SpawnedActors)
	{
		const ARadixiteNuke* const Warhead = Cast<ARadixiteNuke>(SpawnedActor.Get());
		if (IsValid(Warhead) && Warhead->GetPhase() <= ERadixiteNukePhase::Falling)
		{
			return true;
		}
	}
	return false;
}

void AAbilityPreviewDirector::CheckWarheadsLanded()
{
	if (GetHasWarheadInFlight())
	{
		return;
	}
	if (UWorld* const World = GetWorld(); IsValid(World))
	{
		World->GetTimerManager().ClearTimer(M_WarheadWaitTimerHandle);
	}
	MarkAbilityFinished(TEXT("blast"));
}

void AAbilityPreviewDirector::MarkAbilityFinished(const TCHAR* Reason)
{
	if (not bM_IsPreviewRunning || bM_HasAbilityFinished)
	{
		return;
	}
	bM_HasAbilityFinished = true;
	UE_LOG(LogRTSAbilityPreview, Display, TEXT("RTS_ABILITY_PREVIEW FINISHED %s reason=%s t=%.2f"),
	       *UEnum::GetValueAsString(M_Ability), Reason, GetSecondsSincePreviewStart());
	if (UWorld* const World = GetWorld(); IsValid(World))
	{
		World->GetTimerManager().SetTimer(M_LingerTimerHandle, this, &AAbilityPreviewDirector::HandleLingerFinished,
		                                  FMath::Max(M_SecondsAfterAbilityFinished, 0.01f), false);
	}
}

void AAbilityPreviewDirector::HandleLingerFinished()
{
	RequestClose(TEXT("finished"));
}

void AAbilityPreviewDirector::HandleMaxDurationReached()
{
	UE_LOG(LogRTSAbilityPreview, Warning, TEXT("RTS_ABILITY_PREVIEW TIMEOUT %s after %.0f s"),
	       *UEnum::GetValueAsString(M_Ability), M_MaxPreviewSeconds);
	RequestClose(TEXT("timeout"));
}

void AAbilityPreviewDirector::RequestClose(const FString& Reason)
{
	if (not bM_IsPreviewRunning)
	{
		return;
	}
	M_CloseReason = Reason;
	UE_LOG(LogRTSAbilityPreview, Display, TEXT("RTS_ABILITY_PREVIEW AUTO_CLOSE %s reason=%s t=%.2f spawned=%d"),
	       *UEnum::GetValueAsString(M_Ability), *Reason, GetSecondsSincePreviewStart(), M_SpawnedActors.Num());
	// The owner closes the screen; it calls StopPreview. Without an owner the preview stops itself.
	if (OnPreviewWantsToClose.IsBound())
	{
		OnPreviewWantsToClose.Execute();
		return;
	}
	StopPreview();
}

void AAbilityPreviewDirector::StopPreview()
{
	ClearPreviewTimers();
	if (not bM_IsPreviewRunning)
	{
		return;
	}
	bM_IsPreviewRunning = false;
	UE_LOG(LogRTSAbilityPreview, Display, TEXT("RTS_ABILITY_PREVIEW STOP %s t=%.2f executed=%d finished=%d spawned=%d"),
	       *UEnum::GetValueAsString(M_Ability), GetSecondsSincePreviewStart(), bM_HasExecuted ? 1 : 0,
	       bM_HasAbilityFinished ? 1 : 0, M_SpawnedActors.Num());
	RetireAbility();
	ResetGliderFinale();
	DestroyPreviewSpawns();
	// The targets were tracked spawns and are gone with them; this drops the bookkeeping and anything left over.
	if (IsValid(M_PreviewTargets))
	{
		M_PreviewTargets->ClearTargets();
	}
	RestoreHangarLook();
	if (bM_IsPreviewRunning_SuppressingDialogs)
	{
		bM_IsPreviewRunning_SuppressingDialogs = false;
		RTSFunctionLibrary::PopModalDialogSuppression();
	}
	M_Ability = EGlobalAbility::GA_None;
}

float AAbilityPreviewDirector::GetSecondsSincePreviewStart() const
{
	if (not bM_IsPreviewRunning)
	{
		return 0.f;
	}
	return static_cast<float>(FPlatformTime::Seconds() - M_PreviewStartTime);
}

float AAbilityPreviewDirector::GetAutoReturnSecondsLeft() const
{
	const UWorld* const World = GetWorld();
	if (not bM_IsPreviewRunning || not bM_HasAbilityFinished || not IsValid(World))
	{
		return -1.f;
	}
	return FMath::Max(0.f, World->GetTimerManager().GetTimerRemaining(M_LingerTimerHandle));
}

void AAbilityPreviewDirector::RetireAbility()
{
	if (not IsValid(M_PreviewAbility))
	{
		M_PreviewAbility = nullptr;
		return;
	}
	M_PreviewAbility->OnGlobalAbilityEnded.RemoveAll(this);
	if (UWorld* const World = GetWorld(); IsValid(World))
	{
		// Stops the run timers bound to the ability object; lambdas bound without it stay, see M_RetiredAbilities.
		World->GetTimerManager().ClearAllTimersForObject(M_PreviewAbility);
	}
	M_RetiredAbilities.Add(M_PreviewAbility);
	M_PreviewAbility = nullptr;
}

void AAbilityPreviewDirector::BeginTrackingSpawns()
{
	UWorld* const World = GetWorld();
	if (not IsValid(World) || M_ActorSpawnedHandle.IsValid())
	{
		return;
	}
	M_ActorSpawnedHandle = World->AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateUObject(this, &AAbilityPreviewDirector::HandleActorSpawned));
}

void AAbilityPreviewDirector::HandleActorSpawned(AActor* SpawnedActor)
{
	if (not GetIsCleanupCandidateClass(SpawnedActor))
	{
		return;
	}
	if (bM_IsPreviewRunning)
	{
		M_SpawnedActors.Add(SpawnedActor);
		return;
	}
	// A stopped preview's ability can still finish an async load: whatever lands in the desert goes on the next tick
	// (the spawner places it after this callback).
	UWorld* const World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	TWeakObjectPtr<AActor> WeakSpawned(SpawnedActor);
	TWeakObjectPtr<AAbilityPreviewDirector> WeakThis(this);
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [WeakThis, WeakSpawned]()
	{
		if (not WeakThis.IsValid() || not WeakSpawned.IsValid() || WeakThis->bM_IsPreviewRunning)
		{
			return;
		}
		if (WeakThis->GetShouldDestroyOnCleanup(WeakSpawned.Get()))
		{
			UE_LOG(LogRTSAbilityPreview, Display, TEXT("RTS_ABILITY_PREVIEW LATE_SPAWN_DESTROYED %s"),
			       *WeakSpawned->GetName());
			WeakSpawned->Destroy();
		}
	}));
}

bool AAbilityPreviewDirector::GetIsCleanupCandidateClass(const AActor* Actor) const
{
	if (not IsValid(Actor) || Actor == this)
	{
		return false;
	}
	if (Actor->IsA<APlayerController>() || Actor->IsA<AGameStateBase>() || Actor->IsA<AHUD>())
	{
		return false;
	}
	const FString ClassName = Actor->GetClass()->GetName();
	for (const TCHAR* const KeptPart : AbilityPreviewDirectorConstants::KeptClassNameParts)
	{
		if (ClassName.Contains(KeptPart))
		{
			return false;
		}
	}
	return true;
}

bool AAbilityPreviewDirector::GetShouldDestroyOnCleanup(const AActor* Actor) const
{
	// Child actors (a soldier's weapon) go with their parent component; destroying them first makes the parent unit
	// report a missing weapon while it dies.
	if (not GetIsCleanupCandidateClass(Actor) || Actor->IsActorBeingDestroyed() || Actor->IsChildActor())
	{
		return false;
	}
	// Retreating aircraft may be far out by now; they must not linger anywhere.
	if (Actor->IsA<AAircraftMaster>())
	{
		return true;
	}
	if (const AController* const Controller = Cast<AController>(Actor))
	{
		// A unit's AI controller goes with its pawn; controllers have no useful location of their own.
		const APawn* const ControlledPawn = Controller->GetPawn();
		return not IsValid(ControlledPawn) || ControlledPawn->IsActorBeingDestroyed()
			|| GetShouldDestroyOnCleanup(ControlledPawn);
	}
	return FVector::DistSquared2D(Actor->GetActorLocation(), GetActorLocation()) <= FMath::Square(M_CleanupRadius);
}

void AAbilityPreviewDirector::DestroyPreviewSpawns()
{
	TArray<AActor*> ToDestroy;
	TArray<AActor*> ControllersToDestroy;
	for (const TWeakObjectPtr<AActor>& WeakActor : M_SpawnedActors)
	{
		AActor* const Actor = WeakActor.Get();
		if (not IsValid(Actor))
		{
			continue;
		}
		if (Actor->IsA<AController>())
		{
			ControllersToDestroy.Add(Actor);
			continue;
		}
		if (GetShouldDestroyOnCleanup(Actor))
		{
			ToDestroy.Add(Actor);
		}
	}
	// Controllers are judged after their pawns: a pawn being destroyed takes its AI controller along.
	for (AActor* const Controller : ControllersToDestroy)
	{
		const APawn* const ControlledPawn = Cast<AController>(Controller)->GetPawn();
		if (not IsValid(ControlledPawn) || ToDestroy.Contains(ControlledPawn))
		{
			ToDestroy.Add(Controller);
		}
	}
	for (AActor* const Actor : ToDestroy)
	{
		if (IsValid(Actor) && not Actor->IsActorBeingDestroyed())
		{
			Actor->Destroy();
		}
	}
	UE_LOG(LogRTSAbilityPreview, Display, TEXT("RTS_ABILITY_PREVIEW CLEANUP destroyed=%d of tracked=%d"),
	       ToDestroy.Num(), M_SpawnedActors.Num());
	M_SpawnedActors.Reset();
}

void AAbilityPreviewDirector::ApplyPreviewLook()
{
	UWorld* const World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	if (bM_SpawnSkyAtmosphere && not M_SpawnedSky.IsValid())
	{
		bool bWorldHasSky = false;
		for (TActorIterator<ASkyAtmosphere> It(World); It; ++It)
		{
			bWorldHasSky = true;
			break;
		}
		if (not bWorldHasSky)
		{
			// Movable: a stationary atmosphere spawned at runtime counts as unbuilt static lighting ("lighting needs to
			// be rebuilt" in development builds). Destroyed by RestoreHangarLook.
			ASkyAtmosphere* const Sky = World->SpawnActorDeferred<ASkyAtmosphere>(
				ASkyAtmosphere::StaticClass(), GetActorTransform(), nullptr, nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (IsValid(Sky))
			{
				if (USkyAtmosphereComponent* const SkyComponent = Sky->GetComponent())
				{
					SkyComponent->SetMobility(EComponentMobility::Movable);
				}
				Sky->FinishSpawning(GetActorTransform());
				M_SpawnedSky = Sky;
			}
		}
	}
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		UDirectionalLightComponent* const SunComponent = Cast<UDirectionalLightComponent>(It->GetLightComponent());
		if (IsValid(SunComponent))
		{
			M_SunLight = SunComponent;
			bM_SunWasAtmosphereLight = SunComponent->IsUsedAsAtmosphereSunLight();
			SunComponent->SetAtmosphereSunLight(true);
			break;
		}
	}
	for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
	{
		UExponentialHeightFogComponent* const FogComponent = It->GetComponent();
		if (IsValid(FogComponent) && M_PreviewFogDensity >= 0.f)
		{
			M_HeightFog = FogComponent;
			M_HangarFogDensity = FogComponent->FogDensity;
			M_HangarFogInscatteringColor = FogComponent->FogInscatteringLuminance;
			FogComponent->SetFogDensity(M_PreviewFogDensity);
			FogComponent->SetFogInscatteringColor(M_PreviewFogInscatteringColor);
		}
		break;
	}
}

void AAbilityPreviewDirector::RestoreHangarLook()
{
	if (M_SpawnedSky.IsValid())
	{
		M_SpawnedSky->Destroy();
	}
	M_SpawnedSky.Reset();
	if (M_SunLight.IsValid())
	{
		M_SunLight->SetAtmosphereSunLight(bM_SunWasAtmosphereLight);
	}
	M_SunLight.Reset();
	if (M_HeightFog.IsValid())
	{
		M_HeightFog->SetFogDensity(M_HangarFogDensity);
		M_HeightFog->SetFogInscatteringColor(M_HangarFogInscatteringColor);
	}
	M_HeightFog.Reset();
}

void AAbilityPreviewDirector::ClearPreviewTimers()
{
	UWorld* const World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	FTimerManager& TimerManager = World->GetTimerManager();
	TimerManager.ClearTimer(M_ExecuteTimerHandle);
	TimerManager.ClearTimer(M_LingerTimerHandle);
	TimerManager.ClearTimer(M_MaxDurationTimerHandle);
	TimerManager.ClearTimer(M_WarheadWaitTimerHandle);
}
