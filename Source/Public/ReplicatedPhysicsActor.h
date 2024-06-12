// Copyright Hitbox Games, LLC. All Rights Reserved.

#pragma once

#include "ReplicatedPhysics.h"
#include "RepPhysicsAttachmentWithWeld.h"

#include "ReplicatedPhysicsActor.generated.h"

UCLASS()
class REPLICATEDPHYSICS_API AReplicatedPhysicsActor : public AActor
{
	GENERATED_BODY()

public:
	AReplicatedPhysicsActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//~Begin AActor
	virtual void GatherCurrentMovement() override;
	virtual void PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker) override;
	virtual void OnRep_AttachmentReplication() override;
	virtual void OnRep_ReplicateMovement() override;
	virtual void OnRep_ReplicatedMovement() override;
	virtual void PostNetReceivePhysicState() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End AActor

public:
	UFUNCTION(BlueprintCallable, Category="Networking")
	bool AddToClientReplicationBucket();

	UFUNCTION(BlueprintCallable, Category="Networking")
	bool RemoveFromClientReplicationBucket();

	UFUNCTION()
	bool PollReplicationEvent();

	UFUNCTION(Category="Networking")
	void CeaseReplicationBlocking();

	// Notify the server that we are no longer trying to run the throwing auth
	UFUNCTION(Reliable, Server, WithValidation, Category="Networking")
	void Server_EndClientAuthReplication();

	UFUNCTION(Unreliable, Server, WithValidation, Category="Networking")
	void Server_GetClientAuthReplication(const FRepMovementPhysics& NewMovement);

	bool ShouldSkipAttachmentReplication() const
	{
		return false;
	}

	// Getter to make sure ClientAuthReplicationData is dirtied
	FPhysicsClientAuthReplicationData GetClientAuthReplicationData(FPhysicsClientAuthReplicationData& ClientAuthData);

public:
	UPROPERTY(Replicated, ReplicatedUsing=OnRep_AttachmentReplication)
	FRepPhysicsAttachmentWithWeld AttachmentWeldReplication;

protected:
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category="Replication")
	FPhysicsClientAuthReplicationData ClientAuthReplicationData;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category="Replication")
	bool bAllowIgnoringAttachOnOwner;
};
