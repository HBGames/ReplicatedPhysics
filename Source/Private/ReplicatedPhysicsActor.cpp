// Copyright Hitbox Games, LLC. All Rights Reserved.

#include "ReplicatedPhysicsActor.h"

#include "GameFramework/PlayerState.h"
#include "PhysicsBucketUpdateSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Physics/Experimental/PhysScene_Chaos.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ReplicatedPhysicsActor)

AReplicatedPhysicsActor::AReplicatedPhysicsActor()
{
	if (RootComponent)
	{
		RootComponent->SetMobility(EComponentMobility::Movable);
	}

	// Default replication on for multiplayer
	bReplicates = true;
	SetReplicatingMovement(true);

	// Don't use the replicated list, use our custom replication instead
	bReplicateUsingRegisteredSubObjectList = false;

	bAllowIgnoringAttachOnOwner = true;

	// Minimum of 30 Hz for replication consideration
	// Otherwise this object will have MASSIVE slow downs if allowed to replicate at the 2 per second default
	MinNetUpdateFrequency = 30.f;
}

void AReplicatedPhysicsActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams PushModelParams{COND_None, REPNOTIFY_OnChanged, /*bIsPushBased=*/true};

	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, bAllowIgnoringAttachOnOwner, PushModelParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, ClientAuthReplicationData, PushModelParams);

	FDoRepLifetimeParams AttachmentReplicationParams{COND_Custom, REPNOTIFY_Always, /*bIsPushBased=*/true};
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, AttachmentWeldReplication, AttachmentReplicationParams);
}

