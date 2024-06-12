// Copyright Hitbox Games, LLC. All Rights Reserved.

#include "PhysicsBucketUpdateSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PhysicsBucketUpdateSubsystem)

bool UPhysicsBucketUpdateSubsystem::AddObjectToBucket(int32 UpdateHTZ, UObject* InObject, FName FunctionName)
{
	if (!InObject || UpdateHTZ < 1)
		return false;

	return BucketContainer.AddBucketObject(UpdateHTZ, InObject, FunctionName);
}

bool UPhysicsBucketUpdateSubsystem::K2_AddObjectToBucket(int32 UpdateHTZ, UObject* InObject, FName FunctionName)
{
	if (!InObject || UpdateHTZ < 1)
		return false;

	return BucketContainer.AddBucketObject(UpdateHTZ, InObject, FunctionName);
}


bool UPhysicsBucketUpdateSubsystem::K2_AddObjectEventToBucket(FDynamicPhysicsBucketUpdateTickSignature Delegate, int32 UpdateHTZ)
{
	if (!Delegate.IsBound())
		return false;

	return BucketContainer.AddBucketObject(UpdateHTZ, Delegate);
}

bool UPhysicsBucketUpdateSubsystem::RemoveObjectFromBucketByFunctionName(UObject* InObject, FName FunctionName)
{
	if (!InObject)
		return false;

	return BucketContainer.RemoveBucketObject(InObject, FunctionName);
}

bool UPhysicsBucketUpdateSubsystem::RemoveObjectFromBucketByEvent(FDynamicPhysicsBucketUpdateTickSignature Delegate)
{
	if (!Delegate.IsBound())
		return false;

	return BucketContainer.RemoveBucketObject(Delegate);
}

bool UPhysicsBucketUpdateSubsystem::RemoveObjectFromAllBuckets(UObject* InObject)
{
	if (!InObject)
		return false;

	return BucketContainer.RemoveObjectFromAllBuckets(InObject);
}

bool UPhysicsBucketUpdateSubsystem::IsObjectFunctionInBucket(UObject* InObject, FName FunctionName)
{
	if (!InObject)
		return false;

	return BucketContainer.IsObjectFunctionInBucket(InObject, FunctionName);
}

bool UPhysicsBucketUpdateSubsystem::IsActive()
{
	return BucketContainer.bNeedsUpdate;
}

void UPhysicsBucketUpdateSubsystem::Tick(float DeltaTime)
{
	BucketContainer.UpdateBuckets(DeltaTime);
}

bool UPhysicsBucketUpdateSubsystem::IsTickable() const
{
	return BucketContainer.bNeedsUpdate;
}

UWorld* UPhysicsBucketUpdateSubsystem::GetTickableGameObjectWorld() const
{
	return GetWorld();
}

bool UPhysicsBucketUpdateSubsystem::IsTickableInEditor() const
{
	return false;
}

bool UPhysicsBucketUpdateSubsystem::IsTickableWhenPaused() const
{
	return false;
}

ETickableTickType UPhysicsBucketUpdateSubsystem::GetTickableTickType() const
{
	if (IsTemplate(RF_ClassDefaultObject))
		return ETickableTickType::Never;

	return ETickableTickType::Conditional;
}

TStatId UPhysicsBucketUpdateSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVRGripScriptBase, STATGROUP_Tickables);
}

bool FUpdatePhysicsBucketDrop::ExecuteBoundCallback()
{
	if (NativeCallback.IsBound())
	{
		return NativeCallback.Execute();
	}
	else if (DynamicCallback.IsBound())
	{
		DynamicCallback.Execute();
		return true;
	}

	return false;
}

bool FUpdatePhysicsBucketDrop::IsBoundToObjectFunction(UObject* Obj, FName& FuncName)
{
	return (NativeCallback.IsBoundToObject(Obj) && FunctionName == FuncName);
}

bool FUpdatePhysicsBucketDrop::IsBoundToObjectDelegate(FDynamicPhysicsBucketUpdateTickSignature& DynEvent)
{
	return DynamicCallback == DynEvent;
}

bool FUpdatePhysicsBucketDrop::IsBoundToObject(UObject* Obj)
{
	return (NativeCallback.IsBoundToObject(Obj) || DynamicCallback.IsBoundToObject(Obj));
}

