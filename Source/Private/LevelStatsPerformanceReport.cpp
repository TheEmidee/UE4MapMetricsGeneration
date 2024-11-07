#include "LevelStatsPerformanceReport.h"

#include "LevelStatsCollector.h"

void FLevelStatsPerformanceReport::Initialize( const UWorld * world, const FLevelStatsSettings & settings )
{
    CaptureStartTime = FDateTime::Now();

    CaptureReport = MakeShared< FJsonObject >();
    CaptureReport->SetStringField( "CaptureTime", CaptureStartTime.ToString() );
    CaptureReport->SetStringField( "MapName", world->GetMapName() );

    const auto settings_object = MakeShared< FJsonObject >();
    settings_object->SetNumberField( "CellSize", settings.CellSize );
    settings_object->SetNumberField( "CameraHeight", settings.CameraHeight );
    settings_object->SetNumberField( "CameraHeightOffset", settings.CameraHeightOffset );
    settings_object->SetNumberField( "CameraRotationDelta", settings.CameraRotationDelta );
    settings_object->SetNumberField( "MetricsDuration", settings.MetricsDuration );
    CaptureReport->SetObjectField( "Settings", settings_object );

    CaptureReport->SetArrayField( TEXT( "Cells" ), TArray< TSharedPtr< FJsonValue > >() );
}

void FLevelStatsPerformanceReport::StartNewCell( const int32 cell_index, const FVector & center, const float ground_height )
{
    CurrentCellObject = MakeShared< FJsonObject >();

    CurrentCellObject->SetNumberField( "Index", cell_index );

    const auto position_object = MakeShared< FJsonObject >();
    position_object->SetNumberField( "X", center.X );
    position_object->SetNumberField( "Y", center.Y );
    position_object->SetNumberField( "Z", center.Z );
    position_object->SetNumberField( "GroundHeight", ground_height );
    CurrentCellObject->SetObjectField( "Position", position_object );

    CurrentCellObject->SetArrayField( "Rotations", TArray< TSharedPtr< FJsonValue > >() );
}

void FLevelStatsPerformanceReport::AddRotationData(
    const int32 cell_index,
    const float rotation,
    const FStringView screenshot_path,
    const TSharedPtr< FJsonObject > & metrics,
    const FString & output_path ) const
{
    const auto rotation_object = MakeShared< FJsonObject >();
    rotation_object->SetNumberField( TEXT( "Angle" ), rotation );
    rotation_object->SetStringField( TEXT( "Screenshot" ), FString( screenshot_path ) );
    rotation_object->SetObjectField( TEXT( "Metrics" ), metrics );

    if ( CurrentCellObject.IsValid() )
    {
        auto rotations = CurrentCellObject->GetArrayField( TEXT( "Rotations" ) );
        rotations.Add( MakeShared< FJsonValueObject >( rotation_object ) );
        CurrentCellObject->SetArrayField( TEXT( "Rotations" ), rotations );
    }

    const auto position_obj = CurrentCellObject->GetObjectField( TEXT( "Position" ) );
    const auto rotation_report = CreateRotationReport(
        cell_index,
        rotation,
        position_obj->GetNumberField( TEXT( "X" ) ),
        position_obj->GetNumberField( TEXT( "Y" ) ),
        position_obj->GetNumberField( TEXT( "Z" ) ),
        position_obj->GetNumberField( TEXT( "GroundHeight" ) ),
        screenshot_path,
        metrics );

    SaveJsonToFile( rotation_report, output_path + TEXT( "metrics.json" ) );
}

void FLevelStatsPerformanceReport::FinalizeAndSave( const FString & base_path, const int32 total_captures ) const
{
    if ( !CaptureReport.IsValid() )
    {
        return;
    }

    CaptureReport->SetStringField( "CaptureEndTime", FDateTime::Now().ToString() );
    CaptureReport->SetNumberField( "TotalCaptureCount", total_captures );

    SaveJsonToFile( CaptureReport, base_path + TEXT( "capture_report.json" ) );
}

TSharedPtr< FJsonObject > FLevelStatsPerformanceReport::CreateRotationReport(
    const int32 cell_index,
    const float rotation,
    const float pos_x,
    const float pos_y,
    const float pos_z,
    const float ground_height,
    FStringView screenshot_path,
    const TSharedPtr< FJsonObject > & metrics ) const
{
    const auto rotation_report = MakeShared< FJsonObject >();

    rotation_report->SetStringField( TEXT( "CaptureTime" ), FDateTime::Now().ToString() );
    rotation_report->SetNumberField( TEXT( "CellIndex" ), cell_index );
    rotation_report->SetNumberField( TEXT( "Rotation" ), rotation );

    const auto position_object = MakeShared< FJsonObject >();
    position_object->SetNumberField( TEXT( "X" ), pos_x );
    position_object->SetNumberField( TEXT( "Y" ), pos_y );
    position_object->SetNumberField( TEXT( "Z" ), pos_z );
    position_object->SetNumberField( TEXT( "GroundHeight" ), ground_height );
    rotation_report->SetObjectField( TEXT( "CellPosition" ), position_object );

    rotation_report->SetStringField( TEXT( "Screenshot" ), FString( screenshot_path ) );

    if ( metrics.IsValid() )
    {
        rotation_report->SetObjectField( TEXT( "Metrics" ), metrics );
    }

    return rotation_report;
}

void FLevelStatsPerformanceReport::SaveJsonToFile( const TSharedPtr< FJsonObject > & json_object, const FString & path ) const
{
    FString output_string;
    const auto writer = TJsonWriterFactory<>::Create( &output_string );

    if ( FJsonSerializer::Serialize( json_object.ToSharedRef(), writer ) &&
         FFileHelper::SaveStringToFile( output_string, *path ) )
    {
        UE_LOG( LogLevelStatsCollector, Log, TEXT( "Saved JSON report to: %s" ), *path );
    }
    else
    {
        UE_LOG( LogLevelStatsCollector, Error, TEXT( "Failed to save JSON report to: %s" ), *path );
    }
}