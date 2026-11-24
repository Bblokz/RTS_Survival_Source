// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyTrainingSquadDisplayComponent.h"

#include "Animation/AnimSequenceBase.h"
#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Slate/SlateBrushAsset.h"
#include "RTS_Survival/Enemy/TrainingAndUnitCreation/EnemyBuildingTrainer/EnemyBuildingTrainerComponent.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Units/Squads/SquadControllerHpComp/SquadWeaponIcons/SquadWeaponIconSettings/SquadWeaponIconSettings.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/InfantryWeapon/InfantryWeaponMaster.h"

// Warnings only: a building that cannot read a squad's look shows the fallback squad, it never stops the game.
DEFINE_LOG_CATEGORY_STATIC(LogEnemyTrainingSquadDisplay, Log, All);

namespace EnemyTrainingSquadDisplayPrivate
{
	// ASquadController::SquadUnitClasses is protected; read through reflection so this display needs no friend access.
	const TCHAR* const SquadUnitClassesPropertyName = TEXT("SquadUnitClasses");
	constexpr int32 MaxAttachmentsPerFigure = 6;

	bool GetIsBodyMeshParent(const USCS_Node* Node)
	{
		return Node->bIsParentComponentNative && Node->ParentComponentOrVariableName == ACharacter::MeshComponentName;
	}

	bool GetIsShownTemplate(const USceneComponent* Template)
	{
		return IsValid(Template) && Template->GetVisibleFlag() && not Template->bHiddenInGame;
	}

	void CopyMaterials(const UMeshComponent* Template, TArray<TObjectPtr<UMaterialInterface>>& OutMaterials)
	{
		OutMaterials.Reset();
		for (int32 Index = 0; Index < Template->GetNumMaterials(); ++Index)
		{
			OutMaterials.Add(Template->GetMaterial(Index));
		}
	}

	/** @return The node's transform relative to its actor's root (identity for the root itself). */
	FTransform GetTransformRelativeToRoot(const USimpleConstructionScript* SCS, USCS_Node* Node,
	                                      UBlueprintGeneratedClass* MostDerivedClass)
	{
		FTransform Result = FTransform::Identity;
		USCS_Node* Current = Node;
		while (Current != nullptr)
		{
			USCS_Node* const Parent = SCS->FindParentNode(Current);
			const bool bIsActorRoot = Parent == nullptr && not Current->bIsParentComponentNative;
			if (bIsActorRoot)
			{
				break;
			}
			if (const USceneComponent* const Template = Cast<USceneComponent>(
				Current->GetActualComponentTemplate(MostDerivedClass)))
			{
				Result = Result * Template->GetRelativeTransform();
			}
			Current = Parent;
		}
		return Result;
	}

	// The SCS nodes of a Blueprint class and all its Blueprint parents, most derived first.
	void CollectNodes(UClass* Class, TArray<TPair<USimpleConstructionScript*, USCS_Node*>>& OutNodes)
	{
		for (UClass* Current = Class; Current != nullptr; Current = Current->GetSuperClass())
		{
			UBlueprintGeneratedClass* const Generated = Cast<UBlueprintGeneratedClass>(Current);
			if (Generated == nullptr || Generated->SimpleConstructionScript == nullptr)
			{
				continue;
			}
			for (USCS_Node* const Node : Generated->SimpleConstructionScript->GetAllNodes())
			{
				if (Node != nullptr)
				{
					OutNodes.Emplace(Generated->SimpleConstructionScript, Node);
				}
			}
		}
	}
}

UEnemyTrainingSquadDisplayComponent::UEnemyTrainingSquadDisplayComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnemyTrainingSquadDisplayComponent::BeginPlay()
{
	Super::BeginPlay();
	AActor* const Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return;
	}
	CollectTaggedComponents();
	HideAll();
	UEnemyBuildingTrainerComponent* const Trainer = Owner->FindComponentByClass<UEnemyBuildingTrainerComponent>();
	if (not IsValid(Trainer))
	{
		UE_LOG(LogEnemyTrainingSquadDisplay, Warning, TEXT("%s has a squad display but no enemy building trainer."),
		       *Owner->GetName());
		return;
	}
	M_Trainer = Trainer;
	Trainer->OnTrainingEventNative.AddUObject(this, &UEnemyTrainingSquadDisplayComponent::OnTrainerEvent);
	if (Trainer->GetIsTraining())
	{
		RequestShow(Trainer->GetActiveTrainingOption(), false);
	}
}

void UEnemyTrainingSquadDisplayComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelLoad();
	if (UEnemyBuildingTrainerComponent* const Trainer = M_Trainer.Get())
	{
		Trainer->OnTrainingEventNative.RemoveAll(this);
	}
	Super::EndPlay(EndPlayReason);
}

void UEnemyTrainingSquadDisplayComponent::CollectTaggedComponents()
{
	AActor* const Owner = GetOwner();
	TInlineComponentArray<UActorComponent*> Components(Owner);
	for (UActorComponent* const Component : Components)
	{
		if (USkeletalMeshComponent* const Figure = Cast<USkeletalMeshComponent>(Component);
			IsValid(Figure) && Figure->ComponentHasTag(FigureTag))
		{
			M_Figures.Add(Figure);
		}
		else if (USceneComponent* const Slot = Cast<USceneComponent>(Component);
			IsValid(Slot) && Slot->ComponentHasTag(RackSlotTag))
		{
			M_RackSlots.Add(Slot);
		}
		AddBoardIconMaterial(Cast<UPrimitiveComponent>(Component));
	}
	auto ByName = [](const UActorComponent& A, const UActorComponent& B)
	{
		return A.GetFName().LexicalLess(B.GetFName());
	};
	M_Figures.Sort(ByName);
	M_RackSlots.Sort(ByName);

	M_FigureAttachments.SetNum(M_Figures.Num());
	for (int32 FigureIndex = 0; FigureIndex < M_Figures.Num(); ++FigureIndex)
	{
		PrepareFigure(FigureIndex);
	}
	for (int32 SlotIndex = 0; SlotIndex < M_RackSlots.Num(); ++SlotIndex)
	{
		M_RackWeapons.Add(CreateAttachedStaticMesh(
			M_RackSlots[SlotIndex], *FString::Printf(TEXT("%s_Weapon"), *M_RackSlots[SlotIndex]->GetName())));
	}
}

void UEnemyTrainingSquadDisplayComponent::AddBoardIconMaterial(UPrimitiveComponent* Board)
{
	if (not IsValid(Board) || not Board->ComponentHasTag(BoardIconTag))
	{
		return;
	}
	if (UMaterialInstanceDynamic* const Material = Board->CreateAndSetMaterialInstanceDynamic(0))
	{
		M_BoardIconMaterials.Add(Material);
	}
}

void UEnemyTrainingSquadDisplayComponent::PrepareFigure(const int32 FigureIndex)
{
	USkeletalMeshComponent* const Figure = M_Figures[FigureIndex];
	Figure->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Figure->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Figure->SetCanEverAffectNavigation(false);
	for (int32 Index = 0; Index < EnemyTrainingSquadDisplayPrivate::MaxAttachmentsPerFigure; ++Index)
	{
		M_FigureAttachments[FigureIndex].Add(CreateAttachedStaticMesh(
			Figure, *FString::Printf(TEXT("%s_Kit%d"), *Figure->GetName(), Index)));
	}
}

UStaticMeshComponent* UEnemyTrainingSquadDisplayComponent::CreateAttachedStaticMesh(USceneComponent* Parent,
                                                                                   const FName Name)
{
	UStaticMeshComponent* const Mesh = NewObject<UStaticMeshComponent>(GetOwner(), Name);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetupAttachment(Parent);
	Mesh->RegisterComponent();
	Mesh->SetVisibility(false);
	return Mesh;
}

void UEnemyTrainingSquadDisplayComponent::OnTrainerEvent(const EEnemyBuildingTrainingEvent Event,
                                                         const FTrainingOption& Option, AActor* /*Unit*/,
                                                         const float /*Duration*/)
{
	switch (Event)
	{
	case EEnemyBuildingTrainingEvent::Started:
		RequestShow(Option, false);
		break;
	case EEnemyBuildingTrainingEvent::ActivityOff:
		HideAll();
		break;
	case EEnemyBuildingTrainingEvent::Cancelled:
	case EEnemyBuildingTrainingEvent::Finished:
		// The next job's Started follows at once; with nothing queued the building is idle.
		if (not GetIsValidTrainer() || not M_Trainer->GetIsTraining())
		{
			HideAll();
		}
		break;
	default:
		break;
	}
}

