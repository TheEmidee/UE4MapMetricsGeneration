#pragma once

#include <Engine/World.h>
#include <WorldPartition/WorldPartition.h>

class FWorldHandler
{
public:
    explicit FWorldHandler( UWorld * world ) :
        World( world )
    {
        check( World );
        Initialize();
    }
    ~FWorldHandler()
    {
        Cleanup();
    }

    FWorldHandler( const FWorldHandler & ) = delete;
    FWorldHandler & operator=( const FWorldHandler & ) = delete;
    UWorld * GetWorld() const
    {
        return World;
    }

private:
    void Initialize() const
    {
        // :NOTE: Initialize World Partition if it exists
        if ( World->GetWorldPartition() )
        {
            World->GetWorldPartition()->Initialize( World, FTransform::Identity );
        }

        // :NOTE: Setup world
        World->WorldType = EWorldType::Editor;
        World->AddToRoot();

        if ( !World->bIsWorldInitialized )
        {
            UWorld::InitializationValues IVs;
            IVs.RequiresHitProxies( true )
                .ShouldSimulatePhysics( true )
                .EnableTraceCollision( true )
                .CreateNavigation( true )
                .CreateAISystem( true )
                .AllowAudioPlayback( true )
                .CreatePhysicsScene( true );

            World->InitWorld( IVs );
            World->PersistentLevel->UpdateModelComponents();
            World->UpdateWorldComponents( true, false );
        }
    }

    void Cleanup() const
    {
        if ( World->GetWorldPartition() )
        {
            World->GetWorldPartition()->Uninitialize();
        }

        World->CleanupWorld();
        World->RemoveFromRoot();
        World->FlushLevelStreaming( EFlushLevelStreamingType::Full );

        // :NOTE: Force garbage collection
        CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );
    }

    UWorld * World;
};

/*
if ( world->GetWorldPartition() )
{
UE_LOG( LogPerfGrapher, Log, TEXT( "World Partition found, initializing..." ) );
world->GetWorldPartition()->Initialize( world, FTransform::Identity );
}
UE_LOG( LogPerfGrapher, Log, TEXT( "World Partition initialized" ) );

// :NOTE: Load World
world->WorldType = EWorldType::Editor;
world->AddToRoot();

if ( !world->bIsWorldInitialized )
{
UWorld::InitializationValues ivs;
ivs.RequiresHitProxies( true );
ivs.ShouldSimulatePhysics( true );
ivs.EnableTraceCollision( true );
ivs.CreateNavigation( true );
ivs.CreateAISystem( true );
ivs.AllowAudioPlayback( true );
ivs.CreatePhysicsScene( true );

world->InitWorld( ivs );
world->PersistentLevel->UpdateModelComponents();
world->UpdateWorldComponents( true, false );
}
UE_LOG( LogPerfGrapher, Log, TEXT( "World %s initialized" ), *world->GetName() );

// :NOTE: Spawn collector
const auto * collector = world->SpawnActor< ALevelStatsCollector >( FVector::ZeroVector, FRotator::ZeroRotator );

UE_LOG( LogPerfGrapher, Log, TEXT( "Attempting to spawn observer..." ) );
if ( collector == nullptr )
{
UE_LOG( LogPerfGrapher, Error, TEXT( "Failed to spawn observer" ) );
return false;
}
UE_LOG( LogPerfGrapher, Log, TEXT( "Observer spawned successfully at location %s" ), *collector->GetActorLocation().ToString() );

// :NOTE: Cleanup
if ( world->GetWorldPartition() )
{
world->GetWorldPartition()->Uninitialize();
}

world->CleanupWorld();
world->RemoveFromRoot();
world->FlushLevelStreaming( EFlushLevelStreamingType::Full );
UE_LOG( LogPerfGrapher, Log, TEXT( "World %s cleaned up" ), *world->GetName() );

// :NOTE: Force garbage collection to clean up
CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );
UE_LOG( LogPerfGrapher, Log, TEXT( "Garbage collection completed" ) );
*/