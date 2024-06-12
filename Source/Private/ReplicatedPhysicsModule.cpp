// Copyright Hitbox Games, LLC. All Rights Reserved.

#include "IReplicatedPhysicsModule.h"

#define LOCTEXT_NAMESPACE "ReplicatedPhysics"

class FReplicatedPhysicsModule : public IReplicatedPhysicsModule
{
};

IMPLEMENT_MODULE(FReplicatedPhysicsModule, ReplicatedPhysics)

#undef LOCTEXT_NAMESPACE