void UEnemyTrainingSquadDisplayComponent::RequestShow(const FTrainingOption& Option, const bool bIsFallbackAttempt)
{
	CancelLoad();
	const int32 Serial = ++M_RequestSerial;
	auto ShowFallback = [this, &Option, bIsFallbackAttempt]()
	{
		if (bIsFallbackAttempt)
		{
			HideAll();
			return;
		}
		RequestShow(FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(FallbackSquad)), true);
	};
	if (Option.UnitType != EAllUnitType::UNType_Squad)
	{
		ShowFallback();
		return;
	}
	const ARTSAsyncSpawner* const AsyncSpawner = FRTS_Statics::GetAsyncSpawner(this);
	const TSoftClassPtr<AActor> ControllerClass = IsValid(AsyncSpawner)
		                                              ? AsyncSpawner->GetTrainingOptionClass(Option)
		                                              : TSoftClassPtr<AActor>();
	const FSoftObjectPath ControllerPath = ControllerClass.ToSoftObjectPath();
	if (ControllerPath.IsNull())
	{
		UE_LOG(LogEnemyTrainingSquadDisplay, Warning, TEXT("%s: no squad class for %s, showing the fallback squad."),
		       *GetNameSafe(GetOwner()), *Option.GetTrainingName());
		ShowFallback();
		return;
	}
	if (const TArray<FEnemyTrainingDisplayMemberLook>* const Cached = M_LookCache.Find(ControllerPath))
	{
		ShowLooks(Option, *Cached);
		return;
	}
	M_LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		ControllerPath, FStreamableDelegate::CreateUObject(
			this, &UEnemyTrainingSquadDisplayComponent::OnSquadControllerClassLoaded, Serial, Option, ControllerPath,
			bIsFallbackAttempt));
}

void UEnemyTrainingSquadDisplayComponent::OnSquadControllerClassLoaded(const int32 Serial, const FTrainingOption Option,
                                                                       const FSoftObjectPath ControllerPath,
                                                                       const bool bIsFallbackAttempt)
{
	using namespace EnemyTrainingSquadDisplayPrivate;
	if (Serial != M_RequestSerial)
	{
		return;
	}
	UClass* const ControllerClass = Cast<UClass>(ControllerPath.ResolveObject());
	const FArrayProperty* const ArrayProperty = IsValid(ControllerClass)
		                                            ? FindFProperty<FArrayProperty>(ControllerClass, SquadUnitClassesPropertyName)
		                                            : nullptr;
	const FSoftClassProperty* const Inner = ArrayProperty != nullptr
		                                        ? CastField<FSoftClassProperty>(ArrayProperty->Inner)
		                                        : nullptr;
	TArray<FSoftObjectPath> UnitPaths;
	if (Inner != nullptr)
	{
		FScriptArrayHelper Helper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(ControllerClass->GetDefaultObject()));
		for (int32 Index = 0; Index < Helper.Num(); ++Index)
		{
			const FSoftObjectPath UnitPath = Inner->GetPropertyValue(Helper.GetRawPtr(Index)).ToSoftObjectPath();
			if (not UnitPath.IsNull())
			{
				UnitPaths.Add(UnitPath);
			}
		}
	}
	if (UnitPaths.IsEmpty())
	{
		UE_LOG(LogEnemyTrainingSquadDisplay, Warning, TEXT("%s: %s lists no squad units, showing the fallback squad."),
		       *GetNameSafe(GetOwner()), *ControllerPath.ToString());
		if (not bIsFallbackAttempt)
		{
			RequestShow(FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(FallbackSquad)), true);
		}
		return;
	}
	TArray<FSoftObjectPath> UniquePaths;
	for (const FSoftObjectPath& Path : UnitPaths)
	{
		UniquePaths.AddUnique(Path);
	}
	M_LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		UniquePaths, FStreamableDelegate::CreateUObject(
			this, &UEnemyTrainingSquadDisplayComponent::OnSquadUnitClassesLoaded, Serial, Option, ControllerPath,
			UnitPaths, bIsFallbackAttempt));
}

