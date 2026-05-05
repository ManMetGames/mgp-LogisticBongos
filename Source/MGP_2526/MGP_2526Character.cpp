/** Copyright Epic Games, Inc. All Rights Reserved. */
 
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
#include "DrawDebugHelpers.h"
 
#define LOG_FUNCTION_ENTRY(...) UE_LOG(LogMGP_2526, Log, TEXT("%s activated"), ANSI_TO_TCHAR(__FUNCTION__))
 
AMGP_2526Character::AMGP_2526Character()
{
	LOG_FUNCTION_ENTRY();
	PrimaryActorTick.bCanEverTick = true;
 
	/** Set size for collision capsule */
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
 
	/** Create the first person mesh that will be viewed only by this character's owner */
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));
 
	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));
 
	/** Create the Camera Component */
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;
 
	/** Configure the character comps */
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;
 
	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);
 
	/** Configure character movement */
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
	LOG_FUNCTION_ENTRY();
	/** Set up action bindings */
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		/** Jumping */
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AMGP_2526Character::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AMGP_2526Character::DoJumpEnd);
 
		/** Moving */
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMGP_2526Character::MoveInput);
 
		/** Looking/Aiming */
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
	LOG_FUNCTION_ENTRY();
	/** Get the Vector2D move axis */
	FVector2D MovementVector = Value.Get<FVector2D>();
	CachedClimbRight = MovementVector.X;
	CachedClimbForward = MovementVector.Y;
 
	/** Pass the axis values to the move input */
	DoMove(MovementVector.X, MovementVector.Y);
 
}
 
void AMGP_2526Character::LookInput(const FInputActionValue& Value)
{
	LOG_FUNCTION_ENTRY();
	/** Get the Vector2D look axis */
	FVector2D LookAxisVector = Value.Get<FVector2D>();
 
	/** Pass the axis values to the aim input */
	DoAim(LookAxisVector.X, LookAxisVector.Y);
 
}
 
void AMGP_2526Character::DoAim(float Yaw, float Pitch)
{
	LOG_FUNCTION_ENTRY();
	if (GetController())
	{
		/** Pass the rotation inputs */
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}
 
void AMGP_2526Character::DoMove(float Right, float Forward)
{
	LOG_FUNCTION_ENTRY();
	if (GetController())
	{
		if (TraversalState == ETraversalState::Mantling)
		{
			return;
		}
 
		if (TraversalState == ETraversalState::Climbing)
		{
			const FVector WallUp = FVector::VectorPlaneProject(FVector::UpVector, CurrentClimbNormal).GetSafeNormal();
			const FVector WallRight = FVector::CrossProduct(CurrentClimbNormal, WallUp).GetSafeNormal();
			AddMovementInput(WallUp, Forward);
			AddMovementInput(WallRight, Right);
			return;
		}
 
		/** Pass the move inputs */
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}
 
void AMGP_2526Character::DoJumpStart()
{
	LOG_FUNCTION_ENTRY();
	bClimbInputHeld = true;
 
	if (TraversalState == ETraversalState::Climbing)
	{
		EndWallClimb(true);
		return;
	}
 
	if (TraversalState == ETraversalState::Mantling)
	{
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
 
	/** If there is no wall in range, keep the normal jump behaviour. */
	Jump();
}
 
void AMGP_2526Character::DoJumpEnd()
{
	LOG_FUNCTION_ENTRY();
	bClimbInputHeld = false;
 
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ClimbHoldTimer);
	}
 
	if (TraversalState == ETraversalState::Mantling)
	{
		return;
	}
 
	if (TraversalState == ETraversalState::Climbing)
	{
		EndWallClimb(false);
		return;
	}
 
	/** Pass StopJumping to the character */
	StopJumping();
}
 
void AMGP_2526Character::TryStartWallClimb()
{
	LOG_FUNCTION_ENTRY();
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
	UE_LOG(LogMGP_2526, VeryVerbose, TEXT("%s activated"), ANSI_TO_TCHAR(__FUNCTION__));
	const FVector TraceEnd = TraceStart + TraceDirection.GetSafeNormal() * TraceDistance;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WallClimbTrace), false, this);
	if (GetWorld() && GetWorld()->LineTraceSingleByChannel(OutHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
	{
		if (OutHit.GetActor() == nullptr)
		{
			return false;
		}
 
		const bool bIsTagged = OutHit.GetActor()->ActorHasTag(ClimbableTag)
			|| OutHit.GetActor()->ActorHasTag(TEXT("Wall"))
			|| (OutHit.GetComponent() && (OutHit.GetComponent()->ComponentHasTag(ClimbableTag) || OutHit.GetComponent()->ComponentHasTag(TEXT("Wall"))));
		const bool bIsWall = FVector::DotProduct(OutHit.ImpactNormal, FVector::UpVector) < 0.45f;
		return bIsTagged && bIsWall;
	}
 
	return false;
}
 
void AMGP_2526Character::BeginWallClimb(const FHitResult& WallHit)
{
	LOG_FUNCTION_ENTRY();
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
 
	if (FirstPersonMesh)
	{
		FirstPersonMesh->Stop();
		FirstPersonMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		CurrentClimbAnimation = nullptr;
	}
}
 
void AMGP_2526Character::BeginWallMantle(const FHitResult& LedgeHit)
{
    bClimbInputHeld = false;
    CachedClimbRight = 0.0f;
    CachedClimbForward = 0.0f;
    bIsMantling = true;
 
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ClimbHoldTimer);
        World->GetTimerManager().ClearTimer(MantleTimer);
    }
 
    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->StopMovementImmediately();
        Movement->DisableMovement();
        Movement->Velocity = FVector::Zero();
    }
 
    FVector TargetLocation = LedgeHit.ImpactPoint;
 
    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        const float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
 
        // Put the capsule properly on top of the ledge.
        TargetLocation = LedgeHit.ImpactPoint + FVector::UpVector * (CapsuleHalfHeight + 5.0f);
    }
 
    const FRotator TargetRotation = GetController()
        ? FRotator(0.0f, GetControlRotation().Yaw, 0.0f)
        : GetActorRotation();
 
    UE_LOG(LogTemp, Warning, TEXT("Mantle started | Target Location: %s"),
        *TargetLocation.ToString()
    );
 
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            -1,
            2.0f,
            FColor::Green,
            TEXT("Mantle started")
        );
    }
