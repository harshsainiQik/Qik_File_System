# QFS API Endpoint Testing Script (PowerShell)
# Tests all 7 endpoints of the QFS microservice

$baseUrl = "http://localhost:8001"
$testContent = "Hello QFS! This is a test file for endpoint testing."
$passed = 0
$total = 7

Write-Host "Starting QFS API Endpoint Testing..." -ForegroundColor Green
Write-Host "=" * 50

# Test 1: GET /status
Write-Host "`n1. Testing GET /status:" -ForegroundColor Yellow
try {
    $response = Invoke-RestMethod -Uri "$baseUrl/status" -Method Get -TimeoutSec 10
    Write-Host "[PASS] GET /status: Server is running (status: $($response.status))" -ForegroundColor Green
    $passed++
} catch {
    Write-Host "[FAIL] GET /status: Error - $($_.Exception.Message)" -ForegroundColor Red
}

# Test 2: GET /list
Write-Host "`n2. Testing GET /list:" -ForegroundColor Yellow
try {
    $response = Invoke-RestMethod -Uri "$baseUrl/list" -Method Get -TimeoutSec 10
    $fileCount = $response.total_files
    Write-Host "[PASS] GET /list: Listed $fileCount files" -ForegroundColor Green
    $passed++
} catch {
    Write-Host "[FAIL] GET /list: Error - $($_.Exception.Message)" -ForegroundColor Red
}

# Test 3: POST /upload
Write-Host "`n3. Testing POST /upload:" -ForegroundColor Yellow
try {
    # Create test file
    $testFile = "test_file.txt"
    $testContent | Out-File -FilePath $testFile -Encoding ASCII

    # Prepare multipart form data
    $boundary = [System.Guid]::NewGuid().ToString()
    $LF = "`r`n"
    $fileBytes = [System.IO.File]::ReadAllBytes($testFile)

    $bodyLines = (
        "--$boundary",
        "Content-Disposition: form-data; name=`"file`"; filename=`"$testFile`"",
        "Content-Type: text/plain$LF",
        [System.Text.Encoding]::GetEncoding("iso-8859-1").GetString($fileBytes),
        "--$boundary--$LF"
    ) -join $LF

    $response = Invoke-RestMethod -Uri "$baseUrl/upload" -Method Post -Body $bodyLines -ContentType "multipart/form-data; boundary=$boundary" -TimeoutSec 30
    $testCid = $response.cid
    Write-Host "[PASS] POST /upload: File uploaded successfully (CID: $testCid)" -ForegroundColor Green
    $passed++

    # Clean up
    Remove-Item $testFile -Force
} catch {
    Write-Host "[FAIL] POST /upload: Error - $($_.Exception.Message)" -ForegroundColor Red
    if (Test-Path $testFile) { Remove-Item $testFile -Force }
}

# Test 4: GET /download/{cid}
Write-Host "`n4. Testing GET /download/{cid}:" -ForegroundColor Yellow
if ($testCid) {
    try {
        $response = Invoke-WebRequest -Uri "$baseUrl/download/$testCid" -Method Get -TimeoutSec 10
        $downloadedContent = $response.Content.Trim()
        if ($downloadedContent -eq $testContent.Trim()) {
            Write-Host "[PASS] GET /download/{cid}: File downloaded successfully, content matches" -ForegroundColor Green
            $passed++
        } else {
            Write-Host "[FAIL] GET /download/{cid}: Content mismatch" -ForegroundColor Red
        }
    } catch {
        Write-Host "[FAIL] GET /download/{cid}: Error - $($_.Exception.Message)" -ForegroundColor Red
    }
} else {
    Write-Host "[FAIL] GET /download/{cid}: No CID available from upload test" -ForegroundColor Red
}

