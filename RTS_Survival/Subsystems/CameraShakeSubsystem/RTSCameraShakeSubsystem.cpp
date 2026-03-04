#include "RTSCameraShakeSubsystem.h"

#include "Camera/CameraShakeBase.h"
#include "Camera/CameraTypes.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/Camera/Settings/RTSCameraShakeDeveloperSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace RTSCameraShakeConstants
{
	constexpr int32 LocalPlayerIndex = 0;
}

bool URTSCameraShakeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld))
	{
		return false;
	}

	return OuterWorld->IsGameWorld();
}

void URTSCameraShakeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	M_PendingRequests.Reset();
	M_RequestTimesSeconds.Reset();
	M_AggregationWindowStartTimeSeconds = -1.0f;
	M_LastHeavyShakeTimeSeconds = -1000.0f;
}

void URTSCameraShakeSubsystem::Deinitialize()
{
	M_PendingRequests.Reset();
	M_RequestTimesSeconds.Reset();
	Super::Deinitialize();
}

void URTSCameraShakeSubsystem::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	const float CurrentTimeSeconds = World->GetTimeSeconds();
	Tick_PruneRequestRateWindow(CurrentTimeSeconds);
	Tick_FlushAggregationWindow(CurrentTimeSeconds);
}

TStatId URTSCameraShakeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URTSCameraShakeSubsystem, STATGROUP_Tickables);
}

void URTSCameraShakeSubsystem::RequestWeaponFireShake(const FRTSCameraShakeRequest& Request)
{
	QueueRequest(Request, Request.M_EventType == ERTSCameraShakeEventType::WeaponFire);
}

void URTSCameraShakeSubsystem::RequestExplosionShake(const FRTSCameraShakeRequest& Request)
{
	QueueRequest(Request, Request.M_EventType == ERTSCameraShakeEventType::Explosion);
}

void URTSCameraShakeSubsystem::QueueRequest(const FRTSCameraShakeRequest& Request, const bool bAllowEventType)
{
	if (not bAllowEventType)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	const URTSCameraShakeDeveloperSettings* Settings = GetDefault<URTSCameraShakeDeveloperSettings>();
	if (not IsValid(Settings))
	{
		return;
	}

	if (Request.M_EventType == ERTSCameraShakeEventType::WeaponFire && not Settings->bM_EnableWeaponFireShake)
	{
		return;
	}

	if (Request.M_EventType == ERTSCameraShakeEventType::Explosion && not Settings->bM_EnableExplosionShake)
	{
		return;
	}

	const float CurrentTimeSeconds = World->GetTimeSeconds();
	Tick_PruneRequestRateWindow(CurrentTimeSeconds);

	if (M_RequestTimesSeconds.Num() >= Settings->M_MaxRequestsPerSecond)
	{
		return;
	}

	if (M_AggregationWindowStartTimeSeconds < 0.0f)
	{
		M_AggregationWindowStartTimeSeconds = CurrentTimeSeconds;
	}

	M_PendingRequests.Add(Request);
	M_RequestTimesSeconds.Add(CurrentTimeSeconds);
}

void URTSCameraShakeSubsystem::Tick_PruneRequestRateWindow(const float CurrentTimeSeconds)
{
	constexpr float MaxTrackedWindowSeconds = 1.0f;
	for (int32 RequestIndex = M_RequestTimesSeconds.Num() - 1; RequestIndex >= 0; --RequestIndex)
	{
		const float RequestTime = M_RequestTimesSeconds[RequestIndex];
		if (CurrentTimeSeconds - RequestTime <= MaxTrackedWindowSeconds)
		{
			continue;
		}

		M_RequestTimesSeconds.RemoveAtSwap(RequestIndex);
	}
}

void URTSCameraShakeSubsystem::Tick_FlushAggregationWindow(const float CurrentTimeSeconds)
{
	if (M_PendingRequests.IsEmpty())
	{
		ResetAggregationWindow();
		return;
	}

	const URTSCameraShakeDeveloperSettings* Settings = GetDefault<URTSCameraShakeDeveloperSettings>();
	if (not IsValid(Settings))
	{
		ResetAggregationWindow();
		return;
	}

	if (CurrentTimeSeconds - M_AggregationWindowStartTimeSeconds < Settings->M_AggregationWindowSeconds)
	{
		return;
	}

	APlayerController* PlayerController = GetPrimaryPlayerController();
	if (not IsValid(PlayerController) || not IsValid(PlayerController->PlayerCameraManager))
	{
		ResetAggregationWindow();
		return;
	}

	FVector CameraLocation = FVector::ZeroVector;
	FRotator CameraRotation = FRotator::ZeroRotator;
	PlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);
	const FVector CameraForward = CameraRotation.Vector();
	const float HalfFovRadians = FMath::DegreesToRadians(PlayerController->PlayerCameraManager->GetFOVAngle() * 0.5f);
	const float CosHalfFov = FMath::Cos(HalfFovRadians);

	const float AggregatedIntensity = BuildAggregatedIntensity(CameraLocation, CameraForward, CosHalfFov);
	ResetAggregationWindow();
	if (AggregatedIntensity <= 0.0f)
	{
		return;
	}

	TryPlayAggregatedShake(CurrentTimeSeconds, PlayerController, AggregatedIntensity);
}

