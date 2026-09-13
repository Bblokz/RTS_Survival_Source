#include "AnimatedIconWidgetPoolManager.h"

#include "AnimatedIconDataAsset.h"
#include "AnimatedIconPoolActor.h"
#include "AnimatedIconSettings.h"
#include "RTSVerticalAnimatedIcon.h"
#include "Components/WidgetComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

bool UAnimatedIconWidgetPoolManager::Init(UWorld* World)
{
	Shutdown();
	if (not IsValid(World) || not World->IsGameWorld() || World->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	M_World = World;
	if (not Init_LoadCatalog() || not Init_PreloadIconAssets() || not Init_CreatePool())
	{
		Shutdown();
		return false;
	}

	bM_IsReady = true;
	return true;
}

bool UAnimatedIconWidgetPoolManager::Init_LoadCatalog()
{
	const UAnimatedIconSettings* Settings = GetDefault<UAnimatedIconSettings>();
	if (Settings->IconDataAsset.IsNull())
	{
		// An unconfigured catalog disables this optional cosmetic subsystem.
		return false;
	}

	M_CatalogLoadHandle = UAssetManager::GetStreamableManager().RequestSyncLoad(
		Settings->IconDataAsset.ToSoftObjectPath());
	M_IconDataAsset = Settings->IconDataAsset.Get();
	return GetIsValidIconDataAsset();
}

bool UAnimatedIconWidgetPoolManager::Init_PreloadIconAssets()
{
	if (not GetIsValidIconDataAsset())
	{
		return false;
	}

	TArray<FSoftObjectPath> AssetPaths;
	AssetPaths.Add(M_IconDataAsset->WidgetClass.ToSoftObjectPath());
	for (const TPair<ERTSVerticalAnimatedIcon, FRTSVerticalAnimatedIconDefinition>& Entry : M_IconDataAsset->IconDefinitions)
	{
		if (Entry.Key == ERTSVerticalAnimatedIcon::None || Entry.Value.Texture.IsNull())
		{
			continue;
		}
		AssetPaths.AddUnique(Entry.Value.Texture.ToSoftObjectPath());
	}

	M_IconAssetsLoadHandle = UAssetManager::GetStreamableManager().RequestSyncLoad(AssetPaths);
	UClass* WidgetClass = M_IconDataAsset->WidgetClass.Get();
	if (not IsValid(WidgetClass) || not WidgetClass->IsChildOf(UW_RTSVerticalAnimatedIcon::StaticClass())
		|| WidgetClass->HasAnyClassFlags(CLASS_Abstract))
	{
		RTSFunctionLibrary::ReportError(TEXT("Animated Icons: catalog needs a non-abstract vertical icon widget class."));
		return false;
	}
	return true;
}

bool UAnimatedIconWidgetPoolManager::Init_CreatePool()
{
	if (not GetIsValidWorld() || not GetIsValidIconDataAsset())
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags = RF_Transient;
	M_OwnerActor = M_World->SpawnActor<AAnimatedIconPoolActor>(AAnimatedIconPoolActor::StaticClass(),
		FTransform::Identity, SpawnParameters);
	if (not GetIsValidOwnerActor())
	{
		return false;
	}

	const int32 PoolSize = FMath::Max(1, M_IconDataAsset->PoolSize);
	M_Instances.Reserve(PoolSize);
	M_FreeIndices.Reserve(PoolSize);
	M_ActiveIndices.Reserve(PoolSize);
	for (int32 InstanceIndex = 0; InstanceIndex < PoolSize; ++InstanceIndex)
	{
		if (not Init_AddPoolInstance(M_IconDataAsset->WidgetClass.Get()))
		{
			return false;
		}
	}
	return true;
}

bool UAnimatedIconWidgetPoolManager::Init_AddPoolInstance(UClass* WidgetClass)
{
	if (not GetIsValidOwnerActor())
	{
		return false;
	}

	UWidgetComponent* Component = M_OwnerActor->CreateIconComponent(WidgetClass);
	if (not IsValid(Component))
	{
		return false;
	}
	UW_RTSVerticalAnimatedIcon* Widget = Cast<UW_RTSVerticalAnimatedIcon>(Component->GetUserWidgetObject());
	if (not IsValid(Widget))
	{
		RTSFunctionLibrary::ReportError(TEXT("Animated Icons: failed to initialize the pooled widget."));
		return false;
	}
	if (not Widget->IsLayoutReady())
	{
		return false;
	}

	FAnimatedIconPoolInstance Instance;
	Instance.Component = Component;
	Instance.Widget = Widget;
	Instance.CachedSlateWidget = Widget->TakeWidget();
	Widget->SetDormant();
	const int32 InstanceIndex = M_Instances.Add(Instance);
	M_FreeIndices.Add(InstanceIndex);
	return true;
}

void UAnimatedIconWidgetPoolManager::Shutdown()
{
	bM_IsReady = false;
	for (int32 InstanceIndex = 0; InstanceIndex < M_Instances.Num(); ++InstanceIndex)
	{
		ResetInstance(InstanceIndex);
	}
	M_Instances.Reset();
	M_ActiveIndices.Reset();
	M_FreeIndices.Reset();
	M_ActivationCounter = 0;

	AAnimatedIconPoolActor* OwnerActor = M_OwnerActor.Get();
	if (IsValid(OwnerActor))
	{
		OwnerActor->Destroy();
	}
	M_OwnerActor.Reset();
	M_IconDataAsset.Reset();
	M_IconAssetsLoadHandle.Reset();
	M_CatalogLoadHandle.Reset();
	M_World.Reset();
}

void UAnimatedIconWidgetPoolManager::Tick()
{
	if (not bM_IsReady || bM_IsUpdatingPool || M_ActiveIndices.IsEmpty() || not GetIsValidWorld())
	{
		return;
	}

	TGuardValue<bool> UpdateGuard(bM_IsUpdatingPool, true);
	const double NowSeconds = M_World->GetTimeSeconds();
	for (int32 ActiveListIndex = M_ActiveIndices.Num() - 1; ActiveListIndex >= 0; --ActiveListIndex)
	{
		if (not AnimateInstance(M_ActiveIndices[ActiveListIndex], NowSeconds))
		{
			ReleaseActiveInstance(ActiveListIndex);
		}
	}
}

bool UAnimatedIconWidgetPoolManager::AnimateInstance(const int32 InstanceIndex, const double NowSeconds)
{
	if (not GetIsValidInstance(InstanceIndex))
	{
		return false;
	}

	FAnimatedIconPoolInstance& Instance = M_Instances[InstanceIndex];
	AActor* AttachedActor = Instance.AttachedActor.Get();
	if (Instance.bAttachedToActor && (not IsValid(AttachedActor) || not IsValid(AttachedActor->GetRootComponent())))
	{
		return false;
	}

	float WorldOffsetZ = 0.0f;
	float Opacity = 0.0f;
	if (not Instance.AnimationSettings.Evaluate(NowSeconds - Instance.ActivatedAtSeconds, WorldOffsetZ, Opacity))
	{
		return false;
	}

	Instance.Widget->SetRenderOpacity(Opacity);
	if (Instance.bAttachedToActor)
	{
		// Matches the text system: inherit subject movement, then add this frame's world-Z travel.
		Instance.Component->AddWorldOffset(FVector(0.0, 0.0, WorldOffsetZ - Instance.LastAppliedWorldOffsetZ));
	}
	else
	{
		Instance.Component->SetWorldLocation(Instance.StartWorldLocation + FVector(0.0, 0.0, WorldOffsetZ));
	}
	Instance.LastAppliedWorldOffsetZ = WorldOffsetZ;
	return true;
}

void UAnimatedIconWidgetPoolManager::ResetInstance(const int32 InstanceIndex)
{
	if (not M_Instances.IsValidIndex(InstanceIndex))
	{
		return;
	}

	FAnimatedIconPoolInstance& Instance = M_Instances[InstanceIndex];
	UWidgetComponent* Component = Instance.Component.Get();
	if (IsValid(Component))
	{
		Component->SetVisibility(false);
		Component->SetHiddenInGame(true);
		Component->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		Component->SetWorldTransform(FTransform::Identity);
	}
	UW_RTSVerticalAnimatedIcon* Widget = Instance.Widget.Get();
	if (IsValid(Widget))
	{
		Widget->SetDormant();
	}
	Instance.AttachedActor.Reset();
	Instance.AnimationSettings = FRTSVerticalAnimIconSettings();
	Instance.StartWorldLocation = FVector::ZeroVector;
	Instance.ActivatedAtSeconds = 0.0;
	Instance.LastAppliedWorldOffsetZ = 0.0f;
	Instance.bAttachedToActor = false;
}

void UAnimatedIconWidgetPoolManager::ReleaseActiveInstance(const int32 ActiveListIndex)
{
	if (not M_ActiveIndices.IsValidIndex(ActiveListIndex))
	{
		return;
	}
	const int32 InstanceIndex = M_ActiveIndices[ActiveListIndex];
	ResetInstance(InstanceIndex);
	M_ActiveIndices.RemoveAtSwap(ActiveListIndex, 1, EAllowShrinking::No);
	if (GetIsValidInstance(InstanceIndex))
	{
		M_FreeIndices.Add(InstanceIndex);
	}
}

const FRTSVerticalAnimatedIconDefinition* UAnimatedIconWidgetPoolManager::FindDefinition(
	const ERTSVerticalAnimatedIcon IconType) const
{
	if (IconType == ERTSVerticalAnimatedIcon::None || not bM_IsReady || not GetIsValidIconDataAsset())
	{
		return nullptr;
	}

	const FRTSVerticalAnimatedIconDefinition* Definition = M_IconDataAsset->IconDefinitions.Find(IconType);
	if (Definition == nullptr || not IsValid(Definition->Texture.Get()))
	{
		RTSFunctionLibrary::ReportError(FString::Printf(TEXT("Animated Icons: missing resident texture for %s."),
			*UEnum::GetValueAsString(IconType)));
		return nullptr;
	}
	if (Definition->DisplaySize.ContainsNaN() || Definition->DisplaySize.X <= 0.0
		|| Definition->DisplaySize.Y <= 0.0 || not FMath::IsFinite(Definition->Tint.R)
		|| not FMath::IsFinite(Definition->Tint.G) || not FMath::IsFinite(Definition->Tint.B)
		|| not FMath::IsFinite(Definition->Tint.A))
	{
		RTSFunctionLibrary::ReportError(TEXT("Animated Icons: display size and tint must be finite; size must be positive."));
		return nullptr;
	}
	return Definition;
}

int32 UAnimatedIconWidgetPoolManager::FindOldestActiveIndex() const
{
	int32 OldestIndex = INDEX_NONE;
	uint64 OldestActivation = TNumericLimits<uint64>::Max();
	for (const int32 InstanceIndex : M_ActiveIndices)
	{
		if (not M_Instances.IsValidIndex(InstanceIndex))
		{
			continue;
		}
		if (M_Instances[InstanceIndex].ActivationOrder < OldestActivation)
		{
			OldestActivation = M_Instances[InstanceIndex].ActivationOrder;
			OldestIndex = InstanceIndex;
		}
	}
	return OldestIndex;
}

bool UAnimatedIconWidgetPoolManager::ShowIconInternal(const ERTSVerticalAnimatedIcon IconType,
	const FVector& Location, AActor* AttachActor, const FRTSVerticalAnimIconSettings* OverrideSettings)
{
	if (bM_IsUpdatingPool)
	{
		return false;
	}
	TGuardValue<bool> UpdateGuard(bM_IsUpdatingPool, true);
	const FRTSVerticalAnimatedIconDefinition* FoundDefinition = FindDefinition(IconType);
	if (FoundDefinition == nullptr || Location.ContainsNaN() || not GetIsValidWorld() || not GetIsValidOwnerActor())
	{
		return false;
	}
	// Copy before widget calls so no reference into the catalog map spans activation.
	const FRTSVerticalAnimatedIconDefinition Definition = *FoundDefinition;
	const FRTSVerticalAnimIconSettings AnimationSettings = OverrideSettings == nullptr
		? Definition.DefaultAnimationSettings : *OverrideSettings;
	if (not AnimationSettings.IsUsable())
	{
		RTSFunctionLibrary::ReportError(TEXT("Animated Icons: durations must be finite, nonnegative and total more than zero."));
		return false;
	}
	if (AttachActor != nullptr && (not IsValid(AttachActor) || not IsValid(AttachActor->GetRootComponent())
		|| AttachActor->GetWorld() != M_World.Get()))
	{
		return false;
	}

	const bool bFromFreeList = not M_FreeIndices.IsEmpty();
	const int32 InstanceIndex = bFromFreeList ? M_FreeIndices.Last() : FindOldestActiveIndex();
	if (InstanceIndex == INDEX_NONE || not GetIsValidInstance(InstanceIndex))
	{
		return false;
	}
	if (not ActivateInstance(InstanceIndex, Definition, Location, AttachActor, AnimationSettings))
	{
		ResetInstance(InstanceIndex);
		if (not bFromFreeList)
		{
			ReleaseActiveInstance(M_ActiveIndices.Find(InstanceIndex));
		}
		return false;
	}
	if (bFromFreeList)
	{
		M_FreeIndices.Pop(EAllowShrinking::No);
		M_ActiveIndices.Add(InstanceIndex);
	}
	return true;
}

bool UAnimatedIconWidgetPoolManager::ActivateInstance(const int32 InstanceIndex,
	const FRTSVerticalAnimatedIconDefinition& Definition, const FVector& Location, AActor* AttachActor,
	const FRTSVerticalAnimIconSettings& AnimationSettings)
{
	ResetInstance(InstanceIndex);
	FAnimatedIconPoolInstance& Instance = M_Instances[InstanceIndex];
	USceneComponent* AttachRoot = AttachActor == nullptr ? M_OwnerActor->GetRootComponent() : AttachActor->GetRootComponent();
	if (not IsValid(AttachRoot))
	{
		return false;
	}
	if (not Instance.Component->AttachToComponent(AttachRoot, FAttachmentTransformRules::KeepWorldTransform))
	{
		return false;
	}
	if (AttachActor != nullptr)
	{
		Instance.Component->SetRelativeLocation(Location);
	}
	else
	{
		Instance.Component->SetWorldLocation(Location);
	}
	if (not Instance.Widget->ActivateIcon(Definition, Definition.Texture.Get()))
	{
		return false;
	}
	Instance.AttachedActor = AttachActor;
	Instance.bAttachedToActor = AttachActor != nullptr;
	Instance.StartWorldLocation = Instance.Component->GetComponentLocation();
	Instance.AnimationSettings = AnimationSettings;
	Instance.ActivatedAtSeconds = M_World->GetTimeSeconds();
	Instance.ActivationOrder = ++M_ActivationCounter;
	Instance.Component->SetHiddenInGame(false);
	Instance.Component->SetVisibility(true);
	return true;
}

bool UAnimatedIconWidgetPoolManager::ShowAnimatedIcon(const ERTSVerticalAnimatedIcon IconType,
	const FVector& WorldLocation)
{
	return ShowIconInternal(IconType, WorldLocation, nullptr, nullptr);
}

bool UAnimatedIconWidgetPoolManager::ShowAnimatedIconWithSettings(const ERTSVerticalAnimatedIcon IconType,
	const FVector& WorldLocation, const FRTSVerticalAnimIconSettings& AnimationSettings)
{
	return ShowIconInternal(IconType, WorldLocation, nullptr, &AnimationSettings);
}

bool UAnimatedIconWidgetPoolManager::ShowAnimatedIconAttachedToActor(const ERTSVerticalAnimatedIcon IconType,
	AActor* AttachActor, const FVector& LocalOffset)
{
	if (not IsValid(AttachActor))
	{
		return false;
	}
	return ShowIconInternal(IconType, LocalOffset, AttachActor, nullptr);
}

bool UAnimatedIconWidgetPoolManager::ShowAnimatedIconAttachedToActorWithSettings(const ERTSVerticalAnimatedIcon IconType,
	AActor* AttachActor, const FVector& LocalOffset, const FRTSVerticalAnimIconSettings& AnimationSettings)
{
	if (not IsValid(AttachActor))
	{
		return false;
	}
	return ShowIconInternal(IconType, LocalOffset, AttachActor, &AnimationSettings);
}

int32 UAnimatedIconWidgetPoolManager::GetPoolSize() const
{
	return M_Instances.Num();
}

int32 UAnimatedIconWidgetPoolManager::GetActiveIconCount() const
{
	return M_ActiveIndices.Num();
}

bool UAnimatedIconWidgetPoolManager::IsReady() const
{
	return bM_IsReady;
}

bool UAnimatedIconWidgetPoolManager::GetIsValidWorld() const
{
	if (M_World.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, TEXT("M_World"), TEXT("GetIsValidWorld"), this);
	return false;
}

bool UAnimatedIconWidgetPoolManager::GetIsValidOwnerActor() const
{
	if (M_OwnerActor.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, TEXT("M_OwnerActor"), TEXT("GetIsValidOwnerActor"), this);
	return false;
}

bool UAnimatedIconWidgetPoolManager::GetIsValidIconDataAsset() const
{
	if (M_IconDataAsset.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, TEXT("M_IconDataAsset"),
		TEXT("GetIsValidIconDataAsset"), this);
	return false;
}

bool UAnimatedIconWidgetPoolManager::GetIsValidInstance(const int32 InstanceIndex) const
{
	if (M_Instances.IsValidIndex(InstanceIndex) && M_Instances[InstanceIndex].Component.IsValid()
		&& M_Instances[InstanceIndex].Widget.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(FString::Printf(TEXT("Animated Icons: invalid pool slot %d."), InstanceIndex));
	return false;
}
