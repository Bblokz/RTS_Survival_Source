// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FieldMine.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalText/Pooling/AnimatedTextWidgetPoolManager/AnimatedTextWidgetPoolManager.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalText/Pooling/WorldSubSystem/AnimatedTextWorldSubsystem.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalText/RTSVerticalAnimatedText/RTSVerticalAnimatedText.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Utils/AOE/FRTS_AOE.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"
#include "TimerManager.h"
#include "RTS_Survival/Navigation/RTSNavAgents/IRTSNavAgent/IRTSNavAgent.h"

AFieldMine::AFieldMine(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void AFieldMine::BeginPlay()
{
	Super::BeginPlay();

	BeginPlay_DisableMineMeshCollision();
	BeginPlay_CacheAnimatedTextSubsystem();
	BeginPlay_SetupTriggerSphere();
}

void AFieldMine::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	M_RTSComponent = FindComponentByClass<URTSComponent>();
	M_TriggerSphere = FindComponentByClass<USphereComponent>();
}

void AFieldMine::BeginPlay_DisableMineMeshCollision()
{
	if (not GetIsValidFieldConstructionMesh())
	{
		return;
	}

	FieldConstructionMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FieldConstructionMesh->SetCanEverAffectNavigation(false);
	FieldConstructionMesh->SetReceivesDecals(false);
}

void AFieldMine::BeginPlay_CacheAnimatedTextSubsystem()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportError(TEXT("AFieldMine::BeginPlay_CacheAnimatedTextSubsystem - World is invalid."));
		return;
	}

	UAnimatedTextWorldSubsystem* Subsystem = World->GetSubsystem<UAnimatedTextWorldSubsystem>();
	if (not IsValid(Subsystem))
	{
		RTSFunctionLibrary::ReportError(TEXT("AFieldMine failed to cache AnimatedTextWorldSubsystem."));
		return;
	}

	M_AnimatedTextSubsystem = Subsystem;
}

void AFieldMine::BeginPlay_SetupTriggerSphere()
{
	if (not GetIsValidRTSComponent())
	{
		return;
	}

	if (not GetIsValidTriggerSphere())
	{
		return;
	}

	const int32 OwningPlayer = M_RTSComponent->GetOwningPlayer();
	FRTS_CollisionSetup::SetupFieldMineTriggerCollision(M_TriggerSphere, OwningPlayer);
	M_TriggerSphere->OnComponentBeginOverlap.AddDynamic(this, &AFieldMine::OnMineTriggerOverlap);
}

void AFieldMine::OnMineTriggerOverlap(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor,
                                      UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/,
                                      const FHitResult& /*SweepResult*/)
{
	if (bM_HasTriggered)
	{
		return;
	}

	if (not IsValid(OtherActor))
	{
		return;
	}

	if (OtherActor == this)
	{
		return;
	}

	if (not DoesNavAgentTriggerMine(*OtherActor))
	{
		return;
	}

	TriggerMineForActor(*OtherActor);
}