SetActorLocation(TargetLocation, true);
    SetActorRotation(TargetRotation);
 
    SetTraversalState(ETraversalState::Mantling);
 
    if (FirstPersonMesh)
    {
        FirstPersonMesh->Stop();
        CurrentClimbAnimation = nullptr;
 
        FirstPersonMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
 
        if (ClimbAnim_Top)
        {
            FirstPersonMesh->PlayAnimation(ClimbAnim_Top, false);
 
            float AnimDuration = MantleDuration;
 
            if (UAnimSequence* Seq = Cast<UAnimSequence>(ClimbAnim_Top))
            {
                AnimDuration = FMath::Max(0.05f, Seq->GetPlayLength());
            }
 
            if (UWorld* World = GetWorld())
            {
                World->GetTimerManager().SetTimer(
                    MantleTimer,
                    this,
                    &AMGP_2526Character::OnMantleFinished,
                    AnimDuration,
                    false
                );
            }
        }
        else
        {
            if (UWorld* World = GetWorld())
            {
                World->GetTimerManager().SetTimer(
                    MantleTimer,
                    this,
                    &AMGP_2526Character::OnMantleFinished,
                    MantleDuration,
                    false
                );
            }
        }
    }
    else
    {
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimer(
                MantleTimer,
                this,
                &AMGP_2526Character::OnMantleFinished,
                MantleDuration,
                false
            );
        }
    }
}
 
