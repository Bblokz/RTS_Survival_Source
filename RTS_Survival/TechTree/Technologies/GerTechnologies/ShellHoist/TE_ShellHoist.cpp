// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "TE_ShellHoist.h"

UTE_ShellHoist::UTE_ShellHoist()
{
	Technology = ETechnology::Tech_Assault_ShellHoist;
	TanksToApplyTo = {
		ETankSubtype::Tank_Brumbar,
		ETankSubtype::Tank_SturmTiger,
		ETankSubtype::Tank_PzI_15cm,
	};
}
