// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "FowManager.h"

#include "NiagaraComponent.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "RTS_Survival/GameUI/MiniMap/CustomIcons/MinimapIconDataAsset.h"
#include "RTS_Survival/GameUI/MiniMap/CustomIcons/RTSMinimapDeveloperSettings.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowType.h"
#include "RTS_Survival/FOWSystem/FowVisibilityInterface/FowVisibility.h"
#include "RTS_Survival/Player/Camera/CameraController/PlayerCameraController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"


// Sets default values
AFowManager::AFowManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	// Start ticking after we have components registered.
	PrimaryActorTick.bStartWithTickEnabled = false;
	bIsInitialized = false;
	bIsStarted = false;
	bM_IsPendingReadBackAnswer = false;
	// Settings
	PendingStartLimit = 30;
	NiagaraDrawBufferName = "";
	MapExtent = 0.0f;
	RenderTargetSize = 0;
	FowTickRate = 0.1f;
	MiniMapTinyIconSizePixels = 1.0f;
	MiniMapSmallIconSizePixels = 6.0f;
	MiniMapMediumIconSizePixels = 8.0f;
	MiniMapLargeIconSizePixels = 10.0f;
	MiniMapColorDefaultEnemy = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);
	MiniMapColorBossEnemy = FLinearColor(1.0f, 0.5f, 0.0f, 1.0f);
	MiniMapColorDefaultPlayerUnit = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);
	MiniMapColorPlayerBuilding = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);
	FowSystem = nullptr;
	FoWReadBackSystem = nullptr;
	NiagaraDraw = nullptr;
	NiagaraReadBack = nullptr;

	M_ActiveFowComponents = {};
	M_DrawBuffer = {};
	bM_IsPendingEnemyVisionUpdate = false;
	M_StartupAttempts = 0;
	M_BeginPlayBoundsTransferAttempts = 0;
	M_CustomMiniMapIcons = {};
	M_CachedCustomMiniMapIconDrawData = {};
}

void AFowManager::AddFowParticipant(UFowComp* FowComp)
{
	if (not IsValid(FowComp))
	{
		RTSFunctionLibrary::ReportError("Invalid Fow component supplied to fow manager, will not add this fow"
			"comp to participate!"
			"\n See function: AFowManager::AddFowParticipant()");
		return;
	}
	switch (FowComp->GetFowBehaviour())
	{
	case EFowBehaviour::Fow_Active:
		AddActiveFowParticipant(FowComp);
		return;
	case EFowBehaviour::Fow_Passive:
		AddPassiveFowParticipant(FowComp);
		return;
	case EFowBehaviour::Fow_PassiveEnemyVision:
		AddPassiveFowParticipant(FowComp);
		return;
	}
	AddPassiveFowParticipant(FowComp);
}

void AFowManager::RemoveFowParticipant(UFowComp* FowComp)
{
	if (not IsValid(FowComp))
	{
		return;
	}
	const bool bIsActive = FowComp->GetFowBehaviour() == EFowBehaviour::Fow_Active;
	if (bIsActive && not M_ActiveFowComponents.Contains(FowComp))
	{
		return;
	}
	if (not bIsActive && not M_PassiveFowComponents.Contains(FowComp))
	{
		return;
	}
	bIsActive ? M_ActiveFowComponents.Remove(FowComp) : M_PassiveFowComponents.Remove(FowComp);
	const int32 NewLength = M_ActiveFowComponents.Num();
	M_DrawBuffer.SetNum(NewLength);
	if (NewLength <= 0 && IsActorTickEnabled())
	{
		StopFow();
	}
	RefreshMiniMapIconDrawDataCache();
}

void AFowManager::ReceiveParticleData_Implementation(const TArray<FBasicParticleData>& Data,
                                                     UNiagaraSystem* NiagaraSystem,
                                                     const FVector& SimulationPositionOffset)
{
	if (NiagaraSystem == FoWReadBackSystem)
	{
		OnReceivedReadBackUpdate(Data);
		return;
	}
	if (NiagaraSystem == FowEnemyVisionSystem)
	{
		OnReceivedEnemyVisionUpdate(Data);
		return;
	}
	RTSFunctionLibrary::ReportError("unknown Niagara system is providing data to the FowManager!"
		"\n See function: AFowManager::ReceiveParticleData_Implementation()");
}

