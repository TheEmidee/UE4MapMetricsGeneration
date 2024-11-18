﻿#include "LevelStatsPerformanceReport.h"

#include "LevelStatsCollector.h"

void FLevelStatsPerformanceReport::Initialize( const UWorld * world, const FLevelStatsSettings & settings )
{
    CaptureStartTime = FDateTime::Now();

    CaptureReport = MakeShared< FJsonObject >();
    CaptureReport->SetStringField( TEXT( "CaptureTime" ), CaptureStartTime.ToString() );
    CaptureReport->SetStringField( TEXT( "MapName" ), world->GetMapName() );

    const auto settings_object = MakeShared< FJsonObject >();
    settings_object->SetNumberField( TEXT( "CellSize" ), settings.CellSize );
    settings_object->SetNumberField( TEXT( "CameraHeight" ), settings.CameraHeight );
    settings_object->SetNumberField( TEXT( "CameraHeightOffset" ), settings.CameraHeightOffset );
    settings_object->SetNumberField( TEXT( "CameraRotationDelta" ), settings.CameraRotationDelta );
    settings_object->SetNumberField( TEXT( "MetricsDuration" ), settings.MetricsDuration );
    CaptureReport->SetObjectField( TEXT( "Settings" ), settings_object );

    CaptureReport->SetArrayField( TEXT( "Cells" ), TArray< TSharedPtr< FJsonValue > >() );
}

void FLevelStatsPerformanceReport::StartNewCell( const int32 cell_index, const FVector & center, const float ground_height, const float actor_height )
{
    CurrentCellObject = MakeShared< FJsonObject >();

    CurrentCellObject->SetNumberField( TEXT( "Index" ), cell_index );

    const auto position_object = MakeShared< FJsonObject >();
    position_object->SetNumberField( TEXT( "X" ), center.X );
    position_object->SetNumberField( TEXT( "Y" ), center.Y );
    position_object->SetNumberField( TEXT( "Z" ), center.Z );
    position_object->SetNumberField( TEXT( "GroundHeight" ), ground_height );
    position_object->SetNumberField( TEXT( "ActorHeight" ), actor_height );
    CurrentCellObject->SetObjectField( TEXT( "Position" ), position_object );

    CurrentCellObject->SetArrayField( TEXT( "Rotations" ), TArray< TSharedPtr< FJsonValue > >() );
}

void FLevelStatsPerformanceReport::AddRotationData(
    const float rotation,
    const FStringView screenshot_path,
    const TSharedPtr< FJsonObject > & metrics ) const
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
}

void FLevelStatsPerformanceReport::FinishCurrentCell()
{
    if ( CurrentCellObject.IsValid() && CaptureReport.IsValid() )
    {
        auto cells = CaptureReport->GetArrayField( TEXT( "Cells" ) );
        cells.Add( MakeShared< FJsonValueObject >( CurrentCellObject ) );
        CaptureReport->SetArrayField( TEXT( "Cells" ), cells );
    }
}

void FLevelStatsPerformanceReport::FinalizeAndSave( const FStringView base_path, const int32 total_captures ) const
{
    if ( !CaptureReport.IsValid() )
    {
        return;
    }

    if ( CurrentCellObject.IsValid() )
    {
        auto cells = CaptureReport->GetArrayField( TEXT( "Cells" ) );
        cells.Add( MakeShared< FJsonValueObject >( CurrentCellObject ) );
        CaptureReport->SetArrayField( TEXT( "Cells" ), cells );
    }

    CaptureReport->SetStringField( "CaptureEndTime", FDateTime::Now().ToString() );
    CaptureReport->SetNumberField( "TotalCaptureCount", total_captures );

    SaveJsonToFile( CaptureReport, FString::Printf( TEXT( "%scapture_report.json" ), *FString( base_path ) ) );
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

void FLevelStatsPerformanceReport::SaveJsonToFile( const TSharedPtr< FJsonObject > & json_object, const FStringView path ) const
{
    FString output_string;
    const auto writer = TJsonWriterFactory<>::Create( &output_string );

    if ( FJsonSerializer::Serialize( json_object.ToSharedRef(), writer ) &&
         FFileHelper::SaveStringToFile( output_string, *FString( path ) ) )
    {
        UE_LOG( LogLevelStatsCollector, Log, TEXT( "Saved JSON report to: %s" ), *FString( path ) );
    }
    else
    {
        UE_LOG( LogLevelStatsCollector, Error, TEXT( "Failed to save JSON report to: %s" ), *FString( path ) );
    }
}