#pragma once

#include "CoreMinimal.h"
#include "AnimatedTextWorldSubsystem.generated.h"

class UAnimatedTextWidgetPoolManager;

/**
 * @brief World subsystem that owns and ticks the animated text widget pool manager.
 * Automatically initialises on world load and shuts down on world unload.
 */
UCLASS()
class RTS_SURVIVAL_API UAnimatedTextWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// UWorldSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return false; }

	/** Get the pool manager; can be called from Blueprints. */
	UFUNCTION(BlueprintCallable, Category="Animated Text")
	UAnimatedTextWidgetPoolManager* GetAnimatedTextWidgetPoolManager() const { return M_PoolManager; }

private:
	UPROPERTY()
	UAnimatedTextWidgetPoolManager* M_PoolManager = nullptr;

	/** Validity helper as per rule 0.5. */
	bool GetIsValidPoolManager() const;
};
