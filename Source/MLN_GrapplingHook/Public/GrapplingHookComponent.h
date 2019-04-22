// Copyright 2018 Matteo Lorenzo Nasci

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GrapplingHookComponent.generated.h"

UENUM(BlueprintType, Blueprintable, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
/* Mask that represent all the enabled features in the grappling hook
*/
enum class EGrapplingHookActivation : uint8
{
	/* No flag is set
	*/
	GA_None = 0 UMETA(DisplayName = "None"),
	/* Pull feature enabled
	*/
	GA_Pull = 1 UMETA(DisplayName = "Pull"),
	/* Launch feature enabled
	*/
	GA_Launch = 1 << 1 UMETA(DisplayName = "Launch"),
	/* Swing feature enabled. 
	 *@warning To work correctly the Character CapsuleCollider needs to be the only physics component with SetEnabledCollision active, otherwise the Character will remain 'hanging' from the hit surface
	*/
	GA_Swing = 1 << 2 UMETA(DisplayName = "Swing"),
	/* Retracting feature enabled
	*/
	GA_Retracting = 1 << 3 UMETA(DisplayName = "Retracting"),
	/* Extending feature enabled
	*/
	GA_Extending = 1 << 4 UMETA(DisplayName = "Extending"),
	/* Cooldown feature enabled
	*/
	GA_Cooldown = 1 << 5 UMETA(DisplayName = "Cooldown"),
	/* Unused bit
	*/
	GA_Unused1 = 1 << 6 UMETA(DisplayName = "Unused 1"),
	/* Unused bit
	*/
	GA_Unused2 = 1 << 7 UMETA(DisplayName = "Unused 2"),
	/* All features enabled
	*/
	GA_All = 0xFF UMETA(DisplayName = "All"),
};
ENUM_CLASS_FLAGS(EGrapplingHookActivation);

UENUM(BlueprintType, Blueprintable)
/* Collection of grappling hook states
*/
enum class EGrapplingHookState : uint8
{
	/* The grapple is ready to be launched
	*/
	GS_Ready UMETA(DisplayName = "Ready"),
	/* The character is being launched to the designed location
	*/
	GS_Launch UMETA(DisplayName = "Launch"),
	/* The grappled object is being pulled towards the character
	*/
	GS_Pull UMETA(DisplayName = "Pull"),
	/* The character has been attached to the designed location and is swinging
	*/
	GS_Swing UMETA(DisplayName = "Swing"),
	/* The grapple missed all valid targets
	*/
	GS_Missed UMETA(DisplayName = "Missed"),
	/* The grapple is not being used and is on cooldown
	*/
	GS_Disabled UMETA(DisplayName = "Disabled"),
	/* The grapple is being retracted
	*/
	GS_Retracting UMETA(DisplayName = "Retracting"),
	/* The grapple is being launched
	*/
	GS_Extending UMETA(DisplayName = "Extending"),
};

UENUM(BlueprintType, Blueprintable)
/* Collection of grappling hook errors
*/
enum class EGrapplingHookError : uint8
{
	/* The spawn of the AProjectileHook did not succeed because no Cable reference was set
	*/
	GE_HookSpawnCable UMETA(DisplayName = "Failed Hook Spawn Cable"),
	/* The spawn of the AProjectileHook did not succeed because no World was found
	*/
	GE_HookSpawnWorld UMETA(DisplayName = "Failed Hook Spawn World"),
	/* The spawn of the AProjectileHook did not succeed
	*/
	GE_HookSpawnInstanceActor UMETA(DisplayName = "Failed Hook Spawn Actor"),
	/* The spawn of the AProjectileHook did not succeed (Failed Cast to AProjectileHook)
	*/
	GE_HookSpawnInstanceProjectile UMETA(DisplayName = "Failed Hook Spawn Projectile"),
	/* The Swing activation failed due to missing core elements (Owner, Owner movement component,Capsule, Cable, SwingConstraint)
	*/
	GE_SwingActivationCore UMETA(DisplayName = "Failed Swing Activation Core"),
	/* The Swing update failed due to missing core elements (Owner, Capsule, Cable)
	*/
	GE_SwingUpdateCore UMETA(DisplayName = "Failed Swing Update Core"),
	/* The Retract update failed due to missing core elements (Owner, Capsule, Cable)
	*/
	GE_RetractUpdateCore UMETA(DisplayName = "Failed Retract Update Core"),
	/* The Launch update failed due to missing core elements (Owner, Cable)
	*/
	GE_LaunchUpdateCore UMETA(DisplayName = "Failed Launch Update Core"),
	/* The Pull update failed due to missing core elements (PullHandle, Cable)
	*/
	GE_PullUpdateCore UMETA(DisplayName = "Failed Pull Update Core"),
	/* The Pull Activation failed due to missing core elements (PullHandle, Cable)
	*/
	GE_PullActivationCore UMETA(DisplayName = "Failed Pull Activation Core"),
	/* The update failed due to missing core elements (Owner, Cable, Hook)
	*/
	GE_UpdateCore UMETA(DisplayName = "Failed Update Core"),
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGrappleActivated, EGrapplingHookState, State, UPrimitiveComponent*, GrappledObject);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGrappleStateChanged, EGrapplingHookState, OldState, EGrapplingHookState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGrappleInterrupted, EGrapplingHookState, State);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGrappleBreaked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGrappleReady);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGrappleMissed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGrappleDisabled, float, Cooldown);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGrappleError, EGrapplingHookError, Error);

