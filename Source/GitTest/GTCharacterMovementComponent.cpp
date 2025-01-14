// Fill out your copyright notice in the Description page of Project Settings.


#include "GTCharacterMovementComponent.h"

#include "GTPawnMovementManager.h"
#include "AI/Navigation/AvoidanceManager.h"
#include "AI/Navigation/NavigationDataInterface.h"
#include "Components/CapsuleComponent.h"
#include "Engine/NetworkObjectList.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "Kismet/KismetMathLibrary.h"
DECLARE_CYCLE_STAT(TEXT("UGTCharacterMovementComponent PerformMovement"), STAT_UGTCharacterMovementComponent_PerformMovement, STATGROUP_Game);
DECLARE_CYCLE_STAT(TEXT("UGTCharacterMovementComponent StartNewPhysics"), STAT_UGTCharacterMovementComponent_StartNewPhysics, STATGROUP_Game);
DECLARE_CYCLE_STAT(TEXT("UGTCharacterMovementComponent SetUpdatedComponent"), STAT_UGTCharacterMovementComponent_SetUpdatedComponent, STATGROUP_Game);
DECLARE_CYCLE_STAT(TEXT("UGTCharacterMovementComponent PhysNavWalking1"), STAT_UGTCharacterMovementComponent_PhysNavWalking1, STATGROUP_Game);
DECLARE_CYCLE_STAT(TEXT("UGTCharacterMovementComponent PhysNavWalking2"), STAT_UGTCharacterMovementComponent_PhysNavWalking2, STATGROUP_Game);
DECLARE_CYCLE_STAT(TEXT("UGTCharacterMovementComponent PhysNavWalking3"), STAT_UGTCharacterMovementComponent_PhysNavWalking3, STATGROUP_Game);
DECLARE_CYCLE_STAT(TEXT("UGTCharacterMovementComponent PhysNavWalking4"), STAT_UGTCharacterMovementComponent_PhysNavWalking4, STATGROUP_Game);
DECLARE_CYCLE_STAT(TEXT("UGTCharacterMovementComponent PhysNavWalking5"), STAT_UGTCharacterMovementComponent_PhysNavWalking5, STATGROUP_Game);
DECLARE_CYCLE_STAT(TEXT("UGTCharacterMovementComponent PhysNavWalking6"), STAT_UGTCharacterMovementComponent_PhysNavWalking6, STATGROUP_Game);
DECLARE_CYCLE_STAT(TEXT("UGTCharacterMovementComponent PhysNavWalking7"), STAT_UGTCharacterMovementComponent_PhysNavWalking7, STATGROUP_Game);
UGTCharacterMovementComponent::UGTCharacterMovementComponent()
{
	
	PrimaryComponentTick.bCanEverTick = false;
}

void UGTCharacterMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickEnabled(false);

	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGTPawnMovementManager::StaticClass(), Actors);

	if(Actors.IsValidIndex(0)&&(Actors[0]))
	{
		if(AGTPawnMovementManager* MovementManager = Cast<AGTPawnMovementManager>(Actors[0]))
		{
			PawnMovementManager = MovementManager;
			MovementManager->MovementComponents.Add(this);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("The movement manager has not been found") );
	}
}

void UGTCharacterMovementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if(IsValid(PawnMovementManager))
	{
		PawnMovementManager->MovementComponents.Remove(this);
	}
}