void AFieldMine::TriggerMineForActor(AActor& TriggeringActor)
{
	bM_HasTriggered = true;
	M_TargetActor = &TriggeringActor;
	M_CachedDamage = CalculateMineDamage();

	if (GetIsValidTriggerSphere())
	{
		M_TriggerSphere->SetGenerateOverlapEvents(false);
		M_TriggerSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	const float TriggerDelaySeconds = PlayTriggerSound();
	if (TriggerDelaySeconds <= 0.f)
	{
		HandleMineDetonation();
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportError(TEXT("AFieldMine::TriggerMineForActor - World invalid when arming mine."));
		return;
	}

	World->GetTimerManager().SetTimer(
		M_DetonationTimerHandle,
		this,
		&AFieldMine::HandleMineDetonation,
		TriggerDelaySeconds,
		false);
}

void AFieldMine::HandleMineDetonation()
{
	if (M_TargetActor.IsValid())
	{
		ApplyDirectDamage(*M_TargetActor.Get());
		ShowMineDamageText(M_TargetActor->GetActorLocation());
	}

	ApplyAoeDamage(GetActorLocation());
	SpawnCollapseFx();

	Destroy();
}

bool AFieldMine::DoesNavAgentTriggerMine(const AActor& OtherActor) const
{
	const IRTSNavAgentInterface* NavAgentInterface = Cast<IRTSNavAgentInterface>(&OtherActor);
	if (not NavAgentInterface)
	{
		return false;
	}

	return ShouldTriggerForNavAgent(*NavAgentInterface);
}

bool AFieldMine::ShouldTriggerForNavAgent(const IRTSNavAgentInterface& NavAgentInterface) const
{
	const ERTSNavAgents NavAgentType = NavAgentInterface.GetRTSNavAgentType();
	if (MineSettings.TriggeringNavAgents.Num() <= 0)
	{
		return true;
	}

	return MineSettings.TriggeringNavAgents.Contains(NavAgentType);
}

ETriggerOverlapLogic AFieldMine::GetOverlapLogicForMineOwner() const
{
	if (not GetIsValidRTSComponent())
	{
		return ETriggerOverlapLogic::OverlapEnemy;
	}

	return M_RTSComponent->GetOwningPlayer() == 1
		       ? ETriggerOverlapLogic::OverlapEnemy
		       : ETriggerOverlapLogic::OverlapPlayer;
}

float AFieldMine::PlayTriggerSound() const
{
	if (not IsValid(MineSettings.TriggerSound))
	{
		return 0.f;
	}

	const float TriggerDuration = FMath::Max(MineSettings.TriggerSound->GetDuration(), 0.f);
	UGameplayStatics::PlaySoundAtLocation(
		this,
		MineSettings.TriggerSound,
		GetActorLocation(),
		FRotator::ZeroRotator,
		1.f,
		1.f,
		0.f,
		MineSettings.SoundAttenuation,
		MineSettings.SoundConcurrency);

	return TriggerDuration;
}

float AFieldMine::CalculateMineDamage() const
{
	using DeveloperSettings::GameBalance::Weapons::DamageFluxPercentage;
	return FRTSWeaponHelpers::GetDamageWithFlux(
		MineSettings.AverageDamage,
		static_cast<int32>(DamageFluxPercentage));
}

void AFieldMine::ApplyDirectDamage(AActor& TargetActor)
{
	if (M_CachedDamage <= 0.f)
	{
		return;
	}

	FDamageEvent DamageEvent = FRTSWeaponHelpers::MakeBasicDamageEvent(ERTSDamageType::Kinetic);
	TargetActor.TakeDamage(
		M_CachedDamage,
		DamageEvent,
		GetInstigatorController(),
		this);
}

void AFieldMine::ApplyAoeDamage(const FVector& Epicenter)
{
	if (MineSettings.AOERange <= 0.f)
	{
		return;
	}

	if (MineSettings.AOEDamage <= 0.f)
	{
		return;
	}

	const TArray<TWeakObjectPtr<AActor>> ActorsToIgnore = BuildActorsToIgnore();

	FRTS_AOE::DealDamageInRadiusAsync(
		this,
		Epicenter,
		MineSettings.AOERange,
		MineSettings.AOEDamage,
		MineSettings.AOEFallOffScaler,
		ERTSDamageType::Kinetic,
		GetOverlapLogicForMineOwner(),
		ActorsToIgnore);
}

void AFieldMine::ShowMineDamageText(const FVector& TextLocation) const
{
	if (not GetIsValidAnimatedTextSubsystem())
	{
		return;
	}

	UAnimatedTextWidgetPoolManager* AnimatedTextPoolManager = M_AnimatedTextSubsystem->GetAnimatedTextWidgetPoolManager();
	if (not IsValid(AnimatedTextPoolManager))
	{
		RTSFunctionLibrary::ReportError(TEXT("AFieldMine missing AnimatedTextWidgetPoolManager."));
		return;
	}

	const int32 RoundedDamage = FMath::RoundToInt(M_CachedDamage);
	const FString DamageText = FString::Printf(TEXT("<Text_Bad14>%d Mine Damage</>"), RoundedDamage);
	constexpr bool bAutoWrap = false;
	constexpr float WrapWidth = 0.f;
	const TEnumAsByte<ETextJustify::Type> Justification = ETextJustify::Type::Center;
	const FRTSVerticalAnimTextSettings TextSettings;

	AnimatedTextPoolManager->ShowAnimatedText(
		DamageText,
		TextLocation,
		bAutoWrap,
		WrapWidth,
		Justification,
		TextSettings);
}

void AFieldMine::SpawnCollapseFx() const
{
	MineSettings.CollapseFX.CreateFX(this);
}

TArray<TWeakObjectPtr<AActor>> AFieldMine::BuildActorsToIgnore() const
{
	TArray<TWeakObjectPtr<AActor>> ActorsToIgnore;
	ActorsToIgnore.Add(const_cast<AFieldMine*>(this));

	if (M_TargetActor.IsValid())
	{
		ActorsToIgnore.Add(M_TargetActor);
	}

	return ActorsToIgnore;
}

bool AFieldMine::GetIsValidRTSComponent() const
{
	if (IsValid(M_RTSComponent))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_RTSComponent",
		"AFieldMine::GetIsValidRTSComponent",
		this);
	return false;
}

bool AFieldMine::GetIsValidTriggerSphere() const
{
	if (IsValid(M_TriggerSphere))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_TriggerSphere",
		"AFieldMine::GetIsValidTriggerSphere",
		this);
	return false;
}

bool AFieldMine::GetIsValidAnimatedTextSubsystem() const
{
	if (M_AnimatedTextSubsystem.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(TEXT("AFieldMine: AnimatedTextWorldSubsystem not cached correctly."));
	return false;
}
