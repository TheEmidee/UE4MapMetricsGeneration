#pragma once

#include <CoreMinimal.h>

struct FLevelStatsSettings;

class FLevelStatsPerformanceReport
{
public:
    void Initialize( const UWorld * world, const FLevelStatsSettings & settings );
    void StartNewCell( int32 cell_index, const FVector & center, float ground_height, float actor_height );

    void AddRotationData(
        const float rotation,
        const FStringView screenshot_path,
        const TSharedPtr< FJsonObject > & metrics ) const;

    void FinishCurrentCell();
    void FinalizeAndSave( const FStringView base_path, int32 total_captures ) const;

private:
    void SaveJsonToFile( const TSharedPtr< FJsonObject > & json_object, const FStringView path ) const;

    TSharedPtr< FJsonObject > CaptureReport;
    TSharedPtr< FJsonObject > CurrentCellObject;
    FDateTime CaptureStartTime;
};