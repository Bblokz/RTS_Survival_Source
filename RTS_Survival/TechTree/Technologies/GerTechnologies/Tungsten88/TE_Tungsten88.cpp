// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "TE_Tungsten88.h"

UTE_Tungsten88::UTE_Tungsten88()
{
	Technology = ETechnology::Tech_Tungsten88;
	TanksToApplyTo = {
		ETankSubtype::Tank_Tiger,
		ETankSubtype::Tank_TigerH1,
		ETankSubtype::Tank_KingTiger,
		ETankSubtype::Tank_JagdPanther,
		ETankSubtype::Tank_PantherII,
	};
}
