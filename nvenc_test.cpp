#include<iostream>
#include "nvEncodeAPI.h"

using namespace std;

int main() {
    cout << "NVENC test starting.... " << endl;
    NV_ENCODE_API_FUNCTION_LIST nvenv = {};
    nvenv.version = NV_ENCODE_API_FUNCTION_LIST_VER;
    
    NVENCSTATUS status = NvEncodeAPICreateInstance(&nvenv);
    if (status != NV_ENC_SUCCESS) {
        cout << "Failed to create NVENC API instance. Error code: " << status << endl;
        return -1;
    }

    cout << "NVENC API instance created successfully." << endl;
    cout << "Function List version: " << nvenv.version << endl;

    return 0;
}