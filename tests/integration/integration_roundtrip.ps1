$ErrorActionPreference = "Stop"

$source = Join-Path $PWD "test_data\hello.txt"
$archive = Join-Path $PWD "build\integration_test.uatc"
$extracted = Join-Path $PWD "build\integration_extracted.bin"
$exe = Join-Path $PWD "build\integration_roundtrip_test.exe"

if (-not (Test-Path $exe)) {
    throw "Integration test executable not found. Build the project first."
}

try {
    & $exe
    if ($LASTEXITCODE -ne 0) {
        throw "Archive round-trip program failed."
    }

    $sourceHash = (Get-FileHash $source -Algorithm SHA256).Hash
    $extractedHash = (Get-FileHash $extracted -Algorithm SHA256).Hash

    Write-Host ""
    Write-Host "Source SHA-256:    $sourceHash"
    Write-Host "Extracted SHA-256: $extractedHash"

    if ($sourceHash -ne $extractedHash) {
        throw "SHA-256 mismatch: extracted file differs from source."
    }

    Write-Host ""
    Write-Host "INTEGRATION TEST PASSED: SHA-256 hashes match." -ForegroundColor Green
}
finally {
    Remove-Item $archive, $extracted -Force -ErrorAction SilentlyContinue
}