void AReplicatedPhysicsActor::GatherCurrentMovement()
{
	if (IsReplicatingMovement() || (RootComponent && RootComponent->GetAttachParent()))
	{
		bool bWasAttachmentModified = false;
		bool bWasRepMovementModified = false;

		AActor* OldAttachParent = AttachmentWeldReplication.AttachParent;
		USceneComponent* OldAttachComponent = AttachmentWeldReplication.AttachComponent;

		AttachmentWeldReplication.AttachParent = nullptr;
		AttachmentWeldReplication.AttachComponent = nullptr;

		FRepMovement& RepMovement = GetReplicatedMovement_Mutable();

		UPrimitiveComponent* RootPrimComp = Cast<UPrimitiveComponent>(GetRootComponent());
		if (RootPrimComp && RootPrimComp->IsSimulatingPhysics())
		{
#if UE_WITH_IRIS
			const bool bPrevRepPhysics = GetReplicatedMovement_Mutable().bRepPhysics;
#endif // UE_WITH_IRIS

			bool bFoundInCache = false;

			UWorld* World = GetWorld();
			int ServerFrame = 0;
			if (FPhysScene_Chaos* Scene = static_cast<FPhysScene_Chaos*>(World->GetPhysicsScene()))
			{
				if (const FRigidBodyState* FoundState = Scene->GetStateFromReplicationCache(RootPrimComp, ServerFrame))
				{
					RepMovement.FillFrom(*FoundState, this, Scene->ReplicationCache.ServerFrame);
					bFoundInCache = true;
				}
			}

			if (!bFoundInCache)
			{
				// fallback to GT data
				FRigidBodyState RBState;
				RootPrimComp->GetRigidBodyState(RBState);
				RepMovement.FillFrom(RBState, this, 0);
			}

			// Don't replicate movement if we're welded to another parent actor.
			// Their replication will affect our position indirectly since we are attached.
			RepMovement.bRepPhysics = !RootPrimComp->IsWelded();

			if (!RepMovement.bRepPhysics)
			{
				if (RootComponent->GetAttachParent() != nullptr)
				{
					// Networking for attachments assumes the RootComponent of the AttachParent actor. 
					// If that's not the case, we can't update this, as the client wouldn't be able to resolve the Component and would detach as a result.
					AttachmentWeldReplication.AttachParent = RootComponent->GetAttachParent()->GetAttachmentRootActor();
					if (AttachmentWeldReplication.AttachParent != nullptr)
					{
						AttachmentWeldReplication.LocationOffset = RootComponent->GetRelativeLocation();
						AttachmentWeldReplication.RotationOffset = RootComponent->GetRelativeRotation();
						AttachmentWeldReplication.RelativeScale3D = RootComponent->GetRelativeScale3D();
						AttachmentWeldReplication.AttachComponent = RootComponent->GetAttachParent();
						AttachmentWeldReplication.AttachSocket = RootComponent->GetAttachSocketName();
						AttachmentWeldReplication.bIsWelded = RootPrimComp ? RootPrimComp->IsWelded() : false;

						// Technically, the values might have stayed the same, but we'll just assume they've changed.
						bWasAttachmentModified = true;
					}
				}
			}

			// Technically, the values might have stayed the same, but we'll just assume they've changed.
			bWasRepMovementModified = true;

#if UE_WITH_IRIS
			// If RepPhysics has changed value then notify the ReplicationSystem
			if (bPrevRepPhysics != GetReplicatedMovement_Mutable().bRepPhysics)
			{
				UpdateReplicatePhysicsCondition();
			}
#endif // UE_WITH_IRIS
		}
		else if (RootComponent != nullptr)
		{
			// If we are attached, don't replicate absolute position, use AttachmentReplication instead.
			if (RootComponent->GetAttachParent() != nullptr)
			{
				// Networking for attachments assumes the RootComponent of the AttachParent actor. 
				// If that's not the case, we can't update this, as the client wouldn't be able to resolve the Component and would detach as a result.
				AttachmentWeldReplication.AttachParent = RootComponent->GetAttachParentActor();
				if (AttachmentWeldReplication.AttachParent != nullptr)
				{
					AttachmentWeldReplication.LocationOffset = RootComponent->GetRelativeLocation();
					AttachmentWeldReplication.RotationOffset = RootComponent->GetRelativeRotation();
					AttachmentWeldReplication.RelativeScale3D = RootComponent->GetRelativeScale3D();
					AttachmentWeldReplication.AttachComponent = RootComponent->GetAttachParent();
					AttachmentWeldReplication.AttachSocket = RootComponent->GetAttachSocketName();
					AttachmentWeldReplication.bIsWelded = RootPrimComp ? RootPrimComp->IsWelded() : false;

					// Technically, the values might have stayed the same, but we'll just assume they've changed.
					bWasAttachmentModified = true;
				}
			}
			else
			{
				RepMovement.Location = FRepMovement::RebaseOntoZeroOrigin(RootComponent->GetComponentLocation(), this);
				RepMovement.Rotation = RootComponent->GetComponentRotation();
				RepMovement.LinearVelocity = GetVelocity();
				RepMovement.AngularVelocity = FVector::ZeroVector;

				// Technically, the values might have stayed the same, but we'll just assume they've changed.
				bWasRepMovementModified = true;
			}

			bWasRepMovementModified = (bWasRepMovementModified || RepMovement.bRepPhysics);
			RepMovement.bRepPhysics = false;
		}
#if WITH_PUSH_MODEL
		if (bWasRepMovementModified)
		{
			MARK_PROPERTY_DIRTY_FROM_NAME(AActor, ReplicatedMovement, this);
		}

		if (bWasAttachmentModified ||
			OldAttachParent != AttachmentWeldReplication.AttachParent ||
			OldAttachComponent != AttachmentWeldReplication.AttachComponent)
		{
			MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, AttachmentWeldReplication, this);
		}
#endif
	}
}

void AReplicatedPhysicsActor::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
#if WITH_PUSH_MODEL
	const AActor* const OldAttachParent = AttachmentWeldReplication.AttachParent;
	const UActorComponent* const OldAttachComponent = AttachmentWeldReplication.AttachComponent;
