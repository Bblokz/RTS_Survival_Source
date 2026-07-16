#pragma once

#include "PCGSettings.h"
#include "PCGWorldMissionContextData.generated.h"

/**
 * @brief Emits the loaded enemy-object enum as a PCG Attribute Set for runtime base routing.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGGetWorldMissionEnemyObjectSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
#endif

	virtual bool HasDynamicPins() const override { return false; }
	virtual FPCGElementPtr CreateElement() const override;

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
};

/**
 * @brief Reads the game-instance mission snapshot on the main thread and emits its enemy enum value.
 */
class FPCGGetWorldMissionEnemyObjectElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};

/**
 * @brief Emits the loaded operation's summed strength percentage as a PCG Attribute Set.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGGetWorldMissionStrengthSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
#endif

	virtual bool HasDynamicPins() const override { return false; }
	virtual FPCGElementPtr CreateElement() const override;

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
};

/**
 * @brief Reads the game-instance mission snapshot on the main thread and emits its calculated total strength.
 */
class FPCGGetWorldMissionStrengthElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};
