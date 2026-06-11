// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "TE_AutoLoader.h"

UTE_AutoLoader::UTE_AutoLoader()
{
	Technology = ETechnology::Tech_AutoLoader;
	TanksToApplyTo = {
		ETankSubtype::Tank_PzIII_J,
		ETankSubtype::Tank_PzIII_J_Commander,
		ETankSubtype::Tank_PzIII_M,
		ETankSubtype::Tank_Puma,
	};
}
