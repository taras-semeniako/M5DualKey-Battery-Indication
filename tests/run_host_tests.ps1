param(
    [string]$Compiler = "cl.exe",
    [string]$Linker = "link.exe",
    [string]$CompilerInclude = ""
)

$ErrorActionPreference = "Stop"
$repo = Split-Path $PSScriptRoot -Parent
$output = Join-Path ([System.IO.Path]::GetTempPath()) ("dualkey-battery-tests-" + [guid]::NewGuid())
New-Item -ItemType Directory -Path $output | Out-Null
Push-Location $repo
try {
    $includes = @("/I", "main\include", "/I", "components\esp_duo\include", "/I", "tests\stubs",
        "/I", "managed_components\espressif__led_strip\interface")
    if ($CompilerInclude) { $includes += @("/I", $CompilerInclude) }
    foreach ($suite in @(
        @{ Name = "battery_indicator"; Test = "tests\battery_indicator_test.c"; Source = "main\battery_indicator.c" },
        @{ Name = "led_overlay"; Test = "tests\led_overlay_test.c"; Source = "components\esp_duo\led_overlay.c" }
    )) {
        $objects = @()
        $index = 0
        foreach ($source in @($suite.Test, $suite.Source, "tests\freestanding_memory.c")) {
            $object = Join-Path $output ($suite.Name + "-" + $index++ + ".obj")
            & $Compiler /nologo /TC /GS- /Zl @includes "/Fo$object" /c $source
            if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $source" }
            $objects += $object
        }
        $executable = Join-Path $output ($suite.Name + ".exe")
        & $Linker /nologo /entry:main /subsystem:console /nodefaultlib "/out:$executable" @objects
        if ($LASTEXITCODE -ne 0) { throw "Link failed: $($suite.Name)" }
        & $executable
        if ($LASTEXITCODE -ne 0) { throw "$($suite.Name) failed at test line $LASTEXITCODE" }
        Write-Output "$($suite.Name): passed"
    }
    node --check main\web\script.js
    if ($LASTEXITCODE -ne 0) { throw "JavaScript syntax check failed" }
    node --test tests\battery_indicator_web.test.js
    if ($LASTEXITCODE -ne 0) { throw "Web tests failed" }
} finally {
    Pop-Location
    Remove-Item -LiteralPath $output -Recurse -Force
}