void UEnemyTrainingSquadDisplayComponent::OnSquadUnitClassesLoaded(const int32 Serial, const FTrainingOption Option,
                                                                   const FSoftObjectPath ControllerPath,
                                                                   const TArray<FSoftObjectPath> UnitPaths,
                                                                   const bool bIsFallbackAttempt)
{
	if (Serial != M_RequestSerial)
	{
		return;
	}
	TArray<FEnemyTrainingDisplayMemberLook> Looks;
	for (const FSoftObjectPath& UnitPath : UnitPaths)
	{
		FEnemyTrainingDisplayMemberLook Look;
		if (TryBuildMemberLook(Cast<UClass>(UnitPath.ResolveObject()), Look))
		{
			Looks.Add(MoveTemp(Look));
		}
	}
	if (Looks.IsEmpty())
	{
		UE_LOG(LogEnemyTrainingSquadDisplay, Warning, TEXT("%s: no member of %s has a body mesh."),
		       *GetNameSafe(GetOwner()), *ControllerPath.ToString());
		if (not bIsFallbackAttempt)
		{
			RequestShow(FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(FallbackSquad)), true);
		}
		return;
	}
	// The cached looks hold raw asset pointers: keep the assets alive with this component.
	for (const FEnemyTrainingDisplayMemberLook& Look : Looks)
	{
		M_KeptAssets.Add(Look.BodyMesh);
		M_KeptAssets.Append(Look.BodyMaterials);
		for (const FEnemyTrainingDisplayAttachment& Attachment : Look.Attachments)
		{
			M_KeptAssets.Add(Attachment.Mesh);
			M_KeptAssets.Append(Attachment.Materials);
		}
	}
	M_LookCache.Add(ControllerPath, Looks);
	M_LoadHandle.Reset();
	ShowLooks(Option, Looks);
}

bool UEnemyTrainingSquadDisplayComponent::TryBuildMemberLook(UClass* UnitClass, FEnemyTrainingDisplayMemberLook& OutLook)
{
	using namespace EnemyTrainingSquadDisplayPrivate;
	const ACharacter* const DefaultUnit = IsValid(UnitClass) ? Cast<ACharacter>(UnitClass->GetDefaultObject()) : nullptr;
	const USkeletalMeshComponent* const Body = IsValid(DefaultUnit) ? DefaultUnit->GetMesh() : nullptr;
	if (not IsValid(Body) || Body->GetSkeletalMeshAsset() == nullptr)
	{
		return false;
	}
	OutLook.BodyMesh = Body->GetSkeletalMeshAsset();
	CopyMaterials(Body, OutLook.BodyMaterials);
	OutLook.BodyScale = Body->GetRelativeScale3D();

	UBlueprintGeneratedClass* const MostDerived = Cast<UBlueprintGeneratedClass>(UnitClass);
	TArray<TPair<USimpleConstructionScript*, USCS_Node*>> Nodes;
	CollectNodes(UnitClass, Nodes);
	TSet<FName> SeenVariables;
	for (const TPair<USimpleConstructionScript*, USCS_Node*>& Pair : Nodes)
	{
		USCS_Node* const Node = Pair.Value;
		// A child Blueprint's override of an inherited node is read through the most derived class: count each once.
		if (SeenVariables.Contains(Node->GetVariableName()) || not GetIsBodyMeshParent(Node))
		{
			continue;
		}
		SeenVariables.Add(Node->GetVariableName());
		UActorComponent* const Template = Node->GetActualComponentTemplate(MostDerived);
		if (const UChildActorComponent* const ChildActor = Cast<UChildActorComponent>(Template))
		{
			UClass* const ChildClass = ChildActor->GetChildActorClass();
			if (IsValid(ChildClass) && ChildClass->IsChildOf(AInfantryWeaponMaster::StaticClass()))
			{
				CollectWeaponAttachment(ChildClass, Node->AttachToName, ChildActor->GetRelativeTransform(),
				                        OutLook.Attachments);
			}
			continue;
		}
		const UStaticMeshComponent* const Kit = Cast<UStaticMeshComponent>(Template);
		if (not GetIsShownTemplate(Kit) || Kit->GetStaticMesh() == nullptr)
		{
			continue;
		}
		FEnemyTrainingDisplayAttachment& Attachment = OutLook.Attachments.AddDefaulted_GetRef();
		Attachment.Mesh = Kit->GetStaticMesh();
		CopyMaterials(Kit, Attachment.Materials);
		Attachment.Socket = Node->AttachToName;
		Attachment.RelativeTransform = Kit->GetRelativeTransform();
	}
	return true;
}

