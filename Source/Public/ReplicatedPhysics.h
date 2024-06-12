// Copyright Hitbox Games, LLC. All Rights Reserved.

#pragma once

#include "ReplicatedPhysics.generated.h"

USTRUCT()
struct REPLICATEDPHYSICS_API FRepMovementPhysics : public FRepMovement
{
	GENERATED_BODY()

public:
	FRepMovementPhysics();
	FRepMovementPhysics(FRepMovement& Other);
	void CopyTo(FRepMovement& Other) const;
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
	bool GatherActorsMovement(AActor* OwningActor);
};

template <>
struct TStructOpsTypeTraits<FRepMovementPhysics> : public TStructOpsTypeTraitsBase2<FRepMovementPhysics>
{
	enum
	{
		WithNetSerializer = true,
		WithNetSharedSerialization = true,
	};
};

USTRUCT(BlueprintType)
struct REPLICATEDPHYSICS_API FPhysicsClientAuthReplicationData
{
	GENERATED_BODY()

public:
	// The rate that we will be sending events to the server, not replicated, only serialized
	UPROPERTY(EditAnywhere, NotReplicated, BlueprintReadOnly, Category="Networking", meta=(ClampMin="0", ClampMax="100"))
	int32 UpdateRate = 30;

	FTimerHandle ResetReplicationHandle;
	FTransform LastActorTransform = FTransform::Identity;
	float TimeAtInitialThrow = 0.f;
	bool bIsCurrentlyClientAuth = false;
};
