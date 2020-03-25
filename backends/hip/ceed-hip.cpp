// Copyright (c) 2017-2018, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory. LLNL-CODE-734707.
// All Rights reserved. See files LICENSE and NOTICE for details.
//
// This file is part of CEED, a collection of benchmarks, miniapps, software
// libraries and APIs for efficient high-order finite element and spectral
// element discretizations for exascale applications. For more information and
// source code availability see http://github.com/ceed.
//
// The CEED research is supported by the Exascale Computing Project 17-SC-20-SC,
// a collaborative effort of two U.S. Department of Energy organizations (Office
// of Science and the National Nuclear Security Administration) responsible for
// the planning and preparation of a capable exascale ecosystem, including
// software, applications, hardware, advanced system engineering and early
// testbed platforms, in support of the nation's exascale computing imperative.

#include <ceed-backend.h>
#include <string.h>
#include <stdarg.h>
#include "ceed-hip.h"

#include <vector>

int CeedCompileHip(Ceed ceed, const char *source, hipModule_t *module,
                    const CeedInt numopts, ...) {
  int ierr;
  hipFree(0);//Make sure a Context exists for hiprtc
  hiprtcProgram prog;
  CeedChk_Hiprtc(ceed, hiprtcCreateProgram(&prog, source, NULL, 0, NULL, NULL));

  const int optslen = 32;
  const int optsextra = 4;
  const char *opts[numopts + optsextra];
  char buf[numopts][optslen];
  if (numopts>0) {
    va_list args;
    va_start(args, numopts);
    char *name;
    int val;
    for (int i = 0; i < numopts; i++) {
      name = va_arg(args, char *);
      val = va_arg(args, int);
      snprintf(&buf[i][0], optslen,"-D%s=%d", name, val);
      opts[i] = &buf[i][0];
    }
  }
  opts[numopts]     = "-DCeedScalar=double";
  opts[numopts + 1] = "-DCeedInt=int";
  opts[numopts + 2] = "-default-device";
  struct hipDeviceProp_t prop;
  Ceed_Hip *ceed_data;
  ierr = CeedGetData(ceed, (void **)&ceed_data); CeedChk(ierr);
  ierr = hipGetDeviceProperties(&prop, ceed_data->deviceId);
  CeedChk_Hip(ceed, ierr);
  char buff[optslen];
  std::string gfxName = "gfx" + std::to_string(prop.gcnArch);
  std::string archArg = "--gpu-architecture="  + gfxName;
  snprintf(buff, optslen, archArg.c_str());
  opts[numopts + 3] = buff;

  hiprtcResult result = hiprtcCompileProgram(prog, numopts + optsextra, opts);
  if (result != HIPRTC_SUCCESS) {
    size_t logsize;
    CeedChk_Hiprtc(ceed, hiprtcGetProgramLogSize(prog, &logsize));
    char *log;
    ierr = CeedMalloc(logsize, &log); CeedChk(ierr);
    CeedChk_Hiprtc(ceed, hiprtcGetProgramLog(prog, log));
    return CeedError(ceed, (int)result, "%s\n%s", hiprtcGetErrorString(result), log);
  }

  size_t ptxsize;
  CeedChk_Hiprtc(ceed, hiprtcGetCodeSize(prog, &ptxsize));
  char *ptx;
  ierr = CeedMalloc(ptxsize, &ptx); CeedChk(ierr);
  CeedChk_Hiprtc(ceed, hiprtcGetCode(prog, ptx));
  CeedChk_Hiprtc(ceed, hiprtcDestroyProgram(&prog));

  CeedChk_Hip(ceed, hipModuleLoadData(module, ptx));
  ierr = CeedFree(&ptx); CeedChk(ierr);

  return 0;
}

int CeedGetKernelHip(Ceed ceed, hipModule_t module, const char *name,
                      hipFunction_t *kernel) {
  CeedChk_Hip(ceed, hipModuleGetFunction(kernel, module, name));
  return 0;
}

int CeedRunKernelHip(Ceed ceed, hipFunction_t kernel, const int gridSize,
                      const int blockSize, void **args) {

  CeedChk_ModHip(ceed, hipModuleLaunchKernel(kernel,
                                  gridSize, 1, 1,
                                  blockSize, 1, 1,
                                  0, NULL,
                                  args, NULL));

  return 0;
}