void UGTCharacterMovementComponent::PerformMovement(float DeltaSeconds)
{
	const UWorld* MyWorld = GetWorld();
	if (!HasValidData() || MyWorld == nullptr)
	{
		return;
	}
	//SCOPE_CYCLE_COUNTER(STAT_UGTCharacterMovementComponent_PerformMovement);
	bTeleportedSinceLastUpdate = UpdatedComponent->GetComponentLocation() != LastUpdateLocation;
	
	// no movement if we can't move, or if currently doing physical simulation on UpdatedComponent
	if (MovementMode == MOVE_None || UpdatedComponent->Mobility != EComponentMobility::Movable || UpdatedComponent->IsSimulatingPhysics())
	{
		if (!CharacterOwner->bClientUpdating && !CharacterOwner->bServerMoveIgnoreRootMotion)
		{
			// Consume root motion
			if (CharacterOwner->IsPlayingRootMotion() && CharacterOwner->GetMesh())
			{
				TickCharacterPose(DeltaSeconds);
				RootMotionParams.Clear();
			}
			if (CurrentRootMotion.HasActiveRootMotionSources())
			{
				CurrentRootMotion.Clear();
			}
		}
		// Clear pending physics forces
		ClearAccumulatedForces();
		return;
	}

	// Force floor update if we've moved outside of CharacterMovement since last update.
	bForceNextFloorCheck |= (IsMovingOnGround() && bTeleportedSinceLastUpdate);

	// Update saved LastPreAdditiveVelocity with any external changes to character Velocity that happened since last update.
// 	if( CurrentRootMotion.HasAdditiveVelocity() )
// 	{
// 		const FVector Adjustment = (Velocity - LastUpdateVelocity);
// 		CurrentRootMotion.LastPreAdditiveVelocity += Adjustment;
//
// #if ROOT_MOTION_DEBUG
// 		if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnGameThread() == 1)
// 		{
// 			if (!Adjustment.IsNearlyZero())
// 			{
// 				FString AdjustedDebugString = FString::Printf(TEXT("PerformMovement HasAdditiveVelocity LastUpdateVelocityAdjustment LastPreAdditiveVelocity(%s) Adjustment(%s)"),
// 					*CurrentRootMotion.LastPreAdditiveVelocity.ToCompactString(), *Adjustment.ToCompactString());
// 				RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
// 			}
// 		}
// #endif
// 	}

	FVector OldVelocity;
	FVector OldLocation;

	if(UpdatedComponent == GetOwner()->GetRootComponent())
	{
		// Scoped updates can improve performance of multiple MoveComponent calls.
		{
			MaybeUpdateBasedMovement(DeltaSeconds);

			// Clean up invalid RootMotion Sources.
			// This includes RootMotion sources that ended naturally.
			// They might want to perform a clamp on velocity or an override, 
			// so we want this to happen before ApplyAccumulatedForces and HandlePendingLaunch as to not clobber these.
	// 		const bool bHasRootMotionSources = HasRootMotionSources();
	// 		if (bHasRootMotionSources && !CharacterOwner->bClientUpdating && !CharacterOwner->bServerMoveIgnoreRootMotion)
	// 		{
	// 			const FVector VelocityBeforeCleanup = Velocity;
	// 			CurrentRootMotion.CleanUpInvalidRootMotion(DeltaSeconds, *CharacterOwner, *this);
	//
	// #if ROOT_MOTION_DEBUG
	// 			if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnGameThread() == 1)
	// 			{
	// 				if (Velocity != VelocityBeforeCleanup)
	// 				{
	// 					const FVector Adjustment = Velocity - VelocityBeforeCleanup;
	// 					FString AdjustedDebugString = FString::Printf(TEXT("PerformMovement CleanUpInvalidRootMotion Velocity(%s) VelocityBeforeCleanup(%s) Adjustment(%s)"),
	// 						*Velocity.ToCompactString(), *VelocityBeforeCleanup.ToCompactString(), *Adjustment.ToCompactString());
	// 					RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
	// 				}
	// 			}
	// #endif
	// 		}

			OldVelocity = Velocity;
			OldLocation = UpdatedComponent->GetComponentLocation();

			//HB CODE
			ApplyAccumulatedForces(DeltaSeconds);

			// Update the character state before we do our movement
			UpdateCharacterStateBeforeMovement(DeltaSeconds);

			 if (MovementMode == MOVE_NavWalking && bWantsToLeaveNavWalking)
			 {
			 	TryToLeaveNavWalking();
			 }

			// Character::LaunchCharacter() has been deferred until now.
			HandlePendingLaunch();
			ClearAccumulatedForces();

	// #if ROOT_MOTION_DEBUG
	// 		if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnGameThread() == 1)
	// 		{
	// 			if (OldVelocity != Velocity)
	// 			{
	// 				const FVector Adjustment = Velocity - OldVelocity;
	// 				FString AdjustedDebugString = FString::Printf(TEXT("PerformMovement ApplyAccumulatedForces+HandlePendingLaunch Velocity(%s) OldVelocity(%s) Adjustment(%s)"),
	// 					*Velocity.ToCompactString(), *OldVelocity.ToCompactString(), *Adjustment.ToCompactString());
	// 				RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
	// 			}
	// 		}
	// #endif
	//
	// 		// Update saved LastPreAdditiveVelocity with any external changes to character Velocity that happened due to ApplyAccumulatedForces/HandlePendingLaunch
	// 		if( CurrentRootMotion.HasAdditiveVelocity() )
	// 		{
	// 			const FVector Adjustment = (Velocity - OldVelocity);
	// 			CurrentRootMotion.LastPreAdditiveVelocity += Adjustment;
	//
	// #if ROOT_MOTION_DEBUG
	// 			if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnGameThread() == 1)
	// 			{
	// 				if (!Adjustment.IsNearlyZero())
	// 				{
	// 					FString AdjustedDebugString = FString::Printf(TEXT("PerformMovement HasAdditiveVelocity AccumulatedForces LastPreAdditiveVelocity(%s) Adjustment(%s)"),
	// 						*CurrentRootMotion.LastPreAdditiveVelocity.ToCompactString(), *Adjustment.ToCompactString());
	// 					RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
	// 				}
	// 			}
	// #endif
	// 		}

			// // Prepare Root Motion (generate/accumulate from root motion sources to be used later)
			// if (bHasRootMotionSources && !CharacterOwner->bClientUpdating && !CharacterOwner->bServerMoveIgnoreRootMotion)
			// {
			// 	// Animation root motion - If using animation RootMotion, tick animations before running physics.
			// 	if( CharacterOwner->IsPlayingRootMotion() && CharacterOwner->GetMesh() )
			// 	{
			// 		TickCharacterPose(DeltaSeconds);
			//
			// 		// Make sure animation didn't trigger an event that destroyed us
			// 		if (!HasValidData())
			// 		{
			// 			return;
			// 		}
			//
			// 		// For local human clients, save off root motion data so it can be used by movement networking code.
			// 		if( CharacterOwner->IsLocallyControlled() && (CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy) && CharacterOwner->IsPlayingNetworkedRootMotionMontage() )
			// 		{
			// 			CharacterOwner->ClientRootMotionParams = RootMotionParams;
			// 		}
			// 	}
			//
			// 	// Generates root motion to be used this frame from sources other than animation
			// 	{
			// 		CurrentRootMotion.PrepareRootMotion(DeltaSeconds, *CharacterOwner, *this, true);
			// 	}
			//
			// 	// For local human clients, save off root motion data so it can be used by movement networking code.
			// 	if( CharacterOwner->IsLocallyControlled() && (CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy) )
			// 	{
			// 		CharacterOwner->SavedRootMotion = CurrentRootMotion;
			// 	}
			// }

			// Apply Root Motion to Velocity
	// 		if( CurrentRootMotion.HasOverrideVelocity() || HasAnimRootMotion() )
	// 		{
	// 			// Animation root motion overrides Velocity and currently doesn't allow any other root motion sources
	// 			if( HasAnimRootMotion() )
	// 			{
	// 				// Convert to world space (animation root motion is always local)
	// 				USkeletalMeshComponent * SkelMeshComp = CharacterOwner->GetMesh();
	// 				if( SkelMeshComp )
	// 				{
	// 					// Convert Local Space Root Motion to world space. Do it right before used by physics to make sure we use up to date transforms, as translation is relative to rotation.
	// 					RootMotionParams.Set( ConvertLocalRootMotionToWorld(RootMotionParams.GetRootMotionTransform(), DeltaSeconds) );
	// 				}
	//
	// 				// Then turn root motion to velocity to be used by various physics modes.
	// 				if( DeltaSeconds > 0.f )
	// 				{
	// 					AnimRootMotionVelocity = CalcAnimRootMotionVelocity(RootMotionParams.GetRootMotionTransform().GetTranslation(), DeltaSeconds, Velocity);
	// 					Velocity = ConstrainAnimRootMotionVelocity(AnimRootMotionVelocity, Velocity);
	// 					if (IsFalling())
	// 					{
	// 						Velocity += FVector(DecayingFormerBaseVelocity.X, DecayingFormerBaseVelocity.Y, 0.f);
	// 					}
	// 				}
	// 				
	// 				UE_LOG(LogRootMotion, Log,  TEXT("PerformMovement WorldSpaceRootMotion Translation: %s, Rotation: %s, Actor Facing: %s, Velocity: %s")
	// 					, *RootMotionParams.GetRootMotionTransform().GetTranslation().ToCompactString()
	// 					, *RootMotionParams.GetRootMotionTransform().GetRotation().Rotator().ToCompactString()
	// 					, *CharacterOwner->GetActorForwardVector().ToCompactString()
	// 					, *Velocity.ToCompactString()
	// 					);
	// 			}
	// 			else
	// 			{
	// 				// We don't have animation root motion so we apply other sources
	// 				if( DeltaSeconds > 0.f )
	// 				{
	//
	// 					const FVector VelocityBeforeOverride = Velocity;
	// 					FVector NewVelocity = Velocity;
	// 					CurrentRootMotion.AccumulateOverrideRootMotionVelocity(DeltaSeconds, *CharacterOwner, *this, NewVelocity);
	// 					if (IsFalling())
	// 					{
	// 						NewVelocity += CurrentRootMotion.HasOverrideVelocityWithIgnoreZAccumulate() ? FVector(DecayingFormerBaseVelocity.X, DecayingFormerBaseVelocity.Y, 0.f) : DecayingFormerBaseVelocity;
	// 					}
	// 					Velocity = NewVelocity;
	//
	// #if ROOT_MOTION_DEBUG
	// 					if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnGameThread() == 1)
	// 					{
	// 						if (VelocityBeforeOverride != Velocity)
	// 						{
	// 							FString AdjustedDebugString = FString::Printf(TEXT("PerformMovement AccumulateOverrideRootMotionVelocity Velocity(%s) VelocityBeforeOverride(%s)"),
	// 								*Velocity.ToCompactString(), *VelocityBeforeOverride.ToCompactString());
	// 							RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
	// 						}
	// 					}
	// #endif
	// 				}
	// 			}
	// 		}

	// #if ROOT_MOTION_DEBUG
	// 		if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnGameThread() == 1)
	// 		{
	// 			FString AdjustedDebugString = FString::Printf(TEXT("PerformMovement Velocity(%s) OldVelocity(%s)"),
	// 				*Velocity.ToCompactString(), *OldVelocity.ToCompactString());
	// 			RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
	// 		}
	// #endif
			
			// Clear jump input now, to allow movement events to trigger it for next update.
			//CharacterOwner->ClearJumpInput(DeltaSeconds);
			//NumJumpApexAttempts = 0;

			MovementMode = EMovementMode::MOVE_NavWalking;
			// change position
			{
				SCOPE_CYCLE_COUNTER(STAT_UGTCharacterMovementComponent_StartNewPhysics);
				StartNewPhysics(DeltaSeconds, 0);
			}

			if (!HasValidData())
			{
				return;
			}

			// Update character state based on change from movement
			UpdateCharacterStateAfterMovement(DeltaSeconds);

			 if (bAllowPhysicsRotationDuringAnimRootMotion || !HasAnimRootMotion())
			 {
			 	PhysicsRotation(DeltaSeconds);
			 }

			// Apply Root Motion rotation after movement is complete.
	// 		if( HasAnimRootMotion() )
	// 		{
	// 			const FQuat OldActorRotationQuat = UpdatedComponent->GetComponentQuat();
	// 			const FQuat RootMotionRotationQuat = RootMotionParams.GetRootMotionTransform().GetRotation();
	// 			if( !RootMotionRotationQuat.IsIdentity() )
	// 			{
	// 				const FQuat NewActorRotationQuat = RootMotionRotationQuat * OldActorRotationQuat;
	// 				MoveUpdatedComponent(FVector::ZeroVector, NewActorRotationQuat, true);
	// 			}
	//
	// #if !(UE_BUILD_SHIPPING)
	// 			// debug
	// 			if (false)
	// 			{
	// 				const FRotator OldActorRotation = OldActorRotationQuat.Rotator();
	// 				const FVector ResultingLocation = UpdatedComponent->GetComponentLocation();
	// 				const FRotator ResultingRotation = UpdatedComponent->GetComponentRotation();
	//
	// 				// Show current position
	// 				DrawDebugCoordinateSystem(MyWorld, CharacterOwner->GetMesh()->GetComponentLocation() + FVector(0,0,1), ResultingRotation, 50.f, false);
	//
	// 				// Show resulting delta move.
	// 				DrawDebugLine(MyWorld, OldLocation, ResultingLocation, FColor::Red, false, 10.f);
	//
	// 				// Log details.
	// 				UE_LOG(LogRootMotion, Warning,  TEXT("PerformMovement Resulting DeltaMove Translation: %s, Rotation: %s, MovementBase: %s"), //-V595
	// 					*(ResultingLocation - OldLocation).ToCompactString(), *(ResultingRotation - OldActorRotation).GetNormalized().ToCompactString(), *GetNameSafe(CharacterOwner->GetMovementBase()) );
	//
	// 				const FVector RMTranslation = RootMotionParams.GetRootMotionTransform().GetTranslation();
	// 				const FRotator RMRotation = RootMotionParams.GetRootMotionTransform().GetRotation().Rotator();
	// 				UE_LOG(LogRootMotion, Warning,  TEXT("PerformMovement Resulting DeltaError Translation: %s, Rotation: %s"),
	// 					*(ResultingLocation - OldLocation - RMTranslation).ToCompactString(), *(ResultingRotation - OldActorRotation - RMRotation).GetNormalized().ToCompactString() );
	// 			}
	// #endif // !(UE_BUILD_SHIPPING)
	//
	// 			// Root Motion has been used, clear
	// 			RootMotionParams.Clear();
	// 		}
	// 		else if (CurrentRootMotion.HasActiveRootMotionSources())
	// 		{
	// 			FQuat RootMotionRotationQuat;
	// 			if (CharacterOwner && UpdatedComponent && CurrentRootMotion.GetOverrideRootMotionRotation(DeltaSeconds, *CharacterOwner, *this, RootMotionRotationQuat))
	// 			{
	// 				const FQuat OldActorRotationQuat = UpdatedComponent->GetComponentQuat();
	// 				const FQuat NewActorRotationQuat = RootMotionRotationQuat * OldActorRotationQuat;
	// 				MoveUpdatedComponent(FVector::ZeroVector, NewActorRotationQuat, true);
	// 			}
	// 		}

			// consume path following requested velocity
			LastUpdateRequestedVelocity = bHasRequestedVelocity ? RequestedVelocity : FVector::ZeroVector;
			bHasRequestedVelocity = false;

			OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
		} // End scoped movement update
	}

	// Call external post-movement events. These happen after the scoped movement completes in case the events want to use the current state of overlaps etc.
	//CallMovementUpdateDelegate(DeltaSeconds, OldLocation, OldVelocity);
	
	MaybeSaveBaseLocation();
	//UpdateComponentVelocity();

	const bool bHasAuthority = CharacterOwner && CharacterOwner->HasAuthority();

	// If we move we want to avoid a long delay before replication catches up to notice this change, especially if it's throttling our rate.
	if (bHasAuthority && UNetDriver::IsAdaptiveNetUpdateFrequencyEnabled() && UpdatedComponent)
	{
		UNetDriver* NetDriver = MyWorld->GetNetDriver();
		if (NetDriver && NetDriver->IsServer())
		{
			FNetworkObjectInfo* NetActor = NetDriver->FindNetworkObjectInfo(CharacterOwner);
				
			if (NetActor && MyWorld->GetTimeSeconds() <= NetActor->NextUpdateTime && NetDriver->IsNetworkActorUpdateFrequencyThrottled(*NetActor))
			{
				if (ShouldCancelAdaptiveReplication())
				{
					NetDriver->CancelAdaptiveReplication(*NetActor);
				}
			}
		}
	}

	const FVector NewLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
	const FQuat NewRotation = UpdatedComponent ? UpdatedComponent->GetComponentQuat() : FQuat::Identity;

	if (bHasAuthority && UpdatedComponent && !IsNetMode(NM_Client))
	{
		const bool bLocationChanged = (NewLocation != LastUpdateLocation);
		const bool bRotationChanged = (NewRotation != LastUpdateRotation);
		if (bLocationChanged || bRotationChanged)
		{
			// Update ServerLastTransformUpdateTimeStamp. This is used by Linear smoothing on clients to interpolate positions with the correct delta time,
			// so the timestamp should be based on the client's move delta (ServerAccumulatedClientTimeStamp), not the server time when receiving the RPC.
			const bool bIsRemotePlayer = (CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy);
			const FNetworkPredictionData_Server_Character* ServerData = bIsRemotePlayer ? GetPredictionData_Server_Character() : nullptr;
			if (bIsRemotePlayer && ServerData /*&& CharacterMovementCVars::NetUseClientTimestampForReplicatedTransform*/)
			{
				ServerLastTransformUpdateTimeStamp = float(ServerData->ServerAccumulatedClientTimeStamp);
			}
			else
			{
				ServerLastTransformUpdateTimeStamp = MyWorld->GetTimeSeconds();
			}
		}
	}

	LastUpdateLocation = NewLocation;
	LastUpdateRotation = NewRotation;
	LastUpdateVelocity = Velocity;
}