# Test 5: PUT /update/{cid}
Write-Host "`n5. Testing PUT /update/{cid}:" -ForegroundColor Yellow
if ($testCid) {
    try {
        $updatedContent = "Updated content for QFS testing!"
        $updatedFile = "updated_test.txt"
        $updatedContent | Out-File -FilePath $updatedFile -Encoding ASCII

        # Prepare multipart form data for update
        $boundary = [System.Guid]::NewGuid().ToString()
        $LF = "`r`n"
        $fileBytes = [System.IO.File]::ReadAllBytes($updatedFile)

        $bodyLines = (
            "--$boundary",
            "Content-Disposition: form-data; name=`"file`"; filename=`"$updatedFile`"",
            "Content-Type: text/plain$LF",
            [System.Text.Encoding]::GetEncoding("iso-8859-1").GetString($fileBytes),
            "--$boundary--$LF"
        ) -join $LF

        $response = Invoke-RestMethod -Uri "$baseUrl/update/$testCid" -Method Put -Body $bodyLines -ContentType "multipart/form-data; boundary=$boundary" -TimeoutSec 30
        $newCid = $response.new_cid
        Write-Host "[PASS] PUT /update/{cid}: File updated successfully (new CID: $newCid)" -ForegroundColor Green
        $testCid = $newCid  # Update CID for subsequent tests
        $passed++

        # Clean up
        Remove-Item $updatedFile -Force
    } catch {
        Write-Host "[FAIL] PUT /update/{cid}: Error - $($_.Exception.Message)" -ForegroundColor Red
        if (Test-Path $updatedFile) { Remove-Item $updatedFile -Force }
    }
} else {
    Write-Host "[FAIL] PUT /update/{cid}: No CID available from upload test" -ForegroundColor Red
}

# Test 6: GET /chunk/{cid}
Write-Host "`n6. Testing GET /chunk/{cid}:" -ForegroundColor Yellow
try {
    if ($testCid) {
        # Test with main CID (should fail with proper error for RootNode)
        try {
            $response = Invoke-RestMethod -Uri "$baseUrl/chunk/$testCid" -Method Get -TimeoutSec 10
        } catch {
            $errorResponse = $_.ErrorDetails.Message | ConvertFrom-Json
            if ($errorResponse.error -like "*RootNode CID provided*") {
                Write-Host "[PASS] GET /chunk/{cid}: Endpoint correctly rejects RootNode CID" -ForegroundColor Green
                $passed++
            } else {
                Write-Host "[FAIL] GET /chunk/{cid}: Unexpected error response" -ForegroundColor Red
            }
        }
    } else {
        # Test with invalid CID
        try {
            $response = Invoke-RestMethod -Uri "$baseUrl/chunk/invalid_cid" -Method Get -TimeoutSec 10
        } catch {
            if ($_.Exception.Response.StatusCode -eq 400) {
                Write-Host "[PASS] GET /chunk/{cid}: Endpoint correctly handles invalid CID" -ForegroundColor Green
                $passed++
            } else {
                Write-Host "[FAIL] GET /chunk/{cid}: Unexpected response for invalid CID" -ForegroundColor Red
            }
        }
    }
} catch {
    Write-Host "[FAIL] GET /chunk/{cid}: Error - $($_.Exception.Message)" -ForegroundColor Red
}

# Test 7: DELETE /delete/{cid}
Write-Host "`n7. Testing DELETE /delete/{cid}:" -ForegroundColor Yellow
if ($testCid) {
    try {
        $response = Invoke-RestMethod -Uri "$baseUrl/delete/$testCid" -Method Delete -TimeoutSec 30
        Write-Host "[PASS] DELETE /delete/{cid}: File deleted successfully" -ForegroundColor Green
        $passed++
    } catch {
        Write-Host "[FAIL] DELETE /delete/{cid}: Error - $($_.Exception.Message)" -ForegroundColor Red
    }
} else {
    Write-Host "[FAIL] DELETE /delete/{cid}: No CID available for deletion test" -ForegroundColor Red
}

# Results
Write-Host "`n" + ("=" * 50)
Write-Host "Test Results: $passed/$total tests passed" -ForegroundColor Cyan

if ($passed -eq $total) {
    Write-Host "All endpoints are working correctly!" -ForegroundColor Green
    Write-Host "This microservice appears to be production ready." -ForegroundColor Green
    exit 0
} else {
    $failed = $total - $passed
    Write-Host "WARNING: $failed test(s) failed." -ForegroundColor Yellow
    Write-Host "This microservice needs attention before production deployment." -ForegroundColor Yellow
    exit 1
}