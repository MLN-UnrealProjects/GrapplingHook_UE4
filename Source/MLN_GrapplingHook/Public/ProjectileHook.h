// Copyright 2019 Matteo Lorenzo Nasci

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProjectileHook.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHookStopped, FVector, HitNormal, UPrimitiveComponent*, HitComponent);

class UProjectileMovementComponent;
class UCableComponent;
class USphereComponent;
UCLASS(BlueprintType, Blueprintable, ClassGroup = (Grapple))
/*
* Actor that functions as the grapple hook object (Responsible for collision detection)
*/
class MLN_GRAPPLINGHOOK_API AProjectileHook : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AProjectileHook();

	UPROPERTY(BlueprintAssignable, Category = "Config|Dispatchers")
	/* Event invoked when the Hook hit a valid object
	*/
	FOnHookStopped OnHookStopped;

	float MaxDistance;

	UPROPERTY(BlueprintReadOnly, meta = (ExposeOnSpawn = true), Category = "Config")
	UCableComponent* Cable;
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Config")
	USphereComponent* CollisionComponent;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Config")
	UProjectileMovementComponent* ProjectileMovement;

public:
	virtual void Tick(float DeltaSeconds) override;
	/* Releases the actor from the attached grappled object
	*/
	virtual void ReleaseContrainedBody();
	/* Detaches previous cable and attaches new cable to this actor, activating its components
	*/
	virtual void StartSimulation(UCableComponent* const InCable);
	/* Manually interrupts the projectile movement, optionally launching the OnHookStopped event
	*/
	virtual void InterruptProjectileMovement(const bool bLaunchStoppedEvent = false);
	virtual void Destroyed() override;
protected:
	UFUNCTION()
	/* Function binded to ProjectileMovementComponent OnProjectileStop
	*/
	virtual void OnStopped(const FHitResult& Hit);
};
