#include "LevelStatsGridConfiguration.h"

#include <Engine/LevelBounds.h>

DEFINE_LOG_CATEGORY( LogLevelStatsCollectorGrid );

FLevelStatsGridConfiguration::FLevelStatsGridConfiguration() :
    GridDimensions(),
    GridSizeX( 0.0f ),
    GridSizeY( 0.0f ),
    CellSize( 0.0f )
{}

void FLevelStatsGridConfiguration::Initialize( const FVector & center_offset, const float cell_size )
{
    GridCenterOffset = center_offset;
    CellSize = cell_size;
}

void FLevelStatsGridConfiguration::CalculateBounds( UWorld * world )
{
    FBox level_bounds;
    // :NOTE: Use explicit grid dimensions if provided
    if ( GridSizeX > 0.0f && GridSizeY > 0.0f )
    {
        const auto half_size_x = GridSizeX * 0.5f;
        const auto half_size_y = GridSizeY * 0.5f;

        level_bounds = FBox(
            FVector( -half_size_x, -half_size_y, 0.0f ),
            FVector( half_size_x, half_size_y, 0.0f ) );

        UE_LOG( LogLevelStatsCollectorGrid, Log, TEXT( "Using explicit grid dimensions: %f x %f" ), GridSizeX, GridSizeY );
        FinalizeBounds( level_bounds );
        return;
    }

    // :NOTE: Use LevelBoundsActor if no explicit dimensions provided
    if ( const auto * current_level = world->GetCurrentLevel() )
    {
        if ( const auto * level_bounds_actor = current_level->LevelBoundsActor.Get() )
        {
            level_bounds = level_bounds_actor->GetComponentsBoundingBox( true );
            if ( level_bounds.IsValid )
            {
                UE_LOG( LogLevelStatsCollectorGrid, Log, TEXT( "Got bounds from LevelBoundsActor: %s" ), *level_bounds.ToString() );
                FinalizeBounds( level_bounds );
                return;
            }
        }
    }

    // :NOTE: Use default area if no bounds found
    UE_LOG( LogLevelStatsCollectorGrid, Warning, TEXT( "No valid bounds source found, using default 10000x10000 area" ) );
    level_bounds = FBox( FVector( -5000, -5000, 0 ), FVector( 5000, 5000, 0 ) ); // Arbitrary default area
    FinalizeBounds( level_bounds );
}

void FLevelStatsGridConfiguration::GenerateCells()
{
    const auto total_cells = GridDimensions.X * GridDimensions.Y;
    GridCells.Empty( total_cells );

    for ( auto y = 0; y < GridDimensions.Y; ++y )
    {
        for ( auto x = 0; x < GridDimensions.X; ++x )
        {
            GridCells.Emplace( GridBounds.Min + FVector( x * CellSize + CellSize / 2, y * CellSize + CellSize / 2, 0.0f ) );
        }
    }
}

void FLevelStatsGridConfiguration::LogGridInfo() const
{
    UE_LOG( LogLevelStatsCollectorGrid, Log, TEXT( "Grid Configuration:" ) );
    UE_LOG( LogLevelStatsCollectorGrid, Log, TEXT( "  Bounds: Min(%s), Max(%s)" ), *GridBounds.Min.ToString(), *GridBounds.Max.ToString() );
    UE_LOG( LogLevelStatsCollectorGrid, Log, TEXT( "  Dimensions: %dx%d cells" ), GridDimensions.X, GridDimensions.Y );
    UE_LOG( LogLevelStatsCollectorGrid, Log, TEXT( "  Cell Size: %f" ), CellSize );
    UE_LOG( LogLevelStatsCollectorGrid, Log, TEXT( "  Total Cells: %d" ), GridCells.Num() );
    UE_LOG( LogLevelStatsCollectorGrid, Log, TEXT( "  Center Offset: %s" ), *GridCenterOffset.ToString() );
}

void FLevelStatsGridConfiguration::FinalizeBounds( const FBox & bounds )
{
    FBox level_bounds = bounds;

    // :NOTE: Add padding to ensure we capture the edges properly
    const auto bounds_padding = CellSize * 0.5f;
    level_bounds = level_bounds.ExpandBy( FVector( bounds_padding, bounds_padding, 0 ) );

    const auto origin = level_bounds.GetCenter();
    const auto extent = level_bounds.GetExtent();

    GridBounds.Min = FVector(
                         FMath::FloorToFloat( origin.X - extent.X ) / CellSize * CellSize,
                         FMath::FloorToFloat( origin.Y - extent.Y ) / CellSize * CellSize,
                         0 ) +
                     GridCenterOffset;

    GridBounds.Max = FVector(
                         FMath::CeilToFloat( origin.X + extent.X ) / CellSize * CellSize,
                         FMath::CeilToFloat( origin.Y + extent.Y ) / CellSize * CellSize,
                         0 ) +
                     GridCenterOffset;

    const auto grid_size = GridBounds.Max - GridBounds.Min;
    GridDimensions = FIntPoint(
        FMath::CeilToInt( grid_size.X / CellSize ),
        FMath::CeilToInt( grid_size.Y / CellSize ) );

    const auto expected_size_x = GridDimensions.X * CellSize;
    const auto expected_size_y = GridDimensions.Y * CellSize;
    const auto actual_size_x = grid_size.X;
    const auto actual_size_y = grid_size.Y;

    if ( !FMath::IsNearlyEqual( expected_size_x, actual_size_x, KINDA_SMALL_NUMBER ) ||
         !FMath::IsNearlyEqual( expected_size_y, actual_size_y, KINDA_SMALL_NUMBER ) )
    {
        GridBounds.Max = GridBounds.Min + FVector( expected_size_x, expected_size_y, 0.0f );

        UE_LOG( LogLevelStatsCollectorGrid,
            Warning,
            TEXT( "Grid size adjusted for cell alignment. Original: (%f, %f), Adjusted: (%f, %f)" ),
            actual_size_x,
            actual_size_y,
            expected_size_x,
            expected_size_y );
    }

    GridSizeX = expected_size_x;
    GridSizeY = expected_size_y;
}