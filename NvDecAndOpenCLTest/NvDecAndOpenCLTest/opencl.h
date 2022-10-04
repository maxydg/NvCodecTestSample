#pragma once

#include <iostream>
#include <CL/cl_d3d11_ext.h>
#include <CL/cl_ext.h>
#include <sstream>

#pragma warning(disable : 4996)
#define INITPFN(x) x = (x##_fn)clGetExtensionFunctionAddress(#x);

std::string formatError(cl_int error) {
  std::ostringstream str;
  str << "CL error " << error << " in " << __FUNCTION__ << " at " << __FILE__
      << " : " << __LINE__;
  return str.str();
}

#define CHECK_ERROR(e)           \
  if (e != CL_SUCCESS) {         \
    _lastError = formatError(e); \
    return false;                \
  }

std::string _lastError;
bool _inited = false;
cl_context _clContext{nullptr};
cl_command_queue _clCommandQueue{nullptr};
clGetDeviceIDsFromD3D11NV_fn clGetDeviceIDsFromD3D11NV{nullptr};
clCreateFromD3D11Texture2DNV_fn clCreateFromD3D11Texture2DNV{nullptr};
clEnqueueAcquireD3D11ObjectsNV_fn clEnqueueAcquireD3D11ObjectsNV{nullptr};
clEnqueueReleaseD3D11ObjectsNV_fn clEnqueueReleaseD3D11ObjectsNV{nullptr};

ID3D11Device* createDX11Device(
    size_t deviceIndex) {
  ID3D11Device* device;
  ID3D11DeviceContext* context;
  IDXGIFactory1* factory;
  IDXGIAdapter* adapter;
  CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory);
  HRESULT hres = factory->EnumAdapters(deviceIndex, &adapter);
  if (FAILED(hres)) {
    std::cout << "Error querying adapter index " << deviceIndex << std::endl;
  }

  D3D_FEATURE_LEVEL featureLevels[] = {
      D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};

#if 0
  hres = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_REFERENCE,
    NULL, D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_DEBUG,
    featureLevels, 2, D3D11_SDK_VERSION, &device, NULL, &context);
#else
  hres = D3D11CreateDevice(
      adapter,
      D3D_DRIVER_TYPE_UNKNOWN,
      NULL,
      0,
      featureLevels,
      2,
      D3D11_SDK_VERSION,
      &device,
      NULL,
      &context);
#endif

  if (FAILED(hres)) {
    std::cout << "Initialization of D3D11 resurces failed with HRESULT " << hres
              << std::endl;
    return nullptr;
  }

  DXGI_ADAPTER_DESC adapterDesc;
  adapter->GetDesc(&adapterDesc);
  char szDesc[80];
  size_t ret;
  wcstombs_s(
      &ret, szDesc, sizeof(szDesc), adapterDesc.Description, sizeof(szDesc));
  std::cout << "GPU in use: " << szDesc << std::endl;

  return device;
}

bool getPlatformID(cl_platform_id* clSelectedPlatformID) {
  char chBuffer[1024];
  cl_uint numOfPlatforms;
  cl_platform_id* clPlatformIDs;
  *clSelectedPlatformID = nullptr;

  // Get OpenCL platform count
  auto error = clGetPlatformIDs(0, nullptr, &numOfPlatforms);
  CHECK_ERROR(error);

  if (numOfPlatforms == 0) {
    _lastError = "No CL platform found";
    return false;
  }

  // If there's a platform or more, make space for ID's
  if ((clPlatformIDs = (cl_platform_id*)malloc(
           numOfPlatforms * sizeof(cl_platform_id))) == nullptr) {
    return false;
  }

  // Get platform info for each platform and trap the NVIDIA platform if
  // found
  error = clGetPlatformIDs(numOfPlatforms, clPlatformIDs, nullptr);
  CHECK_ERROR(error);

  for (cl_uint i = 0; i < numOfPlatforms; i++) {
    error = clGetPlatformInfo(
        clPlatformIDs[i], CL_PLATFORM_NAME, 1024, &chBuffer, nullptr);

    if (error == CL_SUCCESS) {
      if (strstr(chBuffer, "NVIDIA") != nullptr) {
        *clSelectedPlatformID = clPlatformIDs[i];
        break;
      }
    }
  }

  // Default to zeroeth platform if NVIDIA not found
  if (*clSelectedPlatformID == nullptr) {
    *clSelectedPlatformID = clPlatformIDs[0];
  }

  free(clPlatformIDs);

  return true;
}

bool initOpenCL() {
  cl_platform_id cpPlatform;

  if (!getPlatformID(&cpPlatform)) {
    std::cout << "getPlatformID failed" << std::endl;
    return false;
  }

  ID3D11Device* dx11Device = createDX11Device(0);
  if (dx11Device == nullptr) {
    std::cout << "createDX11Device failed" << std::endl;
    return false;
  }

  INITPFN(clGetDeviceIDsFromD3D11NV);
  INITPFN(clCreateFromD3D11Texture2DNV);
  INITPFN(clEnqueueAcquireD3D11ObjectsNV);
  INITPFN(clEnqueueReleaseD3D11ObjectsNV);

  // Query the OpenCL device that would be good for the current D3D device
  // We need to take the one that is on the same Gfx card.

  // Get the device ids for the adapter
  cl_device_id clDevice;
  cl_uint numOfDevices = 0;

  auto error = clGetDeviceIDsFromD3D11NV(
      cpPlatform,
      CL_D3D11_DEVICE_NV,
      dx11Device,
      CL_PREFERRED_DEVICES_FOR_D3D11_NV,
      1,
      &clDevice,
      &numOfDevices);

  CHECK_ERROR(error);

  cl_context_properties props[] = {
      CL_CONTEXT_D3D11_DEVICE_NV,
      (cl_context_properties)dx11Device,
      CL_CONTEXT_PLATFORM,
      (cl_context_properties)cpPlatform,
      0};

  _clContext = clCreateContext(props, 1, &clDevice, nullptr, nullptr, &error);
  CHECK_ERROR(error);

  // Create a command-queue
  _clCommandQueue = clCreateCommandQueue(_clContext, clDevice, 0, &error);
  CHECK_ERROR(error);

  _inited = true;

  std::cout << "#### initOpenCL finish" << std::endl;

  return _inited;
}