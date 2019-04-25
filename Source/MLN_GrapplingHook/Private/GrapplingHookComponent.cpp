// Fill out your copyright notice in the Description page of Project Settings.

#include "GrapplingHookComponent.h"
#include "ProjectileHook.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Perception/AISense_Hearing.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "../Plugins/Runtime/CableComponent/Source/CableComponent/Classes/CableComponent.h"
#include "Kismet/KismetMathLibrary.h"

float UGrapplingHookComponent::RadToDeg = 180.f / PI;
float UGrapplingHookComponent::DegToRad = PI / 180.f;
float UGrapplingHookComponent::MinTimerValue = 0.f;

UGrapplingHookComponent::UGrapplingHookComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bHighPriority = false;
	PrimaryComponentTick.bRunOnAnyThread = false;

	PrimaryComponentTick.TickGroup = ETickingGroup::TG_PrePhysics;

	CooldownTimerHandle.Invalidate();
	GroundCheckTimerHandle.Invalidate();

	bActivatedSwing = false;
	bInitializeCoreOnBeginPlay = true;
	bInitializeNonCoreOnBeginPlay = true;

	MissedCooldown = UGrapplingHookComponent::MinTimerValue;

	LaunchSpeed = 250.f;
	LaunchCooldown = 2.f;
	OffsetZPercentage = 0.f;

	PullMaxObjectMass = 50.f;
	PullObjectInterpolationSpeed = 5.0f;
	PullDistanceTollerance = 125.f;
	PullDistanceInterrupt = 150.f;
	PullMaxInterruptVelocity = 50.f;
	PullCooldown = 2.f;

	SwingingStrength = 100.f;
	SwingSurfaceNormal = -FVector::UpVector;
	SwingSurfaceDegreesTollerance = 60.01f;
	SwingCooldown = UGrapplingHookComponent::MinTimerValue;

	RetractDistanceTollerance = 100.f;
	RetractDuration = 0.5f;
	CurrentRetractDuration = RetractDuration;

	GroundedCheckDelay = 0.15f;
	HookClass = AProjectileHook::StaticClass();
	Activation = static_cast<uint8>(EGrapplingHookActivation::GA_All);

	BreakDistance = 5000.f;
	BlockingObjects.Add(ECollisionChannel::ECC_Pawn);

	ActivatedSound = nullptr;
	InterruptedSound = nullptr;
	ReadySound = nullptr;

	bPerformNoise = false;
	Loudness = 1.f;
	MaxRange = 0.f;
	NoiseTag = NAME_None;

	Owner = nullptr;
	Cable = nullptr;
	GrappledObject = nullptr;
	Hook = nullptr;

	SwingConstraint = nullptr;
	CurrentSwingingForce = FVector::ZeroVector;
	bAccelChange = true;

	PullHandle = nullptr;
	Audio = nullptr;
	NoiseInstigator = nullptr;

	SetCurrentState(EGrapplingHookState::GS_Ready);
	PreRetractingState = CurrentState;

	RetractTime = 0.f;
}
EGrapplingHookActivation UGrapplingHookComponent::GetActivationFlag() const
{
	return static_cast<EGrapplingHookActivation>(Activation);
}
uint8 UGrapplingHookComponent::GetActivationUFlag() const
{
	return Activation;
}
void UGrapplingHookComponent::SetActivationUFlag(const uint8 InFlags)
{
	Activation = InFlags;
}
void UGrapplingHookComponent::SetActivationFlag(const EGrapplingHookActivation InFlags)
{
	Activation = static_cast<uint8>(InFlags);
}
FVector UGrapplingHookComponent::GetOwnerLaunchVelocity(const float DeltaTime, const float Speed, const FVector& TargetLocation, const FVector& StartLocation) const
{
	return (TargetLocation - StartLocation) * DeltaTime * Speed;
}
FVector UGrapplingHookComponent::GetGrappleStartLocation(bool& bOutValid) const
{
	if (Cable)
	{
		bOutValid = true;
		return Cable->GetComponentLocation();
	}
	bOutValid = false;
	return FVector::ZeroVector;
}
FVector UGrapplingHookComponent::GetGrappleEndLocation(bool& bOutValid) const
{
	if (Cable)
	{
		const USceneComponent* const Attached = Cable->GetAttachedComponent();
		if (Attached)
		{
			bOutValid = true;
			return Attached->GetComponentTransform().TransformPosition(Cable->EndLocation);
		}
	}
	bOutValid = false;
	return FVector::ZeroVector;
}
AActor* UGrapplingHookComponent::GetNoiseInstigator() const
{
	return NoiseInstigator;
}
void UGrapplingHookComponent::SetNoiseInstigator(AActor* const Instigator)
{
	NoiseInstigator = Instigator;
}
UAudioComponent* UGrapplingHookComponent::GetAudioComponent() const
{
	return Audio;
}
void UGrapplingHookComponent::SetAudioComponent(UAudioComponent* const InAudio)
{
	Audio = InAudio;
}
bool UGrapplingHookComponent::ActivateSwing()
{
	if (!Owner || !SwingConstraint)
	{
		OnGrappleError.Broadcast(EGrapplingHookError::GE_SwingActivationCore);
		return false;
	}

	UCharacterMovementComponent* const MoveComponent = Owner->GetCharacterMovement();
	if (!MoveComponent)
	{
		OnGrappleError.Broadcast(EGrapplingHookError::GE_SwingActivationCore);
		return false;
	}

	UCapsuleComponent* const Capsule = Owner->GetCapsuleComponent();
	if (!Capsule)
	{
		OnGrappleError.Broadcast(EGrapplingHookError::GE_SwingActivationCore);
		return false;
	}
	//Activate already executed
	if (bActivatedSwing)
	{
		return true;
	}

	bActivatedSwing = true;

	Capsule->SetSimulatePhysics(true);
	MoveComponent->SetActive(false);

	bool bValid = true;
	SwingConstraint->SetWorldLocation(GetGrappleEndLocation(bValid), false, (FHitResult*)nullptr, ETeleportType::TeleportPhysics);
	SwingConstraint->SetConstrainedComponents(GrappledObject, NAME_None, Capsule, NAME_None);
	SwingConstraint->UpdateConstraintFrames();

	if (!bValid)
	{
		OnGrappleError.Broadcast(EGrapplingHookError::GE_SwingActivationCore);
	}

	const float GrappleLength = GetGrappleLength(bValid);
	if (!bValid)
	{
		OnGrappleError.Broadcast(EGrapplingHookError::GE_SwingActivationCore);
	}
	SwingConstraint->SetLinearXLimit(ELinearConstraintMotion::LCM_Locked, GrappleLength);
	SwingConstraint->SetLinearYLimit(ELinearConstraintMotion::LCM_Locked, GrappleLength);
	SwingConstraint->SetLinearZLimit(ELinearConstraintMotion::LCM_Locked, GrappleLength);

	Owner->bUseControllerRotationYaw = false;

	return true;
}
FVector UGrapplingHookComponent::GetCurrentSwingingForce(bool& bOutAccelChange) const
{
	bOutAccelChange = this->bAccelChange;
	return CurrentSwingingForce;
}
FVector UGrapplingHookComponent::GetGrappleEndLocationWithLaunchOffset(bool& bOutValid) const
{
	FVector EndLocation = GetGrappleEndLocation(bOutValid);
	if (Owner)
	{
		const UCapsuleComponent* const Capsule = Owner->GetCapsuleComponent();
		if (Capsule)
		{
			EndLocation.Z += Capsule->GetScaledCapsuleHalfHeight() * 2.f * OffsetZPercentage;
		}
	}
	return EndLocation;
}
bool UGrapplingHookComponent::IsSurfaceSwingable(const FVector& SurfaceNormal) const
{
	return (FMath::Acos(FVector::DotProduct(SwingSurfaceNormal.GetSafeNormal(), SurfaceNormal)) * UGrapplingHookComponent::RadToDeg) <= SwingSurfaceDegreesTollerance;
}
void UGrapplingHookComponent::UpdateSwing()
{
	if (!Owner)
	{
		OnGrappleError.Broadcast(EGrapplingHookError::GE_SwingUpdateCore);
		return;
	}
	if (CurrentState != EGrapplingHookState::GS_Swing)
	{
		return;
	}

	UCapsuleComponent* const Capsule = Owner->GetCapsuleComponent();

	if (Capsule)
	{
		bool bAccelerationChange;
		const FVector Force = GetCurrentSwingingForce(bAccelerationChange);
		Capsule->AddForce(Force, NAME_None, bAccelerationChange);
	}
	else
	{
		OnGrappleError.Broadcast(EGrapplingHookError::GE_SwingUpdateCore);
	}

	CurrentSwingingForce = FVector::ZeroVector;
}
void UGrapplingHookComponent::UpdateRetractGrapple(const float Deltatime)
{
	RetractTime += Deltatime;
	bool RetractOver = true;
	if (Hook)
	{
		bool bValid = true;
		const FVector StartLocation = GetGrappleStartLocation(bValid);
		if (!bValid)
		{
			OnGrappleError.Broadcast(EGrapplingHookError::GE_RetractUpdateCore);
		}

		const FVector HookLocation = Hook->GetActorLocation();
		const float Alpha = FMath::Clamp((CurrentRetractDuration == 0.f ? 1.f : RetractTime / CurrentRetractDuration), 0.f, 1.f);

		const FVector NewLocation = UKismetMathLibrary::VEase(RetractStartLocation, StartLocation, Alpha, EEasingFunc::Type::Linear);
		const FRotator NewRotation = UKismetMathLibrary::FindLookAtRotation(StartLocation, HookLocation);
		Hook->SetActorLocationAndRotation(NewLocation, NewRotation, false, (FHitResult*)nullptr, ETeleportType::TeleportPhysics);

		RetractOver = GetGrappleLength(bValid) <= RetractDistanceTollerance;
		if (!bValid)
		{
			OnGrappleError.Broadcast(EGrapplingHookError::GE_RetractUpdateCore);
		}
	}
	else
	{
		OnGrappleError.Broadcast(EGrapplingHookError::GE_RetractUpdateCore);
	}

	if (RetractOver)
	{
		EndRetractPhase();
	}
}
bool UGrapplingHookComponent::GetCooldownTimerInfo(float& OutTimeLeft, float& OutTimeElapsed) const
{
	const UWorld* const World = GetWorld();
	if (World)
	{
		FTimerManager& Manager = World->GetTimerManager();
		OutTimeLeft = Manager.GetTimerRemaining(CooldownTimerHandle);
		OutTimeElapsed = Manager.GetTimerElapsed(CooldownTimerHandle);
		return Manager.TimerExists(CooldownTimerHandle);
	}
	return false;
}
void UGrapplingHookComponent::BeginPlay()
{
	Super::BeginPlay();
	if (bInitializeCoreOnBeginPlay || bInitializeNonCoreOnBeginPlay)
	{
		AActor* const ActorOwner = GetOwner();
		UCableComponent* InCable = nullptr;
		ACharacter* InCharacter = nullptr;
		UPhysicsHandleComponent* InHandle = nullptr;
		UPhysicsConstraintComponent* InConstraint = nullptr;
		UAudioComponent* InAudio = nullptr;

		if (ActorOwner)
		{
			InCharacter = Cast<ACharacter>(ActorOwner);
			UActorComponent* const ComponentCable = ActorOwner->GetComponentByClass(UCableComponent::StaticClass());
			UActorComponent* const ComponentAudio = ActorOwner->GetComponentByClass(UAudioComponent::StaticClass());
			UActorComponent* const ComponentHandle = ActorOwner->GetComponentByClass(UPhysicsHandleComponent::StaticClass());
			UActorComponent* const ComponentConstraint = ActorOwner->GetComponentByClass(UPhysicsConstraintComponent::StaticClass());
			if (ComponentCable)
			{
				InCable = Cast<UCableComponent>(ComponentCable);
			}
			if (ComponentAudio)
			{
				InAudio = Cast<UAudioComponent>(ComponentAudio);
			}
			if (ComponentHandle)
			{
				InHandle = Cast<UPhysicsHandleComponent>(ComponentHandle);
			}
			if (ComponentConstraint)
			{
				InConstraint = Cast<UPhysicsConstraintComponent>(ComponentConstraint);
			}
		}

		if (bInitializeCoreOnBeginPlay)
		{
			Initialize(InCharacter, InCable);
		}
		if (bInitializeNonCoreOnBeginPlay)
		{
			SetSwingConstraintComponent(InConstraint);
			SetPullHandleComponent(InHandle);
			SetAudioComponent(InAudio);
			SetNoiseInstigator(ActorOwner);
		}
	}
}
bool UGrapplingHookComponent::IsGrappleActive() const
{
	return CurrentState != EGrapplingHookState::GS_Disabled && CurrentState != EGrapplingHookState::GS_Ready;
}
EGrapplingHookState UGrapplingHookComponent::GetCurrentState() const
{
	return CurrentState;
}
void UGrapplingHookComponent::LaunchGrapple()
{
	if (CurrentState != EGrapplingHookState::GS_Ready)
	{
		return;
	}

	if (!Cable)
	{
		OnGrappleError.Broadcast(EGrapplingHookError::GE_HookSpawnCable);
		return;
	}

	UWorld* const World = GetWorld();
	if (!World)
	{
		OnGrappleError.Broadcast(EGrapplingHookError::GE_HookSpawnWorld);
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Instigator = Owner;
	SpawnParams.Owner = Owner;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* const SpawnedActor = World->SpawnActor(HookClass.Get(), &Cable->GetComponentTransform(), SpawnParams);

	if (!SpawnedActor)
	{
		OnGrappleError.Broadcast(EGrapplingHookError::GE_HookSpawnInstanceActor);
		return;
	}

	AProjectileHook* const SpawnedHook = Cast<AProjectileHook>(SpawnedActor);
	if (!SpawnedHook)
	{
		SpawnedActor->Destroy();
		OnGrappleError.Broadcast(EGrapplingHookError::GE_HookSpawnInstanceProjectile);
		return;
	}

	Hook = SpawnedHook;
	Hook->ReleaseContrainedBody();
	Hook->MaxDistance = BreakDistance;
	Cable->SetVisibility(true, true);
	Hook->StartSimulation(Cable);

	if (IsUFlagNotSet(Activation, EGrapplingHookActivation::GA_Extending))
	{
		FHitResult Hit;
		Hook->AddActorWorldOffset(Cable->GetForwardVector() * BreakDistance, true, &Hit, ETeleportType::TeleportPhysics);
		HookLanded(Hit.ImpactNormal, Hit.Component.Get());
		return;
	}
	SetCurrentState(EGrapplingHookState::GS_Extending);

	PlaySound(ActivatedSound);

	FScriptDelegate Delegate;
	Delegate.BindUFunction(this, TEXT("HookLanded"));
	Hook->OnHookStopped.Add(Delegate);
}
void UGrapplingHookComponent::HookLanded(const FVector HitNormal, UPrimitiveComponent* const HitComponent)
{
	if (Hook)
	{
		FScriptDelegate Delegate;
		Delegate.BindUFunction(this, TEXT("HookLanded"));
		Hook->OnHookStopped.Remove(Delegate);
	}

	ValutateCollision(HitNormal, HitComponent, HitComponent);
}
void UGrapplingHookComponent::ResetComponentState()
{
	StopGrapple();
	EndRetractPhase();
	OnEnableGrapple();
}
UPhysicsHandleComponent* UGrapplingHookComponent::GetPullHandleComponent() const
{
	return PullHandle;
}
void UGrapplingHookComponent::SetPullHandleComponent(UPhysicsHandleComponent* const InHandle)
{
	PullHandle = InHandle;
}
UPhysicsConstraintComponent* UGrapplingHookComponent::GetSwingConstraintComponent() const
{
	return SwingConstraint;
}
void UGrapplingHookComponent::SetSwingConstraintComponent(UPhysicsConstraintComponent* const InSwingConstraint)
{
	SwingConstraint = InSwingConstraint;
}
void UGrapplingHookComponent::Initialize(ACharacter* const InOwner, UCableComponent* const InCable)
{
	ResetComponentState();
	Owner = InOwner;
	Cable = InCable;
}
void UGrapplingHookComponent::AddSwingingForce(const FVector& Force, const bool bInAccelChange)
{
	if (CurrentState == EGrapplingHookState::GS_Swing)
	{
		this->bAccelChange = bInAccelChange;
		CurrentSwingingForce += (Force * SwingingStrength);
	}
}
FString UGrapplingHookComponent::GetErrorInfo(const EGrapplingHookError Error) const
{
	FString Message = TEXT("None");
	switch (Error)
	{
	case EGrapplingHookError::GE_HookSpawnCable:
		Message = TEXT("The spawn of the AProjectileHook did not succeed because no Cable reference was set");
		break;
	case EGrapplingHookError::GE_HookSpawnInstanceActor:
		Message = TEXT("The spawn of the AProjectileHook did not succeed");
		break;
	case EGrapplingHookError::GE_HookSpawnInstanceProjectile:
		Message = TEXT("The spawn of the AProjectileHook did not succeed (Failed Cast to AProjectileHook)");
		break;
	case EGrapplingHookError::GE_HookSpawnWorld:
		Message = TEXT("The spawn of the AProjectileHook did not succeed because no World was found");
		break;
	case EGrapplingHookError::GE_LaunchUpdateCore:
		Message = TEXT("The Launch update failed due to missing core elements (Owner, Cable)");
		break;
	case EGrapplingHookError::GE_PullActivationCore:
		Message = TEXT("The Pull Activation failed due to missing core elements (PullHandle, Cable)");
		break;
	case EGrapplingHookError::GE_PullUpdateCore:
		Message = TEXT("The Pull update failed due to missing core elements (PullHandle, Cable)");
		break;
	case EGrapplingHookError::GE_RetractUpdateCore:
		Message = TEXT("The Retract update failed due to missing core elements (Owner, Capsule, Cable)");
		break;
	case EGrapplingHookError::GE_SwingActivationCore:
		Message = TEXT("The Swing activation failed due to missing core elements (Owner, Owner movement component,Capsule, Cable, SwingConstraint)");
		break;
	case EGrapplingHookError::GE_SwingUpdateCore:
		Message = TEXT("The Swing update failed due to missing core elements (Owner, Capsule, Cable)");
		break;
	case EGrapplingHookError::GE_UpdateCore:
		Message = TEXT("The update failed due to missing core elements (Owner, Cable, Hook)");
		break;
	default:
		break;
	}
	return Message;
}
void UGrapplingHookComponent::InterruptPull()
{
	if (PullHandle)
	{
		PullHandle->ReleaseComponent();
	}
	if (GrappledObject)
	{
		const FVector Velocity = GrappledObject->GetPhysicsLinearVelocity();
		GrappledObject->SetPhysicsLinearVelocity(Velocity.GetClampedToMaxSize(PullMaxInterruptVelocity));
	}
	GrappledObject = nullptr;
}
void UGrapplingHookComponent::InterruptSwing()
{
	CurrentSwingingForce = FVector::ZeroVector;
	if (SwingConstraint)
	{
		SwingConstraint->SetConstrainedComponents(nullptr, NAME_None, nullptr, NAME_None);
	}
	if (Owner)
	{
		FVector EndVelocity = Owner->GetVelocity();
		UCapsuleComponent* const Capsule = Owner->GetCapsuleComponent();
		if (Capsule)
		{
			EndVelocity = Capsule->GetPhysicsLinearVelocity();
			Capsule->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
			Capsule->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
			Capsule->SetSimulatePhysics(false);
			Capsule->SetWorldRotation(FRotator(0.f, Capsule->GetComponentRotation().Yaw, 0.f));
		}
		UCharacterMovementComponent* const MoveComponent = Owner->GetCharacterMovement();
		if (MoveComponent)
		{
			MoveComponent->SetActive(true, true);
		}
		Owner->LaunchCharacter(EndVelocity, true, true);
		Owner->bUseControllerRotationYaw = true;
	}
}
UPrimitiveComponent* UGrapplingHookComponent::GetGrappledObject() const
{
	return GrappledObject;
}
AProjectileHook* UGrapplingHookComponent::GetHook() const
{
	return Hook;
}
void UGrapplingHookComponent::StopGrapple()
{
	bActivatedSwing = false;

	if (Hook)
	{
		Hook->InterruptProjectileMovement(true);
		Hook->ReleaseContrainedBody();
	}

	const UWorld* const World = GetWorld();
	if (World)
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(GroundCheckTimerHandle);
	}

	switch (CurrentState)
	{
	case EGrapplingHookState::GS_Launch:
		break;
	case EGrapplingHookState::GS_Pull:
		InterruptPull();
		break;
	case EGrapplingHookState::GS_Swing:
		InterruptSwing();
		break;
	case EGrapplingHookState::GS_Missed:
		break;
	case EGrapplingHookState::GS_Extending:
		HookLanded(FVector::ZeroVector, nullptr);
		break;
	case EGrapplingHookState::GS_Ready:
	case EGrapplingHookState::GS_Disabled:
	case EGrapplingHookState::GS_Retracting:
	default:
		return;
	}

	bool bValid = true;
	RetractTime = 0.f;
	CurrentRetractDuration = BreakDistance == 0.f ? RetractDuration : (RetractDuration * (GetGrappleLength(bValid) / BreakDistance));
	RetractStartLocation = GetGrappleEndLocation(bValid);

	GrappledObject = nullptr;
	PreRetractingState = CurrentState;
	SetCurrentState(EGrapplingHookState::GS_Retracting);
	PlaySound(InterruptedSound);
	OnGrappleInterrupted.Broadcast(CurrentState);
	if (IsUFlagNotSet(Activation, EGrapplingHookActivation::GA_Retracting))
	{
		EndRetractPhase();
	}
}
void UGrapplingHookComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	Super::OnComponentDestroyed(bDestroyingHierarchy);
	ResetComponentState();
}
void UGrapplingHookComponent::EndRetractPhase()
{
	if (Cable)
	{
		Cable->SetVisibility(false, true);
	}
	if (Hook)
	{
		Hook->Destroy();
		Hook = nullptr;
	}

	float Cooldown = UGrapplingHookComponent::MinTimerValue;
	switch (PreRetractingState)
	{
	case EGrapplingHookState::GS_Launch:
		Cooldown = LaunchCooldown;
		break;
	case EGrapplingHookState::GS_Pull:
		Cooldown = PullCooldown;
		break;
	case EGrapplingHookState::GS_Swing:
		Cooldown = SwingCooldown;
		break;
	case EGrapplingHookState::GS_Missed:
		Cooldown = MissedCooldown;
		break;
	case EGrapplingHookState::GS_Ready:
	case EGrapplingHookState::GS_Disabled:
	case EGrapplingHookState::GS_Retracting:
	case EGrapplingHookState::GS_Extending:
	default:
		break;
	}

	RestartCooldown(Cooldown);
}
void UGrapplingHookComponent::DetachGrappledObject()
{
	if (GrappledObject)
	{
		StopGrapple();
	}
}
UPrimitiveComponent* UGrapplingHookComponent::StartActiveGrapplePhase(UPrimitiveComponent* const InGrappledObject)
{
	bActivatedSwing = false;
	this->GrappledObject = InGrappledObject;
	RetractTime = 0.f;
	CurrentRetractDuration = RetractDuration;
	RetractStartLocation = FVector::ZeroVector;

	SetComponentTickEnabled(true);

	return GrappledObject;
}
void UGrapplingHookComponent::ValutateCollision(const FVector& HitNormal, UPrimitiveComponent* const InGrappledObject, const bool bHit)
{
	StartActiveGrapplePhase(InGrappledObject);
	if (!bHit || !GrappledObject || BlockingObjects.Contains(GrappledObject->GetCollisionObjectType()))
	{
		SetCurrentState(EGrapplingHookState::GS_Missed);
		OnGrappleMissed.Broadcast();
		return;
	}
	else
	{
		if (IsUFlagSet(Activation, EGrapplingHookActivation::GA_Pull) && IsGrappledObjectPullable())
		{
			SetCurrentState(EGrapplingHookState::GS_Pull);
		}
		else
		{
			if (IsUFlagSet(Activation, EGrapplingHookActivation::GA_Swing))
			{
				if (IsSurfaceSwingable(HitNormal))
				{
					SetCurrentState(EGrapplingHookState::GS_Swing);
				}
				else
				{
					SetCurrentState(EGrapplingHookState::GS_Missed);
					OnGrappleMissed.Broadcast();
					return;
				}
			}
			else if (IsUFlagSet(Activation, EGrapplingHookActivation::GA_Launch))
			{
				SetCurrentState(EGrapplingHookState::GS_Launch);
			}
			else
			{
				SetCurrentState(EGrapplingHookState::GS_Missed);
				OnGrappleMissed.Broadcast();
				return;
			}
		}
	}
	OnGrappleActivated.Broadcast(CurrentState, GrappledObject);

	const UWorld* const World = GetWorld();
	if (World)
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(GroundCheckTimerHandle);
		TimerManager.SetTimer(GroundCheckTimerHandle, this, &UGrapplingHookComponent::OnCheckGrounded, GroundedCheckDelay, true);
	}
}
void UGrapplingHookComponent::RestartCooldown(const float Cooldown)
{
	SetComponentTickEnabled(false);
	if (IsUFlagSet(Activation, EGrapplingHookActivation::GA_Cooldown) && Cooldown > 0.f)
	{
		OnGrappleDisabled.Broadcast(Cooldown);
		SetCurrentState(EGrapplingHookState::GS_Disabled);
		PreRetractingState = CurrentState;

		const UWorld* const World = GetWorld();
		if (World)
		{
			FTimerManager& Manager = World->GetTimerManager();
			Manager.ClearTimer(CooldownTimerHandle);
			Manager.SetTimer(CooldownTimerHandle, this, &UGrapplingHookComponent::OnEnableGrapple, Cooldown, false);
			return;
		}
	}

	//If either cooldown feature is not active , cooldown time is not valid or a world could not be found skip cooldown and directly enable grapple
	OnEnableGrapple();
}
void UGrapplingHookComponent::OnEnableGrapple()
{
	const UWorld* const World = GetWorld();
	if (World)
	{
		FTimerManager& Manager = World->GetTimerManager();
		Manager.ClearTimer(CooldownTimerHandle);
	}
	if (CurrentState != EGrapplingHookState::GS_Ready)
	{
		PlaySound(ReadySound);
		OnGrappleReady.Broadcast();
		SetCurrentState(EGrapplingHookState::GS_Ready);
	}
}
EGrapplingHookActivation UGrapplingHookComponent::DiffFlags(const EGrapplingHookActivation First, const EGrapplingHookActivation Second) const
{
	return (First & (~Second));
}
uint8 UGrapplingHookComponent::DiffUFlags(const uint8 First, const uint8 Second) const
{
	return (First & (~Second));
}
void UGrapplingHookComponent::AddActivationUFlag(const uint8 InFlags)
{
	Activation = SumUFlags(Activation, InFlags);
}
void UGrapplingHookComponent::AddActivationFlag(const EGrapplingHookActivation InFlags)
{
	Activation = SumUFlags(Activation, static_cast<uint8>(InFlags));
}
void UGrapplingHookComponent::RemoveActivationUFlag(const uint8 InFlags)
{
	Activation = DiffUFlags(Activation, InFlags);
}
void UGrapplingHookComponent::RemoveActivationFlag(const EGrapplingHookActivation InFlags)
{
	Activation = DiffUFlags(Activation, static_cast<uint8>(InFlags));
}

