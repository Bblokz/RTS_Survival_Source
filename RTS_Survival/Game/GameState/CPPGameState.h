// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Game/GameSettings/RTSGameSettings.h"
#include "RTS_Survival/Game/GameUpdateComponent/RTSGameSettingsHandler.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AttachedRockets/RocketAbilityTypes.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AttachedRockets/AttachedRocketsData/AttachedRocketsData.h"
#include "RTS_Survival/RTSComponents/ExperienceComponent/RTSExperienceLevels.h"
#include "RTS_Survival/UnitData/UnitAbilityEntry.h"
#include "RTS_Survival/UnitData/VehicleData.h"
#include "RTS_Survival/UnitData/NomadicVehicleData.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/UnitData/SquadData.h"
#include "RTS_Survival/UnitData/BuildingExpansionData.h"
#include "RTS_Survival/UnitData/AircraftData.h"
#include "RTS_Survival/UnitData/DigInData.h"
#include "RTS_Survival/Weapons/SmallArmsProjectileManager/ProjectileManagerCallbacks/ProjectileManagerCallBacks.h"

#include "CPPGameState.generated.h"

enum class EDigInType : uint8;
class UGameDecalManager;
class UGameExplosionsManager;
class ASmallArmsProjectileManager;
enum class ETankSubtype : uint8;
class UGameUnitManager;
class RTS_SURVIVAL_API UGameResourceManager;
class UResourceComponent;
class UResourceDropOff;
class URTSGameSettingsHandler;
enum class EWeaponName : uint8;
// Forward Declaration
class RTS_SURVIVAL_API ACPPResourceMaster;
class RTS_SURVIVAL_API ATankMaster;
/**
 * @brief Contains all living actors on the map relevant for gameplay logic.
 *
 * Player 0 has neutral / not s
 * Only player 1 and 2 have tanks and infantry units.
 * 0 Are neutral units.
 * Player 3 has all mutated units and other hostiles.
 */

UENUM(Blueprintable)
enum EGameTimeUnit
{
	Seconds,
	Minutes,
	Hours,
	Days
};

