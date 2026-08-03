$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$dataDir = Join-Path $root "data"
$backendPath = Join-Path $root "src\web_server_manager.cpp"
$backend = Get-Content -Raw -LiteralPath $backendPath
$htmlFiles = Get-ChildItem -LiteralPath $dataDir -Filter "*.html"
$errors = [System.Collections.Generic.List[string]]::new()

function Add-ValidationError([string]$message) {
    $errors.Add($message)
}

$routes = @{}
foreach ($match in [regex]::Matches(
    $backend,
    'server\.on\(\s*"([^"]+)"\s*,\s*(HTTP_[A-Z]+)'
)) {
    $routes["$($match.Groups[2].Value) $($match.Groups[1].Value)"] = $true
}

foreach ($file in $htmlFiles) {
    $content = Get-Content -Raw -LiteralPath $file.FullName

    if ($content -notmatch '(?i)<html\s+lang="en"') {
        Add-ValidationError "$($file.Name): document language must be English"
    }

    $germanTextPattern =
        '(?i)\b(?:einstellungen|messung|batterie|füllstand|wasserstand|hochladen|neustart|angesteckt|sekunden|minuten|stunden)\b'
    if ($content -match $germanTextPattern) {
        Add-ValidationError "$($file.Name): German user-facing text detected"
    }

    $ids = @{}
    foreach ($match in [regex]::Matches($content, '\bid\s*=\s*"([^"]+)"')) {
        $id = $match.Groups[1].Value
        if ($ids.ContainsKey($id)) {
            Add-ValidationError "$($file.Name): duplicate id '$id'"
        }
        $ids[$id] = $true
    }

    foreach ($match in [regex]::Matches(
        $content,
        'getElementById\(\s*"([^"]+)"\s*\)'
    )) {
        $id = $match.Groups[1].Value
        if (!$ids.ContainsKey($id)) {
            Add-ValidationError "$($file.Name): JavaScript references missing id '$id'"
        }
    }

    $formDepth = 0
    foreach ($match in [regex]::Matches($content, '(?is)</?form\b[^>]*>')) {
        if ($match.Value -match '^</') {
            $formDepth--
            if ($formDepth -lt 0) {
                Add-ValidationError "$($file.Name): closing form without opening form"
                $formDepth = 0
            }
        } else {
            if ($formDepth -gt 0) {
                Add-ValidationError "$($file.Name): nested form"
            }
            $formDepth++
        }
    }
    if ($formDepth -ne 0) {
        Add-ValidationError "$($file.Name): unbalanced form tags"
    }

    foreach ($match in [regex]::Matches(
        $content,
        '(?is)<form\b[^>]*\baction\s*=\s*"([^"]+)"[^>]*>'
    )) {
        $action = $match.Groups[1].Value
        if ($action.StartsWith("/") -and !$routes.ContainsKey("HTTP_POST $action")) {
            Add-ValidationError "$($file.Name): form action '$action' has no POST route"
        }
    }

    foreach ($match in [regex]::Matches(
        $content,
        '(?i)\b(?:href|src)\s*=\s*"([^"]+)"'
    )) {
        $asset = $match.Groups[1].Value
        if (
            $asset.StartsWith("/") -and
            !$asset.StartsWith("//") -and
            $asset -notmatch '^/(?:login|info|logs)?$' -and
            $asset -notmatch '^/api/' -and
            !$routes.ContainsKey("HTTP_GET $asset") -and
            !(Test-Path -LiteralPath (Join-Path $dataDir $asset.TrimStart("/")))
        ) {
            Add-ValidationError "$($file.Name): missing local asset '$asset'"
        }
    }

    foreach ($match in [regex]::Matches($content, '\{\{([A-Z0-9_]+)\}\}')) {
        $placeholder = $match.Groups[1].Value
        $dynamicPinPlaceholder =
            $placeholder -match '^(?:BUTTON|STATUS_LED|LED_(?:RED|GREEN|BLUE)|BATTERY_ADC|SENSOR_(?:TRIGGER|ECHO))_PIN_(?:OPTIONS|CURRENT|DEFAULT|BADGE)$'
        if (
            !$dynamicPinPlaceholder -and
            $backend.IndexOf("{{${placeholder}}}") -lt 0
        ) {
            Add-ValidationError "$($file.Name): placeholder '$placeholder' is not handled"
        }
        if ($placeholder -match 'PASSWORD|PASS') {
            Add-ValidationError "$($file.Name): password placeholder '$placeholder' must not be rendered"
        }
    }
}

$css = Get-Content -Raw -LiteralPath (Join-Path $dataDir "style.css")
$cssWithoutComments = [regex]::Replace($css, '(?s)/\*.*?\*/', '')
if (
    ([regex]::Matches($cssWithoutComments, '\{')).Count -ne
    ([regex]::Matches($cssWithoutComments, '\}')).Count
) {
    Add-ValidationError "style.css: unbalanced braces"
}

if ($errors.Count -gt 0) {
    $errors | ForEach-Object { Write-Error $_ }
    exit 1
}

Write-Host (
    "Web validation passed: {0} HTML files, {1} registered routes." -f
    $htmlFiles.Count,
    $routes.Count
)