class AProjectileHook;
class ACharacter;
class UCableComponent;
class UPrimitiveComponent;
class UPhysicsConstraintComponent;
class UPhysicsHandleComponent;
class UAudioComponent;
UCLASS(BlueprintType, Blueprintable, ClassGroup=(Grapple), meta=(BlueprintSpawnableComponent) )
/*
* Component to manage and attuate the grappling hook mechanic
*/
class MLN_GRAPPLINGHOOK_API UGrapplingHookComponent : public UActorComponent
{
	GENERATED_BODY()
private:
	float CurrentRetractDuration;
	FVector RetractStartLocation;
	bool bActivatedSwing;
public:

	UPROPERTY(BlueprintAssignable, Category = "Config|Dispatchers")
	/* Event invoked when an error occurs
	*/
	FOnGrappleError OnGrappleError;
	UPROPERTY(BlueprintAssignable, Category = "Config|Dispatchers")
	/* Event invoked when the grappling hook state changes
	*/
	FOnGrappleStateChanged OnGrappleStateChanged;
	UPROPERTY(BlueprintAssignable, Category = "Config|Dispatchers")
	/* Event invoked when the grappling hook is activated by hitting a valid object (after the extension phase)
	*/
	FOnGrappleActivated OnGrappleActivated;
	UPROPERTY(BlueprintAssignable, Category = "Config|Dispatchers")
	/* Event invoked when the grappling hook is interrupted (either from external request or internally)
	*/
	FOnGrappleInterrupted OnGrappleInterrupted;
	UPROPERTY(BlueprintAssignable, Category = "Config|Dispatchers")
	/* Event invoked when the grappling hook is broken (grapple length surpasses the given BreakDistance)
	*/
	FOnGrappleBreaked OnGrappleBreaked;
	UPROPERTY(BlueprintAssignable, Category = "Config|Dispatchers")
	/* Event invoked when the grappling hook is ready to be launched
	*/
	FOnGrappleReady OnGrappleReady;
	UPROPERTY(BlueprintAssignable, Category = "Config|Dispatchers")
	/* Event invoked when the grappling hook has missed all valid targets
	*/
	FOnGrappleMissed OnGrappleMissed;
	UPROPERTY(BlueprintAssignable, Category = "Config|Dispatchers")
	/* Event invoked when the grappling hook has started its cooldown phase
	*/
	FOnGrappleDisabled OnGrappleDisabled;
	/* Multiplier to convert Radians to Degrees
	*/
	static float RadToDeg;
	/* Multiplier to convert Degrees to Radians
	*/
	static float DegToRad;
protected:
	/* Value used as a common UI display min value
	*/
	static float MinTimerValue;