void UEnemyTrainingSquadDisplayComponent::CollectWeaponAttachment(UClass* WeaponClass, const FName Socket,
                                                                  const FTransform& ChildActorRelative,
                                                                  TArray<FEnemyTrainingDisplayAttachment>& OutAttachments)
{
	using namespace EnemyTrainingSquadDisplayPrivate;
	UBlueprintGeneratedClass* const MostDerived = Cast<UBlueprintGeneratedClass>(WeaponClass);
	TArray<TPair<USimpleConstructionScript*, USCS_Node*>> Nodes;
	CollectNodes(WeaponClass, Nodes);
	for (const TPair<USimpleConstructionScript*, USCS_Node*>& Pair : Nodes)
	{
		const UStaticMeshComponent* const WeaponMesh = Cast<UStaticMeshComponent>(
			Pair.Value->GetActualComponentTemplate(MostDerived));
		if (not GetIsShownTemplate(WeaponMesh) || WeaponMesh->GetStaticMesh() == nullptr)
		{
			continue;
		}
		FEnemyTrainingDisplayAttachment& Attachment = OutAttachments.AddDefaulted_GetRef();
		Attachment.Mesh = WeaponMesh->GetStaticMesh();
		CopyMaterials(WeaponMesh, Attachment.Materials);
		Attachment.Socket = Socket;
		Attachment.RelativeTransform = GetTransformRelativeToRoot(Pair.Key, Pair.Value, MostDerived) * ChildActorRelative;
		Attachment.bIsWeapon = true;
		// AInfantryWeaponMaster uses its first static mesh as the weapon.
		return;
	}
}

void UEnemyTrainingSquadDisplayComponent::ShowLooks(const FTrainingOption& Option,
                                                    const TArray<FEnemyTrainingDisplayMemberLook>& Looks)
{
	M_ShownOption = Option;
	bM_IsShowing = true;
	for (int32 FigureIndex = 0; FigureIndex < M_Figures.Num(); ++FigureIndex)
	{
		if (Looks.IsValidIndex(FigureIndex))
		{
			ApplyFigure(FigureIndex, Looks[FigureIndex]);
		}
		else
		{
			HideFigure(FigureIndex);
		}
	}
	for (int32 SlotIndex = 0; SlotIndex < M_RackSlots.Num(); ++SlotIndex)
	{
		ApplyRackSlot(SlotIndex, Looks.IsValidIndex(SlotIndex) ? GetWeaponMesh(Looks[SlotIndex]) : nullptr);
	}
	ApplyBoardIcon(Option);
}

UStaticMesh* UEnemyTrainingSquadDisplayComponent::GetWeaponMesh(const FEnemyTrainingDisplayMemberLook& Look)
{
	for (const FEnemyTrainingDisplayAttachment& Attachment : Look.Attachments)
	{
		if (Attachment.bIsWeapon)
		{
			return Attachment.Mesh;
		}
	}
	return nullptr;
}