#endif

	// Attachment replication gets filled in by GatherCurrentMovement(), but in the case of a detached root we need to trigger remote detachment.
	AttachmentWeldReplication.AttachParent = nullptr;
	AttachmentWeldReplication.AttachComponent = nullptr;

	GatherCurrentMovement();

	DOREPLIFETIME_ACTIVE_OVERRIDE_FAST(AActor, ReplicatedMovement, IsReplicatingMovement());

	// Don't need to replicate AttachmentReplication if the root component replicates, because it already handles it.
	DOREPLIFETIME_ACTIVE_OVERRIDE_FAST(ThisClass, AttachmentWeldReplication, RootComponent && !RootComponent->GetIsReplicated());

	// Don't need to replicate AttachmentReplication if the root component replicates, because it already handles it.
	DOREPLIFETIME_ACTIVE_OVERRIDE_FAST(AActor, AttachmentReplication, false); // RootComponent && !RootComponent->GetIsReplicated());


#if WITH_PUSH_MODEL
	if (UNLIKELY(OldAttachParent != AttachmentWeldReplication.AttachParent || OldAttachComponent != AttachmentWeldReplication.AttachComponent))
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, AttachmentWeldReplication, this);
	}
#endif
}

void AReplicatedPhysicsActor::PostNetReceivePhysicState()
{
	if (bAllowIgnoringAttachOnOwner && (ClientAuthReplicationData.bIsCurrentlyClientAuth || ShouldSkipAttachmentReplication()))
	{
		return;
	}

	Super::PostNetReceivePhysicState();
}

void AReplicatedPhysicsActor::OnRep_ReplicatedMovement()
{
	if (bAllowIgnoringAttachOnOwner && (ClientAuthReplicationData.bIsCurrentlyClientAuth || ShouldSkipAttachmentReplication()))
	{
		return;
	}

	Super::OnRep_ReplicatedMovement();
}

void AReplicatedPhysicsActor::OnRep_ReplicateMovement()
{
	if (bAllowIgnoringAttachOnOwner && (ClientAuthReplicationData.bIsCurrentlyClientAuth || ShouldSkipAttachmentReplication()))
	{
		return;
	}

	if (RootComponent)
	{
		const FRepAttachment ReplicationAttachment = GetAttachmentReplication();
		if (!ReplicationAttachment.AttachParent)
		{
			const FRepMovement& RepMove = GetReplicatedMovement();

			// This "fix" corrects the simulation state not replicating over correctly
			// If you turn off movement replication, simulate an object, turn movement replication back on and un-simulate, it never knows the difference
			// This change ensures that it is checking against the current state
			if (RootComponent->IsSimulatingPhysics() != RepMove.bRepPhysics) //SavedbRepPhysics != ReplicatedMovement.bRepPhysics)
			{
				// Turn on/off physics sim to match server.
				SyncReplicatedPhysicsSimulation();

				// It doesn't really hurt to run it here, the super can call it again but it will fail out as they already match
			}
		}
	}

	Super::OnRep_ReplicateMovement();
}

