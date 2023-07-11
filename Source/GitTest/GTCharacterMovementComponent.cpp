// Fill out your copyright notice in the Description page of Project Settings.


#include "GTCharacterMovementComponent.h"

#include "AI/Navigation/NavigationDataInterface.h"
#include "Components/CapsuleComponent.h"
#include "Engine/NetworkObjectList.h"
#include "GameFramework/Character.h"

void UGTCharacterMovementComponent::PerformMovement(float DeltaSeconds)
{
	//Super::PerformMovement(DeltaTime);

	//SCOPE_CYCLE_COUNTER(STAT_CharacterMovementPerformMovement);

	const UWorld* MyWorld = GetWorld();
	if (!HasValidData() || MyWorld == nullptr)
	{
		return;
	}

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
	if( CurrentRootMotion.HasAdditiveVelocity() )
	{
		const FVector Adjustment = (Velocity - LastUpdateVelocity);
		CurrentRootMotion.LastPreAdditiveVelocity += Adjustment;

#if ROOT_MOTION_DEBUG
		if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnGameThread() == 1)
		{
			if (!Adjustment.IsNearlyZero())
			{
				FString AdjustedDebugString = FString::Printf(TEXT("PerformMovement HasAdditiveVelocity LastUpdateVelocityAdjustment LastPreAdditiveVelocity(%s) Adjustment(%s)"),
					*CurrentRootMotion.LastPreAdditiveVelocity.ToCompactString(), *Adjustment.ToCompactString());
				RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
			}
		}
#endif
	}

	FVector OldVelocity;
	FVector OldLocation;

	if(UpdatedComponent == GetOwner()->GetRootComponent())
	{
		// Scoped updates can improve performance of multiple MoveComponent calls.
		{
			FScopedMovementUpdate ScopedMovementUpdate(UpdatedComponent, bEnableScopedMovementUpdates ? EScopedUpdate::DeferredUpdates : EScopedUpdate::ImmediateUpdates);

			MaybeUpdateBasedMovement(DeltaSeconds);

			// Clean up invalid RootMotion Sources.
			// This includes RootMotion sources that ended naturally.
			// They might want to perform a clamp on velocity or an override, 
			// so we want this to happen before ApplyAccumulatedForces and HandlePendingLaunch as to not clobber these.
			const bool bHasRootMotionSources = HasRootMotionSources();
			if (bHasRootMotionSources && !CharacterOwner->bClientUpdating && !CharacterOwner->bServerMoveIgnoreRootMotion)
			{
				//SCOPE_CYCLE_COUNTER(STAT_CharacterMovementRootMotionSourceCalculate);

				const FVector VelocityBeforeCleanup = Velocity;
				CurrentRootMotion.CleanUpInvalidRootMotion(DeltaSeconds, *CharacterOwner, *this);

	#if ROOT_MOTION_DEBUG
				if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnGameThread() == 1)
				{
					if (Velocity != VelocityBeforeCleanup)
					{
						const FVector Adjustment = Velocity - VelocityBeforeCleanup;
						FString AdjustedDebugString = FString::Printf(TEXT("PerformMovement CleanUpInvalidRootMotion Velocity(%s) VelocityBeforeCleanup(%s) Adjustment(%s)"),
							*Velocity.ToCompactString(), *VelocityBeforeCleanup.ToCompactString(), *Adjustment.ToCompactString());
						RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
					}
				}
	#endif
			}

			OldVelocity = Velocity;
			OldLocation = UpdatedComponent->GetComponentLocation();

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

	#if ROOT_MOTION_DEBUG
			if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnGameThread() == 1)
			{
				if (OldVelocity != Velocity)
				{
					const FVector Adjustment = Velocity - OldVelocity;
					FString AdjustedDebugString = FString::Printf(TEXT("PerformMovement ApplyAccumulatedForces+HandlePendingLaunch Velocity(%s) OldVelocity(%s) Adjustment(%s)"),
						*Velocity.ToCompactString(), *OldVelocity.ToCompactString(), *Adjustment.ToCompactString());
					RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
				}
			}
	#endif

			// Update saved LastPreAdditiveVelocity with any external changes to character Velocity that happened due to ApplyAccumulatedForces/HandlePendingLaunch
			if( CurrentRootMotion.HasAdditiveVelocity() )
			{
				const FVector Adjustment = (Velocity - OldVelocity);
				CurrentRootMotion.LastPreAdditiveVelocity += Adjustment;

	#if ROOT_MOTION_DEBUG
				if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnGameThread() == 1)
				{
					if (!Adjustment.IsNearlyZero())
					{
						FString AdjustedDebugString = FString::Printf(TEXT("PerformMovement HasAdditiveVelocity AccumulatedForces LastPreAdditiveVelocity(%s) Adjustment(%s)"),
							*CurrentRootMotion.LastPreAdditiveVelocity.ToCompactString(), *Adjustment.ToCompactString());
						RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
					}
				}
	#endif
			}

			// Prepare Root Motion (generate/accumulate from root motion sources to be used later)
			if (bHasRootMotionSources && !CharacterOwner->bClientUpdating && !CharacterOwner->bServerMoveIgnoreRootMotion)
			{
				// Animation root motion - If using animation RootMotion, tick animations before running physics.
				if( CharacterOwner->IsPlayingRootMotion() && CharacterOwner->GetMesh() )
				{
					TickCharacterPose(DeltaSeconds);

					// Make sure animation didn't trigger an event that destroyed us
					if (!HasValidData())
					{
						return;
					}

					// For local human clients, save off root motion data so it can be used by movement networking code.
					if( CharacterOwner->IsLocallyControlled() && (CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy) && CharacterOwner->IsPlayingNetworkedRootMotionMontage() )
					{
						CharacterOwner->ClientRootMotionParams = RootMotionParams;
					}
				}

				// Generates root motion to be used this frame from sources other than animation
				{
					//SCOPE_CYCLE_COUNTER(STAT_CharacterMovementRootMotionSourceCalculate);
					CurrentRootMotion.PrepareRootMotion(DeltaSeconds, *CharacterOwner, *this, true);
				}

				// For local human clients, save off root motion data so it can be used by movement networking code.
				if( CharacterOwner->IsLocallyControlled() && (CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy) )
				{
					CharacterOwner->SavedRootMotion = CurrentRootMotion;
				}
			}

			// Apply Root Motion to Velocity
			if( CurrentRootMotion.HasOverrideVelocity() || HasAnimRootMotion() )
			{
				// Animation root motion overrides Velocity and currently doesn't allow any other root motion sources
				if( HasAnimRootMotion() )
				{
					// Convert to world space (animation root motion is always local)
					USkeletalMeshComponent * SkelMeshComp = CharacterOwner->GetMesh();
					if( SkelMeshComp )
					{
						// Convert Local Space Root Motion to world space. Do it right before used by physics to make sure we use up to date transforms, as translation is relative to rotation.
						RootMotionParams.Set( ConvertLocalRootMotionToWorld(RootMotionParams.GetRootMotionTransform(), DeltaSeconds) );
					}

					// Then turn root motion to velocity to be used by various physics modes.
					if( DeltaSeconds > 0.f )
					{
						AnimRootMotionVelocity = CalcAnimRootMotionVelocity(RootMotionParams.GetRootMotionTransform().GetTranslation(), DeltaSeconds, Velocity);
						Velocity = ConstrainAnimRootMotionVelocity(AnimRootMotionVelocity, Velocity);
						if (IsFalling())
						{
							Velocity += FVector(DecayingFormerBaseVelocity.X, DecayingFormerBaseVelocity.Y, 0.f);
						}
					}
					
					UE_LOG(LogRootMotion, Log,  TEXT("PerformMovement WorldSpaceRootMotion Translation: %s, Rotation: %s, Actor Facing: %s, Velocity: %s")
						, *RootMotionParams.GetRootMotionTransform().GetTranslation().ToCompactString()
						, *RootMotionParams.GetRootMotionTransform().GetRotation().Rotator().ToCompactString()
						, *CharacterOwner->GetActorForwardVector().ToCompactString()
						, *Velocity.ToCompactString()
						);
				}
				else
				{
					// We don't have animation root motion so we apply other sources
					if( DeltaSeconds > 0.f )
					{
						//SCOPE_CYCLE_COUNTER(STAT_CharacterMovementRootMotionSourceApply);

						const FVector VelocityBeforeOverride = Velocity;
						FVector NewVelocity = Velocity;
						CurrentRootMotion.AccumulateOverrideRootMotionVelocity(DeltaSeconds, *CharacterOwner, *this, NewVelocity);
						if (IsFalling())
						{
							NewVelocity += CurrentRootMotion.HasOverrideVelocityWithIgnoreZAccumulate() ? FVector(DecayingFormerBaseVelocity.X, DecayingFormerBaseVelocity.Y, 0.f) : DecayingFormerBaseVelocity;
						}
						Velocity = NewVelocity;

	#if ROOT_MOTION_DEBUG
						if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnGameThread() == 1)
						{
							if (VelocityBeforeOverride != Velocity)
							{
								FString AdjustedDebugString = FString::Printf(TEXT("PerformMovement AccumulateOverrideRootMotionVelocity Velocity(%s) VelocityBeforeOverride(%s)"),
									*Velocity.ToCompactString(), *VelocityBeforeOverride.ToCompactString());
								RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
							}
						}
	#endif
					}
				}
			}

	#if ROOT_MOTION_DEBUG
			if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnGameThread() == 1)
			{
				FString AdjustedDebugString = FString::Printf(TEXT("PerformMovement Velocity(%s) OldVelocity(%s)"),
					*Velocity.ToCompactString(), *OldVelocity.ToCompactString());
				RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
			}
	#endif

			// NaN tracking
			//devCode(ensureMsgf(!Velocity.ContainsNaN(), TEXT("UCharacterMovementComponent::PerformMovement: Velocity contains NaN (%s)\n%s"), *GetPathNameSafe(this), *Velocity.ToString()));

			// Clear jump input now, to allow movement events to trigger it for next update.
			CharacterOwner->ClearJumpInput(DeltaSeconds);
			NumJumpApexAttempts = 0;

			// change position
			StartNewPhysics(DeltaSeconds, 0);

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
			if( HasAnimRootMotion() )
			{
				const FQuat OldActorRotationQuat = UpdatedComponent->GetComponentQuat();
				const FQuat RootMotionRotationQuat = RootMotionParams.GetRootMotionTransform().GetRotation();
				if( !RootMotionRotationQuat.IsIdentity() )
				{
					const FQuat NewActorRotationQuat = RootMotionRotationQuat * OldActorRotationQuat;
					MoveUpdatedComponent(FVector::ZeroVector, NewActorRotationQuat, true);
				}

	#if !(UE_BUILD_SHIPPING)
				// debug
				if (false)
				{
					const FRotator OldActorRotation = OldActorRotationQuat.Rotator();
					const FVector ResultingLocation = UpdatedComponent->GetComponentLocation();
					const FRotator ResultingRotation = UpdatedComponent->GetComponentRotation();

					// Show current position
					DrawDebugCoordinateSystem(MyWorld, CharacterOwner->GetMesh()->GetComponentLocation() + FVector(0,0,1), ResultingRotation, 50.f, false);

					// Show resulting delta move.
					DrawDebugLine(MyWorld, OldLocation, ResultingLocation, FColor::Red, false, 10.f);

					// Log details.
					UE_LOG(LogRootMotion, Warning,  TEXT("PerformMovement Resulting DeltaMove Translation: %s, Rotation: %s, MovementBase: %s"), //-V595
						*(ResultingLocation - OldLocation).ToCompactString(), *(ResultingRotation - OldActorRotation).GetNormalized().ToCompactString(), *GetNameSafe(CharacterOwner->GetMovementBase()) );

					const FVector RMTranslation = RootMotionParams.GetRootMotionTransform().GetTranslation();
					const FRotator RMRotation = RootMotionParams.GetRootMotionTransform().GetRotation().Rotator();
					UE_LOG(LogRootMotion, Warning,  TEXT("PerformMovement Resulting DeltaError Translation: %s, Rotation: %s"),
						*(ResultingLocation - OldLocation - RMTranslation).ToCompactString(), *(ResultingRotation - OldActorRotation - RMRotation).GetNormalized().ToCompactString() );
				}
	#endif // !(UE_BUILD_SHIPPING)

				// Root Motion has been used, clear
				RootMotionParams.Clear();
			}
			else if (CurrentRootMotion.HasActiveRootMotionSources())
			{
				FQuat RootMotionRotationQuat;
				if (CharacterOwner && UpdatedComponent && CurrentRootMotion.GetOverrideRootMotionRotation(DeltaSeconds, *CharacterOwner, *this, RootMotionRotationQuat))
				{
					const FQuat OldActorRotationQuat = UpdatedComponent->GetComponentQuat();
					const FQuat NewActorRotationQuat = RootMotionRotationQuat * OldActorRotationQuat;
					MoveUpdatedComponent(FVector::ZeroVector, NewActorRotationQuat, true);
				}
			}

			// consume path following requested velocity
			LastUpdateRequestedVelocity = bHasRequestedVelocity ? RequestedVelocity : FVector::ZeroVector;
			bHasRequestedVelocity = false;

			OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
		} // End scoped movement update
	}

	// Call external post-movement events. These happen after the scoped movement completes in case the events want to use the current state of overlaps etc.
	CallMovementUpdateDelegate(DeltaSeconds, OldLocation, OldVelocity);

	/*if (CharacterMovementCVars::BasedMovementMode == 0)
	{
		SaveBaseLocation(); // behaviour before implementing this fix
	}
	else
	{
		MaybeSaveBaseLocation();
	}*/
	MaybeSaveBaseLocation();
	UpdateComponentVelocity();

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

