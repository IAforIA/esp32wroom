# Install the ESP32 CP210x driver via Windows Update (run ELEVATED).
# Note: on this machine Windows Update found the driver but failed to bind it,
# so the working path ended up being do-install.ps1 (installs the extracted INF).
$ErrorActionPreference = 'Continue'
$log = Join-Path $PSScriptRoot 'wu-install.log'
Start-Transcript -Path $log -Force | Out-Null

Write-Output "=== Searching Windows Update for the CP2102 driver ==="
try {
    $Session  = New-Object -ComObject Microsoft.Update.Session
    $Searcher = $Session.CreateUpdateSearcher()
    $Result   = $Searcher.Search("IsInstalled=0 and Type='Driver'")
    Write-Output ("Drivers offered by WU: " + $Result.Updates.Count)

    $toInstall = New-Object -ComObject Microsoft.Update.UpdateColl
    foreach ($u in $Result.Updates) {
        Write-Output (" - " + $u.Title)
        if ($u.Title -match "CP210|Silicon|Silabs|UART Bridge") {
            Write-Output ("   >> MATCH, installing: " + $u.Title)
            $u.AcceptEula()
            [void]$toInstall.Add($u)
        }
    }

    if ($toInstall.Count -gt 0) {
        $Installer = $Session.CreateUpdateInstaller()
        $Installer.Updates = $toInstall
        $r = $Installer.Install()
        Write-Output ("=== Install result (2=OK): " + $r.ResultCode + " ===")
    } else {
        Write-Output "=== Windows Update did not offer a CP210x driver ==="
    }
}
catch {
    Write-Output ("WU ERROR: " + $_.Exception.Message)
}

Write-Output "=== Rescanning devices ==="
pnputil /scan-devices | Out-Null
Start-Sleep -Seconds 3
Get-PnpDevice -ErrorAction SilentlyContinue |
    Where-Object { $_.InstanceId -match "VID_10C4" } |
    Select-Object Status, FriendlyName, InstanceId | Format-List

Write-Output "=== Current COM ports ==="
[System.IO.Ports.SerialPort]::GetPortNames()

Stop-Transcript | Out-Null