void URTSCameraShakeSubsystem::ResetAggregationWindow()
{
	M_PendingRequests.Reset();
	M_AggregationWindowStartTimeSeconds = -1.0f;
}

float URTSCameraShakeSubsystem::BuildAggregatedIntensity(const FVector& CameraLocation,
	const FVector& CameraForward,
	const float CosHalfFov) const
{
	float AggregatedIntensity = 0.0f;
	for (const FRTSCameraShakeRequest& Request : M_PendingRequests)
	{
		if (not GetIsRequestPassingGlobalThresholds(Request))
		{
			continue;
		}

		float DistanceMultiplier = 0.0f;
		float NormalizedCalibre = 0.0f;
		if (not GetIsRequestWithinDistanceRules(Request, CameraLocation, DistanceMultiplier, NormalizedCalibre))
		{
			continue;
		}

		if (not GetIsRequestPassingVisibilityRules(Request, CameraLocation, CameraForward, CosHalfFov))
		{
			continue;
		}

		const float EventWeight = GetEventSpecificWeight(Request.M_EventType);
		AggregatedIntensity += NormalizedCalibre * DistanceMultiplier * EventWeight;
	}

	return AggregatedIntensity;
}

void URTSCameraShakeSubsystem::TryPlayAggregatedShake(const float CurrentTimeSeconds,
	APlayerController* PlayerController,
	const float AggregatedIntensity)
{
	const URTSCameraShakeDeveloperSettings* Settings = GetDefault<URTSCameraShakeDeveloperSettings>();
	if (not IsValid(Settings))
	{
		return;
	}

	const float ScaledIntensity = AggregatedIntensity * Settings->M_GlobalIntensityMultiplier;
	const float ClampedIntensity = FMath::Clamp(ScaledIntensity, 0.0f, 2.0f);
	if (ClampedIntensity <= 0.0f)
	{
		return;
	}

	TSoftClassPtr<UCameraShakeBase> DesiredShakeClass = Settings->M_NormalShakeClass;
	const bool bIsHeavyShake = ClampedIntensity >= Settings->M_HeavyShakeThreshold;
	if (bIsHeavyShake)
	{
		if (CurrentTimeSeconds - M_LastHeavyShakeTimeSeconds < Settings->M_MinTimeBetweenHeavyShakesSeconds)
		{
			return;
		}

		if (Settings->M_HeavyShakeClass.IsNull())
		{
			DesiredShakeClass = Settings->M_NormalShakeClass;
		}
		else
		{
			DesiredShakeClass = Settings->M_HeavyShakeClass;
			M_LastHeavyShakeTimeSeconds = CurrentTimeSeconds;
		}
	}

	const TSubclassOf<UCameraShakeBase> LoadedShakeClass = DesiredShakeClass.LoadSynchronous();
	if (not IsValid(LoadedShakeClass))
	{
		return;
	}

	const UCameraShakeBase* ShakeClassDefaultObject = LoadedShakeClass->GetDefaultObject<UCameraShakeBase>();
	if (not IsValid(ShakeClassDefaultObject))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("RTSCameraShakeSubsystem failed to read camera shake default object. Verify shake class setup in RTS Camera Shake settings.")
		);
		return;
	}

	if (not IsValid(ShakeClassDefaultObject->GetRootShakePattern()))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("RTSCameraShakeSubsystem camera shake class has no Root Shake Pattern configured. Using CameraShakeBase directly will not produce visible shake. Assign a configured camera shake asset or subclass.")
		);
		return;
	}

	PlayerController->PlayerCameraManager->StartCameraShake(LoadedShakeClass, ClampedIntensity);
}

bool URTSCameraShakeSubsystem::GetIsRequestPassingGlobalThresholds(const FRTSCameraShakeRequest& Request) const
{
	const URTSCameraShakeDeveloperSettings* Settings = GetDefault<URTSCameraShakeDeveloperSettings>();
	if (not IsValid(Settings))
	{
		return false;
	}

	if (Request.M_EventType == ERTSCameraShakeEventType::WeaponFire)
	{
		return Request.M_CalibreMm >= Settings->M_MinWeaponFireCalibreMm;
	}

	return Request.M_CalibreMm >= Settings->M_MinExplosionCalibreMm;
}

