// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/RTSComponents/AOEBehaviourComponent/AOEBehaviourComponent.h"

#include "Async/Async.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Behaviours/BehaviourComp.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalText/Pooling/AnimatedTextWidgetPoolManager/AnimatedTextWidgetPoolManager.h"

UAOEBehaviourComponent::UAOEBehaviourComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	M_Settings.IntervalSeconds = AOEBehaviourComponentConstants::DefaultIntervalSeconds;
	M_Settings.Radius = AOEBehaviourComponentConstants::DefaultRadius;
	M_Settings.TextSettings.TextOffset = FVector(0.f, 0.f, AOEBehaviourComponentConstants::DefaultTextOffsetZ);
	M_Settings.TextSettings.InWrapAt = AOEBehaviourComponentConstants::DefaultWrapAt;
}

void UAOEBehaviourComponent::SetAOEEnabled(const bool bEnabled)
{
	bM_AOEEnabled = bEnabled;

	if (not bM_AOEEnabled)
	{
		UWorld* World = GetWorld();
		if (not IsValid(World))
		{
			return;
		}

		World->GetTimerManager().ClearTimer(M_AOEIntervalTimerHandle);
		return;
	}

	BeginPlay_StartAoeTimer();
}

void UAOEBehaviourComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return;
	}

	M_RTSComponent = Owner->FindComponentByClass<URTSComponent>();
	(void)GetIsValidRTSComponent();

	BeginPlay_SetupAnimatedTextWidgetPoolManager();
	BeginPlay_StartAoeTimer();
}

void UAOEBehaviourComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (const TWeakObjectPtr<UBehaviourComp>& BehaviourComponent : M_TrackedBehaviourComponents)
	{
		if (not BehaviourComponent.IsValid())
		{
			continue;
		}

		RemoveBehavioursFromTarget(*BehaviourComponent.Get());
	}

	M_TrackedBehaviourComponents.Empty();

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		Super::EndPlay(EndPlayReason);
		return;
	}

	World->GetTimerManager().ClearTimer(M_AOEIntervalTimerHandle);

	Super::EndPlay(EndPlayReason);
}

void UAOEBehaviourComponent::OnApplyTick(const TArray<UBehaviourComp*>& BehaviourComponentsInRange)
{
	static_cast<void>(BehaviourComponentsInRange);
}

void UAOEBehaviourComponent::OnBehavioursApplied(const TArray<UBehaviourComp*>& NewlyAffected)
{
	static_cast<void>(NewlyAffected);
}

void UAOEBehaviourComponent::OnBehavioursRemoved(const TArray<UBehaviourComp*>& NoLongerAffected)
{
	static_cast<void>(NoLongerAffected);
}

bool UAOEBehaviourComponent::IsValidTarget(AActor* ValidActor) const
{
	static_cast<void>(ValidActor);
	return true;
}

float UAOEBehaviourComponent::GetAOERadius() const
{
	return M_Settings.Radius;
}

const FAOEBehaviourSettings& UAOEBehaviourComponent::GetAoeBehaviourSettings() const
{
	return M_Settings;
}

void UAOEBehaviourComponent::BeginPlay_SetupAnimatedTextWidgetPoolManager()
{
	M_AnimatedTextWidgetPoolManager = FRTS_Statics::GetVerticalAnimatedTextWidgetPoolManager(this);

	if (not M_Settings.TextSettings.bUseText)
	{
		return;
	}

	if (not GetIsValidAnimatedTextWidgetPoolManager())
	{
		M_Settings.TextSettings.bUseText = false;
	}
}

void UAOEBehaviourComponent::BeginPlay_StartAoeTimer()
{
	if (not bM_AOEEnabled)
	{
		return;
	}

	if (not GetIsValidSettings())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		M_AOEIntervalTimerHandle,
		this,
		&UAOEBehaviourComponent::HandleApplyTick,
		M_Settings.IntervalSeconds,
		true);
}

void UAOEBehaviourComponent::HandleApplyTick()
{
	if (not GetIsValidSettings())
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return;
	}

	const FVector Epicenter = Owner->GetActorLocation();
	StartAsyncSphereSweep(Owner, Epicenter, M_Settings.Radius);
}

