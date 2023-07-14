// Fill out your copyright notice in the Description page of Project Settings.


#include "GTPawnMovementManager.h"
#include "GTCharacterMovementComponent.h"

AGTPawnMovementManager::AGTPawnMovementManager()
{
	PrimaryActorTick.bCanEverTick = true;

}

void AGTPawnMovementManager::BeginPlay()
{
	Super::BeginPlay();
	
}

void AGTPawnMovementManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	for (const auto MovementComponent : MovementComponents)
	{
		MovementComponent->UpdateMovement(DeltaTime);
	}
}

