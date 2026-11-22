// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Units/Squads/SquadControllerHpComp/SquadWeaponIcons/SquadWeaponIcons.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#include "EnemyTrainingSquadDisplayComponent.generated.h"

class UAnimSequenceBase;
class UEnemyBuildingTrainerComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPrimitiveComponent;
class USceneComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTexture;
struct FStreamableHandle;
enum class EEnemyBuildingTrainingEvent : uint8;

// One static mesh a squad member carries on its body (its weapon, a fuel tank, a helmet): copied from the unit Blueprint.
struct FEnemyTrainingDisplayAttachment
{
	TObjectPtr<UStaticMesh> Mesh = nullptr;
	TArray<TObjectPtr<UMaterialInterface>> Materials;
	FName Socket = NAME_None;
	// Relative to the socket of the body mesh.
	FTransform RelativeTransform = FTransform::Identity;
	bool bIsWeapon = false;
};

// How one squad member looks: its body and what hangs on it.
struct FEnemyTrainingDisplayMemberLook
{
	TObjectPtr<USkeletalMesh> BodyMesh = nullptr;
	TArray<TObjectPtr<UMaterialInterface>> BodyMaterials;
	FVector BodyScale = FVector::OneVector;
	TArray<FEnemyTrainingDisplayAttachment> Attachments;
};

/**
 * @brief Shows WHICH squad a Soviet infantry building trains: stand-in soldiers that look like the squad's own
 * members (body mesh, materials, weapon and kit, all read from the squad's Blueprints), the squad's weapons in a rack
 * and the squad's weapon icon on a board.
 *
 * Reacts to the UEnemyBuildingTrainerComponent of the same actor. While a squad trains:
 * - skeletal meshes tagged FigureTag (in name order) take the looks of the squad's members, one each, and play one of
 *   FigureIdleAnimations from a random point; figures beyond the squad size stay hidden;
 * - scene components tagged RackSlotTag each get one member's weapon, stood upright on the slot;
 * - primitives tagged BoardIconTag get IconTextureParameter = the squad's healthbar weapon icon (USquadWeaponIconSettings)
 *   and IconVisibleParameter = 1. Rifle squads have no healthbar icon and show FallbackIconTexture.
 * When the building goes idle everything is hidden again and the icon goes to 0.
 *
 * The squad class comes from the async spawner, the same lookup the trainer uses to spawn the unit, so director
 * overrides and faction variants show what will walk out. Looks are cached per squad class.
 * @note Everything is attached to the building actor, so the fog of war (actor hidden) hides it with the building.
 */
UCLASS(ClassGroup=(RTS), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UEnemyTrainingSquadDisplayComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyTrainingSquadDisplayComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Squad Display")
	FName FigureTag = TEXT("AITrainingSquadFigure");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Squad Display")
	FName RackSlotTag = TEXT("AITrainingRackSlot");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Squad Display")
	FName BoardIconTag = TEXT("AITrainingSquadBoardIcon");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Squad Display")
	FName IconTextureParameter = TEXT("IconTexture");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Squad Display")
	FName IconVisibleParameter = TEXT("IconVisible");

	// Idle loops for the stand-in soldiers (InfantryMaster skeleton); each figure picks one.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Squad Display")
	TArray<TObjectPtr<UAnimSequenceBase>> FigureIdleAnimations;

	// Board icon of squads whose weapons have no healthbar icon (plain rifles).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Squad Display")
	TObjectPtr<UTexture> FallbackIconTexture = nullptr;

	// The squad's healthbar weapon icon (what the player sees above the squad once it is out).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Squad Display")
	TMap<ESquadSubtype, ESquadWeaponIcon> SquadWeaponIcons;

	// Shown when the trained squad's look cannot be read (no spawner row, a team weapon without crew classes).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Squad Display")
	ESquadSubtype FallbackSquad = ESquadSubtype::Squad_Rus_Mosin;

	// Rack weapons stand with their longest axis up, their lowest point this far above the slot.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Squad Display")
	float RackWeaponLiftCm = 2.f;

	UFUNCTION(BlueprintPure, Category = "Squad Display")
	bool GetIsShowingSquad() const { return bM_IsShowing; }

#if RTS_WITH_MAP_TESTS
	// Shown figures, shown rack weapons and the icon name, for the trainer test's log.
	FString MapTest_DescribeShownState() const;
#endif

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TWeakObjectPtr<UEnemyBuildingTrainerComponent> M_Trainer;

	UPROPERTY()
	TArray<TObjectPtr<USkeletalMeshComponent>> M_Figures;

	// Per figure, the static meshes created for its attachments (reused between squads).
	TArray<TArray<TObjectPtr<UStaticMeshComponent>>> M_FigureAttachments;

	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> M_RackSlots;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> M_RackWeapons;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> M_BoardIconMaterials;

	// Every mesh/attachment created at runtime, kept alive.
	UPROPERTY()
	TArray<TObjectPtr<UObject>> M_KeptAssets;

	TMap<FSoftObjectPath, TArray<FEnemyTrainingDisplayMemberLook>> M_LookCache;
	TSharedPtr<FStreamableHandle> M_LoadHandle;
	// Bumped by every show/hide: a load that finishes after the next one started is dropped.
	int32 M_RequestSerial = 0;
	FTrainingOption M_ShownOption;
	bool bM_IsShowing = false;
	FString M_ShownIconName;

	bool GetIsValidTrainer() const;
	void CollectTaggedComponents();
	void AddBoardIconMaterial(UPrimitiveComponent* Board);
	// Single-node animation, no collision, and the static meshes its kit is shown with.
	void PrepareFigure(const int32 FigureIndex);
	static UStaticMesh* GetWeaponMesh(const FEnemyTrainingDisplayMemberLook& Look);
	void ApplyAttachment(USkeletalMeshComponent* Figure, UStaticMeshComponent* Mesh,
	                     const FEnemyTrainingDisplayAttachment* Attachment);
	void OnTrainerEvent(EEnemyBuildingTrainingEvent Event, const FTrainingOption& Option, AActor* Unit, float Duration);

	void RequestShow(const FTrainingOption& Option, const bool bIsFallbackAttempt);
	void OnSquadControllerClassLoaded(const int32 Serial, const FTrainingOption Option, const FSoftObjectPath ControllerPath,
	                                  const bool bIsFallbackAttempt);
	void OnSquadUnitClassesLoaded(const int32 Serial, const FTrainingOption Option, const FSoftObjectPath ControllerPath,
	                              const TArray<FSoftObjectPath> UnitPaths, const bool bIsFallbackAttempt);
	void ShowLooks(const FTrainingOption& Option, const TArray<FEnemyTrainingDisplayMemberLook>& Looks);
	void HideAll();
	void CancelLoad();

	static bool TryBuildMemberLook(UClass* UnitClass, FEnemyTrainingDisplayMemberLook& OutLook);
	static void CollectWeaponAttachment(UClass* WeaponClass, const FName Socket, const FTransform& ChildActorRelative,
	                                    TArray<FEnemyTrainingDisplayAttachment>& OutAttachments);

	void ApplyFigure(const int32 FigureIndex, const FEnemyTrainingDisplayMemberLook& Look);
	void HideFigure(const int32 FigureIndex);
	void ApplyRackSlot(const int32 SlotIndex, UStaticMesh* WeaponMesh);
	void ApplyBoardIcon(const FTrainingOption& Option);
	UStaticMeshComponent* CreateAttachedStaticMesh(USceneComponent* Parent, const FName Name);
};
