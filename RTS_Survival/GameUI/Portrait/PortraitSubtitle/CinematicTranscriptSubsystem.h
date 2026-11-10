// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PortraitSubtitleTypes.h"

#include "CinematicTranscriptSubsystem.generated.h"

class UDataTable;
class USoundBase;

/**
 * @brief Finds the word-timed transcript of a voice line for the portrait subtitle and the cinematic overlay. Each
 * transcript table (rows: FCinematicTranscriptRow) is loaded once and indexed by voice line asset path.
 */
UCLASS()
class RTS_SURVIVAL_API UCinematicTranscriptSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UCinematicTranscriptSubsystem* Get(const UObject* WorldContextObject);

	/** @return The row for this voice line in Table, or nullptr when it has none. */
	const FCinematicTranscriptRow* FindTranscript(const TSoftObjectPtr<UDataTable>& Table,
	                                              const USoundBase* VoiceLine);

private:
	// Loaded transcript tables by asset path (kept alive here for the game instance's lifetime).
	UPROPERTY()
	TMap<FSoftObjectPath, TObjectPtr<UDataTable>> M_Tables;

	// Per table: voice line asset path -> row name.
	TMap<FSoftObjectPath, TMap<FSoftObjectPath, FName>> M_RowsByVoiceLine;

	UDataTable* LoadAndIndexTable(const TSoftObjectPtr<UDataTable>& Table);
};
