#include "SupplyDropActor.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "ShowdownBattleRoyaleSubsystem.h"
#include "MyCharacter.h"
#include "UObject/ConstructorHelpers.h"

ASupplyDropActor::ASupplyDropActor()
{
	PrimaryActorTick.bCanEverTick = false;

	PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
	SetRootComponent(PickupSphere);
	PickupSphere->InitSphereRadius(160.0f);
	PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PickupSphere->SetGenerateOverlapEvents(true);

	SupplyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SupplyMesh"));
	SupplyMesh->SetupAttachment(GetRootComponent());
	SupplyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SupplyMesh->SetRelativeScale3D(FVector(1.2f, 1.2f, 0.6f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		SupplyMesh->SetStaticMesh(CubeMesh.Object);
	}
}

void ASupplyDropActor::BeginPlay()
{
	Super::BeginPlay();

	PickupSphere->OnComponentBeginOverlap.AddDynamic(this, &ASupplyDropActor::HandlePickupOverlap);
}

FVector2D ASupplyDropActor::GetMapLocation2D() const
{
	const FVector Location = GetActorLocation();
	return FVector2D(Location.X, Location.Y);
}

void ASupplyDropActor::HandlePickupOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	AMyCharacter* Character = Cast<AMyCharacter>(OtherActor);
	if (!Character)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (UShowdownBattleRoyaleSubsystem* Subsystem = World->GetSubsystem<UShowdownBattleRoyaleSubsystem>())
		{
			Subsystem->TryPickupSupply(this, Character);
		}
	}
}
