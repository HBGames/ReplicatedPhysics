// Copyright Hitbox Games, LLC. All Rights Reserved.

#pragma once

#include "Engine/ReplicatedState.h"

#include "RepPhysicsAttachmentWithWeld.generated.h"

USTRUCT()
struct REPLICATEDPHYSICS_API FRepPhysicsAttachmentWithWeld : public FRepAttachment
{
	GENERATED_BODY()

	UPROPERTY()
	bool bIsWelded;

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	FRepPhysicsAttachmentWithWeld();
};

template<>
struct TStructOpsTypeTraits<FRepPhysicsAttachmentWithWeld> : public TStructOpsTypeTraitsBase2<FRepPhysicsAttachmentWithWeld>
{
	enum
	{
		WithNetSerializer = true /*,
		WithNetSharedSerialization = true,*/
	};
};
