$dirs = Get-ChildItem -Path 'C:\Users\stefa\OneDrive\Desktop\SmartRower' -Directory -Force
foreach ($d in $dirs) {
    $size = (Get-ChildItem -Path $d.FullName -Recurse -Force -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum
    Write-Host "$($d.Name) : $([math]::Round($size / 1MB, 2)) MB"
}
