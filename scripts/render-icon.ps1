# Render the same vector design as assets/meshbbs-icon.svg using Windows GDI+.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$root = Split-Path $PSScriptRoot -Parent
$bitmap = New-Object System.Drawing.Bitmap 512,512
$g = [System.Drawing.Graphics]::FromImage($bitmap)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.ScaleTransform((512.0/108),(512.0/108))
$navy = New-Object System.Drawing.SolidBrush ([System.Drawing.ColorTranslator]::FromHtml('#0c1825'))
$coral = New-Object System.Drawing.SolidBrush ([System.Drawing.ColorTranslator]::FromHtml('#ff8b73'))
$white = New-Object System.Drawing.SolidBrush ([System.Drawing.ColorTranslator]::FromHtml('#e7f0f7'))
$pen = New-Object System.Drawing.Pen ([System.Drawing.ColorTranslator]::FromHtml('#58dac2')),5
$pen.StartCap = $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
$bg = New-Object System.Drawing.Drawing2D.GraphicsPath
$bg.AddArc(0,0,48,48,180,90); $bg.AddArc(60,0,48,48,270,90)
$bg.AddArc(60,60,48,48,0,90); $bg.AddArc(0,60,48,48,90,90); $bg.CloseFigure()
$g.FillPath($navy,$bg)
$p = New-Object System.Drawing.Drawing2D.GraphicsPath
$p.AddLine(30,44,78,44); $p.AddBezier(78,44,82,44,84,46,84,50)
$p.AddLine(84,50,84,73); $p.AddBezier(84,73,84,77,82,79,78,79)
$p.AddLine(78,79,49,79); $p.AddLine(49,79,34,90); $p.AddLine(34,90,34,79); $p.AddLine(34,79,30,79)
$p.AddBezier(30,79,26,79,24,77,24,73); $p.AddLine(24,73,24,50)
$p.AddBezier(24,50,24,46,26,44,30,44); $p.CloseFigure()
$g.FillPath($coral,$p)
$g.FillRectangle($navy,36,55,36,5); $g.FillRectangle($navy,36,66,23,5)
$g.DrawBezier($pen,33,31,47,18.33333,61,18.33333,75,31)
$g.DrawBezier($pen,43,37,50.33333,30.33333,57.66667,30.33333,65,37)
$g.FillEllipse($white,68,64,8,8)
$destination = Join-Path $root 'assets\meshbbs-icon.png'
$bitmap.Save($destination,[System.Drawing.Imaging.ImageFormat]::Png)
foreach ($object in @($g,$bitmap,$navy,$coral,$white,$pen,$bg,$p)) { $object.Dispose() }
Write-Output $destination
