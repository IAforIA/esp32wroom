# Install the extracted CP210x driver and bind it to the device (run ELEVATED).
# This is the step that actually worked: the signed INF was extracted from the
# official CAB (Microsoft Update Catalog) into cp210x_extracted/.
$ErrorActionPreference = 'Continue'
$base = $PSScriptRoot
$inf  = Join-Path $base 'cp210x_extracted\silabser.inf'
$log  = Join-Path $base 'do-install.log'
Start-Transcript -Path $log -Force | Out-Null

Write-Output "=== Installing driver: $inf ==="
pnputil /add-driver "$inf" /install

Write-Output "`n=== Rescanning devices ==="
pnputil /scan-devices
Start-Sleep -Seconds 3

Write-Output "`n=== CP2102 status ==="
Get-PnpDevice -ErrorAction SilentlyContinue |
    Where-Object { $_.InstanceId -match "VID_10C4" } |
    Select-Object Status, FriendlyName, InstanceId | Format-List

Write-Output "=== Current COM ports ==="
[System.IO.Ports.SerialPort]::GetPortNames()

Stop-Transcript | Out-Null