void UAOEBehaviourComponent::StartAsyncSphereSweep(
	AActor* InstigatorActor,
	const FVector& Epicenter,
	const float Radius)
{
	if (Radius <= 0.f || Radius < AOEBehaviourComponentConstants::MinimumSweepRadius)
	{
		return;
	}

	if (not IsValid(InstigatorActor))
	{
		return;
	}

	UWorld* World = InstigatorActor->GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	const ETriggerOverlapLogic OverlapLogic = GetOverlapLogicForOwner();
	const FCollisionObjectQueryParams ObjectQueryParams = BuildObjectQueryParams(OverlapLogic);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RTS_AOE_Behaviour_SphereSweep), false, InstigatorActor);
	QueryParams.AddIgnoredActor(InstigatorActor);

	const FCollisionShape SphereShape = FCollisionShape::MakeSphere(Radius);
	const TWeakObjectPtr<UAOEBehaviourComponent> WeakThis(this);

	FTraceDelegate TraceDelegate;
	TraceDelegate.BindLambda(
		[WeakThis](const FTraceHandle& /*TraceHandle*/, FTraceDatum& TraceDatum)
		{
			TArray<FHitResult> HitResults = MoveTemp(TraceDatum.OutHits);
			AsyncTask(
				ENamedThreads::GameThread,
				[WeakThis, HitResults = MoveTemp(HitResults)]() mutable
				{
					if (not WeakThis.IsValid())
					{
						return;
					}

					WeakThis->HandleSweepComplete(MoveTemp(HitResults));
				});
		});

	World->AsyncSweepByObjectType(
		EAsyncTraceType::Multi,
		Epicenter,
		Epicenter,
		FQuat::Identity,
		ObjectQueryParams,
		SphereShape,
		QueryParams,
		&TraceDelegate,
		/*UserData*/ 0u);
}

FCollisionObjectQueryParams UAOEBehaviourComponent::BuildObjectQueryParams(
	const ETriggerOverlapLogic OverlapLogic)
{
	FCollisionObjectQueryParams ObjectQueryParams;
	if (OverlapLogic == ETriggerOverlapLogic::OverlapEnemy || OverlapLogic == ETriggerOverlapLogic::OverlapBoth)
	{
		ObjectQueryParams.AddObjectTypesToQuery(COLLISION_OBJ_ENEMY);
	}

	if (OverlapLogic == ETriggerOverlapLogic::OverlapPlayer || OverlapLogic == ETriggerOverlapLogic::OverlapBoth)
	{
		ObjectQueryParams.AddObjectTypesToQuery(COLLISION_OBJ_PLAYER);
	}

	ObjectQueryParams.AddObjectTypesToQuery(ECC_Destructible);

	return ObjectQueryParams;
}

void UAOEBehaviourComponent::HandleSweepComplete(TArray<FHitResult>&& HitResults)
{
	TSet<TWeakObjectPtr<UBehaviourComp>> CurrentTargets;
	TArray<UBehaviourComp*> BehaviourComponentsInRange;
	for (const FHitResult& Hit : HitResults)
	{
		AActor* HitActor = Hit.GetActor();
		if (not IsValid(HitActor))
		{
			continue;
		}

		if (not IsValidTarget(HitActor))
		{
			continue;
		}

		UBehaviourComp* BehaviourComponent = HitActor->FindComponentByClass<UBehaviourComp>();
		if (not IsValid(BehaviourComponent))
		{
			continue;
		}

		CurrentTargets.Add(BehaviourComponent);
		BehaviourComponentsInRange.Add(BehaviourComponent);
	}

	TArray<UBehaviourComp*> AddedTargets;
	TArray<UBehaviourComp*> RemovedTargets;
	UpdateTrackedComponents(CurrentTargets, AddedTargets, RemovedTargets);

	for (UBehaviourComp* BehaviourComponent : AddedTargets)
	{
		if (not IsValid(BehaviourComponent))
		{
			continue;
		}

		ApplyBehavioursToTarget(*BehaviourComponent);
	}

	for (UBehaviourComp* BehaviourComponent : RemovedTargets)
	{
		if (not IsValid(BehaviourComponent))
		{
			continue;
		}

		RemoveBehavioursFromTarget(*BehaviourComponent);
	}

	OnBehavioursApplied(AddedTargets);
	OnBehavioursRemoved(RemovedTargets);
	OnApplyTick(BehaviourComponentsInRange);

	ApplyTextForTargets(BehaviourComponentsInRange);
	M_TrackedBehaviourComponents = MoveTemp(CurrentTargets);
}

void UAOEBehaviourComponent::ApplyBehavioursToTarget(UBehaviourComp& BehaviourComponent) const
{
	for (const TSubclassOf<UBehaviour>& BehaviourClass : M_Settings.BehaviourTypes)
	{
		if (not BehaviourClass)
		{
			continue;
		}

		BehaviourComponent.AddBehaviour(BehaviourClass);
	}
}