bool URTSCameraShakeSubsystem::GetIsRequestWithinDistanceRules(const FRTSCameraShakeRequest& Request,
                                                              const FVector& CameraLocation,
                                                              float& OutDistanceMultiplier,
                                                              float& OutNormalizedCalibre) const
{
	OutDistanceMultiplier = 0.0f;
	OutNormalizedCalibre = 0.0f;

	const URTSCameraShakeDeveloperSettings* Settings = GetDefault<URTSCameraShakeDeveloperSettings>();
	if (not IsValid(Settings))
	{
		return false;
	}

	const float DistanceToEventCm = FVector::Dist(CameraLocation, Request.M_WorldLocation);
	if (DistanceToEventCm <= Settings->M_AlwaysShakeDistanceCm)
	{
		OutDistanceMultiplier = Settings->M_DistanceIntensityNearMultiplier;
		OutNormalizedCalibre = GetNormalizedCalibre(Request.M_CalibreMm);
		return true;
	}

	OutNormalizedCalibre = GetNormalizedCalibre(Request.M_CalibreMm);
	const float MaxDistanceForEventCm = GetEventSpecificDistanceForCalibre(OutNormalizedCalibre);
	if (DistanceToEventCm > MaxDistanceForEventCm)
	{
		// Event is ignored even if in view; too far for this calibre.
		return false;
	}

	const float DistanceAlpha = FMath::Clamp(DistanceToEventCm / MaxDistanceForEventCm, 0.0f, 1.0f);
	OutDistanceMultiplier = FMath::Lerp(Settings->M_DistanceIntensityNearMultiplier,
		Settings->M_DistanceIntensityFarMultiplier,
		DistanceAlpha);
	return true;
}

bool URTSCameraShakeSubsystem::GetIsRequestPassingVisibilityRules(const FRTSCameraShakeRequest& Request,
                                                                 const FVector& CameraLocation,
                                                                 const FVector& CameraForward,
                                                                 const float CosHalfFov) const
{
	const URTSCameraShakeDeveloperSettings* Settings = GetDefault<URTSCameraShakeDeveloperSettings>();
	if (not IsValid(Settings))
	{
		return false;
	}

	const bool bRequireInFov = Request.M_EventType == ERTSCameraShakeEventType::WeaponFire
		? Settings->bM_RequireInFovForWeaponFire
		: Settings->bM_RequireInFovForExplosion;
	if (not bRequireInFov)
	{
		return true;
	}

	const float DistanceToEventCm = FVector::Dist(CameraLocation, Request.M_WorldLocation);
	if (DistanceToEventCm <= Settings->M_AlwaysShakeDistanceCm)
	{
		return true;
	}

	if (Request.bM_HasOptimizationDistanceHint)
	{
		if (Request.M_OptimizationDistanceHint == ERTSOptimizationDistance::OutFOVFar)
		{
			return false;
		}
	}

	const FVector ToEvent = (Request.M_WorldLocation - CameraLocation).GetSafeNormal();
	const float DotToEvent = FVector::DotProduct(CameraForward, ToEvent);
	return DotToEvent >= CosHalfFov;
}

float URTSCameraShakeSubsystem::GetNormalizedCalibre(const int32 CalibreMm) const
{
	const URTSCameraShakeDeveloperSettings* Settings = GetDefault<URTSCameraShakeDeveloperSettings>();
	if (not IsValid(Settings))
	{
		return 0.0f;
	}

	const int32 SafeMaxCalibre = FMath::Max(1, Settings->M_MaxCalibreForNormalizationMm);
	const float CalibreNormalized = static_cast<float>(CalibreMm) / static_cast<float>(SafeMaxCalibre);
	return FMath::Clamp(CalibreNormalized, 0.0f, 1.0f);
}

float URTSCameraShakeSubsystem::GetEventSpecificWeight(const ERTSCameraShakeEventType EventType) const
{
	const URTSCameraShakeDeveloperSettings* Settings = GetDefault<URTSCameraShakeDeveloperSettings>();
	if (not IsValid(Settings))
	{
		return 0.0f;
	}

	if (EventType == ERTSCameraShakeEventType::WeaponFire)
	{
		return Settings->M_WeaponFireAggregationWeight;
	}

	return Settings->M_ExplosionAggregationWeight;
}

float URTSCameraShakeSubsystem::GetEventSpecificDistanceForCalibre(const float NormalizedCalibre) const
{
	const URTSCameraShakeDeveloperSettings* Settings = GetDefault<URTSCameraShakeDeveloperSettings>();
	if (not IsValid(Settings))
	{
		return 0.0f;
	}

	const float DistanceScale = FMath::Lerp(
		Settings->M_MinCalibreDistanceScale,
		Settings->M_MaxCalibreDistanceScale,
		NormalizedCalibre);
	return Settings->M_BaseMaxShakeDistanceCm * DistanceScale;
}

APlayerController* URTSCameraShakeSubsystem::GetPrimaryPlayerController() const
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return nullptr;
	}

	return UGameplayStatics::GetPlayerController(World, RTSCameraShakeConstants::LocalPlayerIndex);
}
