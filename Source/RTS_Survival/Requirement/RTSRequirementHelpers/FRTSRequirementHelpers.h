// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"

enum class ERTSRequirement : uint8;
enum class ETrainingItemStatus : uint8;
class UExpansionRequirement;
class UObject;
class URTSRequirement;
class UVacantAirPadRequirement;
class UUnitRequirement;
class UTechRequirement;
struct FTrainingOption;
enum class ETechnology : uint8;

/**
 * @brief Factory helpers for creating requirement objects.
 * @note Requirements are created with the provided Outer. Pass GetTransientPackage() for ephemeral templates.
 */
struct RTS_SURVIVAL_API FRTS_RequirementHelpers
{
	/** @brief Create a tech requirement. */
	static UTechRequirement* CreateTechRequirement(UObject* Outer, const ETechnology RequiredTechnology);

	/** @brief Create a unit requirement. */
	static UUnitRequirement* CreateUnitRequirement(UObject* Outer, const FTrainingOption& UnitRequired);

	/** @brief Create a vacant air pad requirement. */
	static UVacantAirPadRequirement* CreateVacantAirPadRequirement(UObject* Outer);

	/** @brief Create an expansion requirement. */
    static UExpansionRequirement* CreateExpansionRequirement(UObject* Outer, const FTrainingOption& ExpansionRequired);

	static ETrainingItemStatus MapKindToStatus(const ERTSRequirement& Requirement);
    

	// ------------------------------------------------------------------------
	// ------------------------ Double (AND) requirements ----------------------
	// ------------------------------------------------------------------------

	/**
	 * @brief Create a composite (AND) requirement from two existing requirements.
	 * @param Outer   Object outer (use GetTransientPackage() for templates).
	 * @param First   First child requirement (Unit/Tech/Pad/etc.). Will be duplicated into the composite.
	 * @param Second  Second child requirement (Unit/Tech/Pad/etc.). Will be duplicated into the composite.
	 * @return The composite requirement (as URTSRequirement*), or nullptr on failure.
	 */
	static URTSRequirement* CreateDoubleRequirement(
		UObject* Outer,
		URTSRequirement* First,
		URTSRequirement* Second);

	/**
	 * @brief Convenience: Unit AND Unit.
	 * @param Outer         Object outer.
	 * @param RequiredUnitA First unit requirement.
	 * @param RequiredUnitB Second unit requirement.
	 */
	static URTSRequirement* CreateDouble_Unit_Unit(
		UObject* Outer,
		const FTrainingOption& RequiredUnitA,
		const FTrainingOption& RequiredUnitB);

	/**
	 * @brief Convenience: Unit AND Tech.
	 * @param Outer        Object outer.
	 * @param RequiredUnit Unit requirement.
	 * @param RequiredTech Tech requirement.
	 */
	static URTSRequirement* CreateDouble_Unit_Tech(
		UObject* Outer,
		const FTrainingOption& RequiredUnit,
		ETechnology RequiredTech);

	/**
	 * @brief Convenience: Tech AND Tech.
	 * @param Outer          Object outer.
	 * @param RequiredTechA  First tech.
	 * @param RequiredTechB  Second tech.
	 */
	static URTSRequirement* CreateDouble_Tech_Tech(
		UObject* Outer,
		ETechnology RequiredTechA,
		ETechnology RequiredTechB);

	/**
	 * @brief Convenience: Unit AND VacantAirPad.
	 * @param Outer        Object outer.
	 * @param RequiredUnit Unit requirement.
	 */
	static URTSRequirement* CreateDouble_Unit_VacantPad(
		UObject* Outer,
		const FTrainingOption& RequiredUnit);

	/**
	 * @brief Convenience: Tech AND VacantAirPad.
	 * @param Outer        Object outer.
	 * @param RequiredTech Tech requirement.
	 */
	static URTSRequirement* CreateDouble_Tech_VacantPad(
		UObject* Outer,
		ETechnology RequiredTech);

	/**
     * @brief Convenience: Expansion AND Expansion.
     * @param Outer               Object outer.
     * @param RequiredExpansionA  First expansion requirement.
     * @param RequiredExpansionB  Second expansion requirement.
     */
    static URTSRequirement* CreateDouble_Expansion_Expansion(
    	UObject* Outer,
    	const FTrainingOption& RequiredExpansionA,
    	const FTrainingOption& RequiredExpansionB);
    
    /**
     * @brief Convenience: Unit AND Expansion.
     * @param Outer            Object outer.
     * @param RequiredUnit     Unit requirement.
     * @param RequiredExpansion Expansion requirement.
     */
    static URTSRequirement* CreateDouble_Unit_Expansion(
    	UObject* Outer,
    	const FTrainingOption& RequiredUnit,
    	const FTrainingOption& RequiredExpansion);
    
    /**
     * @brief Convenience: Tech AND Expansion.
     * @param Outer             Object outer.
     * @param RequiredTech      Tech requirement.
     * @param RequiredExpansion Expansion requirement.
     */
    static URTSRequirement* CreateDouble_Tech_Expansion(
    	UObject* Outer,
    	ETechnology RequiredTech,
    	const FTrainingOption& RequiredExpansion);
    
    /**
     * @brief Convenience: Expansion AND VacantAirPad.
     * @param Outer               Object outer.
     * @param RequiredExpansion   Expansion requirement.
     */
    static URTSRequirement* CreateDouble_Expansion_VacantPad(
    	UObject* Outer,
    	const FTrainingOption& RequiredExpansion);
	
	static URTSRequirement* CreateOrRequirement(
    	UObject* Outer,
    	URTSRequirement* First,
    	URTSRequirement* Second);
    
    static URTSRequirement* CreateOr_Unit_Unit(
    	UObject* Outer,
    	const FTrainingOption& RequiredUnitA,
    	const FTrainingOption& RequiredUnitB);
    
    static URTSRequirement* CreateOr_Unit_Tech(
    	UObject* Outer,
    	const FTrainingOption& RequiredUnit,
    	const ETechnology RequiredTech);
    
    static URTSRequirement* CreateOr_Tech_Tech(
    	UObject* Outer,
    	const ETechnology RequiredTechA,
    	const ETechnology RequiredTechB);
    
    static URTSRequirement* CreateOr_Unit_VacantPad(
    	UObject* Outer,
    	const FTrainingOption& RequiredUnit);
    
    static URTSRequirement* CreateOr_Tech_VacantPad(
    	UObject* Outer,
    	const ETechnology RequiredTech);

static URTSRequirement* CreateOr_Expansion_Unit(
	UObject* Outer,
	const FTrainingOption& RequiredExpansion,
	const FTrainingOption& RequiredUnit);

static URTSRequirement* CreateOr_Expansion_Tech(
	UObject* Outer,
	const FTrainingOption& RequiredExpansion,
	const ETechnology RequiredTech);

static URTSRequirement* CreateOr_Expansion_VacantPad(
	UObject* Outer,
	const FTrainingOption& RequiredExpansion);

static URTSRequirement* CreateOr_Expansion_Expansion(
	UObject* Outer,
	const FTrainingOption& RequiredExpansionA,
	const FTrainingOption& RequiredExpansionB);
	
};