float UGrapplingHookComponent::GetGrappleLength(bool& bOutValid) const
{
	bool bValid = true;
	const FVector End = GetGrappleEndLocation(bValid);
	const FVector Start = GetGrappleStartLocation(bOutValid);
	bOutValid = bOutValid && bValid;
	return FVector::Distance(End, Start);
}
void UGrapplingHookComponent::SetCurrentState(const EGrapplingHookState NewState)
{
	if (NewState != CurrentState)
	{
		const EGrapplingHookState Previous = CurrentState;
		CurrentState = NewState;
		OnGrappleStateChanged.Broadcast(Previous, CurrentState);
	}
}
void UGrapplingHookComponent::PlaySound(USoundBase* const Sound)
{
	if (Audio != nullptr)
	{
		Audio->SetSound(Sound);
		Audio->Play(0.f);
	}
	if (!bPerformNoise)
	{
		return;
	}

	bool bValid = true;
	UAISense_Hearing::ReportNoiseEvent(this, GetGrappleStartLocation(bValid), Loudness, NoiseInstigator, MaxRange, NoiseTag);
}
void UGrapplingHookComponent::UpdateOwnerLaunch(const float Deltatime)
{
	if (Owner)
	{
		bool bValid = true;
		Owner->LaunchCharacter(GetOwnerLaunchVelocity(Deltatime, LaunchSpeed, GetGrappleEndLocationWithLaunchOffset(bValid), Owner->GetActorLocation()), true, true);
		if (!bValid)
		{
			OnGrappleError.Broadcast(EGrapplingHookError::GE_LaunchUpdateCore);
		}
		return;
	}
	OnGrappleError.Broadcast(EGrapplingHookError::GE_LaunchUpdateCore);
}
bool UGrapplingHookComponent::UpdatePulledObject()
{
	if (!PullHandle || !Cable)
	{
		OnGrappleError.Broadcast(EGrapplingHookError::GE_PullUpdateCore);
		return true;
	}
	if (!IsGrappledObjectPullable())
	{
		return true;
	}
	PullHandle->SetInterpolationSpeed(PullObjectInterpolationSpeed);

	bool bValid = true;
	const FVector StartLocation = GetGrappleStartLocation(bValid);
	if (!bValid)
	{
		OnGrappleError.Broadcast(EGrapplingHookError::GE_PullUpdateCore);
	}
	PullHandle->SetTargetLocation(StartLocation + (Cable->GetForwardVector() * PullDistanceTollerance));

	FVector Out;
	return PullDistanceInterrupt >= GrappledObject->GetClosestPointOnCollision(StartLocation, Out);
}
void UGrapplingHookComponent::ActivatePull()
{
	if (!PullHandle)
	{
		OnGrappleError.Broadcast(EGrapplingHookError::GE_PullActivationCore);
		return;
	}
	if (PullHandle->GrabbedComponent != GrappledObject)
	{
		PullHandle->ReleaseComponent();
		bool bValid = true;
		PullHandle->GrabComponentAtLocation(GrappledObject, NAME_None, GetGrappleEndLocation(bValid));
		if (!bValid)
		{
			OnGrappleError.Broadcast(EGrapplingHookError::GE_PullActivationCore);
		}
	}
}
bool UGrapplingHookComponent::IsGrappledObjectPullable() const
{
	if (GrappledObject)
	{
		return GrappledObject->Mobility == EComponentMobility::Type::Movable && GrappledObject->IsSimulatingPhysics() && GrappledObject->GetMass() <= PullMaxObjectMass;
	}
	return false;
}
bool UGrapplingHookComponent::IsOwnerLaunchingMidair() const
{
	if (Owner)
	{
		const UCharacterMovementComponent* const MoveComponent = Owner->GetCharacterMovement();
		if (MoveComponent)
		{
			return CurrentState == EGrapplingHookState::GS_Launch && !MoveComponent->IsMovingOnGround();
		}
	}
	return false;
}
bool UGrapplingHookComponent::IsAimingHitValid(const FVector& StartLocation, const FVector& Direction, const float MaxDistance, const bool bTraceComplex, FHitResult& OutHit, bool& bPossibleValidHit) const
{
	const UWorld* const World = GetWorld();
	if (World)
	{
		FCollisionQueryParams BlockingParams(FCollisionQueryParams::DefaultQueryParam);
		BlockingParams.bTraceComplex = bTraceComplex;
		BlockingParams.AddIgnoredActor(Owner);
		FCollisionObjectQueryParams BlockingQuery;
		for (const ECollisionChannel Item : BlockingObjects)
		{
			BlockingQuery.AddObjectTypesToQuery(Item);
		}

		const bool Hit = World->LineTraceSingleByObjectType(OutHit, StartLocation, StartLocation + (Direction * MaxDistance), BlockingQuery, BlockingParams);
		if (!OutHit.Component.IsValid() || !Hit)
		{
			return false;
		}
		bPossibleValidHit = OutHit.Distance < BreakDistance && (IsUFlagNotSet(Activation, EGrapplingHookActivation::GA_Swing) || IsSurfaceSwingable(OutHit.ImpactNormal));
		return true;
	}
	return false;
}
bool UGrapplingHookComponent::IsUFlagSet(const uint8 Flags, const EGrapplingHookActivation Flag) const
{
	return (static_cast<EGrapplingHookActivation>(Flags) & Flag) != EGrapplingHookActivation::GA_None;
}
bool UGrapplingHookComponent::IsUFlagNotSet(const uint8 Flags, const EGrapplingHookActivation Flag) const
{
	return !IsUFlagSet(Flags, Flag);
}
bool UGrapplingHookComponent::IsFlagSet(const EGrapplingHookActivation Flags, const EGrapplingHookActivation Flag) const
{
	return (Flags & Flag) != EGrapplingHookActivation::GA_None;
}
bool UGrapplingHookComponent::IsFlagNotSet(const EGrapplingHookActivation Flags, const EGrapplingHookActivation Flag) const
{
	return !IsFlagSet(Flags, Flag);
}
EGrapplingHookActivation UGrapplingHookComponent::SumFlags(const EGrapplingHookActivation First, const EGrapplingHookActivation Second) const
{
	return First | Second;
}
uint8 UGrapplingHookComponent::SumUFlags(const uint8 First, const uint8 Second) const
{
	return First | Second;
}
void UGrapplingHookComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Hook || !Owner || !Cable)
	{
		OnGrappleError.Broadcast(EGrapplingHookError::GE_UpdateCore);
		StopGrapple();
	}

	bool bValid = true;
	if (GetGrappleLength(bValid) > BreakDistance)
	{
		StopGrapple();
		OnGrappleBreaked.Broadcast();
	}

	switch (CurrentState)
	{
	case EGrapplingHookState::GS_Launch:
		UpdateOwnerLaunch(DeltaTime);
		break;
	case EGrapplingHookState::GS_Pull:
		ActivatePull();
		if (UpdatePulledObject())
		{
			StopGrapple();
		}
		break;
	case EGrapplingHookState::GS_Swing:
		if (ActivateSwing())
		{
			UpdateSwing();
		}
		else
		{
			StopGrapple();
		}
		break;
	case EGrapplingHookState::GS_Retracting:
		UpdateRetractGrapple(DeltaTime);
		break;
	case EGrapplingHookState::GS_Missed:
	case EGrapplingHookState::GS_Disabled:
	case EGrapplingHookState::GS_Extending:
	case EGrapplingHookState::GS_Ready:
	default:
		StopGrapple();
		break;
	}
}
void UGrapplingHookComponent::OnCheckGrounded()
{
	if (CurrentState == EGrapplingHookState::GS_Swing || CurrentState == EGrapplingHookState::GS_Launch)
	{
		if (Owner)
		{
			const UCharacterMovementComponent* const MoveComponent = Owner->GetCharacterMovement();
			if (MoveComponent)
			{
				if (MoveComponent->IsMovingOnGround())
				{
					StopGrapple();
				}
			}
		}
	}
}
