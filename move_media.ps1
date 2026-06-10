$repoRoot = 'C:\Users\stefa\OneDrive\Desktop\SmartRower'
$dest = Join-Path $repoRoot 'vecchi_media'
if (-not (Test-Path $dest)) { New-Item -ItemType Directory -Path $dest | Out-Null }
$mediaExt = @('.jpg', '.jpeg', '.png', '.gif', '.mp4', '.mov', '.avi')
$files = Get-ChildItem -Path $repoRoot -Recurse -File | Where-Object { $mediaExt -contains $_.Extension.ToLower() }
if ($files.Count -eq 0) {
    Write-Host '⚠️ Nessun file multimediale trovato da spostare.'
    exit 0
}
$git = 'C:\Program Files\Git\bin\git.exe'
foreach ($f in $files) {
    $target = Join-Path $dest $f.Name
    $i = 1
    while (Test-Path $target) {
        $base = [System.IO.Path]::GetFileNameWithoutExtension($f.Name)
        $ext = $f.Extension
        $target = Join-Path $dest ("$base`_$i$ext")
        $i++
    }
    & $git mv $f.FullName $target
}
& $git commit -m "Spostati tutti i file multimediali in vecchi_media per pulizia del repository"
& $git push -u origin master
