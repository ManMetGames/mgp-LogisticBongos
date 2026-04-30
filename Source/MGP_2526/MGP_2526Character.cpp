// Copyright Epic Games, Inc. All Rights Reserved.

#include "MGP_2526Character.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/Engine.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "MGP_2526.h"

AMGP_2526Character::AMGP_2526Character()
{
	PrimaryActorTick.bCanEverTick = true;

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
	
	// Create the first person mesh that will be viewed only by this character's owner
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));

	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

	// Create the Camera Component	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;

	// configure the character comps
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

	// Configure character movement
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->AirControl = 0.5f;
	GetCharacterMovement()->MaxFlySpeed = ClimbSpeed;

	if (FirstPersonCameraComponent)
	{
		FirstPersonCameraComponent->SetFieldOfView(WalkingFOV);
		CurrentFOVTarget = WalkingFOV;
	}
}

void AMGP_2526Character::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AMGP_2526Character::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AMGP_2526Character::DoJumpEnd);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMGP_2526Character::MoveInput);

		// Looking/Aiming
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMGP_2526Character::LookInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AMGP_2526Character::LookInput);
	}
	else
	{
		UE_LOG(LogMGP_2526, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}


void AMGP_2526Character::MoveInput(const FInputActionValue& Value)
{
	// get the Vector2D move axis
	FVector2D MovementVector = Value.Get<FVector2D>();
	CachedClimbRight = MovementVector.X;
	CachedClimbForward = MovementVector.Y;

	// pass the axis values to the move input
	DoMove(MovementVector.X, MovementVector.Y);

}

void AMGP_2526Character::LookInput(const FInputActionValue& Value)
{
	// get the Vector2D look axis
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// pass the axis values to the aim input
	DoAim(LookAxisVector.X, LookAxisVector.Y);

}

void AMGP_2526Character::DoAim(float Yaw, float Pitch)
{
	if (GetController())
	{
		// pass the rotation inputs
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AMGP_2526Character::DoMove(float Right, float Forward)
{
	if (GetController())
	{
		if (TraversalState == ETraversalState::Climbing)
		{
			const FVector WallRight = FVector::CrossProduct(FVector::UpVector, CurrentClimbNormal).GetSafeNormal();
			AddMovementInput(FVector::UpVector, Forward);
			AddMovementInput(WallRight, Right);
			return;
		}

		// pass the move inputs
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void AMGP_2526Character::DoJumpStart()
{
	bClimbInputHeld = true;

	if (TraversalState == ETraversalState::Climbing)
	{
		EndWallClimb(true);
		return;
	}

	FHitResult WallHit;
	const FVector TraceStart = FirstPersonCameraComponent ? FirstPersonCameraComponent->GetComponentLocation() : GetActorLocation();
	const FVector TraceDirection = FirstPersonCameraComponent ? FirstPersonCameraComponent->GetForwardVector() : GetActorForwardVector();

	if (FindClimbableWall(TraceStart, TraceDirection, ClimbTraceDistance, WallHit))
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(ClimbHoldTimer, this, &AMGP_2526Character::TryStartWallClimb, ClimbHoldTime, false);
		}

		return;
	}

	// If there is no wall in range, keep the normal jump behaviour.
	Jump();
}

void AMGP_2526Character::DoJumpEnd()
{
	bClimbInputHeld = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ClimbHoldTimer);
	}

	if (TraversalState == ETraversalState::Climbing)
	{
		EndWallClimb(false);
		return;
	}

	// pass StopJumping to the character
	StopJumping();
}

void AMGP_2526Character::TryStartWallClimb()
{
	if (!bClimbInputHeld || TraversalState == ETraversalState::Climbing)
	{
		return;
	}

	FHitResult WallHit;
	const FVector TraceStart = FirstPersonCameraComponent ? FirstPersonCameraComponent->GetComponentLocation() : GetActorLocation();
	const FVector TraceDirection = FirstPersonCameraComponent ? FirstPersonCameraComponent->GetForwardVector() : GetActorForwardVector();

	if (FindClimbableWall(TraceStart, TraceDirection, ClimbTraceDistance, WallHit))
	{
		BeginWallClimb(WallHit);
	}
}

bool AMGP_2526Character::FindClimbableWall(const FVector& TraceStart, const FVector& TraceDirection, float TraceDistance, FHitResult& OutHit) const
{
	const FVector TraceEnd = TraceStart + TraceDirection.GetSafeNormal() * TraceDistance;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WallClimbTrace), false, this);

	if (GetWorld() && GetWorld()->LineTraceSingleByChannel(OutHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
	{
		if (OutHit.GetActor() == nullptr)
		{
			return false;
		}

		const bool bIsTagged = OutHit.GetActor()->ActorHasTag(ClimbableTag) || (OutHit.GetComponent() && OutHit.GetComponent()->ComponentHasTag(ClimbableTag));
		const bool bIsWall = FVector::DotProduct(OutHit.ImpactNormal, FVector::UpVector) < 0.45f;
		return bIsTagged && bIsWall;
	}

	return false;
}

void AMGP_2526Character::BeginWallClimb(const FHitResult& WallHit)
{
	SetTraversalState(ETraversalState::Climbing);
	CurrentClimbNormal = WallHit.ImpactNormal.GetSafeNormal();
	bClimbInputHeld = true;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->SetMovementMode(MOVE_Flying);
		Movement->MaxFlySpeed = ClimbSpeed;
	}

	SetActorLocation(WallHit.ImpactPoint + CurrentClimbNormal * WallStandOffDistance, true);
	SetActorRotation((-CurrentClimbNormal).Rotation());

	// initialize climb animation based on current input
	if (FirstPersonMesh)
	{
		// ensure any previous animation is stopped so PlayAnimation can start clean
		FirstPersonMesh->Stop();
		CurrentClimbAnimation = nullptr;
	}

}