void AReplicatedPhysicsActor::OnRep_AttachmentReplication()
{
	if (bAllowIgnoringAttachOnOwner && (ClientAuthReplicationData.bIsCurrentlyClientAuth || ShouldSkipAttachmentReplication()))
	{
		return;
	}

	if (AttachmentWeldReplication.AttachParent)
	{
		if (RootComponent)
		{
			if (const auto AttachParentComponent = AttachmentWeldReplication.AttachComponent ? ToRawPtr(AttachmentWeldReplication.AttachComponent) : AttachmentWeldReplication.AttachParent->GetRootComponent())
			{
				RootComponent->SetRelativeLocation_Direct(AttachmentWeldReplication.LocationOffset);
				RootComponent->SetRelativeRotation_Direct(AttachmentWeldReplication.RotationOffset);
				RootComponent->SetRelativeScale3D_Direct(AttachmentWeldReplication.RelativeScale3D);

				// If we're already attached to the correct Parent and Socket, then the update must be position only.
				// AttachToComponent would early out in this case.
				// Note, we ignore the special case for simulated bodies in AttachToComponent as AttachmentReplication shouldn't get updated
				// if the body is simulated (see AActor::GatherMovement).
				if (const bool bAlreadyAttached = AttachParentComponent == RootComponent->GetAttachParent() && AttachmentWeldReplication.AttachSocket == RootComponent->GetAttachSocketName() && AttachParentComponent->GetAttachChildren().Contains(RootComponent))
				{
					// Note, this doesn't match AttachToComponent, but we're assuming it's safe to skip physics (see comment above).
					RootComponent->UpdateComponentToWorld(EUpdateTransformFlags::SkipPhysicsUpdate, ETeleportType::None);
				}
				else
				{
					FAttachmentTransformRules attachRules = FAttachmentTransformRules::KeepRelativeTransform;
					attachRules.bWeldSimulatedBodies = AttachmentWeldReplication.bIsWelded;
					RootComponent->AttachToComponent(AttachParentComponent, attachRules, AttachmentWeldReplication.AttachSocket);
				}
			}
		}
	}
	else
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

		// Handle the case where an object was both detached and moved on the server in the same frame.
		// Calling this extraneously does not hurt but will properly fire events if the movement state changed while attached.
		// This is needed because client side movement is ignored when attached
		if (IsReplicatingMovement())
		{
			OnRep_ReplicatedMovement();
		}
	}
}

void AReplicatedPhysicsActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveFromClientReplicationBucket();

	Super::EndPlay(EndPlayReason);
}

bool AReplicatedPhysicsActor::AddToClientReplicationBucket()
{
	// The subsystem automatically removes entries with the same function signature, so it's safe to just always add here
	GetWorld()->GetSubsystem<UPhysicsBucketUpdateSubsystem>()->AddObjectToBucket(ClientAuthReplicationData.UpdateRate, this, FName("PollReplicationEvent"));
	ClientAuthReplicationData.bIsCurrentlyClientAuth = true;

	if (const auto World = GetWorld())
	{
		ClientAuthReplicationData.TimeAtInitialThrow = World->GetTimeSeconds();
	}

	return true;
}

bool AReplicatedPhysicsActor::RemoveFromClientReplicationBucket()
{
	if (ClientAuthReplicationData.bIsCurrentlyClientAuth)
	{
		GetWorld()->GetSubsystem<UPhysicsBucketUpdateSubsystem>()->RemoveObjectFromBucketByFunctionName(this, FName(TEXT("PollReplicationEvent")));
		CeaseReplicationBlocking();
		return true;
	}

	return false;
}

