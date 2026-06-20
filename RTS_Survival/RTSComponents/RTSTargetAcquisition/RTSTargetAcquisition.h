// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTSEngagementStance/RTSEngagementStance.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/TargetPreference/TargetPreference.h"
#include "RTSTargetAcquisition.generated.h"


class ICommands;
class URTSComponent;
class UGameUnitManager;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API URTSTargetAcquisition : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	URTSTargetAcquisition();
	
	virtual void OnUnitIdleAndNoNewCommands();
	
	void SetEngagementStance(const ERTSEngagementStance NewStance);
	// Called by the owner when the owner is fully Initialized and ready to make use of TargetAcquisition
	void Activate();
	

protected:
	// ------- To overwrite in derived components -------
	
	virtual void IssueAttackClosestVisibleTargetInAggroRange(AActor* TargetActor);
	virtual ETargetPreference GetOwnerTargetPreference()const;
	// Checks whether set to aggressive in the base Target Acquisition component.
	virtual bool CanAggroEnemies()const;
	virtual float GetOwnerRange()const;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	ERTSEngagementStance EngagementStance = ERTSEngagementStance::Stance_HoldPosition;
	
	// Called when the game starts
	virtual void BeginPlay() override;
	
	
	
	
	virtual void OnTargetsFound(const TArray<AActor*>& Targets);
	int32 GetOwningPlayer()const;
	
	
	bool GetOwnerAsICommands(ICommands*& OwnerICommands) const;
	
	
	// Whether the component's stance permit an aggro timer to look for targets.
	bool IsAggroTimerAllowed()const;
	
	void StartAggroTimer();
	void StopAggroTimer();
	
	
private:	
	
	void BeginPlay_InitGameUnitManager();
	void BeginPlay_InitRTSComponent();
	UPROPERTY()
	TWeakObjectPtr<UGameUnitManager> M_GameUnitManager;
	[[nodiscard]] bool EnsureIsValidGameUnitManager()const;
	UPROPERTY()
	TWeakObjectPtr<URTSComponent> M_OwnerRTSComponent;
	[[nodiscard]] bool EnsureIsValidRTSComponent()const;
	
	static const float TargetAcquisitionInterval ;
	
	UPROPERTY()
	FTimerHandle M_AggroTimer;
	
	FVector GetOwnerLocation(bool& OutbValid)const;
	
	void SearchForClosestTarget(const FVector& SearchOrigin,
								const float Range,
								ETargetPreference TargetPreference
	                            
	);

};