void UGTCharacterMovementComponent::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	if (NewUpdatedComponent)
	{
		const ACharacter* NewCharacterOwner = Cast<ACharacter>(NewUpdatedComponent->GetOwner());
		if (NewCharacterOwner == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("%s owned by %s must update a component owned by a Character"), *GetName(), *GetNameSafe(NewUpdatedComponent->GetOwner()));
			return;
		}

		// check that UpdatedComponent is a Capsule
		if (Cast<UCapsuleComponent>(NewUpdatedComponent) == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("%s owned by %s must update a capsule component"), *GetName(), *GetNameSafe(NewUpdatedComponent->GetOwner()));
			return;
		}
	}

	if ( bMovementInProgress )
	{
		// failsafe to avoid crashes in CharacterMovement. 
		bDeferUpdateMoveComponent = true;
		DeferredUpdatedMoveComponent = NewUpdatedComponent;
		return;
	}
	bDeferUpdateMoveComponent = false;
	DeferredUpdatedMoveComponent = nullptr;

	USceneComponent* OldUpdatedComponent = UpdatedComponent;
	UPrimitiveComponent* OldPrimitive = Cast<UPrimitiveComponent>(UpdatedComponent);
	// if (IsValid(OldPrimitive) && OldPrimitive->OnComponentBeginOverlap.IsBound())
	// {
	// 	OldPrimitive->OnComponentBeginOverlap.RemoveDynamic(this, &UCharacterMovementComponent::CapsuleTouched);
	// }
	
	Super::SetUpdatedComponent(NewUpdatedComponent);
	CharacterOwner = Cast<ACharacter>(PawnOwner);

	if (UpdatedComponent != OldUpdatedComponent)
	{
		ClearAccumulatedForces();
	}

	if (UpdatedComponent == nullptr)
	{
		StopActiveMovement();
	}

	const bool bValidUpdatedPrimitive = IsValid(UpdatedPrimitive);

	// if (bValidUpdatedPrimitive && bEnablePhysicsInteraction)
	// {
	// 	UpdatedPrimitive->OnComponentBeginOverlap.AddUniqueDynamic(this, &UCharacterMovementComponent::CapsuleTouched);
	// }

	// if (bNeedsSweepWhileWalkingUpdate)
	// {
	// 	bSweepWhileNavWalking = bValidUpdatedPrimitive ? UpdatedPrimitive->GetGenerateOverlapEvents() : false;
	// 	bNeedsSweepWhileWalkingUpdate = false;
	// }

	if (bUseRVOAvoidance && IsValid(NewUpdatedComponent))
	{
		UAvoidanceManager* AvoidanceManager = GetWorld()->GetAvoidanceManager();
		if (AvoidanceManager)
		{
			AvoidanceManager->RegisterMovementComponent(this, AvoidanceWeight);
		}
	}
}

