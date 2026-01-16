// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Icons/BehaviourIcon.h"
#include "RTS_Survival/RTSComponents/AOEBehaviourComponent/AOEBehaviourComponent.h"
#include "RTS_Survival/RTSComponents/AOEBehaviourComponent/RadiusAOEBehaviourComponent/EAOeBehaviourShowRadiusType.h"
#include "RTS_Survival/Subsystems/RadiusSubsystem/ERTSRadiusType.h"

#include "RadiusAOEBehaviourComponent.generated.h"

class UBehaviour;
class UBehaviourComp;
class UOnHoverShowRadiusBehaviour;
class URTSRadiusPoolSubsystem;
class USelectionComponent;

/**
 * @brief Settings for radius display and hover behaviour on AOE components.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FRadiusAOEBehaviourSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AOE Behaviour|Radius")
	ERTSRadiusType RadiusType = ERTSRadiusType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AOE Behaviour|Radius")
	EAOeBehaviourShowRadiusType ShowRadiusType = EAOeBehaviourShowRadiusType::None;

	// Note that the hover functionality is only used if set to do so by the enum.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AOE Behaviour|Radius")
	TSubclassOf<UOnHoverShowRadiusBehaviour> HoverShowRadiusBehaviourClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AOE Behaviour|Radius")
	FString HostBehaviourText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AOE Behaviour|Radius")
	EBehaviourIcon HostBehaviourIcon = EBehaviourIcon::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AOE Behaviour|Radius")
	bool bMoveRadius = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AOE Behaviour|Radius")
	FVector RadiusRelativeAttachmentOffset = FVector::ZeroVector;
};

/**
 * @brief Extends AOE behaviour components with pooled radius visualisation support.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), BlueprintType)
class RTS_SURVIVAL_API URadiusAOEBehaviourComponent : public UAOEBehaviourComponent
{
	GENERATED_BODY()

public:
	URadiusAOEBehaviourComponent();

	void OnHostBehaviourHovered(const bool bIsHovering);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void SetHostBehaviourUIData(UBehaviour& Behaviour) const;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AOE Behaviour|Radius", meta=(AllowPrivateAccess="true"))
	FRadiusAOEBehaviourSettings RadiusSettings;

	
private:
	static constexpr int32 InvalidRadiusId = -1;


	UPROPERTY()
	TWeakObjectPtr<URTSRadiusPoolSubsystem> M_RadiusPoolSubsystem;

	UPROPERTY()
	TObjectPtr<UBehaviourComp> M_BehaviourComponent = nullptr;

	UPROPERTY()
	TObjectPtr<USelectionComponent> M_SelectionComponent = nullptr;

	int32 M_RadiusId = InvalidRadiusId;

	void BeginPlay_CacheRadiusSubsystem();
	void BeginPlay_CacheBehaviourComponent();
	void BeginPlay_CacheSelectionComponent();
	void BeginPlay_SetupHostBehaviour();
	void BeginPlay_SetupSelectionBindings();

	void ShowRadius();
	void HideRadius();
	bool GetShouldShowRadiusOnHover() const;
	bool GetShouldShowRadiusOnSelection() const;

	bool GetIsValidRadiusSubsystem() const;
	bool GetIsValidBehaviourComponent() const;
};
