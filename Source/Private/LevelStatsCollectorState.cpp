#include "LevelStatsCollectorState.h"

#include "LevelStatsCollector.h"

FLevelStatsCollectorState::FLevelStatsCollectorState( ALevelStatsCollector * collector ) :
    Collector( collector )
{}

void FLevelStatsCollectorState::Enter()
{}

void FLevelStatsCollectorState::Exit()
{}

// :NOTE: FIdleState Implementation
FIdleState::FIdleState( ALevelStatsCollector * collector ) :
    FLevelStatsCollectorState( collector ),
    CurrentDelay( 0.0f )
{}

void FIdleState::Enter()
{
    CurrentDelay = 0.0f;
}

void FIdleState::Tick( const float delta_time )
{
    CurrentDelay += delta_time;
    if ( CurrentDelay >= Collector->MetricsWaitDelay )
    {
        Collector->TransitionToState( MakeShared< FCapturingMetricsState >( Collector ) );
    }
}

void FIdleState::Exit()
{
    CurrentDelay = 0.0f;
}

// :NOTE: FCapturingMetricsState Implementation
FCapturingMetricsState::FCapturingMetricsState( ALevelStatsCollector * collector ) :
    FLevelStatsCollectorState( collector ),
    CurrentCaptureTime( 0.0f )
{}

void FCapturingMetricsState::Enter()
{
    CurrentCaptureTime = 0.0f;
    Collector->StartMetricsCapture();
}

void FCapturingMetricsState::Tick( const float delta_time )
{
    CurrentCaptureTime += delta_time;

    if ( CurrentCaptureTime >= Collector->MetricsDuration )
    {
        Collector->TransitionToState( MakeShared< FWaitingForSnapshotState >( Collector ) );
    }
}

void FCapturingMetricsState::Exit()
{
    Collector->FinishMetricsCapture();
}

// :NOTE: FWaitingForSnapshotState Implementation
FWaitingForSnapshotState::FWaitingForSnapshotState( ALevelStatsCollector * collector ) :
    FLevelStatsCollectorState( collector ),
    CurrentDelay( 0.0f )
{}

void FWaitingForSnapshotState::Enter()
{
    CurrentDelay = 0.0f;
}

void FWaitingForSnapshotState::Tick( const float delta_time )
{
    CurrentDelay += delta_time;
    if ( CurrentDelay >= Collector->CaptureDelay )
    {
        Collector->CaptureCurrentView();
        Collector->TransitionToState( MakeShared< FProcessingNextRotationState >( Collector ) );
    }
}

void FWaitingForSnapshotState::Exit()
{
    CurrentDelay = 0.0f;
}

// :NOTE: FProcessingNextRotationState Implementation
FProcessingNextRotationState::FProcessingNextRotationState( ALevelStatsCollector * collector ) :
    FLevelStatsCollectorState( collector )
{}

void FProcessingNextRotationState::Enter()
{
    Collector->UpdateRotation();
}

void FProcessingNextRotationState::Tick( float delta_time )
{
    if ( Collector->CurrentRotation >= 360.0f )
    {
        Collector->IncrementCellIndex();
        Collector->TransitionToState( MakeShared< FProcessingNextCellState >( Collector ) );
    }
    else
    {
        Collector->TransitionToState( MakeShared< FIdleState >( Collector ) );
    }
}

void FProcessingNextRotationState::Exit()
{}

// :NOTE: FProcessingNextCellState Implementation
FProcessingNextCellState::FProcessingNextCellState( ALevelStatsCollector * collector ) :
    FLevelStatsCollectorState( collector )
{}

void FProcessingNextCellState::Enter()
{
}

void FProcessingNextCellState::Tick( float delta_time )
{
    if ( Collector->ProcessNextCell() )
    {
        Collector->TransitionToState( MakeShared< FIdleState >( Collector ) );
    }
    else
    {
        Collector->FinishCapture();
    }
}

void FProcessingNextCellState::Exit()
{}