void UGTCharacterMovementComponent::StartNewPhysics(float deltaTime, int32 Iterations)
{
	if ((deltaTime < MIN_TICK_TIME) || (Iterations >= MaxSimulationIterations) || !HasValidData())
	{
		return;
	}

	if (UpdatedComponent->IsSimulatingPhysics())
	{
		UE_LOG(LogTemp, Log, TEXT("UCharacterMovementComponent::StartNewPhysics: UpdateComponent (%s) is simulating physics - aborting."), *UpdatedComponent->GetPathName());
		return;
	}

	const bool bSavedMovementInProgress = bMovementInProgress;
	bMovementInProgress = true;

	switch ( MovementMode )
	{
	case MOVE_None:
		break;
	case MOVE_Walking:
		PhysWalking(deltaTime, Iterations);
		break;
	case MOVE_NavWalking:
		PhysNavWalking(deltaTime, Iterations);
		break;
	case MOVE_Falling:
		PhysFalling(deltaTime, Iterations);
		break;
	case MOVE_Flying:
		PhysFlying(deltaTime, Iterations);
		break;
	case MOVE_Swimming:
		PhysSwimming(deltaTime, Iterations);
		break;
	case MOVE_Custom:
		PhysCustom(deltaTime, Iterations);
		break;
	default:
		UE_LOG(LogTemp, Warning, TEXT("%s has unsupported movement mode %d"), *CharacterOwner->GetName(), int32(MovementMode));
		SetMovementMode(MOVE_None);
		break;
	}

	bMovementInProgress = bSavedMovementInProgress;
	if ( bDeferUpdateMoveComponent )
	{
		//SCOPE_CYCLE_COUNTER(STAT_UGTCharacterMovementComponent_SetUpdatedComponent);
		SetUpdatedComponent(DeferredUpdatedMoveComponent);
	}
}