	/* "Owner" of the grappling hook, Launch and Swing mechanics will be used on this
	*/
	ACharacter* Owner;
	/* Cable used by the grappling hook (will be attached to ProjectileHook)
	*/
	UCableComponent* Cable;
	/* The currently grappled object
	*/
	UPrimitiveComponent* GrappledObject;
	/* The Projectile Hook used
	*/
	AProjectileHook* Hook;

	/* Physics constraint used on Owner to simulate the swinging mechanic
	*/
	UPhysicsConstraintComponent* SwingConstraint;
	/* Current amount of SwingingForce to be applied to Owner
	*/
	FVector CurrentSwingingForce;
	/* If true the CurrentSwingingForce will be considered as an acceleration change
	*/
	bool bAccelChange;

	/* Physics handle used to simulate the Pull mechanic
	*/
	UPhysicsHandleComponent* PullHandle;

	/* Audio component used to play sounds
	*/
	UAudioComponent* Audio;

	/* Instigator used in all Report Noise events
	*/
	AActor* NoiseInstigator;

	/* Timer handle used for the grapple cooldown phase
	*/
	FTimerHandle CooldownTimerHandle;
	/* Timer handle used for the ground check after grapple activation
	*/
	FTimerHandle GroundCheckTimerHandle;

	/* Current grapple state
	*/
	EGrapplingHookState CurrentState;
	/* Grapple state before retracting phase commenced, used internally to determine which cooldown to use at the end of retracting phase
	*/
	EGrapplingHookState PreRetractingState;

