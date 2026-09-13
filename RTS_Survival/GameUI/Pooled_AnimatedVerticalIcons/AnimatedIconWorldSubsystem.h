#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AnimatedIconWorldSubsystem.generated.h"

class UAnimatedIconWidgetPoolManager;

/** @brief Blueprints obtain the shared icon pool here; one world tick drives all active icon animations. */
UCLASS()
class RTS_SURVIVAL_API UAnimatedIconWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override;
	virtual TStatId GetStatId() const override;

	UFUNCTION(BlueprintPure, Category="Animated Icons")
	UAnimatedIconWidgetPoolManager* GetAnimatedIconWidgetPoolManager() const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAnimatedIconWidgetPoolManager> M_PoolManager;

	bool GetIsValidPoolManager() const;
};