void UGTCharacterMovementComponent::SimulatedTick(float DeltaSeconds)
{
	//SCOPE_CYCLE_COUNTER(STAT_CharacterMovementSimulated);
    	checkSlow(CharacterOwner != nullptr);
    
    	// If we are playing a RootMotion AnimMontage.
    	if (CharacterOwner->IsPlayingNetworkedRootMotionMontage())
    	{
    		bWasSimulatingRootMotion = true;
    		UE_LOG(LogRootMotion, Verbose, TEXT("UCharacterMovementComponent::SimulatedTick"));
    
    		// Tick animations before physics.
    		if( CharacterOwner && CharacterOwner->GetMesh() )
    		{
    			TickCharacterPose(DeltaSeconds);
    
    			// Make sure animation didn't trigger an event that destroyed us
    			if (!HasValidData())
    			{
    				return;
    			}
    		}
    
    		const FQuat OldRotationQuat = UpdatedComponent->GetComponentQuat();
    		const FVector OldLocation = UpdatedComponent->GetComponentLocation();
    
    		USkeletalMeshComponent* Mesh = CharacterOwner->GetMesh();
    		const FVector SavedMeshRelativeLocation = Mesh ? Mesh->GetRelativeLocation() : FVector::ZeroVector;
    
    		if( RootMotionParams.bHasRootMotion )
    		{
    			SimulateRootMotion(DeltaSeconds, RootMotionParams.GetRootMotionTransform());
    
    #if !(UE_BUILD_SHIPPING)
    			// debug
    			if (CharacterOwner && false)
    			{
    				const FRotator OldRotation = OldRotationQuat.Rotator();
    				const FRotator NewRotation = UpdatedComponent->GetComponentRotation();
    				const FVector NewLocation = UpdatedComponent->GetComponentLocation();
    				DrawDebugCoordinateSystem(GetWorld(), CharacterOwner->GetMesh()->GetComponentLocation() + FVector(0,0,1), NewRotation, 50.f, false);
    				DrawDebugLine(GetWorld(), OldLocation, NewLocation, FColor::Red, false, 10.f);
    
    				UE_LOG(LogRootMotion, Log,  TEXT("UCharacterMovementComponent::SimulatedTick DeltaMovement Translation: %s, Rotation: %s, MovementBase: %s"),
    					*(NewLocation - OldLocation).ToCompactString(), *(NewRotation - OldRotation).GetNormalized().ToCompactString(), *GetNameSafe(CharacterOwner->GetMovementBase()) );
    			}
    #endif // !(UE_BUILD_SHIPPING)
    		}
    
    		// then, once our position is up to date with our animation, 
    		// handle position correction if we have any pending updates received from the server.
    		if( CharacterOwner && (CharacterOwner->RootMotionRepMoves.Num() > 0) )
    		{
    			CharacterOwner->SimulatedRootMotionPositionFixup(DeltaSeconds);
    		}
    
    		if (!bNetworkSmoothingComplete && (NetworkSmoothingMode == ENetworkSmoothingMode::Linear))
    		{
    			// Same mesh with different rotation?
    			const FQuat NewCapsuleRotation = UpdatedComponent->GetComponentQuat();
    			if (Mesh == CharacterOwner->GetMesh() && !NewCapsuleRotation.Equals(OldRotationQuat, 1e-6f) && ClientPredictionData)
    			{
    				// Smoothing should lerp toward this new rotation target, otherwise it will just try to go back toward the old rotation.
    				ClientPredictionData->MeshRotationTarget = NewCapsuleRotation;
    				Mesh->SetRelativeLocationAndRotation(SavedMeshRelativeLocation, CharacterOwner->GetBaseRotationOffset());
    			}
    		}
    	}
    	else if (CurrentRootMotion.HasActiveRootMotionSources())
    	{
    		// We have root motion sources and possibly animated root motion
    		bWasSimulatingRootMotion = true;
    		UE_LOG(LogRootMotion, Verbose, TEXT("UCharacterMovementComponent::SimulatedTick"));
    
    		// If we have RootMotionRepMoves, find the most recent important one and set position/rotation to it
    		bool bCorrectedToServer = false;
    		const FVector OldLocation = UpdatedComponent->GetComponentLocation();
    		const FQuat OldRotation = UpdatedComponent->GetComponentQuat();
    		if( CharacterOwner->RootMotionRepMoves.Num() > 0 )
    		{
    			// Move Actor back to position of that buffered move. (server replicated position).
    			FSimulatedRootMotionReplicatedMove& RootMotionRepMove = CharacterOwner->RootMotionRepMoves.Last();
    			if( CharacterOwner->RestoreReplicatedMove(RootMotionRepMove) )
    			{
    				bCorrectedToServer = true;
    			}
    			Acceleration = RootMotionRepMove.RootMotion.Acceleration;
    
    			CharacterOwner->PostNetReceiveVelocity(RootMotionRepMove.RootMotion.LinearVelocity);
    			LastUpdateVelocity = RootMotionRepMove.RootMotion.LinearVelocity;
    
    			// Convert RootMotionSource Server IDs -> Local IDs in AuthoritativeRootMotion and cull invalid
    			// so that when we use this root motion it has the correct IDs
    			ConvertRootMotionServerIDsToLocalIDs(CurrentRootMotion, RootMotionRepMove.RootMotion.AuthoritativeRootMotion, RootMotionRepMove.Time);
    			RootMotionRepMove.RootMotion.AuthoritativeRootMotion.CullInvalidSources();
    
    			// Set root motion states to that of repped in state
    			CurrentRootMotion.UpdateStateFrom(RootMotionRepMove.RootMotion.AuthoritativeRootMotion, true);
    
    			// Clear out existing RootMotionRepMoves since we've consumed the most recent
    			UE_LOG(LogRootMotion, Log,  TEXT("\tClearing old moves in SimulatedTick (%d)"), CharacterOwner->RootMotionRepMoves.Num());
    			CharacterOwner->RootMotionRepMoves.Reset();
    		}
    
    		// Update replicated movement mode.
    		if (bNetworkMovementModeChanged)
    		{
    			ApplyNetworkMovementMode(CharacterOwner->GetReplicatedMovementMode());
    			bNetworkMovementModeChanged = false;
    		}
    
    		// Perform movement
    		PerformMovement(DeltaSeconds);
    
    		// After movement correction, smooth out error in position if any.
    		if( bCorrectedToServer || CurrentRootMotion.NeedsSimulatedSmoothing() )
    		{
    			SmoothCorrection(OldLocation, OldRotation, UpdatedComponent->GetComponentLocation(), UpdatedComponent->GetComponentQuat());
    		}
    	}
    	// Not playing RootMotion AnimMontage
    	else
    	{
    		// if we were simulating root motion, we've been ignoring regular ReplicatedMovement updates.
    		// If we're not simulating root motion anymore, force us to sync our movement properties.
    		// (Root Motion could leave Velocity out of sync w/ ReplicatedMovement)
    		if( bWasSimulatingRootMotion )
    		{
    			CharacterOwner->RootMotionRepMoves.Empty();
    			CharacterOwner->OnRep_ReplicatedMovement();
    			CharacterOwner->OnRep_ReplicatedBasedMovement();
    			ApplyNetworkMovementMode(GetCharacterOwner()->GetReplicatedMovementMode());
    		}
    
    		if (CharacterOwner->IsReplicatingMovement() && UpdatedComponent)
    		{
    			USkeletalMeshComponent* Mesh = CharacterOwner->GetMesh();
    			const FVector SavedMeshRelativeLocation = Mesh ? Mesh->GetRelativeLocation() : FVector::ZeroVector; 
    			const FQuat SavedCapsuleRotation = UpdatedComponent->GetComponentQuat();
    			const bool bPreventMeshMovement = !bNetworkSmoothingComplete;
    
    			// Avoid moving the mesh during movement if SmoothClientPosition will take care of it.
    			{
    				const FScopedPreventAttachedComponentMove PreventMeshMovement(bPreventMeshMovement ? Mesh : nullptr);
    				if (CharacterOwner->IsPlayingRootMotion())
    				{
    					// Update replicated movement mode.
    					if (bNetworkMovementModeChanged)
    					{
    						ApplyNetworkMovementMode(CharacterOwner->GetReplicatedMovementMode());
    						bNetworkMovementModeChanged = false;
    					}
    
    					PerformMovement(DeltaSeconds);
    				}
    				else
    				{
    					SimulateMovement(DeltaSeconds);
    				}
    			}
    
    			// With Linear smoothing we need to know if the rotation changes, since the mesh should follow along with that (if it was prevented above).
    			// This should be rare that rotation changes during simulation, but it can happen when ShouldRemainVertical() changes, or standing on a moving base.
    			const bool bValidateRotation = bPreventMeshMovement && (NetworkSmoothingMode == ENetworkSmoothingMode::Linear);
    			if (bValidateRotation && UpdatedComponent)
    			{
    				// Same mesh with different rotation?
    				const FQuat NewCapsuleRotation = UpdatedComponent->GetComponentQuat();
    				if (Mesh == CharacterOwner->GetMesh() && !NewCapsuleRotation.Equals(SavedCapsuleRotation, 1e-6f) && ClientPredictionData)
    				{
    					// Smoothing should lerp toward this new rotation target, otherwise it will just try to go back toward the old rotation.
    					ClientPredictionData->MeshRotationTarget = NewCapsuleRotation;
    					Mesh->SetRelativeLocationAndRotation(SavedMeshRelativeLocation, CharacterOwner->GetBaseRotationOffset());
    				}
    			}
    		}
    
    		if (bWasSimulatingRootMotion)
    		{
    			bWasSimulatingRootMotion = false;
    		}
    	}
    
    	// Smooth mesh location after moving the capsule above.
    	if (!bNetworkSmoothingComplete)
    	{
    		//SCOPE_CYCLE_COUNTER(STAT_CharacterMovementSmoothClientPosition);
    		SmoothClientPosition(DeltaSeconds);
    	}
    	else
    	{
    		//UE_LOG(LogCharacterNetSmoothing, Verbose, TEXT("Skipping network smoothing for %s."), *GetNameSafe(CharacterOwner));
    	}
}