	/* Current retract timer
	*/
	float RetractTime;

public:	
	UGrapplingHookComponent();
	void OnComponentDestroyed(bool bDestroyingHierarchy) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Grapple|Miss", meta = (ClampMin = 0.f, UIMin = 0.f))
	/* Cooldown time used after a missed grapple
	*/
	float MissedCooldown;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Grapple|Launch", meta = (ClampMin = 0.f, UIMin = 0.f))
	/* Multiplier used to determine the character launch velocity after a valid grapple. It is based on the distance to the hit location
	*/
	float LaunchSpeed;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Grapple|Launch", meta = (ClampMin = 0.f, UIMin = 0.f))
	/* Cooldown time used after a succesfull grapple in Launch mode
	*/
	float LaunchCooldown;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Grapple|Launch")
	/* Percentage used to determine the height at which the character will be launched towards the launch hit location (percentage based on character height). The less the value the higher the character will end up relative to the hit location
	*/
	float OffsetZPercentage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Grapple|Pull", meta = (ClampMin = 0.f, UIMin = 0.f))
	/* If the object hit by the grapplinghook has a mass inferior to this thereshold it gets pulled towards the player
	*/
	float PullMaxObjectMass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Grapple|Pull", meta = (ClampMin = 0.f, UIMin = 0.f))
	/* Multiplier used to determine the pull velocity force
	*/
	float PullObjectInterpolationSpeed;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Grapple|Pull", meta = (ClampMin = 0.f, UIMin = 0.f))
	/* Distance in front of grapple start location where the pulled object will be attracted
	* @note This should be less than PullDistanceInterrupt in most cases
	*/
	float PullDistanceTollerance;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Grapple|Pull", meta = (ClampMin = 0.f, UIMin = 0.f))
	/* Distance under which the grapple pull will be interrupted
	* @note This should be more than PullDistanceTollerance in most cases
	*/
	float PullDistanceInterrupt;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Grapple|Pull", meta = (ClampMin = 0.f, UIMin = 0.f))
	/* When the object pull is interrupted the pulled object's velocity will be clamped to this max value
	*/
	float PullMaxInterruptVelocity;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Grapple|Pull", meta = (ClampMin = 0.f, UIMin = 0.f))
	/* Cooldown time used after a succesfull grapple in Pull mode
	*/
	float PullCooldown;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Grapple|Swing", meta = (ClampMin = 0.f, UIMin = 0.f))
	/* Cooldown time used after a succesfull grapple in Swing mode
	*/
	float SwingCooldown;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Grapple|Swing", meta = (ClampMin = 0.f, UIMin = 0.f))
	/* Multiplier applied to forces by AddSwingingForce
	*/
	float SwingingStrength;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Grapple|Swing", meta = (ClampMin = 0.f, ClampMax = 180.f, UIMin = 0.f, UIMax = 180.f))
	/* Value used to determine how much deviation in angle from SwingSurfaceNormal is permitted.
	*/
	float SwingSurfaceDegreesTollerance;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Grapple|Swing")
	/* Surface normal used to determine if the hit object is a swingable surface
	*/
	FVector SwingSurfaceNormal;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Grapple|Retract", meta = (ClampMin = 0.f, UIMin = 0.f))
	/* Total duration for full grapple Retract effect
	*/
	float RetractDuration;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Grapple|Retract", meta = (ClampMin = 1.f, UIMin = 1.f))
	/* Threshold for grapple length. When grapple reaches a length less than this value the retract phase will be considered over
	*/
	float RetractDistanceTollerance;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Stats", meta = (ClampMin = 0.f, UIMin = 0.f))
	/* Delay after grapple activation after which if the character is launching and is grounded the grapple will be interrupted
	*/
	float GroundedCheckDelay;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Stats")
	/* Projectile hook class used
	*/
	TSubclassOf<AProjectileHook> HookClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Stats", meta = (Bitmask, BitmaskEnum = "EGrapplingHookActivation"))
	/* Mask that represent all the enabled features in the grappling hook
	*/
	uint8 Activation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Detection")
	/* List of trace types that will invalidate the grapple mechanic if hit
	*/
	TArray<TEnumAsByte<ECollisionChannel>> BlockingObjects;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Stats")
	/* Distance after which the grapple will automatically disjoint
	*/
	float BreakDistance;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Audio")
	/* Sound played when grapple is launched
	*/
	USoundBase* ActivatedSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Audio")
	/* Sound played when grapple is Ready for launch
	*/
	USoundBase* ReadySound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Audio")
	/* Sound played when grapple is interrupted
	*/
	USoundBase* InterruptedSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Noise")
	/* If true noise events will be reported when main events happen in the grappling hook. If false no noise is done
	*/
	bool bPerformNoise;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Noise")
	/* Noise report event loudness, if max range is not zero this modifies the max range, otherwise this modifies the squared distance of the sensor's range
	*/
	float Loudness;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Noise")
	/* Noise report event MaxRange. Max range at which noise can be heard. If negative range is infinite (still limited by listener's hearing range)
	*/
	float MaxRange;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Noise")
	/* Tag associated with noise events
	*/
	FName NoiseTag;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Initialization")
	/* if true grappling hook will attempt to initialize its core components at begin play (Character owner and Cable)
	*/
	bool bInitializeCoreOnBeginPlay;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Initialization")
	/* if true grappling hook will attempt to initialize its non core components at begin play (Audio component, Physics Constraint and Physics Handle)
	*/
	bool bInitializeNonCoreOnBeginPlay;
protected:
	virtual void BeginPlay() override;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Config|Grapple")
	/* Detaches the grappled object (if any)
	*/
	void DetachGrappledObject();
	UFUNCTION(BlueprintCallable, Category = "Config|Flags")
	/* Adds the given Flags to the current Activation flag, which determines which features are enabled
	*/
	void AddActivationUFlag(const uint8 InFlags);
	UFUNCTION(BlueprintCallable, Category = "Config|Flags")
	/* Adds the given Flags to the current Activation flag, which determines which features are enabled
	*/
	void AddActivationFlag(const EGrapplingHookActivation InFlags);
	UFUNCTION(BlueprintCallable, Category = "Config|Flags")
	/* Removes the given Flags to the current Activation flag, which determines which features are enabled
	*/
	void RemoveActivationUFlag(const uint8 InFlags);
	UFUNCTION(BlueprintCallable, Category = "Config|Flags")
	/* Removes the given Flags to the current Activation flag, which determines which features are enabled
	*/
	void RemoveActivationFlag(const EGrapplingHookActivation InFlags);
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Config|Flags")
	/* Returns the current Activation flag, which determines which features are enabled
	*/
	EGrapplingHookActivation GetActivationFlag() const;
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Config|Flags")
	/* Returns the current Activation flag, which determines which features are enabled
	*/
	uint8 GetActivationUFlag() const;
	UFUNCTION(BlueprintCallable, Category = "Config|Flags")
	/* Sets the current Activation flag, which determines which features are enabled
	*/
	void SetActivationUFlag(const uint8 InFlags);
	UFUNCTION(BlueprintCallable, Category = "Config|Flags")
	/* Sets the current Activation flag, which determines which features are enabled
	*/
	void SetActivationFlag(const EGrapplingHookActivation InFlags);
	UFUNCTION(BlueprintCallable, Category = "Config|Initialization")
	/* Sets the core components of the grappling hook (minimum requirement to function)
	*@param InOwner Owner of the grappling hook (all features will be based on it)
	*@param InCable Cable component attached to hook projectile when grapple is active
	*/
	void Initialize(ACharacter* const InOwner, UCableComponent* const InCable);
	UFUNCTION(BlueprintCallable, Category = "Config|Grapple|Swing")
	/* Accumulates force amount to be applied to the owner when Swinging (Applied every tick, then resetted)
	*@param Force Force to be added to internal accumulator
	*@param bInAccelChange If true the accumulated swinging force from now on will be considered as an acceleration change
	*/
	void AddSwingingForce(const FVector& Force, const bool bInAccelChange);
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Config|Debug")
	/* Returns the error message for the given error
	*@param Error The given error to get info about
	*@return Error message
	*/
	FString GetErrorInfo(const EGrapplingHookError Error) const;
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Config|Stats")
	/*
	* Calculate the velocity to be applied to the owner when in Launch mode
	*@param DeltaTime the current deltatime
	*@param Speed Speed value
	*@param TargetLocation Target location to reach
	*@param StartLocation Location to start from
	*@return The Launch velocity
	*/
	FVector GetOwnerLaunchVelocity(const float DeltaTime, const float Speed, const FVector& TargetLocation, const FVector& StartLocation) const;
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Config|Grapple")
	/* Returns the current hook object
	*/
	AProjectileHook* GetHook() const;
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Config|Grapple")
	/* Returns the current grappled object
	*/
	UPrimitiveComponent* GetGrappledObject() const;
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Config|Grapple|Pull")
	/* Returns the current Pull Handle component used by pull feature
	*/
	UPhysicsHandleComponent* GetPullHandleComponent() const;
	UFUNCTION(BlueprintCallable, Category = "Config|Grapple|Pull")
	/* Sets the current Pull Handle component used by pull feature
	*/
	void SetPullHandleComponent(UPhysicsHandleComponent* const InHandle);
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Config|Grapple|Launch")
	/* Returns the current constraint component used by swing mechanic
	*/
	UPhysicsConstraintComponent* GetSwingConstraintComponent() const;
	UFUNCTION(BlueprintCallable, Category = "Config|Grapple|Launch")
	/* Sets the current constraint component used by swing mechanic
	*/
	void SetSwingConstraintComponent(UPhysicsConstraintComponent* const InSwingConstraint);
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Config|Audio")
	/* Returns the current audio component used to play sounds
	*/
	UAudioComponent* GetAudioComponent() const;
	UFUNCTION(BlueprintCallable, Category = "Config|Audio")
	/* Sets the current audio component used to play sounds
	*/
	void SetAudioComponent(UAudioComponent* const InAudio);
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Config|Noise")
	/* Returns the current noise instigator
	*/
	AActor* GetNoiseInstigator() const;
	UFUNCTION(BlueprintCallable, Category = "Config|Noise")
	/* Sets the current noise instigator
	*/
	void SetNoiseInstigator(AActor* const Instigator);
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Config|Stats")
	/* Returns the grapple (cable) start world location
	*/
	FVector GetGrappleStartLocation(bool& bOutValid) const;
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Config|Stats")
	/* Returns the grapple (cable) end world location
	*/
	FVector GetGrappleEndLocation(bool& bOutValid) const;
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Config|Stats")
	/* Returns the grapple (cable) end world location with the given OffsetZPercentage added (used in Launch mode)
	*/
	FVector GetGrappleEndLocationWithLaunchOffset(bool& bOutValid) const;
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Config|Stats")
	/* Returns true if the grappling hook is currently active (neither ready nor disabled)
	*/
	bool IsGrappleActive() const;
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Config|Stats")
	/* Returns the current grappling status
	*/
	EGrapplingHookState GetCurrentState() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Config|Stats")
	/* Returns the current grappling hook length (it may be very different from CableComponent Length)
	*/
	float GetGrappleLength(bool& bOutValid) const;
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Config|Stats")
	/* Returns the current cooldown timer info
	 *@param OutTimeLeft the amount of seconds left to the timer
	 *@param OutTimeElapsed the amount of seconds passed since timer creation
	 *@return true if timer is valid
	*/
	bool GetCooldownTimerInfo(float& OutTimeLeft, float& OutTimeElapsed) const;
	UFUNCTION(BlueprintCallable, Category = "Config|Sound")
	/* Performs a sound if possible as StartGrappleLocation and reports a noise event if enabled
	 *@param Sound  Sound to perform
	*/
	void PlaySound(USoundBase* const Sound);
	UFUNCTION(BlueprintCallable, Category = "Config|Grapple")
	/* Processes the collision and decides which is the most appropriate new state for the grappling hook based on hit data
	 *@param HitNormal Hit surface normal
	 *@param UPrimitiveComponent Hit component
	*/
	void HookLanded(const FVector HitNormal, UPrimitiveComponent* const HitComponent);
	UFUNCTION(BlueprintCallable, Category = "Config|Inputs")
	/* Launches the grapple by activating the extending phase if possible
	*/
	void LaunchGrapple();
	UFUNCTION(BlueprintCallable, Category = "Config|Inputs")
	/* Interrupts the grapple by activating the retracting phase if necessary
	*/
	void StopGrapple();
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Config|Grapple|Launch")
	/* Returns true if grapple is in launch mode and owner is not grounded
	*/
	bool IsOwnerLaunchingMidair() const;
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Config|Grapple|Pull")
	/* Returns true if grappled object is a valid pullable object
	*/
	bool IsGrappledObjectPullable() const;
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Config|Grapple|Swing")
	/* Returns the current swinging force
	 *@param bOutAccelChange True if the swinging force is used as a change of acceleration
	 *@return Swinging force
	*/
	FVector GetCurrentSwingingForce(bool& bOutAccelChange) const;
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Config|Grapple|Swing")
	/* Returns true if the surface with the given normal is valid for the swing mechanic
	 *@param SurfaceNormal Hit surface normal
	 *@return True if the given normal represent a swingable surface
	*/
	bool IsSurfaceSwingable(const FVector& SurfaceNormal) const;
	UFUNCTION(BlueprintCallable, Category = "Config|Aiming")
	/* Utility function to determine if an ipotetic immediate grapple usage may result in a valid hit with the usage of a Linetrace
	 *return True if an object was hit
	 *@param bPossibleValidHit True if ipotetic grapple usage may result in a valid hit
	 *@param OutHit Hit result
	 *@param StartLocation Linetrace start location
	 *@param Direction Linetrace direction
	 *@param MaxDistance Linetrace max distance
	 *@param bTraceComplex Whetever Linetrace should track complex collisions
	*/
	virtual bool IsAimingHitValid(const FVector& StartLocation, const FVector& Direction, const float MaxDistance, const bool bTraceComplex, FHitResult& OutHit, bool& bPossibleValidHit) const;
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Config|Flags")
	/* Returns true if the given Flag is present amongst the given Flags
	 *@param Flags Collection of Flags to test
	 *@param Flag Single flag to check
	 *@return True if flag is Setted
	*/
	bool IsFlagSet(const EGrapplingHookActivation Flags, const EGrapplingHookActivation Flag) const;
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Config|Flags")
	/* Returns true if the given Flag is not present amongst the given Flags
	*@param Flags Collection of Flags to test
	*@param Flag Single flag to check
	*@return True if flag is not Setted
	*/
	bool IsFlagNotSet(const EGrapplingHookActivation Flags, const EGrapplingHookActivation Flag) const;
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Config|Flags")
	/* Returns true if the given Flag is present amongst the given Flags
	 *@param Flags Collection of Flags to test
	 *@param Flag Single flag to check
	 *@return True if flag is Setted
	*/
	bool IsUFlagSet(const uint8 Flags, const EGrapplingHookActivation Flag) const;
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Config|Flags")
	/* Returns true if the given Flag is not present amongst the given Flags
	*@param Flags Collection of Flags to test
	*@param Flag Single flag to check
	*@return True if flag is not Setted
	*/
	bool IsUFlagNotSet(const uint8 Flags, const EGrapplingHookActivation Flag) const;
	UFUNCTION(BlueprintCallable, Category = "Config|Flags")
	/* Returns the sum of the given flags
	*@param First Collection of Flags to sum
	*@param Second Collection of Flags to sum
	*@return The resulting Flag sum
	*/
	EGrapplingHookActivation SumFlags(const EGrapplingHookActivation First, const EGrapplingHookActivation Second) const;
	UFUNCTION(BlueprintCallable, Category = "Config|Flags")
	/* Returns the sum of the given flags
	*@param First Collection of Flags to sum
	*@param Second Collection of Flags to sum
	*@return The resulting Flag sum
	*/
	uint8 SumUFlags(const uint8 First, const uint8 Second) const;
	UFUNCTION(BlueprintCallable, Category = "Config|Flags")
	/* Returns the first flags minus the second flags
	*@param First Collection of Flags to diff
	*@param Second Collection of Flags to diff
	*@return The resulting Flag diff
	*/
	EGrapplingHookActivation DiffFlags(const EGrapplingHookActivation First, const EGrapplingHookActivation Second) const;
	UFUNCTION(BlueprintCallable, Category = "Config|Flags")
	/*  Returns the first flags minus the second flags
	*@param First Collection of Flags to diff
	*@param Second Collection of Flags to diff
	*@return The resulting Flag diff
	*/
	uint8 DiffUFlags(const uint8 First, const uint8 Second) const;
protected:
	/* Sends the grappling hook into disabled state and starts the cooldown. If GA_Cooldown feature is not enabled the grapple will go directly to Ready
	*@param Cooldown  Time to wait before grapple is GS_Ready
	*/
	void RestartCooldown(const float Cooldown);
	/* Sets the given states as the current state. Invokes the state changed event if necessary
	*@param NewState New state to be used
	*/
	void SetCurrentState(const EGrapplingHookState NewState);