void AFowManager::RequestComponentFogVisibility(UFowComp* RequestingComponent)
{
	if (not NiagaraReadBack)
	{
		return;
	}
	if (not IsValid(RequestingComponent) || not RequestingComponent->Implements<UFowVisibility>())
	{
		const FName ActorName = IsValid(RequestingComponent) ? RequestingComponent->GetFName() : FName("InvalidActor");
		RTSFunctionLibrary::PrintString(
			"Will not check actor visibility, actor is not valid or does not implement IFowVisibility!"
			"\n Actor requested visiblity update: " + ActorName.ToString()
			+ "\n See function: AFowManager::RequestActorFogVisibility()");
		return;
	}
	if (M_ComponentsRequestingFogVisibility.Contains(RequestingComponent))
	{
		RTSFunctionLibrary::PrintString("Actor already requested fog visibility update, will not request again!"
			"\n Actor requested visiblity update: " + RequestingComponent->GetFName().ToString()
			+ "\n See function: AFowManager::RequestActorFogVisibility()");
		return;
	}
	M_ComponentsRequestingFogVisibility.Add(RequestingComponent);
}

void AFowManager::RemoveComponentFromVisibilityRequest(UFowComp* Component)
{
	if (M_ComponentsRequestingFogVisibility.Contains(Component))
	{
		M_ComponentsRequestingFogVisibility.Remove(Component);
	}
}

UTextureRenderTarget2D* AFowManager::GetIsValidActiveRT() const
{
	if(not IsValid(ActiveRT))
	{
		RTSFunctionLibrary::ReportError("Invalid Active RT On Fow Manager! Cannot provide requested RT!");
		return nullptr;
	}
	return ActiveRT;
}

UTextureRenderTarget2D* AFowManager::GetIsValidPassiveRT() const
{
	if(not IsValid(PassiveRT))
	{
		RTSFunctionLibrary::ReportError("Invalid Passive RT On Fow Manager! Cannot provide requested RT!");
		return nullptr;
	}
	return PassiveRT;
}

FLinearColor AFowManager::GetMiniMapIconColorValue(const ERTSMinimapIconColor IconColor) const
{
	switch (IconColor)
	{
	case ERTSMinimapIconColor::DefaultEnemy:
		return MiniMapColorDefaultEnemy;
	case ERTSMinimapIconColor::BossEnemy:
		return MiniMapColorBossEnemy;
	case ERTSMinimapIconColor::DefaultPlayerUnit:
		return MiniMapColorDefaultPlayerUnit;
	case ERTSMinimapIconColor::PlayerBuilding:
		return MiniMapColorPlayerBuilding;
	}

	return MiniMapColorDefaultPlayerUnit;
}

const TArray<FRTSMinimapIconDrawData>& AFowManager::GetMiniMapIconDrawData() const
{
	return M_CachedMiniMapIconDrawData;
}

const TArray<FRTSMinimapCustomIconDrawData>& AFowManager::GetCustomMiniMapIconDrawData()
{
	RefreshCustomMiniMapIconDrawDataCache();
	return M_CachedCustomMiniMapIconDrawData;
}

FName AFowManager::AddCustomMiniMapIcon(const FName IconId,
                                        const EMinimapIconType IconType,
                                        const FVector& WorldLocation,
                                        const FRotator& WorldRotation)
{
	if (not GetCanAddCustomMiniMapIcon(IconId, IconType))
	{
		return NAME_None;
	}

	FFowManagerCustomMinimapIcon& NewCustomIcon = M_CustomMiniMapIcons.Add(IconId);
	NewCustomIcon.M_IconId = IconId;
	NewCustomIcon.M_IconType = IconType;
	NewCustomIcon.M_WorldLocation = WorldLocation;
	NewCustomIcon.M_WorldRotation = WorldRotation;
	NewCustomIcon.bM_IsAttachedToActor = false;

	RefreshCustomMiniMapIconDrawDataCache();
	return IconId;
}

FName AFowManager::AddCustomMiniMapIconAttachedToActor(const FName IconId,
                                                       const EMinimapIconType IconType,
                                                       const FVector& WorldLocation,
                                                       AActor* AttachedActor,
                                                       const FRotator& StaticWorldRotation,
                                                       const bool bUseStaticRotation)
{
	if (not IsValid(AttachedActor))
	{
		RTSFunctionLibrary::ReportError(
			"Cannot add attached custom minimap icon because the attached actor is invalid."
			"\n See function: AFowManager::AddCustomMiniMapIconAttachedToActor");
		return NAME_None;
	}

	if (not GetCanAddCustomMiniMapIcon(IconId, IconType))
	{
		return NAME_None;
	}

	FFowManagerCustomMinimapIcon& NewCustomIcon = M_CustomMiniMapIcons.Add(IconId);
	NewCustomIcon.M_IconId = IconId;
	NewCustomIcon.M_IconType = IconType;
	NewCustomIcon.M_WorldLocation = WorldLocation;
	NewCustomIcon.M_WorldRotation = bUseStaticRotation ? StaticWorldRotation : AttachedActor->GetActorRotation();
	NewCustomIcon.M_StaticWorldRotation = StaticWorldRotation;
	NewCustomIcon.M_AttachedActor = AttachedActor;
	NewCustomIcon.bM_IsAttachedToActor = true;
	NewCustomIcon.bM_UseStaticRotation = bUseStaticRotation;

	RefreshCustomMiniMapIconDrawDataCache();
	return IconId;
}

