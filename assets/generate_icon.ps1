param([string]$Output = (Join-Path $PSScriptRoot 'arcnova.ico'))
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$sizes = @(16, 24, 32, 48, 64, 128, 256)
$images = New-Object System.Collections.Generic.List[byte[]]
foreach ($size in $sizes) {
    $bitmap = New-Object System.Drawing.Bitmap $size, $size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $graphics.Clear([System.Drawing.Color]::Transparent)
        $s = [single]$size
        $background = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 18, 23, 34))
        $graphics.FillEllipse($background, $s * 0.04, $s * 0.04, $s * 0.92, $s * 0.92)
        $points = [System.Drawing.PointF[]]@(
            (New-Object System.Drawing.PointF ($s * 0.50), ($s * 0.12)),
            (New-Object System.Drawing.PointF ($s * 0.86), ($s * 0.80)),
            (New-Object System.Drawing.PointF ($s * 0.14), ($s * 0.80))
        )
        $triangle = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 0, 191, 255)), ([Math]::Max(1.5, $s * 0.075))
        $triangle.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
        $graphics.DrawPolygon($triangle, $points)
        $circle = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 255, 191, 0)), ([Math]::Max(1.25, $s * 0.06))
        $graphics.DrawEllipse($circle, $s * 0.38, $s * 0.43, $s * 0.24, $s * 0.24)
        $stream = New-Object System.IO.MemoryStream
        $bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
        $images.Add($stream.ToArray())
        $stream.Dispose(); $background.Dispose(); $triangle.Dispose(); $circle.Dispose()
    } finally {
        $graphics.Dispose(); $bitmap.Dispose()
    }
}

$file = [System.IO.File]::Create($Output)
$writer = New-Object System.IO.BinaryWriter $file
try {
    $writer.Write([UInt16]0); $writer.Write([UInt16]1); $writer.Write([UInt16]$sizes.Count)
    $offset = 6 + (16 * $sizes.Count)
    for ($i = 0; $i -lt $sizes.Count; ++$i) {
        $dimension = if ($sizes[$i] -eq 256) { 0 } else { $sizes[$i] }
        $writer.Write([byte]$dimension); $writer.Write([byte]$dimension)
        $writer.Write([byte]0); $writer.Write([byte]0)
        $writer.Write([UInt16]1); $writer.Write([UInt16]32)
        $writer.Write([UInt32]$images[$i].Length); $writer.Write([UInt32]$offset)
        $offset += $images[$i].Length
    }
    foreach ($image in $images) { $writer.Write($image) }
} finally {
    $writer.Dispose()
}
