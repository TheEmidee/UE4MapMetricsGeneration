#include "LevelStatsPerformanceThresholds.h"

#include <Dom/JsonObject.h>

Thresholds::FThresholdValues::FThresholdValues( const float danger, const float warning, const float good ) :
    Danger( danger ),
    Warning( warning ),
    Good( good )
{
    checkf( good != warning && warning != danger,
        TEXT( "Threshold values must be distinct" ) );
}

FLevelStatsPerformanceThresholds::FLevelStatsPerformanceThresholds( const FName name, const Thresholds::EEvaluator eval, const Thresholds::FThresholdValues & values, FString units ) :
    MetricName( name ),
    Evaluator( eval ),
    Values( values ),
    Units( MoveTemp( units ) )
{
    ValidateThresholds();
}

TMap< FName, FLevelStatsPerformanceThresholds > FLevelStatsPerformanceThresholds::CreateDefaultThresholds()
{
    TMap< FName, FLevelStatsPerformanceThresholds > Thresholds;

    // :NOTE: Core Performance
    Thresholds.Add( TEXT( "AverageFramerate" ),
        CreateFrameRateThreshold( TEXT( "AverageFramerate" ) ) );

    // :NOTE: Frame Times
    Thresholds.Add( TEXT( "GameThread_Avg" ),
        CreateFrameTimeThreshold( TEXT( "GameThread_Avg" ) ) );
    Thresholds.Add( TEXT( "RenderThread_Avg" ),
        CreateFrameTimeThreshold( TEXT( "RenderThread_Avg" ) ) );
    Thresholds.Add( TEXT( "GPU_Avg" ),
        CreateFrameTimeThreshold( TEXT( "GPU_Avg" ) ) );

    // :NOTE: Thread Boundedness
    Thresholds.Add( TEXT( "GameThread_Bound_Pct" ),
        CreatePercentageThreshold( TEXT( "GameThread_Bound_Pct" ) ) );
    Thresholds.Add( TEXT( "RenderThread_Bound_Pct" ),
        CreatePercentageThreshold( TEXT( "RenderThread_Bound_Pct" ) ) );
    Thresholds.Add( TEXT( "GPU_Bound_Pct" ),
        CreatePercentageThreshold( TEXT( "GPU_Bound_Pct" ) ) );

    // :NOTE: Hitching
    Thresholds.Add( TEXT( "Total_Hitches" ),
        FLevelStatsPerformanceThresholds( TEXT( "Total_Hitches" ),
            Thresholds::EEvaluator::LessThanOrEqual,
            Thresholds::FThresholdValues( 10.0f, 5.0f, 2.0f ),
            TEXT( "count" ) ) );

    Thresholds.Add( TEXT( "Hitches_Per_Minute" ),
        FLevelStatsPerformanceThresholds( TEXT( "Hitches_Per_Minute" ),
            Thresholds::EEvaluator::LessThanOrEqual,
            Thresholds::FThresholdValues( 6.0f, 3.0f, 1.0f ),
            TEXT( "count/min" ) ) );

    Thresholds.Add( TEXT( "Average_Hitch_Length_MS" ),
        CreateFrameTimeThreshold( TEXT( "Average_Hitch_Length_MS" ) ) );

    // :NOTE: Memory
    Thresholds.Add( TEXT( "Physical_Memory_Used_MB" ),
        CreateMemoryThreshold( TEXT( "Physical_Memory_Used_MB" ) ) );
    Thresholds.Add( TEXT( "Memory_Pressure_Frames_Pct" ),
        CreatePercentageThreshold( TEXT( "Memory_Pressure_Frames_Pct" ) ) );

    // :NOTE: Rendering
    Thresholds.Add( TEXT( "Average_DrawCalls" ),
        FLevelStatsPerformanceThresholds( TEXT( "Average_DrawCalls" ),
            Thresholds::EEvaluator::LessThanOrEqual,
            Thresholds::FThresholdValues( 3000.0f, 2000.0f, 1000.0f ),
            TEXT( "drawcalls" ) ) );

    Thresholds.Add( TEXT( "Peak_DrawCalls" ),
        FLevelStatsPerformanceThresholds( TEXT( "Peak_DrawCalls" ),
            Thresholds::EEvaluator::LessThanOrEqual,
            Thresholds::FThresholdValues( 4000.0f, 3000.0f, 2000.0f ),
            TEXT( "drawcalls" ) ) );

    return Thresholds;
}

TSharedPtr< FJsonObject > FLevelStatsPerformanceThresholds::ToJson() const
{
    const auto threshold_object = MakeShared< FJsonObject >();

    threshold_object->SetStringField( TEXT( "Evaluator" ),
        Evaluator == Thresholds::EEvaluator::LessThanOrEqual ? TEXT( "<=" ) : TEXT( ">=" ) );
    threshold_object->SetNumberField( TEXT( "Danger" ), Values.Danger );
    threshold_object->SetNumberField( TEXT( "Warning" ), Values.Warning );
    threshold_object->SetNumberField( TEXT( "Good" ), Values.Good );

    if ( !Units.IsEmpty() )
    {
        threshold_object->SetStringField( TEXT( "Units" ), Units );
    }

    return threshold_object;
}

void FLevelStatsPerformanceThresholds::ValidateThresholds() const
{
    if ( Evaluator == Thresholds::EEvaluator::LessThanOrEqual )
    {
        checkf( Values.Good < Values.Warning && Values.Warning < Values.Danger,
            TEXT( "For <= evaluator, Good must be less than Warning, and Warning must be less than Danger. Metric: %s" ),
            *MetricName.ToString() );
    }
    else
    {
        checkf( Values.Good > Values.Warning && Values.Warning > Values.Danger,
            TEXT( "For >= evaluator, Good must be greater than Warning, and Warning must be greater than Danger. Metric: %s" ),
            *MetricName.ToString() );
    }
}