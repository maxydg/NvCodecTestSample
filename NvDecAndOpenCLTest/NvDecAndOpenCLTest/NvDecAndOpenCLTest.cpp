// NvDecAndOpenCLTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <cuda.h>
#include <iostream>
#include "NvDecoder/NvDecoder.h"
#include "../Utils/NvCodecUtils.h"
#include "../Utils/FFmpegDemuxer.h"
#include "opencl.h"
#include <windows.h>

CUcontext cuContext = NULL;
char szInFilePath[256] = "C:\\workstation\\media\\video";
//bool openclInit = false;

simplelogger::Logger *logger = simplelogger::LoggerFactory::CreateConsoleLogger();

bool initCuda() {
  int iGpu = 0;
  ck(cuInit(0));
  int nGpu = 0;
  ck(cuDeviceGetCount(&nGpu));
  if (iGpu < 0 || iGpu >= nGpu) {
    std::ostringstream err;
    err << "GPU ordinal out of range. Should be within [" << 0 << ", "
        << nGpu - 1 << "]" << std::endl;
    throw std::invalid_argument(err.str());
  }
  CUdevice cuDevice = 0;
  ck(cuDeviceGet(&cuDevice, iGpu));
  char szDeviceName[80];
  ck(cuDeviceGetName(szDeviceName, sizeof(szDeviceName), cuDevice));
  std::cout << "GPU in use: " << szDeviceName << std::endl;
  ck(cuCtxCreate(&cuContext, CU_CTX_SCHED_BLOCKING_SYNC, cuDevice));

  return true;
}


int startDecode(CUcontext cuContext, char* szInFilePath) {
  FFmpegDemuxer demuxer(szInFilePath);
  NvDecoder dec(cuContext, true, FFmpeg2NvCodecId(demuxer.GetVideoCodec()));
  CUdeviceptr dpFrame = 0;
  int nVideoBytes = 0, nFrameReturned = 0, nFrame = 0;
  uint8_t *pVideo = NULL, **ppFrame;
  int64_t pts, *pTimestamp;
  bool m_bFirstFrame = true;
  int64_t firstPts = 0, startTime = 0;
  do {
      demuxer.Demux(&pVideo, &nVideoBytes, &pts);
      dec.Decode(pVideo, nVideoBytes, &ppFrame, &nFrameReturned, 0, &pTimestamp, pts);
      std::cout << "nFrameReturned = " << nFrameReturned << std::endl;
  } while (nVideoBytes);
  std::cout << "Total frame decoded: " << nFrame << std::endl;
  return 0;
}




int main()
{
  std::cout << "initCuda" << std::endl;
  initCuda();
  std::cout << "initOpenCL" << std::endl;
  initOpenCL();
  std::cout << "startDecode" << std::endl;
  startDecode(cuContext, szInFilePath);
}
