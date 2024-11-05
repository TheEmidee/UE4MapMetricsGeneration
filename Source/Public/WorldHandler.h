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