bool AFowManager::RemoveCustomMiniMapIcon(const FName IconId)
{
	if (IconId.IsNone())
	{
		return false;
	}

	const int32 RemovedIcons = M_CustomMiniMapIcons.Remove(IconId);
	if (RemovedIcons <= 0)
	{
		return false;
	}

	RefreshCustomMiniMapIconDrawDataCache();
	return true;
}

bool AFowManager::SwapCustomMiniMapIcon(const FName IconId, const EMinimapIconType NewIconType)
{
	if (IconId.IsNone())
	{
		return false;
	}

	if (NewIconType == EMinimapIconType::None)
	{
		RTSFunctionLibrary::ReportError(
			"Cannot swap custom minimap icon to EMinimapIconType::None."
			"\n See function: AFowManager::SwapCustomMiniMapIcon");
		return false;
	}

	if (not FindValidCustomMiniMapIcon(NewIconType))
	{
		return false;
	}

	FFowManagerCustomMinimapIcon* const CustomIcon = M_CustomMiniMapIcons.Find(IconId);
	if (CustomIcon == nullptr)
	{
		return false;
	}

	CustomIcon->M_IconType = NewIconType;
	RefreshCustomMiniMapIconDrawDataCache();
	return true;
}

// Called when the game starts or when spawned
void AFowManager::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_InitMinimapIconDataAsset();
	BeginPlay_InitTransferMapExtentToPlayerCamera();
}

void AFowManager::BeginPlay_InitTransferMapExtentToPlayerCamera()
{
	if (UPlayerCameraController* const PlayerCameraController = FRTS_Statics::GetPlayerCameraController(this))
	{
		PlayerCameraController->SetPlayableAreaBounds(GetActorLocation(), MapExtent);
		return;
	}

	constexpr int32 MaxBeginPlayBoundsTransferAttempts = 30;
	if (M_BeginPlayBoundsTransferAttempts >= MaxBeginPlayBoundsTransferAttempts)
	{
		RTSFunctionLibrary::ReportError(
			"Fow manager could not transfer map extent to player camera controller in begin play."
			"\n See function: AFowManager::BeginPlay_InitTransferMapExtentToPlayerCamera()");
		return;
	}

	const UWorld* const World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	++M_BeginPlayBoundsTransferAttempts;
	const FTimerDelegate RetryDelegate = FTimerDelegate::CreateUObject(
		this,
		&AFowManager::BeginPlay_InitTransferMapExtentToPlayerCamera);
	World->GetTimerManager().SetTimerForNextTick(RetryDelegate);
}

void AFowManager::BeginDestroy()
{
	Super::BeginDestroy();
}

void AFowManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// Ensure passive and active components are valid.
	EnsureComponentsAreValid();
	UpdateDrawBuffer();
	AskUpdateEnemyVision();
	AskReadBack();
	RefreshMiniMapIconDrawDataCache();
}

bool AFowManager::GetIsActiveParticipantUnique(UFowComp* FowComp) const
{
	if (M_ActiveFowComponents.Contains(FowComp))
	{
		return false;
	}
	return true;
}

bool AFowManager::GetIsPassiveParticipantUnique(UFowComp* FowComp) const
{
	if (M_PassiveFowComponents.Contains(FowComp))
	{
		return false;
	}
	return true;
}

bool AFowManager::GetMiniMapUVFromWorldLocation(const FVector& WorldLocation, FVector2D& OutUV) const
{
	const float MapSize = MapExtent * 2.0f;
	if (MapSize <= 0.0f)
	{
		return false;
	}

	const FVector RelativeLocation = WorldLocation - GetActorLocation();
	const FVector2D BaseUV = FVector2D(RelativeLocation.X / MapSize, RelativeLocation.Y / MapSize)
		+ FVector2D(0.5f, 0.5f);

	// Niagara draws vision circles in RT texel space; matching icon placement to texel centres keeps icons centred
	// on top of their corresponding vision bubbles.
	const float HalfTexelUVOffset = RenderTargetSize > 0 ? (0.5f / static_cast<float>(RenderTargetSize)) : 0.0f;
	const FVector2D UV = BaseUV - FVector2D(HalfTexelUVOffset, HalfTexelUVOffset);

	if (not FMath::IsWithinInclusive(UV.X, 0.0f, 1.0f)
		|| not FMath::IsWithinInclusive(UV.Y, 0.0f, 1.0f))
	{
		return false;
	}

	OutUV = UV;
	return true;
}

