// Copyright Hitbox Games, LLC. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"

#include "PhysicsBucketUpdateSubsystem.generated.h"

DECLARE_DELEGATE_RetVal(bool, FPhysicsBucketUpdateTickSignature);
DECLARE_DYNAMIC_DELEGATE(FDynamicPhysicsBucketUpdateTickSignature);

USTRUCT()
struct REPLICATEDPHYSICS_API FUpdatePhysicsBucketDrop
{
	GENERATED_BODY()

public:
	FPhysicsBucketUpdateTickSignature NativeCallback;
	FDynamicPhysicsBucketUpdateTickSignature DynamicCallback;

	FName FunctionName;

	bool ExecuteBoundCallback();
	bool IsBoundToObjectFunction(UObject* Obj, FName& FuncName);
	bool IsBoundToObjectDelegate(FDynamicPhysicsBucketUpdateTickSignature& DynEvent);
	bool IsBoundToObject(UObject* Obj);
	FUpdatePhysicsBucketDrop();
	FUpdatePhysicsBucketDrop(FDynamicPhysicsBucketUpdateTickSignature& DynCallback);
	FUpdatePhysicsBucketDrop(UObject* Obj, FName FuncName);
};


USTRUCT()
struct REPLICATEDPHYSICS_API FUpdatePhysicsBucket
{
	GENERATED_BODY()

public:
	float nUpdateRate;
	float nUpdateCount;

	TArray<FUpdatePhysicsBucketDrop> Callbacks;

	bool Update(float DeltaTime);
	
	FUpdatePhysicsBucket() {}

	FUpdatePhysicsBucket(uint32 UpdateHTZ) :
		nUpdateRate(1.0f / UpdateHTZ),
		nUpdateCount(0.0f)
	{
	}
};

USTRUCT()
struct REPLICATEDPHYSICS_API FUpdatePhysicsBucketContainer
{
	GENERATED_BODY()

public:
	bool bNeedsUpdate;
	TMap<uint32, FUpdatePhysicsBucket> ReplicationBuckets;

	void UpdateBuckets(float DeltaTime);

	bool AddBucketObject(uint32 UpdateHTZ, UObject* InObject, FName FunctionName);
	bool AddBucketObject(uint32 UpdateHTZ, FDynamicPhysicsBucketUpdateTickSignature& Delegate);

	/*
	template<typename classType>
	bool AddReplicatingObject(uint32 UpdateHTZ, classType* InObject, void(classType::* _Func)())
	{
	}
	*/

	bool RemoveBucketObject(UObject* ObjectToRemove, FName FunctionName);
	bool RemoveBucketObject(FDynamicPhysicsBucketUpdateTickSignature& DynEvent);
	bool RemoveObjectFromAllBuckets(UObject* ObjectToRemove);

	bool IsObjectInBucket(UObject* ObjectToRemove);
	bool IsObjectFunctionInBucket(UObject* ObjectToRemove, FName FunctionName);
	bool IsObjectDelegateInBucket(FDynamicPhysicsBucketUpdateTickSignature& DynEvent);

	FUpdatePhysicsBucketContainer()
	{
		bNeedsUpdate = false;
	};
};

UCLASS()
class REPLICATEDPHYSICS_API UPhysicsBucketUpdateSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
		// Not allowing for editor type as this is a replication subsystem
	}

	//UPROPERTY()
	FUpdatePhysicsBucketContainer BucketContainer;

	// Adds an object to an update bucket with the set HTZ, calls the passed in UFUNCTION name
	// If one of the bucket contains an entry with the function already then the existing one is removed and the new one is added
	bool AddObjectToBucket(int32 UpdateHTZ, UObject* InObject, FName FunctionName);

	// Adds an object to an update bucket with the set HTZ, calls the passed in UFUNCTION name
	// If one of the bucket contains an entry with the function already then the existing one is removed and the new one is added
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Add Object to Bucket Updates", ScriptName = "AddObjectToBucket"), Category = "BucketUpdateSubsystem")
	bool K2_AddObjectToBucket(int32 UpdateHTZ = 100, UObject* InObject = nullptr, FName FunctionName = NAME_None);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Add Object to Bucket Updates by Event", ScriptName = "AddBucketObjectEvent"), Category = "BucketUpdateSubsystem")
	bool K2_AddObjectEventToBucket(UPARAM(DisplayName = "Event") FDynamicPhysicsBucketUpdateTickSignature Delegate, int32 UpdateHTZ = 100);

	// Remove the entry in the bucket updates with the passed in UFUNCTION name
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Remove Object From Bucket Updates By Function", ScriptName = "RemoveObjectFromBucketByFunction"), Category = "BucketUpdateSubsystem")
	bool RemoveObjectFromBucketByFunctionName(UObject* InObject = nullptr, FName FunctionName = NAME_None);

	// Remove the entry in the bucket updates with the passed in UFUNCTION name
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Remove Object From Bucket Updates By Event", ScriptName = "RemoveObjectFromBucketByEvent"), Category = "BucketUpdateSubsystem")
	bool RemoveObjectFromBucketByEvent(UPARAM(DisplayName = "Event") FDynamicPhysicsBucketUpdateTickSignature Delegate);

	// Removes ALL entries in the bucket update system with the specified object
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Remove Object From All Bucket Updates", ScriptName = "RemoveObjectFromAllBuckets"), Category = "BucketUpdateSubsystem")
	bool RemoveObjectFromAllBuckets(UObject* InObject = nullptr);

	// Returns if an update bucket contains an entry with the passed in function
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Is Object In Bucket", ScriptName = "IsObjectInBucket"), Category = "BucketUpdateSubsystem")
	bool IsObjectFunctionInBucket(UObject* InObject = nullptr, FName FunctionName = NAME_None);

	// Returns if an update bucket contains an entry with the passed in function
	UFUNCTION(BlueprintPure, Category = "BucketUpdateSubsystem")
	bool IsActive();

	// FTickableGameObject functions
	/**
	 * Function called every frame on this GripScript. Override this function to implement custom logic to be executed every frame.
	 * Only executes if bCanEverTick is true and bAllowTicking is true
	 *
	 * @param DeltaTime - The time since the last tick.
	 */

	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;
	virtual bool IsTickableInEditor() const;
	virtual bool IsTickableWhenPaused() const override;
	virtual ETickableTickType GetTickableTickType() const;
	virtual TStatId GetStatId() const override;

	// End tickable object information
};
