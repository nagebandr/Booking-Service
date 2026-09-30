# Test script in PowerShell: Concurrent booking of the same seat
$ServerUrl = "http://localhost:8080"
$EventId = 1
$SeatId = 12

Write-Host "=========================================================="
Write-Host " Testing Race Condition: User A and User B -> Seat #$SeatId"
Write-Host "=========================================================="

$JobA = Start-ThreadJob -ScriptBlock {
    param($url, $eventId, $seatId)
    $body = @{ user_id = 1; seat_id = $seatId } | ConvertTo-Json
    try {
        $res = Invoke-RestMethod -Uri "$url/events/$eventId/bookings" -Method Post -Body $body -ContentType "application/json"
        return @{ User = "User A"; Status = 201; Data = $res }
    } catch {
        return @{ User = "User A"; Status = $_.Exception.Response.StatusCode.value__; Message = $_.ErrorDetails.Message }
    }
} -ArgumentList $ServerUrl, $EventId, $SeatId

$JobB = Start-ThreadJob -ScriptBlock {
    param($url, $eventId, $seatId)
    $body = @{ user_id = 2; seat_id = $seatId } | ConvertTo-Json
    try {
        $res = Invoke-RestMethod -Uri "$url/events/$eventId/bookings" -Method Post -Body $body -ContentType "application/json"
        return @{ User = "User B"; Status = 201; Data = $res }
    } catch {
        return @{ User = "User B"; Status = $_.Exception.Response.StatusCode.value__; Message = $_.ErrorDetails.Message }
    }
} -ArgumentList $ServerUrl, $EventId, $SeatId

$ResultA = Receive-Job -Job $JobA -Wait
$ResultB = Receive-Job -Job $JobB -Wait

Write-Host "User A Result: Status $($ResultA.Status)"
$ResultA | Format-List
Write-Host "User B Result: Status $($ResultB.Status)"
$ResultB | Format-List
