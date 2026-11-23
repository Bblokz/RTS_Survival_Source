#pragma once

#include "CoreMinimal.h"

#include "RTSPortraitTypes.generated.h"

UENUM()
enum class ERTSPortraitTypes :uint8
{
	None,
	GermanAnnouncer,
	SovietHazmat,
	SovietCortexCommander,
	SovietCyborgEmissive,
	GermanScavengers,
	GermanResistanceInfantry,
	GermanSniper,
	GermanVultures,
	GermanMechanizedPioneers,
	GermanCommando,
	// Soviet Mosin rifle squad: rifleman with a scarf over his mouth (head-nod + blink portrait material).
	SovietMosin,
	// Tiger I loader in red battle light holding an 8.8 cm round (head-nod + engine-tremor portrait material).
	GermanTigerLoader,
	// Scavenger in the open hatch of a speeding Panzer II Luchs (ride jolt + background flow + head-nod material).
	GermanLuchsScavenger,
	// IS-2 driver at his station, band of daylight across his eyes (panel-glow + dial + station-tremor material;
	// his head never moves).
	SovietIS2Driver,
	// Intercepted Soviet radio, no face: a receiver card with a live signal trace. Speaker "Starik", the recurring
	// 1945 veteran first heard on Oder-Warthe; Shield operator intercepts reuse it with a transcript speaker override.
	SovietInterceptStarik,
};
