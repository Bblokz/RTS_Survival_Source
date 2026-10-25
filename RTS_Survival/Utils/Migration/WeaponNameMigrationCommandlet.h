#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WeaponNameMigrationCommandlet.generated.h"

/**
 * @brief Records, restores and verifies every EWeaponName value authored in Blueprints and data assets.
 * Run with UnrealEditor-Cmd -run=WeaponNameMigration -Mode=Snapshot|Apply|Verify -Tsv=<path>.
 * Temporary tooling for the uint8 -> uint32 migration (Source/Refactor/MigrationGuides/EWeaponName_Uint32_Migration.md).
 */
UCLASS()
class RTS_SURVIVAL_API UWeaponNameMigrationCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWeaponNameMigrationCommandlet();

	virtual int32 Main(const FString& Params) override;
};
