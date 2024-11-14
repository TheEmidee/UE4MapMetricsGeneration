#pragma once

#include <CoreMinimal.h>

DECLARE_LOG_CATEGORY_EXTERN( LogLevelStatsCollectorGrid, Log, All );

class FLevelStatsGridConfiguration
{
    friend class ALevelStatsCollector;
    friend class FWaitingForSnapshotState;

public:
    FLevelStatsGridConfiguration();

    void Initialize( const FVector & center_offset, float cell_size );
    void CalculateBounds( UWorld * world );
    void GenerateCells();
    void LogGridInfo() const;
    bool IsValidCellIndex( int32 index ) const;

private:
    void FinalizeBounds( const FBox & bounds );

    struct FGridCell
    {
        explicit FGridCell( const FVector & center = FVector::ZeroVector ) :
            Center( center ),
            GroundHeight( 0.0f ),
            CameraHeight( 0.0f )
        {}

        FVector Center;
        float GroundHeight;
        float CameraHeight;
    };

    FVector GridCenterOffset;
    FIntPoint GridDimensions;
    FBox GridBounds;
    float GridSizeX;
    float GridSizeY;
    float CellSize;
    TArray< FGridCell > GridCells;
};

FORCEINLINE bool FLevelStatsGridConfiguration::IsValidCellIndex( const int32 index ) const
{
    return GridCells.IsValidIndex( index );
}
