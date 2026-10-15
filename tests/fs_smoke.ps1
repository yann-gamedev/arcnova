param([Parameter(Mandatory=$true)][string]$Executable)
$ErrorActionPreference = 'Stop'
$root = Join-Path $env:LOCALAPPDATA ('Temp/opencode/arcnova-fs-' + [guid]::NewGuid())
[void](New-Item -ItemType Directory -Path $root)
try {
    [void](New-Item -ItemType Directory -Path (Join-Path $root 'Folder Baru'))
    [IO.File]::WriteAllText((Join-Path $root 'plain.txt'), 'hello')
    [IO.File]::WriteAllText((Join-Path $root '.dot'), 'dot')
    [IO.File]::WriteAllText((Join-Path $root 'hidden.txt'), 'hidden')
    [IO.File]::SetAttributes((Join-Path $root 'hidden.txt'), [IO.FileAttributes]::Hidden)
    [IO.File]::WriteAllText((Join-Path $root 'RUN.EXE'), '')
    function Run-Shell([string]$InputText) {
        $s = New-Object Diagnostics.ProcessStartInfo
        $s.FileName = $Executable
        $s.WorkingDirectory = $root
        $s.UseShellExecute = $false
        $s.RedirectStandardInput = $true
        $s.RedirectStandardOutput = $true
        $s.RedirectStandardError = $true
        $s.StandardOutputEncoding = [Text.Encoding]::UTF8
        $p = [Diagnostics.Process]::Start($s)
        $p.StandardInput.WriteLine($InputText)
        $p.StandardInput.Close()
        $out = $p.StandardOutput.ReadToEnd()
        $err = $p.StandardError.ReadToEnd()
        $p.WaitForExit()
        if ($p.ExitCode -ne 0) { throw "Process failed: $err" }
        return @{ Out=$out; Err=$err }
    }
    $normal = Run-Shell "ls`nexit"
    $esc = [char]27
    if ($normal.Err -or $normal.Out.Contains('.dot') -or $normal.Out.Contains('hidden.txt') -or
        -not $normal.Out.Contains("${esc}[1;38;5;75mFolder Baru/") -or
        -not $normal.Out.Contains("${esc}[1;38;5;82mRUN.EXE")) { throw 'Default ls/color test failed' }
    $all = Run-Shell "ls -alh`nexit"
    if ($all.Err -or -not $all.Out.Contains("${esc}[38;5;244mhidden.txt") -or
        -not $all.Out.Contains('.dot') -or -not $all.Out.Contains('5B') -or
        -not $all.Out.Contains('drwxr-xr-x') -or $all.Out -notmatch '\d{4}-\d{2}-\d{2} \d{2}:\d{2}') { throw 'Long listing test failed' }
    $nav = Run-Shell "pwd`npwd -W`ncd -`ncd `"Folder Baru`"`ncd missing`ncd -`ncd ..`ncd ~`nls -z`nls missing`nexit"
    if (-not $nav.Out.Contains($root.Replace('\','/')) -or -not $nav.Out.Contains($root) -or
        -not $nav.Err.Contains('previous directory is not set') -or
        -not $nav.Err.Contains('cd: missing:') -or -not $nav.Err.Contains('unsupported flag -z') -or
        -not $nav.Err.Contains('ls: missing:') -or -not $nav.Out.Contains("${esc}[1;38;5;39marcnova${esc}[0m ${esc}[1;38;5;214m~${esc}[0m ${esc}[32m")) { throw 'Navigation/error test failed' }
    'PASS: pwd, Windows paths, cd/home/previous, failure recovery, ls filtering, colors, long format and flags.'
} finally {
    Remove-Item -LiteralPath $root -Recurse -Force
}