FUpdatePhysicsBucketDrop::FUpdatePhysicsBucketDrop()
{
	FunctionName = NAME_None;
}

FUpdatePhysicsBucketDrop::FUpdatePhysicsBucketDrop(FDynamicPhysicsBucketUpdateTickSignature& DynCallback)
{
	DynamicCallback = DynCallback;
}

FUpdatePhysicsBucketDrop::FUpdatePhysicsBucketDrop(UObject* Obj, FName FuncName)
{
	if (Obj && Obj->FindFunction(FuncName))
	{
		FunctionName = FuncName;
		NativeCallback.BindUFunction(Obj, FunctionName);
	}
	else
	{
		FunctionName = NAME_None;
	}
}

bool FUpdatePhysicsBucket::Update(float DeltaTime)
{
	//#TODO: Need to consider batching / spreading out load if there are a lot of updating objects in the bucket
	if (Callbacks.Num() < 1)
		return false;

	// Check for if this bucket is ready to fire events
	nUpdateCount += DeltaTime;
	if (nUpdateCount >= nUpdateRate)
	{
		nUpdateCount = 0.0f;
		for (int i = Callbacks.Num() - 1; i >= 0; --i)
		{
			if (Callbacks[i].ExecuteBoundCallback())
			{
				// If this returns true then we keep it in the queue
				continue;
			}

			// Remove the callback, it is complete or invalid
			Callbacks.RemoveAt(i);
		}
	}

	return Callbacks.Num() > 0;
}

void FUpdatePhysicsBucketContainer::UpdateBuckets(float DeltaTime)
{
	TArray<uint32> BucketsToRemove;
	for (auto& Bucket : ReplicationBuckets)
	{
		if (!Bucket.Value.Update(DeltaTime))
		{
			// Add Bucket to list to remove at end of update
			BucketsToRemove.Add(Bucket.Key);
		}
	}

	// Remove unused buckets so that they don't get ticked
	for (const uint32 Key : BucketsToRemove)
	{
		ReplicationBuckets.Remove(Key);
	}

	if (ReplicationBuckets.Num() < 1)
		bNeedsUpdate = false;
}

bool FUpdatePhysicsBucketContainer::AddBucketObject(uint32 UpdateHTZ, UObject* InObject, FName FunctionName)
{
	if (!InObject || InObject->FindFunction(FunctionName) == nullptr || UpdateHTZ < 1)
		return false;

	// First verify that this object isn't already contained in a bucket, if it is then erase it so that we can replace it below
	RemoveBucketObject(InObject, FunctionName);

	if (ReplicationBuckets.Contains(UpdateHTZ))
	{
		ReplicationBuckets[UpdateHTZ].Callbacks.Add(FUpdatePhysicsBucketDrop(InObject, FunctionName));
	}
	else
	{
		FUpdatePhysicsBucket& newBucket = ReplicationBuckets.Add(UpdateHTZ, FUpdatePhysicsBucket(UpdateHTZ));
		ReplicationBuckets[UpdateHTZ].Callbacks.Add(FUpdatePhysicsBucketDrop(InObject, FunctionName));
	}

	if (ReplicationBuckets.Num() > 0)
		bNeedsUpdate = true;

	return true;
}


bool FUpdatePhysicsBucketContainer::AddBucketObject(uint32 UpdateHTZ, FDynamicPhysicsBucketUpdateTickSignature& Delegate)
{
	if (!Delegate.IsBound() || UpdateHTZ < 1)
		return false;

	// First verify that this object isn't already contained in a bucket, if it is then erase it so that we can replace it below
	RemoveBucketObject(Delegate);

	if (ReplicationBuckets.Contains(UpdateHTZ))
	{
		ReplicationBuckets[UpdateHTZ].Callbacks.Add(FUpdatePhysicsBucketDrop(Delegate));
	}
	else
	{
		FUpdatePhysicsBucket& newBucket = ReplicationBuckets.Add(UpdateHTZ, FUpdatePhysicsBucket(UpdateHTZ));
		ReplicationBuckets[UpdateHTZ].Callbacks.Add(FUpdatePhysicsBucketDrop(Delegate));
	}

	if (ReplicationBuckets.Num() > 0)
		bNeedsUpdate = true;

	return true;
}