void UGTCharacterMovementComponent::PhysNavWalking(float deltaTime, int32 Iterations)
{
	
	//SCOPE_CYCLE_COUNTER(STAT_UGTCharacterMovementComponent_PhysNavWalking1);
	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	if ((!CharacterOwner || !CharacterOwner->Controller) && !bRunPhysicsWithNoController && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		Acceleration = FVector::ZeroVector;
		Velocity = FVector::ZeroVector;
		return;
	}

	//RestorePreAdditiveRootMotionVelocity();

	// Ensure velocity is horizontal.
	MaintainHorizontalGroundVelocity();
	
	//bound acceleration
	Acceleration.Z = 0.f;
	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		CalcVelocity(deltaTime, GroundFriction, false, GetMaxBrakingDeceleration());
	}

	//ApplyRootMotionToVelocity(deltaTime);

	if( IsFalling() )
	{
		// Root motion could have put us into Falling
		StartNewPhysics(deltaTime, Iterations);
		return;
	}

	Iterations++;

	FVector DesiredMove = Velocity;
	DesiredMove.Z = 0.f;

	const FVector OldLocation = GetActorFeetLocation();
	const FVector DeltaMove = DesiredMove * deltaTime;
	const bool bDeltaMoveNearlyZero = DeltaMove.IsNearlyZero();

	FVector AdjustedDest = OldLocation + DeltaMove;
	FNavLocation DestNavLocation;

	bool bSameNavLocation = false;
	if (CachedNavLocation.NodeRef != INVALID_NAVNODEREF)
	{
		//SCOPE_CYCLE_COUNTER(STAT_UGTCharacterMovementComponent_PhysNavWalking2);
		if (bProjectNavMeshWalking)
		{
			const float DistSq2D = (OldLocation - CachedNavLocation.Location).SizeSquared2D();
			const float DistZ = FMath::Abs(OldLocation.Z - CachedNavLocation.Location.Z);

			const float TotalCapsuleHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2.0f;
			const float ProjectionScale = (OldLocation.Z > CachedNavLocation.Location.Z) ? NavMeshProjectionHeightScaleUp : NavMeshProjectionHeightScaleDown;
			const float DistZThr = TotalCapsuleHeight * FMath::Max(0.f, ProjectionScale);

			bSameNavLocation = (DistSq2D <= UE_KINDA_SMALL_NUMBER) && (DistZ < DistZThr);
		}
		else
		{
			bSameNavLocation = CachedNavLocation.Location.Equals(OldLocation);
		}

		if (bDeltaMoveNearlyZero && bSameNavLocation)
		{
			
			//SCOPE_CYCLE_COUNTER(STAT_UGTCharacterMovementComponent_PhysNavWalking3);
			if (const INavigationDataInterface* NavData = GetNavData())
			{
				if (!NavData->IsNodeRefValid(CachedNavLocation.NodeRef))
				{
					CachedNavLocation.NodeRef = INVALID_NAVNODEREF;
					bSameNavLocation = false;
				}
			}
		}
	}

	if (bDeltaMoveNearlyZero && bSameNavLocation)
	{
		DestNavLocation = CachedNavLocation;
	}
	else
	{
		
		//SCOPE_CYCLE_COUNTER(STAT_UGTCharacterMovementComponent_PhysNavWalking4);
		// Start the trace from the Z location of the last valid trace.
		// Otherwise if we are projecting our location to the underlying geometry and it's far above or below the navmesh,
		// we'll follow that geometry's plane out of range of valid navigation.
		if (bSameNavLocation && bProjectNavMeshWalking)
		{
			AdjustedDest.Z = CachedNavLocation.Location.Z;
		}

		// Find the point on the NavMesh
		const bool bHasNavigationData = FindNavFloor(AdjustedDest, DestNavLocation);
		if (!bHasNavigationData)
		{
			SetMovementMode(MOVE_Walking);
			return;
		}

		CachedNavLocation = DestNavLocation;
	}

	if (DestNavLocation.NodeRef != INVALID_NAVNODEREF)
	{
		//SCOPE_CYCLE_COUNTER(STAT_UGTCharacterMovementComponent_PhysNavWalking5);
		FVector NewLocation(AdjustedDest.X, AdjustedDest.Y, DestNavLocation.Location.Z);
		if (bProjectNavMeshWalking)
		{
			//SCOPE_CYCLE_COUNTER(STAT_UGTCharacterMovementComponent_PhysNavWalking6);
			const float TotalCapsuleHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2.0f;
			const float UpOffset = TotalCapsuleHeight * FMath::Max(0.f, NavMeshProjectionHeightScaleUp);
			const float DownOffset = TotalCapsuleHeight * FMath::Max(0.f, NavMeshProjectionHeightScaleDown);
			NewLocation = ProjectLocationFromNavMesh(deltaTime, OldLocation, NewLocation, UpOffset, DownOffset);
		}

		FVector AdjustedDelta = NewLocation - OldLocation;

		if (!AdjustedDelta.IsNearlyZero())
		{
			//SCOPE_CYCLE_COUNTER(STAT_UGTCharacterMovementComponent_PhysNavWalking7);
			FHitResult HitResult;
			FVector OwnerLocation = UpdatedComponent->GetOwner()->GetActorLocation();
			FRotator NewRotation = UKismetMathLibrary::FindLookAtRotation(FVector(OwnerLocation.X, OwnerLocation.Y, 100), FVector(CachedNavLocation.Location.X, CachedNavLocation.Location.Y, 100));
			
			//MoveUpdatedComponentImpl(AdjustedDelta, NewRotation.Quaternion(),false, nullptr, ETeleportType::TeleportPhysics);
			UpdatedComponent->MoveComponent(AdjustedDelta, NewRotation, false, nullptr, MoveComponentFlags, ETeleportType::ResetPhysics);
			//SafeMoveUpdatedComponent(AdjustedDelta, NewRotation/*UpdatedComponent->GetComponentQuat()*/, bSweepWhileNavWalking, HitResult);
		}

		// Update velocity to reflect actual move
		if (!bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasVelocity())
		{
			Velocity = (GetActorFeetLocation() - OldLocation) / deltaTime;
			MaintainHorizontalGroundVelocity();
		}

		bJustTeleported = false;
	}
	else
	{
		StartFalling(Iterations, deltaTime, deltaTime, DeltaMove, OldLocation);
	}
}
bool UGTCharacterMovementComponent::MoveUpdatedComponentImpl( const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit, ETeleportType Teleport)
{
	if (UpdatedComponent)
	{
		//const FVector NewDelta = ConstrainDirectionToPlane(Delta);
		return UpdatedComponent->MoveComponent(Delta, NewRotation, bSweep, OutHit, MoveComponentFlags, Teleport);
	}

	return false;
}