UCLASS()
class RTS_SURVIVAL_API ACPPGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ACPPGameState();

	inline UGameResourceManager* GetGameResourceManager() const { return M_GameResourceManager; };

	inline UGameUnitManager* GetGameUnitManager() const { return M_GameUnitManager; };
	// BP callable so we can call init on it with the explosions mapping.
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	inline UGameExplosionsManager* GetGameExplosionManager() const { return M_GameExplosionsManager; };

	// Bp callable so we can init the decals mapping in bp reference casts.
		UFUNCTION(BlueprintCallable, NotBlueprintable)
	inline UGameDecalManager* GetGameDecalManager() const { return M_GameDecalManager; };

	void RegisterCallbackForSmallArmsProjectileMgr(
		const TFunction<void(const TObjectPtr<ASmallArmsProjectileManager>&)>& Callback, TWeakObjectPtr<UObject> WeakCallbackOwner);

	/**
	 * @brief Updates the provided gamesetting. Non templated as blueprints do not support templates
	 * @param Setting Enum value of the setting to update.
	 * @param bValue The new value of the setting.
	 */
	UFUNCTION(BlueprintCallable, Category="GameSettings")
	void SetGameSetting(ERTSGameSetting Setting, bool bValue = false, float fValue = 0, int iValue = 0);

	/**
	 * 
	 * @tparam SettingType The type of setting to obtain.
	 * @param Setting The enum that identifies the setting.
	 * @return The value of the requested setting.
	 */
	template <typename SettingType>
	SettingType GetGameSetting(const ERTSGameSetting Setting) const;

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="GameTime")
	void StartClock();

	FWeaponData* GetWeaponDataOfPlayer(const int32 PlayerOwningWeapon, const EWeaponName WeaponName);
	const FWeaponData* GetDefaultFallBackWeapon() const;
	/**
	 * @brief Update a weapon with the new weapon data struct.
	 * @param PlayerOwningWeapon For which player we update the weapon.
	 * @param WeaponName Enum identifying the weapon.
	 * @param NewWeaponData The new weapon data to update the weapon with.
	 * @return Whether the upgrade completed, if there is no WeaponData entry for the specified enum the function will return false.
	 */
	bool UpgradeWeaponDataForPlayer(const int32 PlayerOwningWeapon, const EWeaponName WeaponName,
	                                const FWeaponData& NewWeaponData);

	/**
	 * @param PlayerOwningTank Which player owns the tank; specifies in which data hashmap to look.
	 * @param TankSubtype THe subtype of the tank to get the data for.
	 * @return The tank data for the specified player and tank subtype.
	 * @note May return null if map does not contain the type, check always before using.
	 */
	FTankData GetTankDataOfPlayer(const int32 PlayerOwningTank, const ETankSubtype TankSubtype) const;

	/**
	 * @brief Updates tankdata with the new data struct provided.
	 * @param PlayerOwningTank For which player we update the tank.
	 * @param TankSubtype What tank subtype to update.
	 * @param NewTankData The new tank data to update the tank with.
	 * @return Whether the upgrade completed, if there is no TankData entry for the specified enum the function will return false.
	 */
	bool UpgradeTankDataForPlayer(const int32 PlayerOwningTank, const ETankSubtype TankSubtype,
	                              const FTankData& NewTankData);

	FNomadicData GetNomadicDataOfPlayer(const int32 PlayerOwningNomadic, const ENomadicSubtype NomadicSubtype) const;

	FSquadData GetSquadDataOfPlayer(const int32 PlayerOwningSquad, const ESquadSubtype SquadSubtype) const;

	FDigInData GetDigInDataOfPlayer(const int32 PlayerOwningDigIn, const EDigInType DigInType) const;

	FAttachedRocketsData GetAttachedRocketDataOfPlayer(const int32 PlayerOwningRocket,
		const ERocketAbility RocketType) const;

	FBxpData GetPlayerBxpData(const EBuildingExpansionType BxpType) const;
	FAircraftData GetAircraftDataOfPlayer(const EAircraftSubtype AircraftSubtype, const uint8 PlayerOwningAircraft) const;
	FBxpData GetEnemyBxpData(const EBuildingExpansionType BxpType) const;

	FTankData GetTankData(const ETankSubtype TankSubtype) const;

	bool UpgradeSquadDataForPlayer(const int32 PlayerOwningSquad, const ESquadSubtype SquadSubtype,
	                               const FSquadData& NewSquadData);

	bool UpgradeNomadicDataForPlayer(const int32 PlayerOwningNomadic, const ENomadicSubtype NomadicSubtype,
	                                 const FNomadicData& NewNomadicData);

	inline const FRTSGameSettings& GetGameSettings() const { return M_RTSGameSettings->GetGameSettings(); }

        TArray<FUnitAbilityEntry> GetNomadicAbilities(const ENomadicSubtype NomadicSubtype) const;
        TArray<FUnitAbilityEntry> GetSquadAbilities(const ESquadSubtype SquadSubtype) const;
        TArray<FUnitAbilityEntry> GetTankAbilities(const ETankSubtype TankSubtype) const;
        TArray<FUnitAbilityEntry> GetBxpAbilities(const EBuildingExpansionType BxpType) const;

	/**
	 * For the given GameTimeUnit, returns the current game time (time survived on map)
	 * @param GameTimeUnit Seconds, Minutes, Hours, Days
	 * @return The current game time in the given GameTimeUnit.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="GameTime")
	int GetGameTime(const EGameTimeUnit GameTimeUnit) const;

	/**
	 * To provide the string format of the global game clock.
	 * @param GameTimeUnit The unit of time to return the game time in.
	 * @return Returns the game time as days, hours, minutes, seconds in xxxx xx:xx:xx format.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="GameTime")
	FString GetGameTimeAsString(const EGameTimeUnit GameTimeUnit) const;

	// blueprint class derived from ASmallArmsProjectileManager to spawn to handle projectiles.
	UPROPERTY(EditDefaultsOnly, Category="Projectile")
	TSubclassOf<ASmallArmsProjectileManager> ProjectileManagerClass;

protected:
	virtual void Tick(float DeltaSeconds) override;

	virtual void BeginPlay() override;

	virtual void PostInitializeComponents() override;

	// Call at begin play; will ini the MPC time material collection to save world time.
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetupMPCTime(UMaterialParameterCollection* MPC_Time);

	
private:
	// For when regular weapon data is not found.
	FWeaponData DefaultWeapon;

	// Contains the arrays of resource components on the map and the resource
	// drop off components of the player.
	UPROPERTY()
	TObjectPtr<UGameResourceManager> M_GameResourceManager;

	// Keeps track of all the playable units of both the player and the enemies.
	UPROPERTY()
	TObjectPtr<UGameUnitManager> M_GameUnitManager;

	// Pools explosions for the game.
	UPROPERTY()
	TObjectPtr<UGameExplosionsManager> M_GameExplosionsManager;

	UPROPERTY()
	TObjectPtr<UGameDecalManager> M_GameDecalManager;

	// Sets the reference on the ExplosionManagerSubsystem.
	void InitExplMgrOnSubsystem();
	void InitDecalMgrOnSubSystem();

	int M_GameTimeDays;
	int M_GameTimeHours;
	int M_GameTimeMinutes;
	float M_GameTimeSeconds;

	bool bM_IsClockRunning;

	// Fills the WeaponDataHasMap of M_TWeaponDataHashMap with the data for each weapon name.
	void InitAllGameWeaponData();
	void InitAllGameSmallArmsWeapons();
	void InitAllGameRailGunData();
	void InitAllGameLaserWeapons();
	void InitAllGameFlameWeapons();
	void InitAllGameBombWeapons();
	void InitAllGameLightWeapons();
	void InitAllGameMediumWeapons();
	void InitAllGameHeavyWeapons();

	UPROPERTY()
	TMap<EWeaponName, FWeaponData> M_TPlayerWeaponDataHashMap;

	UPROPERTY()
	TMap<EWeaponName, FWeaponData> M_TEnemyWeaponDataHashMap;

	UPROPERTY()
	TMap<ETankSubtype, FTankData> M_TPlayerTankDataHashMap;

	UPROPERTY()
	TMap<ETankSubtype, FTankData> M_TEnemyTankDataHashMap;

	UPROPERTY()
	TMap<EAircraftSubtype, FAircraftData> M_TPlayerAircraftDataHashMap;

	UPROPERTY()
	TMap<EAircraftSubtype, FAircraftData> M_TEnemyAircraftDataHashMap;

	UPROPERTY()
	TMap<ENomadicSubtype, FNomadicData> M_TPlayerNomadicDataHashMap;

	UPROPERTY()
	TMap<ENomadicSubtype, FNomadicData> M_TEnemyNomadicDataHashMap;

	// NEW: Pair helper for easy section assignment when initializing BXP options.
static FORCEINLINE TPair<EBuildingExpansionType, EBxpOptionSection> AsTech(const EBuildingExpansionType Type)
{
	return TPair<EBuildingExpansionType, EBxpOptionSection>(Type, EBxpOptionSection::BOS_Tech);
}

static FORCEINLINE TPair<EBuildingExpansionType, EBxpOptionSection> AsEconomic(const EBuildingExpansionType Type)
{
	return TPair<EBuildingExpansionType, EBxpOptionSection>(Type, EBxpOptionSection::BOS_Economic);
}

static FORCEINLINE TPair<EBuildingExpansionType, EBxpOptionSection> AsDefense(const EBuildingExpansionType Type)
{
	return TPair<EBuildingExpansionType, EBxpOptionSection>(Type, EBxpOptionSection::BOS_Defense);
}

/**
 * @brief Builds FBxpOptionData array with explicit section per option.
 * @param TypedOptions Array of (Type, Section) pairs (see AsTech/AsEconomic/AsDefense helpers).
 */
