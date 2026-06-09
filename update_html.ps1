$ErrorActionPreference = "Stop"

function Update-WebUIHeader {
    param([string]$targetFile)
    
    Write-Host "Aggiornamento file: $targetFile"
    # 1. Extract bytes
    $content = Get-Content $targetFile -Raw
    $matches = [regex]::Matches($content, '0x[0-9A-Fa-f]{2}')
    [byte[]]$bytes = new-object byte[] $matches.Count
    for ($i=0; $i -lt $matches.Count; $i++) {
        $bytes[$i] = [convert]::ToByte($matches[$i].Value, 16)
    }

    # 2. Decompress
    $ms = New-Object System.IO.MemoryStream(,$bytes)
    $gs = New-Object System.IO.Compression.GZipStream($ms, [System.IO.Compression.CompressionMode]::Decompress)
    $sr = New-Object System.IO.StreamReader($gs)
    $html = $sr.ReadToEnd()
    $gs.Close()

    # 3. Modify HTML
    $favicon = '<link rel="icon" href="data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAxMDAgMTAwIj48cmVjdCB3aWR0aD0iMTAwIiBoZWlnaHQ9IjEwMCIgcng9IjIwIiBmaWxsPSIjMDIwNjE3Ii8+PHBhdGggZD0iTTIwIDcwIFEgMzUgNTAgNTAgNzAgVCA4MCA3MCIgZmlsbD0ibm9uZSIgc3Ryb2tlPSIjMjJkM2VlIiBzdHJva2Utd2lkdGg9IjgiIHN0cm9rZS1saW5lY2FwPSJyb3VuZCIvPjxwYXRoIGQ9Ik0zMCA0MCBMIDUwIDYwIEwgNzAgNDAiIGZpbGw9Im5vbmUiIHN0cm9rZT0iI2VmNDQ0NCIgc3Ryb2tlLXdpZHRoPSI4IiBzdHJva2UtbGluZWNhcD0icm91bmQiIHN0cm9rZS1saW5lam9pbj0icm91bmQiLz48Y2lyY2xlIGN4PSI1MCIgY3k9IjI1IiByPSI4IiBmaWxsPSIjMTBiOTgxIi8+PC9zdmc+" type="image/svg+xml">'
    
    if ($html -notmatch 'rel="icon"') {
        $html = $html -replace '</head>', "$favicon`n</head>"
    }
    
    # Check if logo was already added
    if ($html -notmatch 'logo-smartrower') {
        # Trova la navbar per aggiungere il logo (probabilmente ha la classe navbar o simile)
        # Sostituiamo il testo "Smart Rower Pro" o aggiungiamo un img tag prima di esso
        $logoImg = '<img class="logo-smartrower" src="data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAxMDAgMTAwIj48cmVjdCB3aWR0aD0iMTAwIiBoZWlnaHQ9IjEwMCIgcng9IjIwIiBmaWxsPSIjMDIwNjE3Ii8+PHBhdGggZD0iTTIwIDcwIFEgMzUgNTAgNTAgNzAgVCA4MCA3MCIgZmlsbD0ibm9uZSIgc3Ryb2tlPSIjMjJkM2VlIiBzdHJva2Utd2lkdGg9IjgiIHN0cm9rZS1saW5lY2FwPSJyb3VuZCIvPjxwYXRoIGQ9Ik0zMCA0MCBMIDUwIDYwIEwgNzAgNDAiIGZpbGw9Im5vbmUiIHN0cm9rZT0iI2VmNDQ0NCIgc3Ryb2tlLXdpZHRoPSI4IiBzdHJva2UtbGluZWNhcD0icm91bmQiIHN0cm9rZS1saW5lam9pbj0icm91bmQiLz48Y2lyY2xlIGN4PSI1MCIgY3k9IjI1IiByPSI4IiBmaWxsPSIjMTBiOTgxIi8+PC9zdmc+" style="height: 32px; margin-right: 10px; vertical-align: middle;">'
        $html = $html -replace 'Smart Rower Pro', "$logoImg Smart Rower Pro"
    }

    # 4. Compress
    $msOut = New-Object System.IO.MemoryStream
    $gsOut = New-Object System.IO.Compression.GZipStream($msOut, [System.IO.Compression.CompressionMode]::Compress)
    $sw = New-Object System.IO.StreamWriter($gsOut)
    $sw.Write($html)
    $sw.Close()
    $compressedBytes = $msOut.ToArray()

    # 5. Format to C array
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

    # 6. Rebuild .h file
    $newContent = "#pragma once`r`n#include <pgmspace.h>`r`n`r`nconst uint8_t index_html_gz[] PROGMEM = {`r`n" + $sb.ToString() + "`r`n};`r`n"
    $utf8NoBom = New-Object System.Text.UTF8Encoding $False
    [System.IO.File]::WriteAllText($targetFile, $newContent, $utf8NoBom)
    Write-Host "Completato $targetFile"
}

Update-WebUIHeader "C:\Users\stefa\OneDrive\Desktop\SmartRower\SmartRowerPro_S3\src\WebUI_HTML.h"
