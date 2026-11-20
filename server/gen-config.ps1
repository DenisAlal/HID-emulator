# Generates server/config.json from a Soundpad sound list (.spl backup).
#
# The .spl file is plain XML:
#   <Soundlist>
#     <Sound title="..." url="..."/>
#     ...
#
# Buttons are written in list order, all on the numpad:
#   order 0  -> "Stop"  -> numpad0
#   order 1+ -> songs   -> numpad1 ... numpad9
#   (more than 9 songs -> ctrl+numpad0, ctrl+numpad1, ...)
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File gen-config.ps1
#   powershell -ExecutionPolicy Bypass -File gen-config.ps1 -SplPath "E:\...\backup.spl"
#
# Existing host/port/token/numlock in config.json are kept.

param(
    [string]$SplPath = (Join-Path $env:APPDATA 'Leppsoft\soundlist.spl'),
    [string]$OutFile = (Join-Path $PSScriptRoot 'config.json'),
    [string]$StopLabel = 'Stop'
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path $SplPath)) {
    throw "Sound list not found: $SplPath"
}

# Keep connection settings from the existing config, if any.
$hostAddr = '0.0.0.0'
$port = 45454
$token = 'secret'
$numLock = $true
if (Test-Path $OutFile) {
    try {
        $old = Get-Content -Raw -Encoding UTF8 $OutFile | ConvertFrom-Json
        if ($old.host)  { $hostAddr = $old.host }
        if ($old.port)  { $port = [int]$old.port }
        if ($null -ne $old.token) { $token = $old.token }
        if ($null -ne $old.numlock) { $numLock = [bool]$old.numlock }
    } catch {
        Write-Warning "Could not read existing config, using defaults: $_"
    }
}

[xml]$xml = Get-Content -Raw -Encoding UTF8 $SplPath
$sounds = @($xml.Soundlist.Sound)

$keys = [ordered]@{}
$keys['stop'] = [ordered]@{ label = $StopLabel; combo = 'numpad0'; order = 0 }

$i = 0
foreach ($s in $sounds) {
    $i++
    $label = $s.title
    if ([string]::IsNullOrWhiteSpace($label)) {
        $label = [System.IO.Path]::GetFileNameWithoutExtension($s.url)
    }
    if ([string]::IsNullOrWhiteSpace($label)) {
        $label = "Sound $i"
    }
    if ($i -le 9) {
        $combo = "numpad$i"
    } else {
        $combo = "ctrl+numpad$($i - 10)"
    }
    $keys["sound$i"] = [ordered]@{
        label = $label
        combo = $combo
        order = $i
    }
}

# Hand-format the JSON so config.json stays readable (PS 5.1's ConvertTo-Json
# indents nested objects very aggressively).
function Q([string]$s) { ConvertTo-Json $s }

$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine('{')
[void]$sb.AppendLine(('  "host": {0},' -f (Q $hostAddr)))
[void]$sb.AppendLine(('  "port": {0},' -f $port))
[void]$sb.AppendLine(('  "token": {0},' -f (Q $token)))
[void]$sb.AppendLine(('  "numlock": {0},' -f $numLock.ToString().ToLower()))
[void]$sb.AppendLine('  "keys": {')
$lines = @()
foreach ($k in $keys.Keys) {
    $e = $keys[$k]
    $lines += ('    "{0}": {{ "label": {1}, "combo": {2}, "order": {3} }}' -f $k, (Q $e.label), (Q $e.combo), $e.order)
}
[void]$sb.AppendLine(($lines -join ",`r`n"))
[void]$sb.AppendLine('  }')
[void]$sb.Append('}')

[System.IO.File]::WriteAllText($OutFile, $sb.ToString() + "`n", (New-Object System.Text.UTF8Encoding($false)))

Write-Host "Wrote $OutFile ($($sounds.Count) sounds + Stop)"
foreach ($k in $keys.Keys) {
    Write-Host ("  {0,-8} {1,-10} {2}" -f $k, $keys[$k].combo, $keys[$k].label)
}
