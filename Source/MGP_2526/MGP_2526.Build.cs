// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MGP_2526 : ModuleRules
{
	public MGP_2526(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"MGP_2526",
			"MGP_2526/Variant_Horror",
			"MGP_2526/Variant_Horror/UI",
			"MGP_2526/Variant_Shooter",
			"MGP_2526/Variant_Shooter/AI",
			"MGP_2526/Variant_Shooter/UI",
			"MGP_2526/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
