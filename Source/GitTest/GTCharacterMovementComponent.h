// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GTPawnMovementManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GTCharacterMovementComponent.generated.h"

UCLASS()
class GITTEST_API UGTCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UGTCharacterMovementComponent();
	virtual void PerformMovement(float DeltaTime) override;
	virtual void PhysNavWalking(float deltaTime, int32 Iterations) override;
	virtual bool MoveUpdatedComponentImpl(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit,
	                                      ETeleportType Teleport) override;
	virtual void ControlledCharacterMove(const FVector& InputVector, float DeltaSeconds) override;
	virtual void ApplyRootMotionToVelocity(float deltaTime) override;
	virtual void CallMovementUpdateDelegate(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void SetUpdatedComponent(USceneComponent* NewUpdatedComponent) override;
	/** changes physics based on MovementMode */
	virtual void StartNewPhysics(float deltaTime, int32 Iterations) override;

	void UpdateMovement(float DeltaTime);
	/** Special Tick to allow custom server-side functionality on Autonomous Proxies. 
	 * Called for all remote APs, including APs controlled on Listen Servers such as the hosting player's Character.
	 * If full server-side control is desired, you may need to override ControlledCharacterMove as well.
	 */
	virtual void ServerAutonomousProxyTick(float DeltaSeconds) { }

protected:
	
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	AGTPawnMovementManager* PawnMovementManager;

};
