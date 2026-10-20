// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "TE_StereoRangefinder.h"

UTE_StereoRangefinder::UTE_StereoRangefinder()
{
	Technology = ETechnology::Tech_StereoRangefinder;
	TanksToApplyTo = {
		ETankSubtype::Tank_Stug,
		ETankSubtype::Tank_Hetzer,
		ETankSubtype::Tank_PzIV_70,
		ETankSubtype::Tank_JagdPanther,
		ETankSubtype::Tank_JagdTiger,
	};
}