void AMGP_2526Character::EndWallClimb(bool bLaunchAway)
{
	SetTraversalState(ETraversalState::Falling);


	// stop climb animation when leaving the wall
	if (FirstPersonMesh)
	{
		FirstPersonMesh->Stop();
		CurrentClimbAnimation = nullptr;
	}
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Falling);

		if (bLaunchAway)
		{
			LaunchCharacter((-CurrentClimbNormal * 250.0f) + FVector::UpVector * 150.0f, true, true);
		}
	}

}

void AMGP_2526Character::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (TraversalState == ETraversalState::Climbing)
	{
		FHitResult WallHit;
		const FVector TraceStart = GetActorLocation();
		const FVector TraceDirection = -CurrentClimbNormal;

		if (FindClimbableWall(TraceStart, TraceDirection, WallStandOffDistance + 30.0f, WallHit))
		{
			const FVector DesiredLocation = WallHit.ImpactPoint + WallHit.ImpactNormal * WallStandOffDistance;
			SetActorLocation(DesiredLocation, true);
		}
		else
		{
			EndWallClimb(false);
		}

		// update and play appropriate climb animation based on input
		if (FirstPersonMesh)
		{
			// determine desired animation
			UAnimationAsset* DesiredAnim = nullptr;
			const float F = CachedClimbForward;
			const float R = CachedClimbRight;
			const float absF = FMath::Abs(F);
			const float absR = FMath::Abs(R);
			const float diagThreshold = 0.3f;

			if (absF > diagThreshold && absR > diagThreshold)
			{
				// diagonal
				if (F > 0 && R > 0) DesiredAnim = ClimbAnim_UpRight;
				else if (F > 0 && R < 0) DesiredAnim = ClimbAnim_UpLeft;
				else if (F < 0 && R > 0) DesiredAnim = ClimbAnim_DownRight;
				else if (F < 0 && R < 0) DesiredAnim = ClimbAnim_DownLeft;
			}
			else if (absF >= absR)
			{
				// prioritize vertical
				if (F > 0) DesiredAnim = ClimbAnim_Up;
				else if (F < 0) DesiredAnim = ClimbAnim_Down;
			}
			else
			{
				// prioritize horizontal
				if (R > 0) DesiredAnim = ClimbAnim_Right;
				else if (R < 0) DesiredAnim = ClimbAnim_Left;
			}

			if (DesiredAnim && DesiredAnim != CurrentClimbAnimation)
			{
				FirstPersonMesh->PlayAnimation(DesiredAnim, true);
				CurrentClimbAnimation = DesiredAnim;
			}
			else if (!DesiredAnim && CurrentClimbAnimation)
			{
				FirstPersonMesh->Stop();
				CurrentClimbAnimation = nullptr;
			}
		}
	}
	else if (GetCharacterMovement() && GetCharacterMovement()->IsFalling() && bClimbInputHeld)
	{
		FHitResult WallHit;
		const FVector TraceStart = FirstPersonCameraComponent ? FirstPersonCameraComponent->GetComponentLocation() : GetActorLocation();
		const FVector TraceDirection = FirstPersonCameraComponent ? FirstPersonCameraComponent->GetForwardVector() : GetActorForwardVector();

		if (FindClimbableWall(TraceStart, TraceDirection, AutoLatchDistance, WallHit))
		{
			BeginWallClimb(WallHit);
		}
	}
	else if (GetCharacterMovement())
	{
		SetTraversalState(GetCharacterMovement()->IsFalling() ? ETraversalState::Falling : ETraversalState::Walking);
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(42, 0.0f, FColor::Green, FString::Printf(TEXT("Traversal State: %s"), *GetTraversalStateText(TraversalState)));
	}

	// Handle FOV grace timer and interpolation

	// Track fall duration and only switch to falling FOV after the configured delay
	if (GetCharacterMovement() && GetCharacterMovement()->IsFalling())
	{
		FallTimeCounter += DeltaSeconds;
		if (FallTimeCounter >= FallFOVDelay && !bFOVInGrace && CurrentFOVTarget != FallingFOV)
		{
			CurrentFOVTarget = FallingFOV;
		}
	}
	else
	{
		FallTimeCounter = 0.0f;
	}

	if (bFOVInGrace)
	{
		FOVGraceTimer -= DeltaSeconds;
		if (FOVGraceTimer <= 0.0f)
		{
			bFOVInGrace = false;
			// choose new target based on current traversal state
			switch (TraversalState)
			{
			case ETraversalState::Walking:
				CurrentFOVTarget = WalkingFOV;
				break;
			case ETraversalState::Climbing:
				CurrentFOVTarget = ClimbingFOV;
				break;
			case ETraversalState::Falling:
				CurrentFOVTarget = FallingFOV;
				break;
			default:
				CurrentFOVTarget = WalkingFOV;
				break;
			}
		}
	}

	if (FirstPersonCameraComponent)
	{
		const float CurrentFOV = FirstPersonCameraComponent->FieldOfView;
		FirstPersonCameraComponent->SetFieldOfView(FMath::FInterpTo(CurrentFOV, CurrentFOVTarget, DeltaSeconds, FOVInterpSpeed));
	}
}