bool AReplicatedPhysicsActor::PollReplicationEvent()
{
	if (!ClientAuthReplicationData.bIsCurrentlyClientAuth || !HasLocalNetOwner())
		return false; // Tell the bucket subsystem to remove us from consideration

	UWorld* World = GetWorld();
	if (!World) return false; // Tell the bucket subsystem to remove us from consideration

	bool bRemoveBlocking = false;

	if ((World->GetTimeSeconds() - ClientAuthReplicationData.TimeAtInitialThrow) > 10.0f)
	{
		// Time out the sending. It's been 10 seconds since we threw the object, so it's likely conflicting with some other
		// server Authed movement, forcing it to keep momentum.
		bRemoveBlocking = true;
	}

	// Store the current transform for the resting check
	FTransform CurrentTransform = GetActorTransform();

	if (!bRemoveBlocking)
	{
		if (!CurrentTransform.GetRotation().Equals(ClientAuthReplicationData.LastActorTransform.GetRotation())
			|| !CurrentTransform.GetLocation().Equals(ClientAuthReplicationData.LastActorTransform.GetLocation()))
		{
			ClientAuthReplicationData.LastActorTransform = CurrentTransform;

			if (const auto PrimitiveComponent = Cast<UPrimitiveComponent>(GetRootComponent()))
			{
				// Need to clamp to a max time since start to handle cases with conflicting collisions
				// This is a failsafe to prevent the object from getting stuck in the world.
				if (PrimitiveComponent->IsSimulatingPhysics() && ShouldSkipAttachmentReplication())
				{
					FRepMovementPhysics ClientAuthMovementRep;
					if (ClientAuthMovementRep.GatherActorsMovement(this))
					{
						Server_GetClientAuthReplication(ClientAuthMovementRep);

						if (PrimitiveComponent->RigidBodyIsAwake())
						{
							return true;
						}
					}
				}
			}
			else
			{
				bRemoveBlocking = true;
			}
		}
	}

	bool bTimedBlockingRelease = false;
	AActor* TopOwner = GetOwner();
	if (TopOwner != nullptr)
	{
		AActor* TempOwner = TopOwner->GetOwner();

		while (TempOwner)
		{
			TopOwner = TempOwner;
			TempOwner = TopOwner->GetOwner();
		}

		if (const auto PlayerController = Cast<APlayerController>(TopOwner))
		{
			if (const auto PlayerState = PlayerController->PlayerState)
			{
				if (ClientAuthReplicationData.ResetReplicationHandle.IsValid())
				{
					World->GetTimerManager().ClearTimer(ClientAuthReplicationData.ResetReplicationHandle);
				}

				// Clamp the ping to a min/max value
				float ClampedPing = FMath::Clamp(PlayerState->ExactPing, 0.f, 1000.f);
				World->GetTimerManager().SetTimer(ClientAuthReplicationData.ResetReplicationHandle, this, &ThisClass::CeaseReplicationBlocking, ClampedPing, false);
				bTimedBlockingRelease = true;
			}
		}
	}

	if (!bTimedBlockingRelease)
	{
		CeaseReplicationBlocking();
	}

	// Tell server to kill us
	Server_EndClientAuthReplication();

	return false; // Tell the bucket subsystem to remove us from consideration
}

void AReplicatedPhysicsActor::CeaseReplicationBlocking()
{
	if (ClientAuthReplicationData.bIsCurrentlyClientAuth)
	{
		ClientAuthReplicationData.bIsCurrentlyClientAuth = false;
	}

	ClientAuthReplicationData.LastActorTransform = FTransform::Identity;

	if (ClientAuthReplicationData.ResetReplicationHandle.IsValid())
	{
		if (const auto World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ClientAuthReplicationData.ResetReplicationHandle);
		}
	}
}

FPhysicsClientAuthReplicationData AReplicatedPhysicsActor::GetClientAuthReplicationData(FPhysicsClientAuthReplicationData& ClientAuthData)
{
#if WITH_PUSH_MODEL
	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, ClientAuthReplicationData, this);
#endif
	return ClientAuthReplicationData;
}

void AReplicatedPhysicsActor::Server_GetClientAuthReplication_Implementation(const FRepMovementPhysics& NewMovement)
{
	if (!NewMovement.Location.ContainsNaN() && !NewMovement.Rotation.ContainsNaN())
	{
		FRepMovement& MovementRep = GetReplicatedMovement_Mutable();
		NewMovement.CopyTo(MovementRep);
		OnRep_ReplicatedMovement();
	}
}

bool AReplicatedPhysicsActor::Server_GetClientAuthReplication_Validate(const FRepMovementPhysics& NewMovement)
{
	return true;
}

void AReplicatedPhysicsActor::Server_EndClientAuthReplication_Implementation()
{
	if (const auto World = GetWorld())
	{
		if (const auto PrimitiveComponent = Cast<UPrimitiveComponent>(GetRootComponent()))
		{
			if (const auto PhysicsScene = World->GetPhysicsScene())
			{
				if (const auto PhysicsReplication = PhysicsScene->GetPhysicsReplication())
				{
					PhysicsReplication->RemoveReplicatedTarget(PrimitiveComponent);
				}
			}
		}
	}
}

bool AReplicatedPhysicsActor::Server_EndClientAuthReplication_Validate()
{
	return true;
}