void UGTCharacterMovementComponent::PhysNavWalking(float deltaTime, int32 Iterations)
{
	//Super::PhysNavWalking(deltaTime, Iterations);
	//SCOPE_CYCLE_COUNTER(STAT_CharPhysNavWalking);

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

	RestorePreAdditiveRootMotionVelocity();

	// Ensure velocity is horizontal.
	MaintainHorizontalGroundVelocity();
	//devCode(ensureMsgf(!Velocity.ContainsNaN(), TEXT("PhysNavWalking: Velocity contains NaN before CalcVelocity (%s)\n%s"), *GetPathNameSafe(this), *Velocity.ToString()));

	//bound acceleration
	Acceleration.Z = 0.f;
	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		CalcVelocity(deltaTime, GroundFriction, false, GetMaxBrakingDeceleration());
		//devCode(ensureMsgf(!Velocity.ContainsNaN(), TEXT("PhysNavWalking: Velocity contains NaN after CalcVelocity (%s)\n%s"), *GetPathNameSafe(this), *Velocity.ToString()));
	}

	ApplyRootMotionToVelocity(deltaTime);

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
		//UE_LOG(LogNavMeshMovement, VeryVerbose, TEXT("%s using cached navmesh location! (bProjectNavMeshWalking = %d)"), *GetNameSafe(CharacterOwner), bProjectNavMeshWalking);
	}
	else
	{
		//SCOPE_CYCLE_COUNTER(STAT_CharNavProjectPoint);

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
		FVector NewLocation(AdjustedDest.X, AdjustedDest.Y, DestNavLocation.Location.Z);
		if (bProjectNavMeshWalking)
		{
			//SCOPE_CYCLE_COUNTER(STAT_CharNavProjectLocation);
			const float TotalCapsuleHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2.0f;
			const float UpOffset = TotalCapsuleHeight * FMath::Max(0.f, NavMeshProjectionHeightScaleUp);
			const float DownOffset = TotalCapsuleHeight * FMath::Max(0.f, NavMeshProjectionHeightScaleDown);
			NewLocation = ProjectLocationFromNavMesh(deltaTime, OldLocation, NewLocation, UpOffset, DownOffset);
		}

		FVector AdjustedDelta = NewLocation - OldLocation;

		if (!AdjustedDelta.IsNearlyZero())
		{
			FHitResult HitResult;
			SafeMoveUpdatedComponent(AdjustedDelta, UpdatedComponent->GetComponentQuat(), bSweepWhileNavWalking, HitResult);
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