bool AMGP_2526Character::TryFinishWallClimbOnLedge(FHitResult& OutLedgeHit) const
{
    const UWorld* World = GetWorld();
    const UCapsuleComponent* Capsule = GetCapsuleComponent();
 
    if (!World || !Capsule)
    {
        return false;
    }
 
	const float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
 
	const FVector WallNormal = CurrentClimbNormal.GetSafeNormal();
 
	// Direction into the wall / over the ledge.
	const FVector WallDirection = -WallNormal;
 
	const float WallTraceDistance = FMath::Max(ClimbTraceDistance, WallStandOffDistance + 150.0f);
 
    FHitResult LowWallHit;
    FHitResult HighWallHit;
 
	const FVector ActorLocation = GetActorLocation();
	const FVector ProbeOrigin = ActorLocation + WallNormal * 15.0f;
 
    // Low trace checks that we are still against the wall.
	const FVector LowTraceStart = ProbeOrigin + FVector::UpVector * (CapsuleHalfHeight * -0.35f);
 
    // High trace checks if the wall continues above us.
    // If high trace does NOT hit, there is probably a ledge.
	const FVector HighTraceStart = ProbeOrigin + FVector::UpVector * (CapsuleHalfHeight + 45.0f);
 
	FCollisionQueryParams WallQueryParams(SCENE_QUERY_STAT(WallClimbLedgeWallTrace), false, this);
	const FVector LowTraceEnd = LowTraceStart + WallDirection * WallTraceDistance;
	const FVector HighTraceEnd = HighTraceStart + WallDirection * WallTraceDistance;
 
	const auto IsWallHit = [](const FHitResult& Hit)
	{
		return Hit.GetActor() != nullptr && FVector::DotProduct(Hit.ImpactNormal, FVector::UpVector) < 0.45f;
	};
 
	const bool bLowRawHit = World->LineTraceSingleByChannel(LowWallHit, LowTraceStart, LowTraceEnd, ECC_Visibility, WallQueryParams);
	const bool bHighRawHit = World->LineTraceSingleByChannel(HighWallHit, HighTraceStart, HighTraceEnd, ECC_Visibility, WallQueryParams);
 
	const bool bLowHit = bLowRawHit && IsWallHit(LowWallHit);
	const bool bHighHit = bHighRawHit && IsWallHit(HighWallHit);
 
    UE_LOG(LogTemp, Warning, TEXT("Mantle wall check | LowHit: %s | HighHit: %s"),
        bLowHit ? TEXT("true") : TEXT("false"),
        bHighHit ? TEXT("true") : TEXT("false")
    );
 
	UE_LOG(LogTemp, Warning, TEXT("Mantle wall raw | LowRaw: %s %s | HighRaw: %s %s"),
		bLowRawHit ? TEXT("true") : TEXT("false"),
		*GetNameSafe(LowWallHit.GetActor()),
		bHighRawHit ? TEXT("true") : TEXT("false"),
		*GetNameSafe(HighWallHit.GetActor())
	);
 
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            -1,
            1.0f,
            FColor::Cyan,
			FString::Printf(TEXT("Mantle Wall | Low: %s | High: %s"),
                bLowHit ? TEXT("true") : TEXT("false"),
                bHighHit ? TEXT("true") : TEXT("false")
            )
        );
    }
// For mantling:
    // low wall should hit,
    // high wall should NOT hit.
    if (!bLowHit || bHighHit)
    {
        return false;
    }
 
    // CurrentClimbNormal points out of the wall toward the player.
    // So to check the top of the ledge, we go in the opposite direction.
	const FVector LedgeTraceStart =
		ActorLocation
		+ FVector::UpVector * (CapsuleHalfHeight + 100.0f)
		- CurrentClimbNormal * MantleWindowForward;
 
    const FVector LedgeTraceEnd =
        LedgeTraceStart
		- FVector::UpVector * (CapsuleHalfHeight + 150.0f);
 
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WallClimbLedgeTrace), false, this);
 
    FHitResult Hit;
    const bool bLedgeHit = World->LineTraceSingleByChannel(
        Hit,
        LedgeTraceStart,
        LedgeTraceEnd,
        ECC_Visibility,
        QueryParams
    );
 
    DrawDebugLine(
        World,
        LedgeTraceStart,
        LedgeTraceEnd,
        bLedgeHit ? FColor::Green : FColor::Red,
        false,
        2.0f,
        0,
        2.0f
    );
 
    UE_LOG(LogTemp, Warning, TEXT("Mantle ledge trace | Hit: %s | Actor: %s"),
        bLedgeHit ? TEXT("true") : TEXT("false"),
        *GetNameSafe(Hit.GetActor())
    );
 
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            -1,
            1.0f,
            bLedgeHit ? FColor::Green : FColor::Red,
            FString::Printf(TEXT("Ledge Trace Hit: %s"),
                bLedgeHit ? TEXT("true") : TEXT("false")
            )
        );
    }
 
    if (bLedgeHit)
    {
        const bool bIsWalkableTop = FVector::DotProduct(Hit.ImpactNormal, FVector::UpVector) > 0.7f;
 
        UE_LOG(LogTemp, Warning, TEXT("Mantle top check | WalkableTop: %s | NormalZ: %f"),
            bIsWalkableTop ? TEXT("true") : TEXT("false"),
            Hit.ImpactNormal.Z
        );
 
        if (Hit.GetActor() && bIsWalkableTop)
        {
            OutLedgeHit = Hit;
            return true;
        }
    }
 
    return false;
}
 