void AFowManager::AppendMiniMapIconDrawData(const TArray<TWeakObjectPtr<UFowComp>>& FowComponents,
                                            TArray<FRTSMinimapIconDrawData>& OutMiniMapIcons) const
{
	for (const TWeakObjectPtr<UFowComp>& EachComponent : FowComponents)
	{
		if (not EachComponent.IsValid())
		{
			continue;
		}

		const UFowComp* const FowComponent = EachComponent.Get();
		if (not FowComponent->GetShouldDrawMiniMapIcon())
		{
			continue;
		}

		const FFowCompMiniMapRuntimeState& MiniMapRuntimeState = FowComponent->GetMiniMapRuntimeState();
		FVector2D IconUV = FVector2D::ZeroVector;
		if (not GetMiniMapUVFromWorldLocation(MiniMapRuntimeState.M_WorldLocation, IconUV))
		{
			continue;
		}

		const FMinimapIconSettings& IconSettings = FowComponent->GetMiniMapIconSettings();
		FRTSMinimapIconDrawData& NewIcon = OutMiniMapIcons.AddDefaulted_GetRef();
		NewIcon.M_UV = IconUV;
		NewIcon.M_IconSizePixels = IconSettings.M_IconSizePixels;
		NewIcon.M_IconColor = GetMiniMapIconColorValue(IconSettings.M_IconColor);
	}
}

void AFowManager::RefreshMiniMapIconDrawDataCache()
{
	M_CachedMiniMapIconDrawData.Reset();
	M_CachedMiniMapIconDrawData.Reserve(M_ActiveFowComponents.Num() + M_PassiveFowComponents.Num());

	AppendMiniMapIconDrawData(M_ActiveFowComponents, M_CachedMiniMapIconDrawData);
	AppendMiniMapIconDrawData(M_PassiveFowComponents, M_CachedMiniMapIconDrawData);
}

void AFowManager::RefreshCustomMiniMapIconDrawDataCache()
{
	M_CachedCustomMiniMapIconDrawData.Reset();
	M_CachedCustomMiniMapIconDrawData.Reserve(M_CustomMiniMapIcons.Num());

	TArray<FName> InvalidAttachedIconIds;
	for (TPair<FName, FFowManagerCustomMinimapIcon>& CustomIconPair : M_CustomMiniMapIcons)
	{
		FFowManagerCustomMinimapIcon& CustomIcon = CustomIconPair.Value;
		if (not UpdateCustomMinimapIconAttachedActorTransform(CustomIcon))
		{
			InvalidAttachedIconIds.Add(CustomIconPair.Key);
			continue;
		}

		AppendCustomMiniMapIconDrawData(CustomIcon);
	}

	for (const FName InvalidAttachedIconId : InvalidAttachedIconIds)
	{
		M_CustomMiniMapIcons.Remove(InvalidAttachedIconId);
	}
}

bool AFowManager::GetIsValidMinimapIconDataAsset() const
{
	if (IsValid(M_MinimapIconDataAsset))
	{
		bM_HasReportedMissingMinimapIconDataAsset = false;
		return true;
	}

	if (not bM_HasReportedMissingMinimapIconDataAsset)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			"M_MinimapIconDataAsset",
			"GetIsValidMinimapIconDataAsset",
			this);
		bM_HasReportedMissingMinimapIconDataAsset = true;
	}
	return false;
}

bool AFowManager::GetCanAddCustomMiniMapIcon(const FName IconId, const EMinimapIconType IconType) const
{
	if (IconId.IsNone())
	{
		RTSFunctionLibrary::ReportError(
			"Cannot add custom minimap icon because the supplied icon ID is NAME_None."
			"\n See function: AFowManager::GetCanAddCustomMiniMapIcon");
		return false;
	}

	if (IconType == EMinimapIconType::None)
	{
		RTSFunctionLibrary::ReportError(
			"Cannot add custom minimap icon with EMinimapIconType::None."
			"\n See function: AFowManager::GetCanAddCustomMiniMapIcon");
		return false;
	}

	if (M_CustomMiniMapIcons.Contains(IconId))
	{
		RTSFunctionLibrary::ReportError(
			"Cannot add custom minimap icon because the supplied icon ID already exists."
			"\n Icon ID: " + IconId.ToString() +
			"\n See function: AFowManager::GetCanAddCustomMiniMapIcon");
		return false;
	}

	return FindValidCustomMiniMapIcon(IconType) != nullptr;
}

