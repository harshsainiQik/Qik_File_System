# -*- coding: utf-8 -*-
"""
Simple QFS API Endpoint Testing Script
Tests all 7 endpoints of the QFS microservice
Compatible with older Python versions
"""

import requests
import json
import os
import sys
from datetime import datetime

BASE_URL = "http://localhost:8001"
TEST_CONTENT = "Hello QFS! This is a test file for endpoint testing."

def test_status():
    """Test GET /status endpoint"""
    try:
        response = requests.get(BASE_URL + "/status", timeout=10)
        if response.status_code == 200:
            data = response.json()
            print("[PASS] GET /status: Server is running (status: {})".format(data.get('status', 'N/A')))
            return True, data
        else:
            print("[FAIL] GET /status: HTTP {} - {}".format(response.status_code, response.text))
            return False, None
    except Exception as e:
        print("[FAIL] GET /status: Error - {}".format(str(e)))
        return False, None

def test_list():
    """Test GET /list endpoint"""
    try:
        response = requests.get(BASE_URL + "/list", timeout=10)
        if response.status_code == 200:
            data = response.json()
            file_count = data.get('total_files', 0)
            print("[PASS] GET /list: Listed {} files".format(file_count))
            return True, data
        else:
            print("[FAIL] GET /list: HTTP {} - {}".format(response.status_code, response.text))
            return False, None
    except Exception as e:
        print("[FAIL] GET /list: Error - {}".format(str(e)))
        return False, None

def test_upload():
    """Test POST /upload endpoint"""
    try:
        # Create test file
        test_file = "test_file.txt"
        with open(test_file, 'w') as f:
            f.write(TEST_CONTENT)

        with open(test_file, 'rb') as f:
            files = {'file': (test_file, f, 'text/plain')}
            response = requests.post(BASE_URL + "/upload", files=files, timeout=30)

        if response.status_code == 200:
            data = response.json()
            cid = data.get('cid')
            print("[PASS] POST /upload: File uploaded successfully (CID: {})".format(cid))
            return True, cid
        else:
            print("[FAIL] POST /upload: HTTP {} - {}".format(response.status_code, response.text))
            return False, None
    except Exception as e:
        print("[FAIL] POST /upload: Error - {}".format(str(e)))
        return False, None
    finally:
        if os.path.exists(test_file):
            os.remove(test_file)

def test_download(cid):
    """Test GET /download/{cid} endpoint"""
    if not cid:
        print("[FAIL] GET /download/{{cid}}: No CID available from upload test")
        return False

    try:
        response = requests.get(BASE_URL + "/download/" + cid, timeout=10)
        if response.status_code == 200:
            downloaded_content = response.text
            if downloaded_content.strip() == TEST_CONTENT.strip():
                print("[PASS] GET /download/{{cid}}: File downloaded successfully, content matches")
                return True
            else:
                print("[FAIL] GET /download/{{cid}}: Content mismatch")
                return False
        else:
            print("[FAIL] GET /download/{{cid}}: HTTP {} - {}".format(response.status_code, response.text))
            return False
    except Exception as e:
        print("[FAIL] GET /download/{{cid}}: Error - {}".format(str(e)))
        return False

def test_update(cid):
    """Test PUT /update/{cid} endpoint"""
    if not cid:
        print("[FAIL] PUT /update/{{cid}}: No CID available from upload test")
        return False, None

    try:
        updated_content = "Updated content for QFS testing!"
        updated_file = "updated_test.txt"

        with open(updated_file, 'w') as f:
            f.write(updated_content)

        with open(updated_file, 'rb') as f:
            files = {'file': (updated_file, f, 'text/plain')}
            response = requests.put(BASE_URL + "/update/" + cid, files=files, timeout=30)

        if response.status_code == 200:
            data = response.json()
            new_cid = data.get('new_cid')
            print("[PASS] PUT /update/{{cid}}: File updated successfully (new CID: {})".format(new_cid))
            return True, new_cid
        else:
            print("[FAIL] PUT /update/{{cid}}: HTTP {} - {}".format(response.status_code, response.text))
            return False, None
    except Exception as e:
        print("[FAIL] PUT /update/{{cid}}: Error - {}".format(str(e)))
        return False, None
    finally:
        if os.path.exists(updated_file):
            os.remove(updated_file)

def test_chunk(cid):
    """Test GET /chunk/{cid} endpoint"""
    try:
        # Test with main CID (should fail with proper error for RootNode)
        if cid:
            response = requests.get(BASE_URL + "/chunk/" + cid, timeout=10)
            if response.status_code == 400:
                error_data = response.json()
                if "RootNode CID provided" in error_data.get('error', ''):
                    print("[PASS] GET /chunk/{{cid}}: Endpoint correctly rejects RootNode CID")
                    return True

        # Test with invalid CID
        response = requests.get(BASE_URL + "/chunk/invalid_cid", timeout=10)
        if response.status_code == 400:
            print("[PASS] GET /chunk/{{cid}}: Endpoint correctly handles invalid CID")
            return True
        else:
            print("[FAIL] GET /chunk/{{cid}}: Unexpected response for invalid CID: {}".format(response.status_code))
            return False

    except Exception as e:
        print("[FAIL] GET /chunk/{{cid}}: Error - {}".format(str(e)))
        return False

def test_delete(cid):
    """Test DELETE /delete/{cid} endpoint"""
    if not cid:
        print("[FAIL] DELETE /delete/{{cid}}: No CID available for deletion test")
        return False

    try:
        response = requests.delete(BASE_URL + "/delete/" + cid, timeout=30)
        if response.status_code == 200:
            print("[PASS] DELETE /delete/{{cid}}: File deleted successfully")
            return True
        else:
            print("[FAIL] DELETE /delete/{{cid}}: HTTP {} - {}".format(response.status_code, response.text))
            return False
    except Exception as e:
        print("[FAIL] DELETE /delete/{{cid}}: Error - {}".format(str(e)))
        return False

def main():
    """Run all endpoint tests"""
    print("Starting QFS API Endpoint Testing...")
    print("=" * 50)

    passed = 0
    total = 7
    test_cid = None

    # Test 1: Status
    print("\n1. Testing GET /status:")
    success, _ = test_status()
    if success:
        passed += 1

    # Test 2: List
    print("\n2. Testing GET /list:")
    success, _ = test_list()
    if success:
        passed += 1

    # Test 3: Upload
    print("\n3. Testing POST /upload:")
    success, cid = test_upload()
    if success:
        passed += 1
        test_cid = cid

    # Test 4: Download
    print("\n4. Testing GET /download/{{cid}}:")
    if test_download(test_cid):
        passed += 1

    # Test 5: Update
    print("\n5. Testing PUT /update/{{cid}}:")
    success, new_cid = test_update(test_cid)
    if success:
        passed += 1
        test_cid = new_cid  # Use new CID for subsequent tests

    # Test 6: Chunk
    print("\n6. Testing GET /chunk/{{cid}}:")
    if test_chunk(test_cid):
        passed += 1

    # Test 7: Delete
    print("\n7. Testing DELETE /delete/{{cid}}:")
    if test_delete(test_cid):
        passed += 1

    # Results
    print("\n" + "=" * 50)
    print("Test Results: {}/{} tests passed".format(passed, total))

    if passed == total:
        print("All endpoints are working correctly!")
        print("This microservice appears to be production ready.")
        return True
    else:
        print("WARNING: {} test(s) failed.".format(total - passed))
        print("This microservice needs attention before production deployment.")
        return False

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)