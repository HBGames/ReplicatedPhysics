// Copyright Hitbox Games, LLC. All Rights Reserved.

using UnrealBuildTool;

public class ReplicatedPhysics : ModuleRules
{
	public ReplicatedPhysics(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
			}
		);

        PublicIncludePathModuleNames.AddRange(
            new string[] {
            }
        );
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"NetCore"
			}
		);
	}
}
