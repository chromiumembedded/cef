# Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generate test code signing certificate for CEF installer integration tests
# Run from the testdata directory or provide -OutputPath parameter
#
# Usage:
#   .\generate_test_cert.ps1
#   powershell -ExecutionPolicy Bypass -File generate_test_cert.ps1

param(
    [string]$OutputPath = $PSScriptRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# Ensure output directory exists
if (-not (Test-Path $OutputPath)) {
    New-Item -ItemType Directory -Path $OutputPath -Force | Out-Null
}

Write-Host "Generating test certificate in: $OutputPath"

# Create self-signed code signing certificate
$cert = New-SelfSignedCertificate `
    -Type CodeSigningCert `
    -Subject "CN=CEF Integration Test Signing, O=Test, C=US" `
    -CertStoreLocation "Cert:\CurrentUser\My" `
    -KeyExportPolicy Exportable `
    -KeyLength 2048 `
    -KeyAlgorithm RSA `
    -HashAlgorithm SHA256 `
    -NotAfter (Get-Date).AddYears(100)  # Long expiry for test cert

$password = ConvertTo-SecureString -String "test" -Force -AsPlainText

# Export PFX (private key + certificate)
$pfxPath = Join-Path $OutputPath "test_signing.pfx"
Export-PfxCertificate -Cert $cert -FilePath $pfxPath -Password $password | Out-Null
Write-Host "Created: $pfxPath"

# Export CER (public certificate only)
$cerPath = Join-Path $OutputPath "test_signing.cer"
Export-Certificate -Cert $cert -FilePath $cerPath | Out-Null
Write-Host "Created: $cerPath"

# Save thumbprint (uppercase, no spaces)
$thumbprintPath = Join-Path $OutputPath "test_thumbprint.txt"
$cert.Thumbprint | Out-File -FilePath $thumbprintPath -NoNewline -Encoding ASCII
Write-Host "Created: $thumbprintPath"

Write-Host ""
Write-Host "Certificate thumbprint: $($cert.Thumbprint)"

# Clean up from cert store (cert was only needed for export)
Remove-Item -Path "Cert:\CurrentUser\My\$($cert.Thumbprint)"

Write-Host ""
Write-Host "Done. Test certificate files generated successfully."