void UGTCharacterMovementComponent::ControlledCharacterMove(const FVector& InputVector, float DeltaSeconds)
{
	{

		// We need to check the jump state before adjusting input acceleration, to minimize latency
		// and to make sure acceleration respects our potentially new falling state.
		CharacterOwner->CheckJumpInput(DeltaSeconds);

		// apply input to acceleration
		Acceleration = ScaleInputAcceleration(ConstrainInputAcceleration(InputVector));
		AnalogInputModifier = ComputeAnalogInputModifier();
	}

	if (CharacterOwner->GetLocalRole() == ROLE_Authority)
	{
		PerformMovement(DeltaSeconds);
	}
	else if (CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy && IsNetMode(NM_Client))
	{
		ReplicateMoveToServer(DeltaSeconds, Acceleration);
	}
}

void UGTCharacterMovementComponent::ApplyRootMotionToVelocity(float deltaTime)
{

	// Animation root motion is distinct from root motion sources right now and takes precedence
	if( HasAnimRootMotion() && deltaTime > 0.f )
	{
		Velocity = ConstrainAnimRootMotionVelocity(AnimRootMotionVelocity, Velocity);
		if (IsFalling())
		{
			Velocity += FVector(DecayingFormerBaseVelocity.X, DecayingFormerBaseVelocity.Y, 0.f);
		}
		return;
	}

	const FVector OldVelocity = Velocity;

	bool bAppliedRootMotion = false;

	// Apply override velocity
	if( CurrentRootMotion.HasOverrideVelocity() )
	{
		CurrentRootMotion.AccumulateOverrideRootMotionVelocity(deltaTime, *CharacterOwner, *this, Velocity);
		if (IsFalling())
		{
			Velocity += CurrentRootMotion.HasOverrideVelocityWithIgnoreZAccumulate() ? FVector(DecayingFormerBaseVelocity.X, DecayingFormerBaseVelocity.Y, 0.f) : DecayingFormerBaseVelocity;
		}
		bAppliedRootMotion = true;

#if ROOT_MOTION_DEBUG
		if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnGameThread() == 1)
		{
			FString AdjustedDebugString = FString::Printf(TEXT("ApplyRootMotionToVelocity HasOverrideVelocity Velocity(%s)"),
				*Velocity.ToCompactString());
			RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
		}
#endif
	}

	// Next apply additive root motion
	if( CurrentRootMotion.HasAdditiveVelocity() )
	{
		CurrentRootMotion.LastPreAdditiveVelocity = Velocity; // Save off pre-additive Velocity for restoration next tick
		CurrentRootMotion.AccumulateAdditiveRootMotionVelocity(deltaTime, *CharacterOwner, *this, Velocity);
		CurrentRootMotion.bIsAdditiveVelocityApplied = true; // Remember that we have it applied
		bAppliedRootMotion = true;

#if ROOT_MOTION_DEBUG
		if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnGameThread() == 1)
		{
			FString AdjustedDebugString = FString::Printf(TEXT("ApplyRootMotionToVelocity HasAdditiveVelocity Velocity(%s) LastPreAdditiveVelocity(%s)"),
				*Velocity.ToCompactString(), *CurrentRootMotion.LastPreAdditiveVelocity.ToCompactString());
			RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
		}
