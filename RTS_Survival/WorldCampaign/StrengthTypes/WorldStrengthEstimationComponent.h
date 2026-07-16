#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldStrengthContribution.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/MapObjects/W_EnemyMapItem/StrengthEstimation/DataAndUtils/RTSStrengthEstimationTypes.h"
#include "WorldStrengthEstimationComponent.generated.h"

/**
 * @brief Authoritative category-aware strength report for a world map object.
 */
UCLASS(ClassGroup=(WorldCampaign), meta=(BlueprintSpawnableComponent), BlueprintType)
class RTS_SURVIVAL_API UWorldStrengthEstimationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWorldStrengthEstimationComponent();

	/**
	 * @brief Returns the cached, category-aware report used by map item UI widgets.
	 * @return The current strength report. The reference stays owned by this component.
	 */
	const FWorldStrengthReport& GetStrengthEstimationMessage() const;

	/**
	 * @brief Gets the summed strength percentage across fortifications, field divisions, and strategic support.
	 * @return Total strength percentage currently shown to the player.
	 */
	int32 GetTotalStrengthPercentage() const;

	/**
	 * @brief Builds the typed contribution ledger copied into an operation map context.
	 * @return Value-only strength contributions with their enum source identities.
	 */
	TArray<FWorldStrengthContribution> GetStrengthContributions() const;

	/**
	 * @brief Gets the strategic support enum values currently affecting this object.
	 * @return Active strategic support types collected during the latest strategic support recalculation.
	 */
	const TArray<EWorldStrategicSupport>& GetStrategicSupportTypes() const;

	/**
	 * @brief Replaces the base fortification strength reason for this map object.
	 * @param StrengthReason Reason built from world data for the object's enemy or mission type.
	 */
	void SetBaseFortificationStrengthReason(const FWorldStrengthReason& StrengthReason);

	/**
	 * @brief Gets the base fortification strength percentage without modifiers or support.
	 * @return Base fortification strength percentage.
	 */
	int32 GetBaseFortificationStrengthPercentage() const;

	/**
	 * @brief Sets only the base fortification strength percentage using the default base-strength text.
	 * @param StrengthPercentage New base fortification strength percentage.
	 */
	void SetBaseFortificationStrengthPercentage(int32 StrengthPercentage);

	/**
	 * @brief Adds to the cached base fortification strength percentage.
	 * @param AddedStrengthPercentage Delta to apply to the base fortification strength.
	 */
	void AddBaseFortificationStrengthPercentage(int32 AddedStrengthPercentage);

	/**
	 * @brief Clears only fortification modifier reasons while preserving base, field division, and strategic support data.
	 */
	void ResetFortificationModifierReport();

	/**
	 * @brief Adds one non-zero fortification modifier reason to the fortification category.
	 * @param FortificationStrength Fortification enum that produced the contribution.
	 * @param StrengthReason Reason built from an EWorldFortificationStrength definition in WorldData.
	 */
	void AddFortificationModifierReason(EWorldFortificationStrength FortificationStrength,
	                                    const FWorldStrengthReason& StrengthReason);

	/**
	 * @brief Clears only strategic support reasons while preserving fortification and field division data.
	 */
	void ResetStrategicSupportReport();

	/**
	 * @brief Adds one non-zero strategic support reason to the strategic support category.
	 * @param StrengthReason Reason built from an EWorldStrategicSupport definition in WorldData.
	 */
	void AddStrategicSupportReason(const FWorldStrengthReason& StrengthReason);

	/**
	 * @brief Adds one non-zero strategic support reason and caches the enum type that produced it.
	 * @param StrategicSupport Strategic support enum affecting this object.
	 * @param StrengthReason Reason built from the strategic support definition in WorldData.
	 */
	void AddStrategicSupportReason(EWorldStrategicSupport StrategicSupport,
	                               const FWorldStrengthReason& StrengthReason);

	/**
	 * @brief Adds strategic support with the campaign anchor that supplied it.
	 * @param StrategicSupport Strategic support enum affecting this object.
	 * @param SourceAnchorKey Stable anchor key of the supporting world map object.
	 * @param StrengthReason Reason built from the strategic support definition in WorldData.
	 */
	void AddStrategicSupportReason(EWorldStrategicSupport StrategicSupport,
	                               const FGuid& SourceAnchorKey,
	                               const FWorldStrengthReason& StrengthReason);

	/**
	 * @brief Clears only field division reasons while preserving fortification and strategic support data.
	 */
	void ResetFieldDivisionReport();

	/**
	 * @brief Adds one non-zero field division reason to the field division category.
	 * @param FieldDivision Division enum that produced the contribution.
	 * @param DivisionKey Stable campaign identity of the contributing division.
	 * @param OwningPlayer Player id used to determine whether the signed contribution is friendly or hostile.
	 * @param StrengthReason Reason supplied by the runtime field division system.
	 */
	void AddFieldDivisionReason(EWorldFieldDivisions FieldDivision,
	                            const FGuid& DivisionKey,
	                            int32 OwningPlayer,
	                            const FWorldStrengthReason& StrengthReason);

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Strength Estimation",
		meta = (AllowPrivateAccess = "true"))
	FWorldStrengthContribution M_BaseFortificationStrengthContribution;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Strength Estimation",
		meta = (AllowPrivateAccess = "true"))
	TArray<FWorldStrengthContribution> M_FortificationModifierContributions;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Strength Estimation",
		meta = (AllowPrivateAccess = "true"))
	TArray<FWorldStrengthContribution> M_StrategicSupportContributions;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Strength Estimation",
		meta = (AllowPrivateAccess = "true"))
	TArray<EWorldStrategicSupport> M_StrategicSupportTypes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Strength Estimation",
		meta = (AllowPrivateAccess = "true"))
	TArray<FWorldStrengthContribution> M_FieldDivisionContributions;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Strength Estimation",
		meta = (AllowPrivateAccess = "true", ShowOnlyInnerProperties))
	FWorldStrengthReport M_StrengthEstimationMessage;

	/**
	 * @brief Rebuilds the UI-facing report from the component's separate category caches.
	 */
	void RebuildStrengthEstimationMessage();
};
