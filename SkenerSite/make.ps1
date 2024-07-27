$zdrojak = "skener.c"
$program = "skener.exe"
$architektura = "x64"
$optimalizace = "/O1"

$knihovny = "/link /LIBPATH:Lib/$architektura ws2_32.lib wpcap.lib"
$hlavicky = "/I Include"

if ($args[0] -eq "clean") {
    if (Test-Path -Path "build") {
        Remove-Item -Path "build\*" -Recurse -Force
        Write-Output "Adresář build/ a veškerý jeho obsah byl smazán"
    } else {
        Write-Output "Adresář build/ neexistuje"
    }
    Remove-Item -Path "./*.exe" -Force
    exit
}

$datum = Get-Date -Format "yyyyMMdd_HHmmss"
$pracovniAdresar = "build/$datum"

if (-Not (Test-Path -Path $pracovniAdresar)) {
    New-Item -ItemType Directory -Path $pracovniAdresar
}

Copy-Item -Path "./oui.txt" -Destination "$pracovniAdresar/oui.txt" -Force
Copy-Item -Path $zdrojak -Destination "$pracovniAdresar/$zdrojak" -Force

$sestaveni = "cl.exe /nologo $optimalizace $hlavicky $zdrojak /Fo$pracovniAdresar/ /Fe:$pracovniAdresar/$program $knihovny"

Write-Output ""
Write-Output "Povel k sestavení:"
Write-Output $sestaveni
Write-Output ""

Invoke-Expression $sestaveni

if ($LASTEXITCODE -ne 0) {
    Write-Output "Chyba při kompilaci! Uklízím pracovní adresář $pracovniAdresar"
    Remove-Item -Path $pracovniAdresar -Recurse -Force
    exit 1
}

Copy-Item -Path "$pracovniAdresar/$program" -Destination "./$program" -Force

Write-Output "Aktuální sestavení najdete archivováno v $pracovniAdresar"
Write-Output "Aktuální verze programu najdete zároveň v ./$program"
