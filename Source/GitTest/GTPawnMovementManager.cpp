// Fill out your copyright notice in the Description page of Project Settings.


#include "GTPawnMovementManager.h"

#include "GTCharacterMovementComponent.h"
#include "GTPawnMovementComponent.h"
#include "GameFramework/Character.h"

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
		if(UGTCharacterMovementComponent* MovementComponent = Cast<UGTCharacterMovementComponent>(PawnMovement->GetComponentByClass(UGTCharacterMovementComponent::StaticClass())))
		{
			MovementComponent->UpdateMovement(DeltaTime);
		}
	}
}

