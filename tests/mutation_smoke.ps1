param([Parameter(Mandatory=$true)][string]$Executable)
$ErrorActionPreference = 'Stop'
$root = Join-Path $env:LOCALAPPDATA ('Temp/opencode/arcnova-mutation-' + [guid]::NewGuid())
[void](New-Item -ItemType Directory -Path $root)
try {
    $seed = Join-Path $root 'seed.txt'
    [IO.File]::WriteAllText($seed, "first`nsecond`n")
    [IO.File]::SetLastWriteTimeUtc($seed, [datetime]'2000-01-01')
    # Protected-path test uses a disposable Windows-named directory, not the OS.
    $protected = Join-Path $root 'protected'
    [void](New-Item -ItemType Directory -Path $protected)
    [IO.File]::WriteAllText((Join-Path $protected 'keep.txt'), 'keep')
    $s = New-Object Diagnostics.ProcessStartInfo
    $s.FileName = $Executable
    $s.WorkingDirectory = $root
    $s.UseShellExecute = $false
    $s.EnvironmentVariables['ProgramData'] = $protected
    $s.RedirectStandardInput = $true
    $s.RedirectStandardOutput = $true
    $s.RedirectStandardError = $true
    $s.StandardOutputEncoding = [Text.Encoding]::UTF8
    $p = [Diagnostics.Process]::Start($s)
    $p.StandardInput.WriteLine(@'
touch one.txt "two files.txt" seed.txt
mkdir -p parent/child
mkdir absent/child
cp seed.txt copied.txt
cp seed.txt copied.txt
mv copied.txt moved.txt
cat -n moved.txt
cp -r parent duplicate
cp -r parent parent/nested
rm parent
rm -rf duplicate
rm one.txt "two files.txt"
rm -f missing.txt
rm -rf protected
cat missing.txt
mkdir
exit
'@)
    $p.StandardInput.Close()
    $out = $p.StandardOutput.ReadToEnd()
    $err = $p.StandardError.ReadToEnd()
    $p.WaitForExit()
    if ($p.ExitCode -ne 0) { throw "Unexpected exit: $err" }
    foreach ($path in @('one.txt', 'two files.txt', 'duplicate', 'copied.txt', 'parent/nested')) {
        if (Test-Path (Join-Path $root $path)) { throw "Unexpected path: $path" }
    }
    foreach ($path in @('parent/child', 'moved.txt', 'protected/keep.txt')) {
        if (-not (Test-Path (Join-Path $root $path))) { throw "Missing path: $path" }
    }
    if ([IO.File]::ReadAllText($seed) -ne "first`nsecond`n" -or
        [IO.File]::GetLastWriteTimeUtc($seed).Year -eq 2000) { throw 'touch damaged contents or did not update timestamp' }
    if ($out -notmatch '1  first' -or $out -notmatch '2  second') { throw 'cat numbering failed' }
    foreach ($message in @('destination already exists', 'directory requires -r', 'deletion refused', 'missing operand', 'destination cannot be')) {
        if (-not $err.Contains($message)) { throw "Expected error missing: $message" }
    }
    'PASS: touch, mkdir, rm, cp, mv, cat, timestamps, refusal checks and protected-path guard.'
} finally {
    Remove-Item -LiteralPath $root -Recurse -Force
}