void AFowManager::BeginPlay_InitMinimapIconDataAsset()
{
	const URTSMinimapDeveloperSettings* const MinimapSettings = GetDefault<URTSMinimapDeveloperSettings>();
	if (MinimapSettings == nullptr)
	{
		return;
	}

	M_MinimapIconDataAsset = MinimapSettings->M_MinimapIconDataAsset.LoadSynchronous();
	bM_HasReportedMissingMinimapIconDataAsset = false;
}

const FMinimapIcon* AFowManager::FindValidCustomMiniMapIcon(const EMinimapIconType IconType) const
{
	if (not GetIsValidMinimapIconDataAsset())
	{
		return nullptr;
	}

	const FMinimapIcon* const MinimapIcon = M_MinimapIconDataAsset->FindMinimapIcon(IconType);
	if (MinimapIcon == nullptr)
	{
		RTSFunctionLibrary::ReportError(
			"Cannot find custom minimap icon type in the minimap icon data asset."
			"\n Icon type: " + UEnum::GetValueAsString(IconType) +
			"\n See function: AFowManager::FindValidCustomMiniMapIcon");
		return nullptr;
	}

	if (MinimapIcon->M_SizeXY <= 0.0f)
	{
		RTSFunctionLibrary::ReportError(
			"Custom minimap icon size must be larger than zero."
			"\n Icon type: " + UEnum::GetValueAsString(IconType) +
			"\n See function: AFowManager::FindValidCustomMiniMapIcon");
		return nullptr;
	}

	if (not IsValid(MinimapIcon->M_Texture))
	{
		RTSFunctionLibrary::ReportError(
			"Custom minimap icon texture is invalid."
			"\n Icon type: " + UEnum::GetValueAsString(IconType) +
			"\n See function: AFowManager::FindValidCustomMiniMapIcon");
		return nullptr;
	}

	return MinimapIcon;
}

void AFowManager::AppendCustomMiniMapIconDrawData(const FFowManagerCustomMinimapIcon& CustomIcon)
{
	FVector2D IconUV = FVector2D::ZeroVector;
	if (not GetMiniMapUVFromWorldLocation(CustomIcon.M_WorldLocation, IconUV))
	{
		return;
	}

	const FMinimapIcon* const MinimapIcon = FindValidCustomMiniMapIcon(CustomIcon.M_IconType);
	if (MinimapIcon == nullptr)
	{
		return;
	}

	FRTSMinimapCustomIconDrawData& NewIcon = M_CachedCustomMiniMapIconDrawData.AddDefaulted_GetRef();
	NewIcon.M_UV = IconUV;
	NewIcon.M_IconSizePixels = MinimapIcon->M_SizeXY;
	NewIcon.M_RotationDegrees = GetCustomMinimapIconRotationDegrees(CustomIcon.M_WorldRotation);
	NewIcon.M_Texture = MinimapIcon->M_Texture;
}

bool AFowManager::UpdateCustomMinimapIconAttachedActorTransform(FFowManagerCustomMinimapIcon& CustomIcon) const
{
	if (not CustomIcon.bM_IsAttachedToActor)
	{
		return true;
	}

	if (not CustomIcon.M_AttachedActor.IsValid())
	{
		return false;
	}

	const AActor* const AttachedActor = CustomIcon.M_AttachedActor.Get();
	CustomIcon.M_WorldLocation = AttachedActor->GetActorLocation();
	CustomIcon.M_WorldRotation = CustomIcon.bM_UseStaticRotation
		? CustomIcon.M_StaticWorldRotation
		: AttachedActor->GetActorRotation();
	return true;
}

float AFowManager::GetCustomMinimapIconRotationDegrees(const FRotator& WorldRotation) const
{
	return WorldRotation.Yaw;
}


