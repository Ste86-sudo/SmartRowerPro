$ErrorActionPreference = "Stop"

function Pack-WebUIHeader {
    param([string]$sourceFile, [string]$targetFile)
    
    Write-Host "Packing $sourceFile to $targetFile"
    $html = Get-Content $sourceFile -Raw
    
    $msOut = New-Object System.IO.MemoryStream
    $gsOut = New-Object System.IO.Compression.GZipStream($msOut, [System.IO.Compression.CompressionMode]::Compress)
    $sw = New-Object System.IO.StreamWriter($gsOut)
    $sw.Write($html)
    $sw.Close()
    $compressedBytes = $msOut.ToArray()

    $sb = New-Object System.Text.StringBuilder
    for ($i=0; $i -lt $compressedBytes.Count; $i++) {
        [void]$sb.AppendFormat("0x{0:X2}", $compressedBytes[$i])
        if ($i -ne $compressedBytes.Count - 1) {
            [void]$sb.Append(", ")
        }
        if (($i + 1) % 16 -eq 0) {
            [void]$sb.AppendLine()
        }
    }

    $newContent = "#pragma once`r`n#include <pgmspace.h>`r`n`r`nconst uint8_t index_html_gz[] PROGMEM = {`r`n" + $sb.ToString() + "`r`n};`r`n"
    $utf8NoBom = New-Object System.Text.UTF8Encoding $False
    [System.IO.File]::WriteAllText($targetFile, $newContent, $utf8NoBom)
    Write-Host "Completato $targetFile"
}

Pack-WebUIHeader "C:\Users\stefa\OneDrive\Desktop\SmartRower\index.txt" "C:\Users\stefa\OneDrive\Desktop\SmartRower\SmartRower_Telaio_S3\src\WebUI_HTML.h"
Pack-WebUIHeader "C:\Users\stefa\OneDrive\Desktop\SmartRower\index.txt" "C:\Users\stefa\OneDrive\Desktop\SmartRower\SmartRower_Maniglia_S3\src\WebUI_HTML.h"