bool UEnemyTrainingSquadDisplayComponent::GetIsValidTrainer() const
{
	if (M_Trainer.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Trainer", "GetIsValidTrainer", this);
	return false;
}

void UEnemyTrainingSquadDisplayComponent::ApplyFigure(const int32 FigureIndex,
                                                      const FEnemyTrainingDisplayMemberLook& Look)
{
	USkeletalMeshComponent* const Figure = M_Figures[FigureIndex];
	if (not IsValid(Figure))
	{
		return;
	}
	Figure->SetSkeletalMeshAsset(Look.BodyMesh);
	for (int32 Index = 0; Index < Look.BodyMaterials.Num(); ++Index)
	{
		Figure->SetMaterial(Index, Look.BodyMaterials[Index]);
	}
	Figure->SetRelativeScale3D(Look.BodyScale);
	Figure->SetVisibility(true);
	if (not FigureIdleAnimations.IsEmpty())
	{
		UAnimSequenceBase* const Animation = FigureIdleAnimations[FigureIndex % FigureIdleAnimations.Num()];
		if (IsValid(Animation))
		{
			Figure->PlayAnimation(Animation, true);
			// Out of step, so the queue does not breathe as one.
			Figure->SetPosition(FMath::FRandRange(0.f, Animation->GetPlayLength()), false);
		}
	}
	const TArray<TObjectPtr<UStaticMeshComponent>>& Kit = M_FigureAttachments[FigureIndex];
	for (int32 Index = 0; Index < Kit.Num(); ++Index)
	{
		ApplyAttachment(Figure, Kit[Index], Look.Attachments.IsValidIndex(Index) ? &Look.Attachments[Index] : nullptr);
	}
}

void UEnemyTrainingSquadDisplayComponent::ApplyAttachment(USkeletalMeshComponent* Figure, UStaticMeshComponent* Mesh,
                                                          const FEnemyTrainingDisplayAttachment* Attachment)
{
	if (not IsValid(Mesh))
	{
		return;
	}
	if (Attachment == nullptr)
	{
		Mesh->SetVisibility(false);
		return;
	}
	Mesh->SetStaticMesh(Attachment->Mesh);
	for (int32 MaterialIndex = 0; MaterialIndex < Attachment->Materials.Num(); ++MaterialIndex)
	{
		Mesh->SetMaterial(MaterialIndex, Attachment->Materials[MaterialIndex]);
	}
	Mesh->AttachToComponent(Figure, FAttachmentTransformRules::KeepRelativeTransform, Attachment->Socket);
	Mesh->SetRelativeTransform(Attachment->RelativeTransform);
	Mesh->SetVisibility(true);
}

void UEnemyTrainingSquadDisplayComponent::HideFigure(const int32 FigureIndex)
{
	if (USkeletalMeshComponent* const Figure = M_Figures[FigureIndex])
	{
		Figure->Stop();
		Figure->SetVisibility(false);
	}
	for (UStaticMeshComponent* const Mesh : M_FigureAttachments[FigureIndex])
	{
		if (IsValid(Mesh))
		{
			Mesh->SetVisibility(false);
		}
	}
}

void UEnemyTrainingSquadDisplayComponent::ApplyRackSlot(const int32 SlotIndex, UStaticMesh* WeaponMesh)
{
	UStaticMeshComponent* const Mesh = M_RackWeapons.IsValidIndex(SlotIndex) ? M_RackWeapons[SlotIndex] : nullptr;
	if (not IsValid(Mesh))
	{
		return;
	}
	if (WeaponMesh == nullptr)
	{
		Mesh->SetVisibility(false);
		return;
	}
	Mesh->SetStaticMesh(WeaponMesh);
	// Stand the weapon on its longest axis, centred over the slot, lowest point just above it.
	const FBox Box = WeaponMesh->GetBoundingBox();
	const FVector Size = Box.GetSize();
	FRotator Rotation = FRotator::ZeroRotator;
	if (Size.X >= Size.Y && Size.X >= Size.Z)
	{
		Rotation = FRotator(90.f, 0.f, 0.f);
	}
	else if (Size.Y >= Size.X && Size.Y >= Size.Z)
	{
		Rotation = FRotator(0.f, 0.f, -90.f);
	}
	const FBox Rotated = Box.TransformBy(FTransform(Rotation));
	const FVector Offset(-Rotated.GetCenter().X, -Rotated.GetCenter().Y, RackWeaponLiftCm - Rotated.Min.Z);
	Mesh->SetRelativeTransform(FTransform(Rotation, Offset));
	Mesh->SetVisibility(true);
}

void UEnemyTrainingSquadDisplayComponent::ApplyBoardIcon(const FTrainingOption& Option)
{
	UTexture* Icon = FallbackIconTexture;
	M_ShownIconName = TEXT("fallback");
	const ESquadWeaponIcon* const IconType = Option.UnitType == EAllUnitType::UNType_Squad
		                                         ? SquadWeaponIcons.Find(static_cast<ESquadSubtype>(Option.SubtypeValue))
		                                         : nullptr;
	FSquadWeaponIconDisplaySettings Settings;
	const USquadWeaponIconSettings* const IconSettings = USquadWeaponIconSettings::Get();
	if (IconType != nullptr && *IconType != ESquadWeaponIcon::None && IconSettings != nullptr
		&& IconSettings->TryGetWeaponIconSettings(*IconType, Settings) && IsValid(Settings.WeaponIconBrush))
	{
		if (UTexture* const BrushTexture = Cast<UTexture>(Settings.WeaponIconBrush->Brush.GetResourceObject()))
		{
			Icon = BrushTexture;
			M_ShownIconName = UEnum::GetValueAsString(*IconType);
		}
	}
	for (UMaterialInstanceDynamic* const Material : M_BoardIconMaterials)
	{
		if (not IsValid(Material))
		{
			continue;
		}
		if (IsValid(Icon))
		{
			Material->SetTextureParameterValue(IconTextureParameter, Icon);
		}
		Material->SetScalarParameterValue(IconVisibleParameter, IsValid(Icon) ? 1.f : 0.f);
	}
}

void UEnemyTrainingSquadDisplayComponent::HideAll()
{
	CancelLoad();
	++M_RequestSerial;
	bM_IsShowing = false;
	M_ShownIconName.Reset();
	for (int32 FigureIndex = 0; FigureIndex < M_Figures.Num(); ++FigureIndex)
	{
		HideFigure(FigureIndex);
	}
	for (UStaticMeshComponent* const Mesh : M_RackWeapons)
	{
		if (IsValid(Mesh))
		{
			Mesh->SetVisibility(false);
		}
	}
	for (UMaterialInstanceDynamic* const Material : M_BoardIconMaterials)
	{
		if (IsValid(Material))
		{
			Material->SetScalarParameterValue(IconVisibleParameter, 0.f);
		}
	}
}

void UEnemyTrainingSquadDisplayComponent::CancelLoad()
{
	if (M_LoadHandle.IsValid())
	{
		M_LoadHandle->CancelHandle();
		M_LoadHandle.Reset();
	}
}

#if RTS_WITH_MAP_TESTS
FString UEnemyTrainingSquadDisplayComponent::MapTest_DescribeShownState() const
{
	int32 Figures = 0;
	int32 Kit = 0;
	TArray<FString> Bodies;
	for (int32 FigureIndex = 0; FigureIndex < M_Figures.Num(); ++FigureIndex)
	{
		const USkeletalMeshComponent* const Figure = M_Figures[FigureIndex];
		if (not IsValid(Figure) || not Figure->IsVisible())
		{
			continue;
		}
		++Figures;
		Bodies.AddUnique(GetNameSafe(Figure->GetSkeletalMeshAsset()));
		for (const UStaticMeshComponent* const Mesh : M_FigureAttachments[FigureIndex])
		{
			Kit += IsValid(Mesh) && Mesh->IsVisible() ? 1 : 0;
		}
	}
	TArray<FString> RackWeapons;
	for (const UStaticMeshComponent* const Mesh : M_RackWeapons)
	{
		if (IsValid(Mesh) && Mesh->IsVisible())
		{
			RackWeapons.Add(GetNameSafe(Mesh->GetStaticMesh()));
		}
	}
	return FString::Printf(TEXT("showing=%d option=%s figures=%d bodies=[%s] kit=%d rack=[%s] icon=%s"),
	                       bM_IsShowing ? 1 : 0, *M_ShownOption.GetTrainingName(), Figures,
	                       *FString::Join(Bodies, TEXT(",")), Kit, *FString::Join(RackWeapons, TEXT(",")),
	                       *M_ShownIconName);
}
#endif