void AMGP_2526Character::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	SetTraversalState(ETraversalState::Walking);
	bClimbInputHeld = false;
	CachedClimbRight = 0.0f;
	CachedClimbForward = 0.0f;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ClimbHoldTimer);
	}

	// FOV handled by grace/interp logic

	if (LandingThudSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, LandingThudSound, GetActorLocation());
	}
}

void AMGP_2526Character::SetTraversalState(ETraversalState NewState)
{
	if (TraversalState == NewState)
	{
		return;
	}

	const ETraversalState PreviousState = TraversalState;
	TraversalState = NewState;

	UE_LOG(LogMGP_2526, Log, TEXT("Traversal state changed from %s to %s"), *GetTraversalStateText(PreviousState), *GetTraversalStateText(NewState));

	// Manage FOV targets and a short grace period when reverting from falling
	if (PreviousState == ETraversalState::Falling && NewState != ETraversalState::Falling)
	{
		// hold the falling FOV for a moment before reverting
		bFOVInGrace = true;
		FOVGraceTimer = FOVRevertGrace;
		CurrentFOVTarget = FallingFOV;
	}
	else if (NewState == ETraversalState::Falling)
	{
		bFOVInGrace = false;
		// start counting fall time; we will apply the falling FOV after FallFOVDelay
		FallTimeCounter = 0.0f;
	}
	else
	{
		bFOVInGrace = false;
		switch (NewState)
		{
		case ETraversalState::Walking:
			CurrentFOVTarget = WalkingFOV;
			break;
		case ETraversalState::Climbing:
			CurrentFOVTarget = ClimbingFOV;
			break;
		default:
			CurrentFOVTarget = WalkingFOV;
			break;
		}
	}
}

FString AMGP_2526Character::GetTraversalStateText(ETraversalState State) const
{
	switch (State)
	{
	case ETraversalState::Walking:
		return TEXT("Walking");
	case ETraversalState::Climbing:
		return TEXT("Climbing");
	case ETraversalState::Falling:
		return TEXT("Falling");
	default:
		return TEXT("Unknown");
	}
}
