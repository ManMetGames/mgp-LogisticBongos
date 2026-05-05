// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "MGP_2526Character.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class USoundBase;
class UAnimationAsset;
struct FInputActionValue;

UENUM(BlueprintType)
enum class ETraversalState : uint8
{
	Walking,
	Climbing,
	Mantling,
	Falling
};

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A basic first person character
 */
UCLASS(abstract)
class AMGP_2526Character : public ACharacter
{
	GENERATED_BODY()

	/** Pawn mesh: first person view (arms; seen only by self) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMesh;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* MouseLookAction;

	/** Current traversal state for wall climbing */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Traversal", meta = (AllowPrivateAccess = "true"))
	ETraversalState TraversalState = ETraversalState::Walking;

	/** How long space must be held near a wall before climbing starts */
	UPROPERTY(EditAnywhere, Category="Traversal|Climb", meta = (ClampMin = 0, Units = "s"))
	float ClimbHoldTime = 1.0f;

	/** How far ahead we can detect a climbable wall from the camera */
	UPROPERTY(EditAnywhere, Category="Traversal|Climb", meta = (ClampMin = 0, Units = "cm"))
	float ClimbTraceDistance = 220.0f;

	/** How far ahead we keep looking for a wall while falling */
	UPROPERTY(EditAnywhere, Category="Traversal|Climb", meta = (ClampMin = 0, Units = "cm"))
	float AutoLatchDistance = 150.0f;

	/** Tag used to mark climbable walls */
	UPROPERTY(EditAnywhere, Category="Traversal|Climb")
	FName ClimbableTag = TEXT("Climbable");

	/** Movement speed while climbing */
	UPROPERTY(EditAnywhere, Category="Traversal|Climb", meta = (ClampMin = 0, Units = "cm/s"))
	float ClimbSpeed = 160.0f;

	/** Distance we keep the capsule from the wall while climbing */
	UPROPERTY(EditAnywhere, Category="Traversal|Climb", meta = (ClampMin = 0, Units = "cm"))
	float WallStandOffDistance = 45.0f;

	/** How far forward from the wall to probe for a mantle (cm) */
	UPROPERTY(EditAnywhere, Category="Traversal|Climb", meta=(ClampMin=0, Units="cm"))
	float MantleWindowForward = 70.0f;

	/** How long the mantle should take (s) */
	UPROPERTY(EditAnywhere, Category="Traversal|Climb", meta=(ClampMin=0.01f, Units="s"))
	float MantleDuration = 0.45f;

	/** Default FOV while walking */
	UPROPERTY(EditAnywhere, Category="Traversal|Camera", meta = (ClampMin = 0, Units = "deg"))
	float WalkingFOV = 70.0f;

	/** FOV while climbing */
	UPROPERTY(EditAnywhere, Category="Traversal|Camera", meta = (ClampMin = 0, Units = "deg"))
	float ClimbingFOV = 80.0f;

	/** How quickly the character turns to face the climb surface */
	UPROPERTY(EditAnywhere, Category="Traversal|Climb", meta = (ClampMin = 0))
	float ClimbFacingInterpSpeed = 10.0f;

	/** FOV while falling */
	UPROPERTY(EditAnywhere, Category="Traversal|Camera", meta = (ClampMin = 0, Units = "deg"))
	float FallingFOV = 82.0f;

	/** How quickly the FOV changes between traversal states */
	UPROPERTY(EditAnywhere, Category="Traversal|Camera", meta = (ClampMin = 0))
	float FOVInterpSpeed = 10.0f;

	/** Grace time to hold the falling FOV before reverting when returning to walk/climb */
	UPROPERTY(EditAnywhere, Category="Traversal|Camera", meta=(ClampMin=0))
	float FOVRevertGrace = 0.22f;

	/** Current FOV target used for interpolation */
	float CurrentFOVTarget = 70.0f;

	/** Remaining grace timer (counts down) */
	float FOVGraceTimer = 0.0f;

	/** True while we're holding the FOV (grace) before reverting */
	bool bFOVInGrace = false;

	/** How long the character must be falling before we switch to the falling FOV */
	UPROPERTY(EditAnywhere, Category="Traversal|Camera", meta=(ClampMin=0))
	float FallFOVDelay = 1.0f;

