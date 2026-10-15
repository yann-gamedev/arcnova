param([Parameter(Mandatory=$true)][string]$Executable)
$ErrorActionPreference = 'Stop'
$Executable = (Resolve-Path $Executable).Path
$root = Join-Path $env:LOCALAPPDATA ('Temp/opencode/arcnova-theme-' + [guid]::NewGuid())
[void](New-Item -ItemType Directory -Path $root)
try {
    [void](New-Item -ItemType Directory -Path (Join-Path $root 'folder'))
    $rc = Join-Path $root '.arcnovarc'
    $esc = [char]27
    function Run-Shell([string]$InputText) {
        $s = New-Object Diagnostics.ProcessStartInfo
        $s.FileName = $Executable
        $s.WorkingDirectory = $root
        $s.EnvironmentVariables['USERPROFILE'] = $root
        $s.UseShellExecute = $false
        $s.RedirectStandardInput = $true
        $s.RedirectStandardOutput = $true
        $s.RedirectStandardError = $true
        $s.StandardOutputEncoding = [Text.Encoding]::UTF8
        $p = [Diagnostics.Process]::Start($s)
        try {
            $p.StandardInput.WriteLine($InputText)
            $p.StandardInput.Close()
            $out = $p.StandardOutput.ReadToEnd()
            $err = $p.StandardError.ReadToEnd()
            $p.WaitForExit()
            if ($p.ExitCode -ne 0) { throw "Process failed: $err" }
            return @{ Out=$out; Err=$err }
        } finally { $p.Dispose() }
    }
    $first = Run-Shell "theme`nexit"
    if ($first.Err -or -not $first.Out.Contains('Active theme: default') -or
        -not (Get-Content $rc -Raw).Contains('theme default')) { throw 'Default rc/theme failed' }

    $presets = @(
        @{ Name='default'; Primary='1;38;5;39'; Accent='1;38;5;214'; Border='38;5;244'; Dir='1;38;5;75' },
        @{ Name='dracula'; Primary='1;38;5;141'; Accent='1;38;5;212'; Border='38;5;61'; Dir='1;38;5;141' },
        @{ Name='nord'; Primary='1;38;5;110'; Accent='1;38;5;152'; Border='38;5;60'; Dir='1;38;5;110' },
        @{ Name='matrix'; Primary='1;38;5;46'; Accent='38;5;34'; Border='38;5;22'; Dir='1;38;5;46' }
    )
    foreach ($preset in $presets) {
        $name = $preset.Name
        [IO.File]::WriteAllText($rc, "# startup theme`r`n`r`n theme `"$name`"`r`nalias listing='ls'`r`n")
        $startup = Run-Shell "theme`nlisting`nexit"
        $primary = "${esc}[$($preset.Primary)m"
        $accent = "${esc}[$($preset.Accent)m"
        $border = "${esc}[$($preset.Border)m"
        $dir = "${esc}[$($preset.Dir)m"
        if ($startup.Err -or -not $startup.Out.StartsWith($border) -or
            -not $startup.Out.Contains("${primary}arcnova${esc}[0m ${accent}~${esc}[0m") -or
            -not $startup.Out.Contains("${dir}folder/") -or
            -not $startup.Out.Contains("Active theme: $name")) { throw "Startup theme failed: $name" }

        $runtime = Run-Shell "theme default`ntheme $name`nclear`nls`ntheme`nexit"
        if ($runtime.Err -or -not $runtime.Out.Contains("${esc}[2J${esc}[H${border}") -or
            -not $runtime.Out.Contains("${dir}folder/") -or
            -not $runtime.Out.Contains("Active theme: $name")) { throw "Runtime theme failed: $name" }
    }
    $invalid = Run-Shell "theme unknown && echo SHOULD_NOT_RUN`ntheme nord extra`ntheme -x`ntheme`nexit"
    if (-not $invalid.Err.Contains("unknown theme 'unknown'") -or
        -not $invalid.Err.Contains('usage: theme') -or $invalid.Out.Contains('SHOULD_NOT_RUN') -or
        -not $invalid.Out.Contains('Active theme: matrix') -or
        -not $invalid.Out.Contains('Available themes: default dracula nord matrix')) {
        throw 'Invalid theme changed state or returned success'
    }
    [IO.File]::WriteAllText($rc, "theme dracula`ntheme unknown`ntheme 'unclosed`nalias ok='echo recovered'`n")
    $recovery = Run-Shell "theme`nok`nexit"
    if (-not $recovery.Err.Contains('.arcnovarc:2:') -or
        -not $recovery.Err.Contains('.arcnovarc:3:') -or
        -not $recovery.Out.Contains('Active theme: dracula') -or
        -not $recovery.Out.Contains('recovered')) { throw 'RC recovery failed' }
    'PASS: theme presets, startup banner/prompt, runtime switching, ls, aliases and invalid theme recovery.'
} finally {
    Remove-Item -LiteralPath $root -Recurse -Force
}
