// Fill out your copyright notice in the Description page of Project Settings.


#include "GTCharacterUnit.h"

#include "GTCharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Canvas.h"
#include "Net/UnrealNetwork.h"


AGTCharacterUnit::AGTCharacterUnit()
{
	PrimaryActorTick.bCanEverTick = false;

	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>("Capsule Component");
	SetRootComponent(CapsuleComponent);
	SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>("Skeletal Mesh");
	//SkeletalMeshComponent->SetMobility(EComponentMobility::Movable);
	//SetRootComponent(SkeletalMeshComponent);
	SkeletalMeshComponent->SetupAttachment(CapsuleComponent);
	SkeletalMeshComponent->PrimaryComponentTick.bCanEverTick = false;
	
	SkeletalMeshComponent->bUseAttachParentBound = true;
	//SkeletalMeshComponent->SetupAttachment(CapsuleComponent);
	
	//CharacterMovementComponent = CreateDefaultSubobject<UCharacterMovementComponent>("Character Movement Component");
	//CharacterMovementComponent->UpdatedComponent = SkeletalMeshComponent;
	//FloatingPawnMovement = CreateDefaultSubobject<UFloatingPawnMovement>("Movement");
	//PawnMovementComponent = CreateDefaultSubobject<UPawnMovementComponent>("Movement component");
	//PawnMovementComponent->UpdatedComponent = GetRootComponent();
	PawnMovementComponent = CreateDefaultSubobject<UGTPawnMovementComponent>("Pawn");
	//PawnMovementComponent->UpdatedComponent = GetRootComponent();
}

void AGTCharacterUnit::BeginPlay()
{
	Super::BeginPlay();
	SkeletalMeshComponent->bComputeBoundsOnceForGame = true;
	SkeletalMeshComponent->bComputeFastLocalBounds = true;
	//PawnMovementComponent->UpdatedComponent = GetRootComponent();
}




