// Copyright Hitbox Games, LLC. All Rights Reserved.

#include "RepPhysicsAttachmentWithWeld.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RepPhysicsAttachmentWithWeld)

bool FRepPhysicsAttachmentWithWeld::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	// Our additional weld bit is here
	Ar.SerializeBits(&bIsWelded, 1);
	Ar << AttachParent;
	LocationOffset.NetSerialize(Ar, Map, bOutSuccess);
	RelativeScale3D.NetSerialize(Ar, Map, bOutSuccess);
	RotationOffset.SerializeCompressedShort(Ar);
	Ar << AttachSocket;
	Ar << AttachComponent;
	return true;
}

FRepPhysicsAttachmentWithWeld::FRepPhysicsAttachmentWithWeld()
{
	bIsWelded = false;
}