void AFowManager::StartFow()
{
	if (bIsInitialized)
	{
		SetActorTickInterval(FowTickRate);
		SetActorTickEnabled(true);
		bIsStarted = true;
		M_StartupAttempts = 0;
	}
	else
	{
		if (M_StartupAttempts >= PendingStartLimit)
		{
			RTSFunctionLibrary::ReportError("Fow Manager failed to start the Fog of War subsystem, "
				"pending start limit reached!"
				"\n See function: AFowManager::StartFow()");
			return;
		}
		if (const UWorld* World = GetWorld())
		{
			M_StartupAttempts++;
			const FTimerDelegate TimerDel = FTimerDelegate::CreateUObject(this, &AFowManager::StartFow);
			World->GetTimerManager().SetTimerForNextTick(TimerDel);
		}
	}
}

void AFowManager::StopFow()
{
	if (not bIsStarted)
	{
		return;
	}
	bIsStarted = false;
	SetActorTickEnabled(false);
	const TArray<FVector> EmptyArray = {};
	if (IsValid(NiagaraDraw))
	{
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
			NiagaraDraw, FName(*NiagaraDrawBufferName), EmptyArray);
	}
	const TArray<FVector2D> EmptyArray2d = {};
	if (IsValid(NiagaraReadBack))
	{
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector2D(
			NiagaraReadBack, FName(*NiagaraReadBackBufferName), EmptyArray2d);
		const FName EnabledName = FName("bEnabled");
		NiagaraReadBack->SetVariableBool(EnabledName, false);
	}
}

void AFowManager::AddActiveFowParticipant(UFowComp* FowComp)
{
	if (not GetIsActiveParticipantUnique(FowComp))
	{
		RTSFunctionLibrary::ReportError("Fow component already participating in fow manager, will not add this fow"
			"comp to partcipate again!"
			"\n See function: AFowManager::AddFowParticipant()");
		return;
	}
	const int32 PreviousLenght = M_ActiveFowComponents.Num();
	M_ActiveFowComponents.Add(FowComp);
	const int32 NewLenght = M_ActiveFowComponents.Num();
	M_DrawBuffer.SetNum(NewLenght);
	if (PreviousLenght <= 0 && NewLenght > 0 && not bIsStarted)
	{
		StartFow();
	}
	RefreshMiniMapIconDrawDataCache();
}

void AFowManager::AddPassiveFowParticipant(UFowComp* FowComp)
{
	if (not GetIsPassiveParticipantUnique(FowComp))
	{
		RTSFunctionLibrary::ReportError("Fow component already participating in fow manager, will not add this fow"
			"comp to partcipate again!"
			"\n See function: AFowManager::AddFowParticipant()");
		return;
	}
	M_PassiveFowComponents.Add(FowComp);
	RefreshMiniMapIconDrawDataCache();
}


void AFowManager::UpdateDrawBuffer()
{
	if (not IsValid(NiagaraDraw))
	{
		OnInvalidNiagaraDraw();
		return;
	}
	int32 Index = 0;
	for (const TWeakObjectPtr<UFowComp>& EachComp : M_ActiveFowComponents)
	{
		AActor* const OwnerActor = EachComp->GetOwner();
		const FVector OwnerWorldLocation = OwnerActor->GetActorLocation();
		FVector Location = OwnerWorldLocation;
		const float VisionRadius = EachComp->GetVisionRadius();
		// Save the vision radius in the z component.
		Location.Z = VisionRadius;
		EachComp->CacheMiniMapWorldLocation(OwnerWorldLocation);
		EachComp->SetShouldDrawMiniMapIcon(not OwnerActor->IsHidden());
		M_DrawBuffer[Index] = Location;
		Index++;
	}
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
		NiagaraDraw, FName(*NiagaraDrawBufferName), M_DrawBuffer);
}

void AFowManager::AskUpdateEnemyVision()
{
	if (IsValid(NiagaraEnemyVision) && not bM_IsPendingEnemyVisionUpdate)
	{
		UpdateEnemyVision();
		bM_IsPendingEnemyVisionUpdate = true;
	}
}