	/** Accumulated fall time since entering falling state */
	float FallTimeCounter = 0.0f;

	/** Optional landing thud sound used when leaving a climb and hitting the ground */
	UPROPERTY(EditAnywhere, Category="Traversal|Audio")
	USoundBase* LandingThudSound;

	/** True while the climb input is being held */
	bool bClimbInputHeld = false;

	/** The wall normal we are currently climbing against */
	FVector CurrentClimbNormal = FVector::ForwardVector;

	/** Cached movement axis values while climbing */
	float CachedClimbRight = 0.0f;
	float CachedClimbForward = 0.0f;

	/** Climb animation assets (looping) for directional movement */
	UPROPERTY(EditAnywhere, Category="Traversal|Animation")
	UAnimationAsset* ClimbAnim_Up;

	UPROPERTY(EditAnywhere, Category="Traversal|Animation")
	UAnimationAsset* ClimbAnim_Down;

	UPROPERTY(EditAnywhere, Category="Traversal|Animation")
	UAnimationAsset* ClimbAnim_Left;

	UPROPERTY(EditAnywhere, Category="Traversal|Animation")
	UAnimationAsset* ClimbAnim_Right;

	UPROPERTY(EditAnywhere, Category="Traversal|Animation")
	UAnimationAsset* ClimbAnim_UpLeft;

	UPROPERTY(EditAnywhere, Category="Traversal|Animation")
	UAnimationAsset* ClimbAnim_UpRight;

	UPROPERTY(EditAnywhere, Category="Traversal|Animation")
	UAnimationAsset* ClimbAnim_DownLeft;

	UPROPERTY(EditAnywhere, Category="Traversal|Animation")
	UAnimationAsset* ClimbAnim_DownRight;

	/** Animation played once when the climb finishes onto a ledge */
	UPROPERTY(EditAnywhere, Category="Traversal|Animation")
	UAnimationAsset* ClimbAnim_Top;

	/** Currently playing climb animation asset */
	UAnimationAsset* CurrentClimbAnimation = nullptr;

	/** Pending climb hold timer */
	FTimerHandle ClimbHoldTimer;

	/** Timer used while playing the mantle/top animation */
	FTimerHandle MantleTimer;

	/** True while the character is actively mantling onto a ledge */
	bool bIsMantling = false;

	/** Starts the mantle sequence from a valid ledge hit */
	void BeginWallMantle(const FHitResult& LedgeHit);

	/** Called when the mantle/top animation finishes */
	void OnMantleFinished();

public:
	AMGP_2526Character();

protected:

	/** Called from Input Actions for movement input */
	void MoveInput(const FInputActionValue& Value);

	/** Called from Input Actions for looking input */
	void LookInput(const FInputActionValue& Value);

	/** Handles aim inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoAim(float Yaw, float Pitch);

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles jump start inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump end inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

	/** Called when wall climb should begin from a held jump input */
	void TryStartWallClimb();

	/** Attempts to detect a climbable wall along a trace direction */
	bool FindClimbableWall(const FVector& TraceStart, const FVector& TraceDirection, float TraceDistance, FHitResult& OutHit) const;

	/** Starts climbing the wall that was hit by a valid trace */
	void BeginWallClimb(const FHitResult& WallHit);

	/** Attempts to finish a climb onto a walkable ledge instead of falling */
	bool TryFinishWallClimbOnLedge(FHitResult& OutLedgeHit) const;

	/** Ends climbing by placing the character into a walking state */
	void FinishWallClimbOnLedge(const FHitResult& LedgeHit);

	/** Ends climbing and transitions into the falling state */
	void EndWallClimb(bool bLaunchAway);

	/** Updates camera and state bookkeeping every frame */
	virtual void Tick(float DeltaSeconds) override;

	/** Handles landing after a fall */
	virtual void Landed(const FHitResult& Hit) override;

	/** Writes the current traversal state to the screen and logs state transitions */
	void SetTraversalState(ETraversalState NewState);

	/** Converts the traversal state to readable text */
	FString GetTraversalStateText(ETraversalState State) const;

protected:

	/** Set up input action bindings */
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	

public:

	/** Returns the first person mesh **/
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	/** Returns first person camera component **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

};