int CeedRunKernelDimHip(Ceed ceed, hipFunction_t kernel, const int gridSize,
                         const int blockSizeX, const int blockSizeY,
                         const int blockSizeZ, void **args) {
  CeedChk_ModHip(ceed, hipModuleLaunchKernel(kernel,
                                  gridSize, 1, 1,
                                  blockSizeX, blockSizeY, blockSizeZ,
                                  0, NULL,
                                  args, NULL));
  return 0;
}

int CeedRunKernelDimSharedHip(Ceed ceed, hipFunction_t kernel, const int gridSize,
                               const int blockSizeX, const int blockSizeY,
                               const int blockSizeZ, const int sharedMemSize,
                               void **args) {

  CeedChk_ModHip(ceed, hipModuleLaunchKernel(kernel,
                                  gridSize, 1, 1,
                                  blockSizeX, blockSizeY, blockSizeZ,
                                  sharedMemSize, NULL,
                                  args, NULL));
  return 0;
}

static int CeedGetPreferredMemType_Hip(CeedMemType *type) {
  *type = CEED_MEM_DEVICE;
  return 0;
}

int CeedHipInit(Ceed ceed, const char *resource, int nrc) {
  int ierr;
  const int rlen = strlen(resource);
  const bool slash = (rlen>nrc) ? (resource[nrc] == '/') : false;
  const int deviceID = (slash && rlen > nrc + 1) ? atoi(&resource[nrc + 1]) : 0;

  int currentDeviceID;
  ierr = hipGetDevice(&currentDeviceID); CeedChk_Hip(ceed,ierr);
  if (currentDeviceID!=deviceID) {
    ierr = hipSetDevice(deviceID); CeedChk_Hip(ceed,ierr);
  }

  struct hipDeviceProp_t deviceProp;
  ierr = hipGetDeviceProperties(&deviceProp, deviceID); CeedChk_Hip(ceed,ierr);

  Ceed_Hip *data;
  ierr = CeedGetData(ceed, (void **)&data); CeedChk(ierr);
  data->deviceId = deviceID;
  data->optblocksize = deviceProp.maxThreadsPerBlock;
  return 0;
}

static int CeedInit_Hip(const char *resource, Ceed ceed) {
  int ierr;
  const int nrc = 9; // number of characters in resource
  if (strncmp(resource, "/gpu/hip/ref", nrc))
    return CeedError(ceed, 1, "Hip backend cannot use resource: %s", resource);

  Ceed_Hip *data;
  ierr = CeedCalloc(1,&data); CeedChk(ierr);
  ierr = CeedSetData(ceed,(void **)&data); CeedChk(ierr);
  ierr = CeedHipInit(ceed, resource, nrc); CeedChk(ierr);

  ierr = CeedSetBackendFunction(ceed, "Ceed", ceed, "GetPreferredMemType",
                                reinterpret_cast<int (*)()>(CeedGetPreferredMemType_Hip)); CeedChk(ierr);
  ierr = CeedSetBackendFunction(ceed, "Ceed", ceed, "VectorCreate",
                                reinterpret_cast<int (*)()>(CeedVectorCreate_Hip)); CeedChk(ierr);
  ierr = CeedSetBackendFunction(ceed, "Ceed", ceed, "BasisCreateTensorH1",
                                reinterpret_cast<int (*)()>(CeedBasisCreateTensorH1_Hip)); CeedChk(ierr);
  ierr = CeedSetBackendFunction(ceed, "Ceed", ceed, "BasisCreateH1",
                                reinterpret_cast<int (*)()>(CeedBasisCreateH1_Hip)); CeedChk(ierr);
  ierr = CeedSetBackendFunction(ceed, "Ceed", ceed, "ElemRestrictionCreate",
                                reinterpret_cast<int (*)()>(CeedElemRestrictionCreate_Hip)); CeedChk(ierr);
  ierr = CeedSetBackendFunction(ceed, "Ceed", ceed,
                                "ElemRestrictionCreateBlocked",
                                reinterpret_cast<int (*)()>(CeedElemRestrictionCreateBlocked_Hip));
  CeedChk(ierr);
  ierr = CeedSetBackendFunction(ceed, "Ceed", ceed, "QFunctionCreate",
                                reinterpret_cast<int (*)()>(CeedQFunctionCreate_Hip)); CeedChk(ierr);
  ierr = CeedSetBackendFunction(ceed, "Ceed", ceed, "OperatorCreate",
                                reinterpret_cast<int (*)()>(CeedOperatorCreate_Hip)); CeedChk(ierr);
  return 0;
}

__attribute__((constructor))
static void Register(void) {
  CeedRegister("/gpu/hip/ref", CeedInit_Hip, 20);
}