void AFowManager::UpdateEnemyVision()
{
	M_CurrentPlayerCompsForEnemyVisionReadBack.Empty();

	// Prepare the enemy positions and vision ranges
	// Using FVector3f for compatibility with Niagara	
	TArray<FVector3f> EnemyData;
	for (auto EnemyComp : M_PassiveFowComponents)
	{
		if (EnemyComp->GetFowBehaviour() == EFowBehaviour::Fow_PassiveEnemyVision && EnemyComp->GetVisionRadius() > 0)
		{
			const FVector Position = EnemyComp->GetOwner()->GetActorLocation();
			const float VisionRange = EnemyComp->GetVisionRadius();
			EnemyData.Add(FVector3f(Position.X, Position.Y, VisionRange));
		}
	}

	// Prepare the player positions
	TArray<FVector2f> PlayerPositions;
	for (auto PlayerComp : M_ActiveFowComponents)
	{
		FVector Position = PlayerComp->GetOwner()->GetActorLocation();
		PlayerPositions.Add(FVector2f(Position.X, Position.Y));
		M_CurrentPlayerCompsForEnemyVisionReadBack.Add(PlayerComp);
	}

	// Send data to the Niagara system
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
		NiagaraEnemyVision,
		*EnemyPositionVisionBufferName,
		EnemyData
	);

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector2D(
		NiagaraEnemyVision,
		*PlayerPositionsBufferName,
		PlayerPositions
	);

	NiagaraReadBack->SetVariableBool("bEnabled", true);

	// Reinitialize the system to process the new data
	NiagaraEnemyVision->ReinitializeSystem();

	if constexpr (DeveloperSettings::Debugging::GFowSystem_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(
			"Amount of enemy vision fow components (Update Buffers): " + FString::FromInt(EnemyData.Num()),
			FColor::Purple);

		RTSFunctionLibrary::PrintString(
			"Amount of player fow components (Update Buffers): " + FString::FromInt(PlayerPositions.Num()),
			FColor::Purple);
		RTSFunctionLibrary::PrintString("Enemy vision data sent to Niagara system", FColor::Purple);
	}
}

void AFowManager::OnReceivedEnemyVisionUpdate(const TArray<FBasicParticleData>& Data)
{
	if (not bM_IsPendingEnemyVisionUpdate)
	{
		return;
	}
	bM_IsPendingEnemyVisionUpdate = false;
	// Loop through the particle data to get visibility information
	for (const FBasicParticleData& ParticleData : Data)
	{
		// The index of the player unit is stored in Velocity.X
		int32 PlayerIndex = static_cast<int32>(ParticleData.Velocity.X);

		if (M_CurrentPlayerCompsForEnemyVisionReadBack.IsValidIndex(PlayerIndex))
		{
			TWeakObjectPtr<UFowComp> PlayerComp = M_CurrentPlayerCompsForEnemyVisionReadBack[PlayerIndex];
			if (PlayerComp.IsValid())
			{
				// The Size represents visibility (0.0f for hidden, 1.0f for visible)
				const float Visibility = ParticleData.Size;

				// Update the player's visibility status
				PlayerComp->OnFowVisibilityUpdated(Visibility);
			}
		}
		else
		{
			// todo renable
			// RTSFunctionLibrary::PrintString("Invalid index received from Niagara system for enemy vision update!"
			// 	"\n See function: AFowManager::OnReceivedEnemyVisionUpdate()"
			// 	"\n Index: " + FString::FromInt(PlayerIndex) +
			// 	" \n but CurrentAmountOfPlayerFowComponents: " + FString::FromInt(
			// 		M_CurrentPlayerCompsForEnemyVisionReadBack.Num()));
		}
	}

	if (IsValid(NiagaraEnemyVision))
	{
		// Reset enemy vision components in niagara system.
		const TArray<FVector3f> ResetEnemyPosVisionArray;
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
			NiagaraEnemyVision, FName(*EnemyPositionVisionBufferName), ResetEnemyPosVisionArray);
		const TArray<FVector2D> ResetPlayerPositionsArray;
		// Reset player position buffer in enemy vision niagara system.
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector2D(
			NiagaraEnemyVision, FName(*PlayerPositionsBufferName), ResetPlayerPositionsArray);
		// Flag that stops updating this manager with data from the data interface on the enemy vision niagara system.
		NiagaraEnemyVision->SetVariableBool("bEnabled", false);
		NiagaraEnemyVision->ReinitializeSystem();
		// Clear the valid components requesting fog visibility.
		M_CurrentPlayerCompsForEnemyVisionReadBack.Empty();

		if constexpr (DeveloperSettings::Debugging::GFowSystem_Compile_DebugSymbols)
		{
			if (Data.Num() > 0)
			{
				RTSFunctionLibrary::PrintString(
					"Amount of enemy components in compute shader:" + FString::SanitizeFloat(Data[0].Velocity.Z));
			}
			for (const auto& PaticleData : Data)
			{
				RTSFunctionLibrary::PrintString("velocity vector found: " + PaticleData.Velocity.ToString());
			}
			RTSFunctionLibrary::PrintString("Reset vision arrays in niagara system");
		}
	}
}


void AFowManager::OnInvalidNiagaraDraw()
{
	RTSFunctionLibrary::ReportError("FOW manager does not have a valid niagara draw component, "
		"cannot update draw buffer!"
		"\n See function: AFowManager::UpdateDrawBuffer()"
		"\n Active fow components will be cleared!");
	M_ActiveFowComponents.Empty();
	RefreshMiniMapIconDrawDataCache();
	StopFow();
}