	UFUNCTION()
	/* Sets the grappling hook as Ready to be used
	*/
	void OnEnableGrapple();

	/* Ends retract phase, activating cooldown phase if necessary
	*/
	void EndRetractPhase();
	/* Interrupts the swing phase
	*/
	void InterruptSwing();
	/* Updates the swing phase
	*/
	void UpdateSwing();
	/* Activates the swing phase. Returns True if Swing was successfully activated
	*/
	bool ActivateSwing();
	/* Interrupts the pull phase
	*/
	void InterruptPull();
	/* Activates the pull feature
	*/
	void ActivatePull();
	/* Update the owner position in launch mode
	*/
	void UpdateOwnerLaunch(const float Deltatime);
	/* Update the grapple in retract mode
	*/
	void UpdateRetractGrapple(const float Deltatime);
	/* Update the grappled object for Pull feature, returning True if the grapple should be interrupted
	*/
	bool UpdatePulledObject();
	/* Initializes fields when grapple finished its extending phase
	*/
	UPrimitiveComponent* StartActiveGrapplePhase(UPrimitiveComponent* const InGrappledObject);
	/* Decides which state to set the grappling hook after an Hook collision
	*@param HitNormal Impact world normal
	*@param InGrappledObject Impacted object
	*@param bHit False if hit was not valid
	*/
	void ValutateCollision(const FVector& HitNormal, UPrimitiveComponent* const InGrappledObject, const bool bHit);
	/* Reset component state, invalidating all undergoing logic
	*/
	void ResetComponentState();
	UFUNCTION()
	/* Checks whetever the owner is grounded while Launch/Swing phase is active. If it is the case then the grapple will be interrupted
	*/
	void OnCheckGrounded();
};