#endif
	}

	// Switch to Falling if we have vertical velocity from root motion so we can lift off the ground
	const FVector AppliedVelocityDelta = Velocity - OldVelocity;
	if( bAppliedRootMotion && AppliedVelocityDelta.Z != 0.f && IsMovingOnGround() )
	{
		float LiftoffBound;
		if( CurrentRootMotion.LastAccumulatedSettings.HasFlag(ERootMotionSourceSettingsFlags::UseSensitiveLiftoffCheck) )
		{
			// Sensitive bounds - "any positive force"
			LiftoffBound = UE_SMALL_NUMBER;
		}
		else
		{
			// Default bounds - the amount of force gravity is applying this tick
			LiftoffBound = FMath::Max(-GetGravityZ() * deltaTime, UE_SMALL_NUMBER);
		}

		if( AppliedVelocityDelta.Z > LiftoffBound )
		{
			SetMovementMode(MOVE_Falling);
		}
	}
}

void UGTCharacterMovementComponent::CallMovementUpdateDelegate(float DeltaTime, const FVector& OldLocation,
                                                               const FVector& OldVelocity)
{

	// Update component velocity in case events want to read it
	UpdateComponentVelocity();

	// Delegate (for blueprints)
	if (CharacterOwner)
	{
		CharacterOwner->OnCharacterMovementUpdated.Broadcast(DeltaTime, OldLocation, OldVelocity);
	}
}

void UGTCharacterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                  FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	
}

void UGTCharacterMovementComponent::UpdateMovement(float DeltaTime)
{
	FVector InputVector = FVector::ZeroVector;
	bool bUsingAsyncTick = (CharacterMovementCVars::AsyncCharacterMovement == 1) && IsAsyncCallbackRegistered();
	if (!bUsingAsyncTick)
	{
		// Do not consume input if simulating asynchronously, we will consume input when filling out async inputs.
		InputVector = ConsumeInputVector();
	}

	if (!HasValidData() || ShouldSkipUpdate(DeltaTime))
	{
		return;
	}

	// Super tick may destroy/invalidate CharacterOwner or UpdatedComponent, so we need to re-check.
	if (!HasValidData())
	{
		return;
	}

	if (bUsingAsyncTick)
	{
		check(CharacterOwner && CharacterOwner->GetMesh());
		USkeletalMeshComponent* CharacterMesh = CharacterOwner->GetMesh();
		if (CharacterMesh->ShouldTickPose())
		{
			const bool bWasPlayingRootMotion = CharacterOwner->IsPlayingRootMotion();

			CharacterMesh->TickPose(DeltaTime, true);
			// We are simulating character movement on physics thread, do not tick movement.
			const bool bIsPlayingRootMotion = CharacterOwner->IsPlayingRootMotion();
			if (bIsPlayingRootMotion || bWasPlayingRootMotion)
			{
				FRootMotionMovementParams RootMotion = CharacterMesh->ConsumeRootMotion();
				if (RootMotion.bHasRootMotion)
				{
					RootMotion.ScaleRootMotionTranslation(CharacterOwner->GetAnimRootMotionTranslationScale());
					RootMotionParams.Accumulate(RootMotion);
				}
			}
		}

		AccumulateRootMotionForAsync(DeltaTime, AsyncRootMotion);

		return;
	}

	// See if we fell out of the world.
	const bool bIsSimulatingPhysics = UpdatedComponent->IsSimulatingPhysics();
	if (CharacterOwner->GetLocalRole() == ROLE_Authority && (!bCheatFlying || bIsSimulatingPhysics) && !CharacterOwner->CheckStillInWorld())
	{
		return;
	}

	// We don't update if simulating physics (eg ragdolls).
	if (bIsSimulatingPhysics)
	{
		// Update camera to ensure client gets updates even when physics move it far away from point where simulation started
		if (CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy && IsNetMode(NM_Client))
		{
			MarkForClientCameraUpdate();
		}

		ClearAccumulatedForces();
		return;
	}

	AvoidanceLockTimer -= DeltaTime;

	if (CharacterOwner->GetLocalRole() > ROLE_SimulatedProxy)
	{

		// If we are a client we might have received an update from the server.
		const bool bIsClient = (CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy && IsNetMode(NM_Client));
		if (bIsClient)
		{
			FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
			if (ClientData && ClientData->bUpdatePosition)
			{
				//ClientUpdatePositionAfterServerUpdate();
			}
		}

		// Perform input-driven move for any locally-controlled character, and also
		// allow animation root motion or physics to move characters even if they have no controller
		const bool bShouldPerformControlledCharMove = CharacterOwner->IsLocallyControlled() 
													  || (!CharacterOwner->Controller && bRunPhysicsWithNoController)		
													  || (!CharacterOwner->Controller && CharacterOwner->IsPlayingRootMotion());
		
		if (bShouldPerformControlledCharMove)
		{
			ControlledCharacterMove(InputVector, DeltaTime);

			const bool bIsaListenServerAutonomousProxy = CharacterOwner->IsLocallyControlled()
													 	 && (CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy);

			if (bIsaListenServerAutonomousProxy)
			{
				ServerAutonomousProxyTick(DeltaTime);
			}
		}
		else if (CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy)
		{
			// Server ticking for remote client.
			// Between net updates from the client we need to update position if based on another object,
			// otherwise the object will move on intermediate frames and we won't follow it.
			MaybeUpdateBasedMovement(DeltaTime);
			MaybeSaveBaseLocation();

			ServerAutonomousProxyTick(DeltaTime);

			static int32 NetEnableListenServerSmoothing = 1;
			// Smooth on listen server for local view of remote clients. We may receive updates at a rate different than our own tick rate.
			if (NetEnableListenServerSmoothing && !bNetworkSmoothingComplete && IsNetMode(NM_ListenServer))
			{
				SmoothClientPosition(DeltaTime);
			}
		}
	}
	else if (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)
	{
		if (bShrinkProxyCapsule)
		{
			AdjustProxyCapsuleSize();
		}
		SimulatedTick(DeltaTime);
	}

	if (bUseRVOAvoidance)
	{
		UpdateDefaultAvoidance();
	}

	// if (bEnablePhysicsInteraction)
	// {
	// 	ApplyDownwardForce(DeltaTime);
	// 	ApplyRepulsionForce(DeltaTime);
	// }

}