bool FUpdatePhysicsBucketContainer::RemoveBucketObject(UObject* ObjectToRemove, FName FunctionName)
{
	if (!ObjectToRemove || ObjectToRemove->FindFunction(FunctionName) == nullptr)
		return false;

	// Store if we ended up removing it
	bool bRemovedObject = false;

	TArray<uint32> BucketsToRemove;
	for (auto& Bucket : ReplicationBuckets)
	{
		for (int i = Bucket.Value.Callbacks.Num() - 1; i >= 0; --i)
		{
			if (Bucket.Value.Callbacks[i].IsBoundToObjectFunction(ObjectToRemove, FunctionName))
			{
				Bucket.Value.Callbacks.RemoveAt(i);
				bRemovedObject = true;

				// Leave the loop, this is called in add as well so we should never get duplicate entries
				break;
			}
		}

		if (bRemovedObject)
		{
			break;
		}
	}

	return bRemovedObject;
}

bool FUpdatePhysicsBucketContainer::RemoveBucketObject(FDynamicPhysicsBucketUpdateTickSignature& DynEvent)
{
	if (!DynEvent.IsBound())
		return false;

	// Store if we ended up removing it
	bool bRemovedObject = false;

	TArray<uint32> BucketsToRemove;
	for (auto& Bucket : ReplicationBuckets)
	{
		for (int i = Bucket.Value.Callbacks.Num() - 1; i >= 0; --i)
		{
			if (Bucket.Value.Callbacks[i].IsBoundToObjectDelegate(DynEvent))
			{
				Bucket.Value.Callbacks.RemoveAt(i);
				bRemovedObject = true;

				// Leave the loop, this is called in add as well so we should never get duplicate entries
				break;
			}
		}

		if (bRemovedObject)
		{
			break;
		}
	}

	return bRemovedObject;
}

bool FUpdatePhysicsBucketContainer::RemoveObjectFromAllBuckets(UObject* ObjectToRemove)
{
	if (!ObjectToRemove)
		return false;

	// Store if we ended up removing it
	bool bRemovedObject = false;

	TArray<uint32> BucketsToRemove;
	for (auto& Bucket : ReplicationBuckets)
	{
		for (int i = Bucket.Value.Callbacks.Num() - 1; i >= 0; --i)
		{
			if (Bucket.Value.Callbacks[i].IsBoundToObject(ObjectToRemove))
			{
				Bucket.Value.Callbacks.RemoveAt(i);
				bRemovedObject = true;
			}
		}
	}

	return bRemovedObject;
}

bool FUpdatePhysicsBucketContainer::IsObjectInBucket(UObject* ObjectToRemove)
{
	if (!ObjectToRemove)
		return false;
	for (auto& Bucket : ReplicationBuckets)
	{
		for (int i = Bucket.Value.Callbacks.Num() - 1; i >= 0; --i)
		{
			if (Bucket.Value.Callbacks[i].IsBoundToObject(ObjectToRemove))
			{
				return true;
			}
		}
	}

	return false;
}

bool FUpdatePhysicsBucketContainer::IsObjectFunctionInBucket(UObject* ObjectToRemove, FName FunctionName)
{
	if (!ObjectToRemove)
		return false;
	for (auto& Bucket : ReplicationBuckets)
	{
		for (int i = Bucket.Value.Callbacks.Num() - 1; i >= 0; --i)
		{
			if (Bucket.Value.Callbacks[i].IsBoundToObjectFunction(ObjectToRemove, FunctionName))
			{
				return true;
			}
		}
	}

	return false;
}

bool FUpdatePhysicsBucketContainer::IsObjectDelegateInBucket(FDynamicPhysicsBucketUpdateTickSignature& DynEvent)
{
	if (!DynEvent.IsBound())
		return false;

	for (auto& Bucket : ReplicationBuckets)
	{
		for (int i = Bucket.Value.Callbacks.Num() - 1; i >= 0; --i)
		{
			if (Bucket.Value.Callbacks[i].IsBoundToObjectDelegate(DynEvent))
			{
				return true;
			}
		}
	}

	return false;
}
