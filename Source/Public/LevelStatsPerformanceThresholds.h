#pragma once

#include <CoreMinimal.h>

namespace LevelStatsPerformanceThresholds
{
    enum class EEvaluator : uint8
    {
        LessThanOrEqual,
        GreaterThanOrEqual
    };

    struct FThresholdValues
    {
        float Danger;
        float Warning;
        float Good;

        FThresholdValues( float danger, float warning, float good );
    };
}
namespace Thresholds = LevelStatsPerformanceThresholds;

class FLevelStatsPerformanceThresholds
{
public:
    FLevelStatsPerformanceThresholds( FName name, Thresholds::EEvaluator eval, const Thresholds::FThresholdValues & values, FString units = TEXT( "" ) );

    static TMap< FName, FLevelStatsPerformanceThresholds > CreateDefaultThresholds();
    TSharedPtr< FJsonObject > ToJson() const;

private:
    static FLevelStatsPerformanceThresholds CreateFrameRateThreshold( const FName name );
    static FLevelStatsPerformanceThresholds CreateFrameTimeThreshold( const FName name );
    static FLevelStatsPerformanceThresholds CreatePercentageThreshold( const FName name );
    static FLevelStatsPerformanceThresholds CreateMemoryThreshold( const FName name );

    void ValidateThresholds() const;

    FName MetricName;
    Thresholds::EEvaluator Evaluator;
    Thresholds::FThresholdValues Values;
    FString Units;
};

FORCEINLINE FLevelStatsPerformanceThresholds FLevelStatsPerformanceThresholds::CreateFrameRateThreshold( const FName name )
{
    return FLevelStatsPerformanceThresholds( name,
        Thresholds::EEvaluator::GreaterThanOrEqual,
        Thresholds::FThresholdValues( 30.0f, 45.0f, 60.0f ),
        TEXT( "FPS" ) );
}

FORCEINLINE FLevelStatsPerformanceThresholds FLevelStatsPerformanceThresholds::CreateFrameTimeThreshold( const FName name )
{
    return FLevelStatsPerformanceThresholds( name,
        Thresholds::EEvaluator::LessThanOrEqual,
        Thresholds::FThresholdValues( 16.0f, 8.0f, 4.0f ),
        TEXT( "ms" ) );
}

FORCEINLINE FLevelStatsPerformanceThresholds FLevelStatsPerformanceThresholds::CreatePercentageThreshold( const FName name )
{
    return FLevelStatsPerformanceThresholds( name,
        Thresholds::EEvaluator::LessThanOrEqual,
        Thresholds::FThresholdValues( 50.0f, 30.0f, 15.0f ),
        TEXT( "ms" ) );
}

FORCEINLINE FLevelStatsPerformanceThresholds FLevelStatsPerformanceThresholds::CreateMemoryThreshold( const FName name )
{
    return FLevelStatsPerformanceThresholds( name,
        Thresholds::EEvaluator::LessThanOrEqual,
        Thresholds::FThresholdValues( 8192.0f, 4096.0f, 2048.0f ),
        TEXT( "MB" ) );
}