void AMGP_2526Character::FinishWallClimbOnLedge(const FHitResult& LedgeHit)
{
	LOG_FUNCTION_ENTRY();
	BeginWallMantle(LedgeHit);
}
 
void AMGP_2526Character::OnMantleFinished()
{
	LOG_FUNCTION_ENTRY();
	bIsMantling = false;
 
	SetTraversalState(ETraversalState::Walking);
 
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->SetMovementMode(MOVE_Walking);
	}
 
	if (FirstPersonMesh)
	{
		// Stop single-node play and restore anim instance usage
		FirstPersonMesh->Stop();
		FirstPersonMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		CurrentClimbAnimation = nullptr;
	}
 
	// Clear mantle timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MantleTimer);
	}
}
 
void AMGP_2526Character::EndWallClimb(bool bLaunchAway)
{
	LOG_FUNCTION_ENTRY();
	if (!bLaunchAway)
	{
		FHitResult LedgeHit;
		if (TryFinishWallClimbOnLedge(LedgeHit))
		{
			BeginWallMantle(LedgeHit);
			return;
		}
	}
 
	SetTraversalState(ETraversalState::Falling);
 
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
 
	bIsMantling = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MantleTimer);
	}
 
	if (FirstPersonMesh)
	{
		FirstPersonMesh->Stop();
		FirstPersonMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		CurrentClimbAnimation = nullptr;
	}
 
}
 
void AMGP_2526Character::Tick(float DeltaSeconds)
{
	UE_LOG(LogMGP_2526, VeryVerbose, TEXT("%s activated"), ANSI_TO_TCHAR(__FUNCTION__));
	Super::Tick(DeltaSeconds);
 
	if (TraversalState == ETraversalState::Climbing)
	{
		FHitResult WallHit;
		const FVector TraceStart = GetActorLocation();
		const FVector TraceDirection = -CurrentClimbNormal;
 
		if (FindClimbableWall(TraceStart, TraceDirection, WallStandOffDistance + 30.0f, WallHit))
		{
			CurrentClimbNormal = WallHit.ImpactNormal.GetSafeNormal();
			const FVector DesiredLocation = WallHit.ImpactPoint + WallHit.ImpactNormal * WallStandOffDistance;
			SetActorLocation(DesiredLocation, true);
			const FRotator TargetRotation = (-CurrentClimbNormal).Rotation();
			SetActorRotation(FMath::RInterpTo(GetActorRotation(), TargetRotation, DeltaSeconds, ClimbFacingInterpSpeed));
		}
		else
		{
			FHitResult FallbackLedgeHit;
			if (TryFinishWallClimbOnLedge(FallbackLedgeHit))
			{
				BeginWallMantle(FallbackLedgeHit);
			}
			else
			{
				EndWallClimb(false);
			}
		}
 
		if (FirstPersonMesh)
		{
			UAnimationAsset* DesiredAnim = nullptr;
			const float F = CachedClimbForward;
			const float R = CachedClimbRight;
			const float absF = FMath::Abs(F);
			const float absR = FMath::Abs(R);
			const float diagThreshold = 0.3f;
 
			if (absF > diagThreshold && absR > diagThreshold)
			{
				if (F > 0 && R > 0)
				{
					DesiredAnim = ClimbAnim_UpRight;
				}
				else if (F > 0 && R < 0)
				{
					DesiredAnim = ClimbAnim_UpLeft;
				}
				else if (F < 0 && R > 0)
				{
					DesiredAnim = ClimbAnim_DownRight;
				}
				else if (F < 0 && R < 0)
				{
					DesiredAnim = ClimbAnim_DownLeft;
				}
			}
			else if (absF >= absR)
			{
				if (F > 0)
				{
					DesiredAnim = ClimbAnim_Up;
				}
				else if (F < 0)
				{
					DesiredAnim = ClimbAnim_Down;
				}
			}
			else
			{
				if (R > 0)
				{
					DesiredAnim = ClimbAnim_Right;
				}
				else if (R < 0)
				{
					DesiredAnim = ClimbAnim_Left;
				}
			}
 
			if (DesiredAnim && DesiredAnim != CurrentClimbAnimation)
			{
				FirstPersonMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
				FirstPersonMesh->PlayAnimation(DesiredAnim, true);
				CurrentClimbAnimation = DesiredAnim;
			}
		}
	}
	else if (TraversalState == ETraversalState::Mantling)
	{
		return;
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
		// Allow holding the climb input while walking to start a climb
		if (bClimbInputHeld && TraversalState != ETraversalState::Climbing)
		{
			if (!GetWorld()->GetTimerManager().IsTimerActive(ClimbHoldTimer))
			{
				FHitResult WallHit;
				const FVector TraceStart = FirstPersonCameraComponent ? FirstPersonCameraComponent->GetComponentLocation() : GetActorLocation();
				const FVector TraceDirection = FirstPersonCameraComponent ? FirstPersonCameraComponent->GetForwardVector() : GetActorForwardVector();
				if (FindClimbableWall(TraceStart, TraceDirection, ClimbTraceDistance, WallHit))
				{
					if (UWorld* World = GetWorld())
					{
						World->GetTimerManager().SetTimer(ClimbHoldTimer, this, &AMGP_2526Character::TryStartWallClimb, ClimbHoldTime, false);
					}
				}
			}
		}
		SetTraversalState(GetCharacterMovement()->IsFalling() ? ETraversalState::Falling : ETraversalState::Walking);
	}
 
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(42, 0.0f, FColor::Green, FString::Printf(TEXT("Traversal State: %s"), *GetTraversalStateText(TraversalState)));
	}
 
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
		const float NewFOV = FMath::FInterpTo(CurrentFOV, CurrentFOVTarget, DeltaSeconds, FOVInterpSpeed);
		FirstPersonCameraComponent->SetFieldOfView(NewFOV);
		FirstPersonCameraComponent->FirstPersonFieldOfView = NewFOV;
	}
}
 
