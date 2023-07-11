// Fill out your copyright notice in the Description page of Project Settings.


#include "GTPawnMovementManager.h"

#include "GTPawnMovementComponent.h"

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

	for (const auto PawnMovement : PawnMovements)
	{
		if(UGTPawnMovementComponent* MovementComponent = Cast<UGTPawnMovementComponent>(PawnMovement->GetComponentByClass(UGTPawnMovementComponent::StaticClass())))
		{
			MovementComponent->UpdateMovement(DeltaTime);
		}
	}
}