void UAOEBehaviourComponent::RemoveBehavioursFromTarget(UBehaviourComp& BehaviourComponent) const
{
	for (const TSubclassOf<UBehaviour>& BehaviourClass : M_Settings.BehaviourTypes)
	{
		if (not BehaviourClass)
		{
			continue;
		}

		BehaviourComponent.RemoveBehaviour(BehaviourClass);
	}
}

void UAOEBehaviourComponent::UpdateTrackedComponents(
	const TSet<TWeakObjectPtr<UBehaviourComp>>& CurrentTargets,
	TArray<UBehaviourComp*>& OutAddedTargets,
	TArray<UBehaviourComp*>& OutRemovedTargets)
{
	for (const TWeakObjectPtr<UBehaviourComp>& BehaviourComponent : CurrentTargets)
	{
		if (not BehaviourComponent.IsValid())
		{
			continue;
		}

		if (not M_TrackedBehaviourComponents.Contains(BehaviourComponent))
		{
			OutAddedTargets.Add(BehaviourComponent.Get());
		}
	}

	for (const TWeakObjectPtr<UBehaviourComp>& BehaviourComponent : M_TrackedBehaviourComponents)
	{
		if (not BehaviourComponent.IsValid())
		{
			continue;
		}

		if (not CurrentTargets.Contains(BehaviourComponent))
		{
			OutRemovedTargets.Add(BehaviourComponent.Get());
		}
	}
}

void UAOEBehaviourComponent::ApplyTextForTargets(const TArray<UBehaviourComp*>& BehaviourComponentsInRange) const
{
	if (not M_Settings.TextSettings.bUseText)
	{
		return;
	}

	if (not GetIsValidAnimatedTextWidgetPoolManager())
	{
		return;
	}

	for (UBehaviourComp* BehaviourComponent : BehaviourComponentsInRange)
	{
		if (not IsValid(BehaviourComponent))
		{
			continue;
		}

		AActor* TargetActor = BehaviourComponent->GetOwner();
		if (not IsValid(TargetActor))
		{
			continue;
		}

		M_AnimatedTextWidgetPoolManager->ShowAnimatedTextAttachedToActor(
			M_Settings.TextSettings.TextOnSubjects,
			TargetActor,
			M_Settings.TextSettings.TextOffset,
			M_Settings.TextSettings.bAutoWrap,
			M_Settings.TextSettings.InWrapAt,
			M_Settings.TextSettings.InJustification,
			M_Settings.TextSettings.InSettings);
	}
}

bool UAOEBehaviourComponent::GetIsValidRTSComponent() const
{
	if (not IsValid(M_RTSComponent))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			"M_RTSComponent",
			"GetIsValidRTSComponent",
			GetOwner());
		return false;
	}

	return true;
}

bool UAOEBehaviourComponent::GetIsValidAnimatedTextWidgetPoolManager() const
{
	if (not M_AnimatedTextWidgetPoolManager.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			"M_AnimatedTextWidgetPoolManager",
			"GetIsValidAnimatedTextWidgetPoolManager",
			GetOwner());
		return false;
	}

	return true;
}

bool UAOEBehaviourComponent::GetIsValidSettings() const
{
	if (M_Settings.IntervalSeconds <= 0.f)
	{
		RTSFunctionLibrary::ReportError(TEXT("AOEBehaviourComponent: IntervalSeconds must be > 0."));
		return false;
	}

	if (M_Settings.Radius <= 0.f)
	{
		RTSFunctionLibrary::ReportError(TEXT("AOEBehaviourComponent: Radius must be > 0."));
		return false;
	}

	if (M_Settings.Radius < AOEBehaviourComponentConstants::MinimumSweepRadius)
	{
		RTSFunctionLibrary::ReportError(TEXT("AOEBehaviourComponent: Radius is below minimum async sweep size."));
		return false;
	}

	if (M_Settings.BehaviourTypes.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(TEXT("AOEBehaviourComponent: BehaviourTypes must have at least one entry."));
		return false;
	}

	return true;
}

ETriggerOverlapLogic UAOEBehaviourComponent::GetOverlapLogicForOwner() const
{
	if (not GetIsValidRTSComponent())
	{
		return ETriggerOverlapLogic::OverlapEnemy;
	}

	return M_RTSComponent->GetOwningPlayer() == 1
		       ? ETriggerOverlapLogic::OverlapEnemy
		       : ETriggerOverlapLogic::OverlapPlayer;
}