TArray<FBxpOptionData> InitBxpOptions(
	const TArray<TPair<EBuildingExpansionType, EBxpOptionSection>>& TypedOptions) const;
FBxpOptionData InitBxpOptionEntry(const EBuildingExpansionType Type,
                                  const EBxpOptionSection Section) const;


	UPROPERTY()
	TMap<EDigInType, FDigInData> M_TPlayerDigInDataHashMap;
	
	UPROPERTY()
	TMap<EDigInType, FDigInData> M_TEnemyDigInDataHashMap;

	UPROPERTY()
	TMap<ERocketAbility, FAttachedRocketsData> M_TPlayerAttachedRocketDataHashMap;

	UPROPERTY()
	TMap<ERocketAbility, FAttachedRocketsData> M_EnemyAttachedRocketDataHashMap;
	
	void InitAllGameDigInData();

	void InitAllGameRocketData();
	// Fills the TankDataHashMap of M_TPlayerTankDataHashMap with the data for each tank subtype.
	void InitAllGameTankData();
	void InitAllGameArmoredCarData();
	void InitAllGameLightTankData();
	void InitAllGameMediumTankData();
	void InitAllGameHeavyTankData();
	void InitAllGameAircraftData();

	// ---- Experience Data ----
	TArray<FExperienceLevel> GetLightTankExpLevels() const;
	TArray<FExperienceLevel> GetLightTankDestroyerExpLevels() const;
	TArray<FExperienceLevel> GetArmoredCarExpLevels() const;
	TArray<FExperienceLevel> GetLightMediumTankExpLevels() const;
	TArray<FExperienceLevel> GetMediumTankExpLevels() const;
	TArray<FExperienceLevel> GetMediumTankDestroyerExpLevels() const;
	TArray<FExperienceLevel> GetHeavyTankExpLevels() const;
	TArray<FExperienceLevel> GetHeavyTankDestroyerExpLevels() const;
	TArray<FExperienceLevel> GetNomadicTruckExpLevels() const;

	TArray<FExperienceLevel> GetTier1InfantryExpLevels() const;
	// todo update when exp is properly implemented.
	TArray<FExperienceLevel> GetTier2InfantryExpLevels() const;
	// todo update when exp is properly implemented.
	TArray<FExperienceLevel> GetEliteInfantryExpLevels() const;

	TArray<FExperienceLevel> GetFighterExpLevels() const;
	// ---- End Experience Data ----
	
	UPROPERTY()
	TMap<EBuildingExpansionType, FBxpData> M_TPlayerBxpDataHashMap;

	UPROPERTY()
	TMap<EBuildingExpansionType, FBxpData> M_TEnemyBxpDataHashMap;

	void InitAllGameBxpData();

	// FIlls the NomadicDataHashMap of M_TPlayerNomadicDataHashMap with the data for each nomadic subtype.
	void InitAllGameNomadicData();

	UPROPERTY()
	TMap<ESquadSubtype, FSquadData> M_TPlayerSquadDataHashMap;

	UPROPERTY()
	TMap<ESquadSubtype, FSquadData> M_TEnemySquadDataHashMap;

	void InitAllGameSquadData();

	// Handels all game updates and settings.
	UPROPERTY()
	URTSGameSettingsHandler* M_RTSGameSettings;

	void NotifyPlayerGameSettingsLoaded();

	UPROPERTY()
	FProjectileManagerCallBacks M_ProjectileManagerCallbacks;
	
	void InitializeSmallArmsProjectileManager();
	void InitializeWeaponVFXSettings();

	 TWeakObjectPtr<UMaterialParameterCollectionInstance> M_MPC_Time_Instance;

};

template <typename SettingType>
SettingType ACPPGameState::GetGameSetting(const ERTSGameSetting Setting) const
{
	return M_RTSGameSettings->GetGameSetting<SettingType>(Setting);
}