void AFowManager::EnsureComponentsAreValid()
{
	TArray<TWeakObjectPtr<UFowComp>> ValidComponents = {};
	for (auto EachComp : M_ActiveFowComponents)
	{
		if (EachComp.IsValid())
		{
			if (IsValid(EachComp->GetOwner()))
			{
				ValidComponents.Add(EachComp);
			}
		}
	}
	M_ActiveFowComponents = ValidComponents;
	ValidComponents.Empty();
	for (const auto EachComp : M_PassiveFowComponents)
	{
		if (EachComp.IsValid())
		{
			if (IsValid(EachComp->GetOwner()))
			{
				ValidComponents.Add(EachComp);
			}
		}
	}
	M_PassiveFowComponents = ValidComponents;
	// Ensure that the draw buffer has sufficient space.
	if (M_DrawBuffer.Num() != M_ActiveFowComponents.Num())
	{
		M_DrawBuffer.SetNum(M_ActiveFowComponents.Num());
	}
}

void AFowManager::AskReadBack()
{
	if (not bM_IsPendingReadBackAnswer && IsValid(NiagaraReadBack) && M_PassiveFowComponents.Num() > 0)
	{
		UpdateReadBackBuffer();
		bM_IsPendingReadBackAnswer = true;
	}
}

void AFowManager::UpdateReadBackBuffer()
{
	M_CurrentReadBackPassiveComponents.Empty();
	TArray<FVector2D> NiagaraData = {};
	for (const TWeakObjectPtr<UFowComp>& PassiveFowComp : M_PassiveFowComponents)
	{
		M_CurrentReadBackPassiveComponents.Add(PassiveFowComp);
		const FVector Location = PassiveFowComp->GetOwner()->GetActorLocation();
		PassiveFowComp->CacheMiniMapWorldLocation(Location);
		NiagaraData.Add(FVector2D(Location.X, Location.Y));
	}
	if (M_CurrentReadBackPassiveComponents.Num() > 0)
	{
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector2D(
			NiagaraReadBack, FName(*NiagaraReadBackBufferName), NiagaraData);
		NiagaraReadBack->SetVariableBool("bEnabled", true);
		NiagaraReadBack->ReinitializeSystem();
	}
}

void AFowManager::OnReceivedReadBackUpdate(const TArray<FBasicParticleData>& Data)
{
	if (not bM_IsPendingReadBackAnswer)
	{
		return;
	}
	bM_IsPendingReadBackAnswer = false;
	int32 Index = 0;
	for (const auto EachElm : Data)
	{
		// Obtain the index from the velocity vector's x component, it turns out that for arrays bigger than 64
		// The GPU does not respect the order of the particles, so we need to use the velocity vector to store the index.
		if (GetValidParticleIndex(EachElm.Velocity, Index))
		{
			TWeakObjectPtr<UFowComp> PassiveComp = M_CurrentReadBackPassiveComponents[Index];
			if (PassiveComp.IsValid())
			{
				PassiveComp->OnFowVisibilityUpdated(EachElm.Size);
			}
		}
	}
	if (IsValid(NiagaraReadBack))
	{
		const TArray<FVector2D> EmptyArray;
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector2D(
			NiagaraReadBack, FName(*NiagaraReadBackBufferName), EmptyArray);
		NiagaraReadBack->SetVariableBool("bEnabled", false);
		NiagaraReadBack->ReinitializeSystem();
	}
	// Clear the valid components requesting fog visibility.
	M_CurrentReadBackPassiveComponents.Empty();
	RefreshMiniMapIconDrawDataCache();
}

bool AFowManager::GetValidParticleIndex(const FVector& ParticleVector, int32& OutIndex) const
{
	OutIndex = FMath::TruncToInt32(ParticleVector.X);
	if (OutIndex < 0 || OutIndex >= M_CurrentReadBackPassiveComponents.Num())
	{
		// todo off by one error here, one particle too many is used on GPU.
		RTSFunctionLibrary::PrintString("Dangerous particle index requested, index out of bounds!"
			"\n Particle index requested: " + FString::FromInt(OutIndex) +
			"\n Particle vector: " + ParticleVector.ToString() +
			"\n Amount of valid actors requesting fog visibility: " + FString::FromInt(
				M_CurrentReadBackPassiveComponents.Num()) +
			"\n See function: AFowManager::GetValidParticleIndex())");
		return false;
	}
	return true;
}