void AMGP_2526Character::Landed(const FHitResult& Hit)
{
	LOG_FUNCTION_ENTRY();
	Super::Landed(Hit);
 
	SetTraversalState(ETraversalState::Walking);
	bClimbInputHeld = false;
	CachedClimbRight = 0.0f;
	CachedClimbForward = 0.0f;
 
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ClimbHoldTimer);
		if (World->GetTimerManager().IsTimerActive(MantleTimer))
		{
			World->GetTimerManager().ClearTimer(MantleTimer);
			OnMantleFinished();
		}
	}
 
	/** FOV handled by grace/interp logic */
 
	if (LandingThudSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, LandingThudSound, GetActorLocation());
	}
}
 
void AMGP_2526Character::SetTraversalState(ETraversalState NewState)
{
	LOG_FUNCTION_ENTRY();
	if (TraversalState == NewState)
	{
		return;
	}
 
	const ETraversalState PreviousState = TraversalState;
	TraversalState = NewState;
 
	UE_LOG(LogMGP_2526, Log, TEXT("Traversal state changed from %s to %s"), *GetTraversalStateText(PreviousState), *GetTraversalStateText(NewState));
 
		/** Manage FOV targets and a short grace period when reverting from falling */
	if (PreviousState == ETraversalState::Falling && NewState != ETraversalState::Falling)
	{
		/** Hold the falling FOV for a moment before reverting */
		bFOVInGrace = true;
		FOVGraceTimer = FOVRevertGrace;
		CurrentFOVTarget = FallingFOV;
	}
	else if (NewState == ETraversalState::Falling)
	{
		bFOVInGrace = false;
		/** Start counting fall time; we will apply the falling FOV after FallFOVDelay */
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
		case ETraversalState::Mantling:
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
	UE_LOG(LogMGP_2526, VeryVerbose, TEXT("%s activated"), ANSI_TO_TCHAR(__FUNCTION__));
	switch (State)
	{
	case ETraversalState::Walking:
		return TEXT("Walking");
	case ETraversalState::Climbing:
		return TEXT("Climbing");
	case ETraversalState::Mantling:
		return TEXT("Mantling");
	case ETraversalState::Falling:
		return TEXT("Falling");
	default:
		return TEXT("Unknown");
	}
}