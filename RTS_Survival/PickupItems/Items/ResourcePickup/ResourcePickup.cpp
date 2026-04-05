// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "ResourcePickup.h"

#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Components/PrimitiveComponent.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalText/Pooling/AnimatedTextWidgetPoolManager/AnimatedTextWidgetPoolManager.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

AResourcePickup::AResourcePickup(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
}

void AResourcePickup::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	PostInitialize_SetupTriggerCircle();
}

void AResourcePickup::BeginPlay()
{
	Super::BeginPlay();

	BeginPlay_CachePlayerResourceManager();
	BeginPlay_CacheAnimatedTextManager();
}

void AResourcePickup::PostInitialize_SetupTriggerCircle()
{
	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (not IsValid(PrimitiveComponent))
		{
			continue;
		}

		const FString ComponentName = PrimitiveComponent->GetName();
		if (not ComponentName.Contains(TEXT("Circle")))
		{
			continue;
		}

		M_TriggerCircle = PrimitiveComponent;
		break;
	}

	if (not GetIsValidTriggerCircle())
	{
		return;
	}

	FRTS_CollisionSetup::SetupTriggerOverlapCollision(M_TriggerCircle, ETriggerOverlapLogic::OverlapPlayer);
	M_TriggerCircle->OnComponentBeginOverlap.AddDynamic(this, &AResourcePickup::OnTriggerBeginOverlap);
}

void AResourcePickup::BeginPlay_CachePlayerResourceManager()
{
	M_PlayerResourceManager = FRTS_Statics::GetPlayerResourceManager(this);
	(void)GetIsValidPlayerResourceManager();
}

void AResourcePickup::BeginPlay_CacheAnimatedTextManager()
{
	M_AnimatedTextManager = FRTS_Statics::GetVerticalAnimatedTextWidgetPoolManager(this);
}

void AResourcePickup::OnTriggerBeginOverlap(UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/)
{
	if (bM_HasBeenCollected)
	{
		return;
	}

	if (not GetIsItemEnabled())
	{
		return;
	}

	if (not IsValid(OtherActor) || OtherActor == this)
	{
		return;
	}

	URTSComponent* RTSComponent = OtherActor->FindComponentByClass<URTSComponent>();
	if (not IsValid(RTSComponent))
	{
		return;
	}

	if (RTSComponent->GetOwningPlayer() != 1)
	{
		return;
	}

	HandlePickupByPlayerUnit(*OtherActor);
}

void AResourcePickup::HandlePickupByPlayerUnit(AActor& /*OverlappingActor*/)
{
	if (not GrantResourcesToPlayer())
	{
		return;
	}

	bM_HasBeenCollected = true;
	SetItemDisabled();

	PlayPickupSoundAtActorLocation();
	SpawnPickupNiagaraAtActorLocation();
	ShowVerticalResourceGainText();
	Destroy();
}

bool AResourcePickup::GrantResourcesToPlayer() const
{
	if (not GetIsValidPlayerResourceManager())
	{
		return false;
	}

	UPlayerResourceManager* const PlayerResourceManager = M_PlayerResourceManager.Get();
	if (not IsValid(PlayerResourceManager))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			TEXT("M_PlayerResourceManager"),
			TEXT("GrantResourcesToPlayer"),
			this);
		return false;
	}

	for (const TPair<ERTSResourceType, int32>& ResourceCostPair : PickupSettings.ResourceGain.ResourceCosts)
	{
		if (ResourceCostPair.Value <= 0)
		{
			continue;
		}

		if (not PlayerResourceManager->AddResource(ResourceCostPair.Key, ResourceCostPair.Value))
		{
			RTSFunctionLibrary::ReportError(
				TEXT("AResourcePickup failed to add resource amount to player.")
				TEXT("\n Function: GrantResourcesToPlayer")
				TEXT("\n Actor: ") + GetName());
			return false;
		}
	}

	return true;
}

void AResourcePickup::PlayPickupSoundAtActorLocation() const
{
	if (not IsValid(PickupSettings.PickupSound))
	{
		return;
	}

	UGameplayStatics::PlaySoundAtLocation(
		this,
		PickupSettings.PickupSound,
		GetActorLocation(),
		FRotator::ZeroRotator,
		1.f,
		1.f,
		0.f,
		PickupSettings.PickupSoundAttenuation,
		PickupSettings.PickupSoundConcurrency);
}

void AResourcePickup::SpawnPickupNiagaraAtActorLocation() const
{
	if (not IsValid(PickupSettings.PickupNiagaraSystem))
	{
		return;
	}

	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(),
		PickupSettings.PickupNiagaraSystem,
		GetActorLocation());
}

void AResourcePickup::ShowVerticalResourceGainText() const
{
	if (not GetIsValidAnimatedTextManager())
	{
		return;
	}

	constexpr ERTSResourceType DisplayOrder[] = {
		ERTSResourceType::Resource_Radixite,
		ERTSResourceType::Resource_Metal,
		ERTSResourceType::Resource_VehicleParts
	};

	int32 DisplayLineIndex = 0;
	for (const ERTSResourceType ResourceType : DisplayOrder)
	{
		const int32 ResourceAmount = GetConfiguredResourceAmount(ResourceType);
		if (ResourceAmount <= 0)
		{
			continue;
		}

		FRTSVerticalSingleResourceTextSettings ResourceTextSettings;
		ResourceTextSettings.ResourceType = ResourceType;
		ResourceTextSettings.AddOrSubtractAmount = ResourceAmount;

		const FVector TextLocation = GetActorLocation()
			+ PickupSettings.VerticalTextOffset
			+ FVector(0.f, 0.f, PickupSettings.VerticalTextLineSpacing * DisplayLineIndex);

		(void)M_AnimatedTextManager->ShowSingleAnimatedResourceText(
			ResourceTextSettings,
			TextLocation,
			true,
			300.f,
			ETextJustify::Center,
			PickupSettings.VerticalTextSettings);

		++DisplayLineIndex;
	}
}

int32 AResourcePickup::GetConfiguredResourceAmount(const ERTSResourceType ResourceType) const
{
	if (const int32* ResourceAmount = PickupSettings.ResourceGain.ResourceCosts.Find(ResourceType))
	{
		return *ResourceAmount;
	}

	return 0;
}

bool AResourcePickup::GetIsValidTriggerCircle() const
{
	if (IsValid(M_TriggerCircle))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("M_TriggerCircle"),
		TEXT("GetIsValidTriggerCircle"),
		this);

	return false;
}

bool AResourcePickup::GetIsValidPlayerResourceManager() const
{
	if (M_PlayerResourceManager.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("M_PlayerResourceManager"),
		TEXT("GetIsValidPlayerResourceManager"),
		this);

	return false;
}

bool AResourcePickup::GetIsValidAnimatedTextManager() const
{
	if (M_AnimatedTextManager.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportWarning(
		TEXT("AResourcePickup has no AnimatedTextWidgetPoolManager cached; skipping resource pickup text."));

	return false;
}
