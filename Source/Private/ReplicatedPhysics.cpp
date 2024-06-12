// Copyright Hitbox Games, LLC. All Rights Reserved.

#include "ReplicatedPhysics.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ReplicatedPhysics)

FRepMovementPhysics::FRepMovementPhysics()
{
	LocationQuantizationLevel = EVectorQuantization::RoundTwoDecimals;
	VelocityQuantizationLevel = EVectorQuantization::RoundTwoDecimals;
	RotationQuantizationLevel = ERotatorQuantization::ShortComponents;
}

FRepMovementPhysics::FRepMovementPhysics(FRepMovement& Other)
{
	FRepMovementPhysics();

	LinearVelocity = Other.LinearVelocity;
	AngularVelocity = Other.AngularVelocity;
	Location = Other.Location;
	Rotation = Other.Rotation;
	bSimulatedPhysicSleep = Other.bSimulatedPhysicSleep;
	bRepPhysics = Other.bRepPhysics;
}

void FRepMovementPhysics::CopyTo(FRepMovement& Other) const
{
	Other.LinearVelocity = LinearVelocity;
	Other.AngularVelocity = AngularVelocity;
	Other.Location = Location;
	Other.Rotation = Rotation;
	Other.bSimulatedPhysicSleep = bSimulatedPhysicSleep;
	Other.bRepPhysics = bRepPhysics;
}

bool FRepMovementPhysics::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	return FRepMovement::NetSerialize(Ar, Map, bOutSuccess);
}

bool FRepMovementPhysics::GatherActorsMovement(AActor* OwningActor)
{
	UPrimitiveComponent* RootPrimComp = Cast<UPrimitiveComponent>(OwningActor->GetRootComponent());
	if (RootPrimComp && RootPrimComp->IsSimulatingPhysics())
	{
		FRigidBodyState RBState;
		RootPrimComp->GetRigidBodyState(RBState);

		FillFrom(RBState, OwningActor);
		// Don't replicate movement if we're welded to another parent actor.
		// Their replication will affect our position indirectly since we are attached.
		bRepPhysics = !RootPrimComp->IsWelded();
	}
	else if (RootPrimComp != nullptr)
	{
		// If we are attached, don't replicate absolute position, use AttachmentReplication instead.
		if (RootPrimComp->GetAttachParent() != nullptr)
		{
			return false; // We don't handle attachment rep
		}
		else
		{
			Location = FRepMovement::RebaseOntoZeroOrigin(RootPrimComp->GetComponentLocation(), OwningActor);
			Rotation = RootPrimComp->GetComponentRotation();
			LinearVelocity = OwningActor->GetVelocity();
			AngularVelocity = FVector::ZeroVector;
		}

		bRepPhysics = false;
	}

	return true;
}
