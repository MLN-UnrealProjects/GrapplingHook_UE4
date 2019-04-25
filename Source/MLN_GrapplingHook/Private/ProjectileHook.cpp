// Fill out your copyright notice in the Description page of Project Settings.

#include "ProjectileHook.h"
#include "../Plugins/Runtime/CableComponent/Source/CableComponent/Classes/CableComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/SphereComponent.h"

AProjectileHook::AProjectileHook()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	Cable = nullptr;
	MaxDistance = -1.f;

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Projectile Movement"));
	if (ProjectileMovement)
	{
		ProjectileMovement->bEditableWhenInherited = true;
		ProjectileMovement->InitialSpeed = 3000.f;
		ProjectileMovement->MaxSpeed = 0.f;
		ProjectileMovement->bRotationFollowsVelocity = true;
		ProjectileMovement->ProjectileGravityScale = 0.f;

		FScriptDelegate Delegate;
		Delegate.BindUFunction(this, TEXT("OnStopped"));
		ProjectileMovement->OnProjectileStop.Add(Delegate);
	}

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	if (CollisionComponent)
	{
		SetRootComponent(CollisionComponent);

		CollisionComponent->SetUseCCD(true);
		CollisionComponent->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
		CollisionComponent->CanCharacterStepUpOn = ECanBeCharacterBase::ECB_No;
		CollisionComponent->InitSphereRadius(10.f);
		CollisionComponent->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
		CollisionComponent->SetCanEverAffectNavigation(false);
		if (ProjectileMovement)
		{
			ProjectileMovement->SetUpdatedComponent(CollisionComponent);
		}
	}
}
void AProjectileHook::Tick(float DeltaSeconds)
{
	if (!Cable || MaxDistance < 0.f)
	{
		return;
	}
	if (FVector::Distance(GetActorLocation(), Cable->GetComponentLocation()) > MaxDistance)
	{
		InterruptProjectileMovement(true);
	}
}
void AProjectileHook::Destroyed()
{
	Super::Destroyed();
	StartSimulation(nullptr);
	InterruptProjectileMovement();
	ReleaseContrainedBody();
}
void AProjectileHook::OnStopped(const FHitResult& Hit)
{
	InterruptProjectileMovement(false);

	OnHookStopped.Broadcast(Hit.ImpactNormal, Hit.Component.Get());

	if (CollisionComponent)
	{
		CollisionComponent->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
	}

	ReleaseContrainedBody();

	FAttachmentTransformRules Rules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, true);
	//AttachToActor(Hit.GetActor(), Rules);
	AttachToComponent(Hit.GetComponent(), Rules);
}
void AProjectileHook::InterruptProjectileMovement(const bool bLaunchStoppedEvent)
{
	SetActorTickEnabled(false);

	if (ProjectileMovement)
	{
		if (!bLaunchStoppedEvent)
		{
			FScriptDelegate Delegate;
			Delegate.BindUFunction(this, TEXT("OnStopped"));
			ProjectileMovement->OnProjectileStop.Remove(Delegate);
		}
		const FHitResult Hit;
		ProjectileMovement->StopSimulating(Hit);
	}
}
void AProjectileHook::StartSimulation(UCableComponent* const InCable)
{
	if (Cable)
	{
		Cable->SetAttachEndTo(nullptr, NAME_None);
	}
	Cable = InCable;
	if (Cable)
	{
		CollisionComponent->SetCollisionEnabled(ECollisionEnabled::Type::QueryAndPhysics);
		Cable->bAttachEnd = true;
		Cable->SetAttachEndTo(this, NAME_None);
		Cable->EndLocation = FVector::ZeroVector;
	}
}
void AProjectileHook::ReleaseContrainedBody()
{
	FDetachmentTransformRules Rules(EDetachmentRule::KeepWorld, EDetachmentRule::KeepWorld, EDetachmentRule::KeepWorld, true);
	RootComponent->DetachFromComponent(Rules);
	//DetachFromActor(Rules);
}

