// Copyright (C) Bas Blokzijl - All rights reserved.

#include "AnimatedDoodadOptimizer.h"

void UAnimatedDoodadOptimizer::OutFovCloseUpdateComponents()
{
	Super::OutFovCloseUpdateComponents();
	StopSkeletalMeshAnimations();
}

void UAnimatedDoodadOptimizer::OutFovFarUpdateComponents()
{
	Super::OutFovFarUpdateComponents();
	StopSkeletalMeshAnimations();
}

bool UAnimatedDoodadOptimizer::ShouldAlwaysConsiderOwnerInFOVAtCloseRange() const
{
	return false;
}
