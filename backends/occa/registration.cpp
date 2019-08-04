// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
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

#include <occa.hpp>

#include "qfunction.hpp"
#include "types.hpp"
#include "vector.hpp"


namespace ceed {
  namespace occa {
    std::string getDefaultDeviceMode(const bool cpuMode, const bool gpuMode) {
      // In case both cpuMode and gpuMode are set, prioritize the GPU if available
      // For example, if the resource is "/*/occa"
      if (gpuMode) {
        if (::occa::modeIsEnabled("CUDA")) {
          return "CUDA";
        }
        if (::occa::modeIsEnabled("HIP")) {
          return "HIP";
        }
        if (::occa::modeIsEnabled("Metal")) {
          return "Metal";
        }
        if (::occa::modeIsEnabled("OpenCL")) {
          return "OpenCL";
        }
      }

      if (cpuMode) {
        if (::occa::modeIsEnabled("OpenMP")) {
          return "OpenMP";
        }
        return "Serial";
      }

      return "";
    }

    static int initCeed(const char *c_resource, Ceed ceed) {
      const std::string resource(c_resource);
      bool cpuMode = resource.find("/cpu/occa") != std::string::npos;
      bool gpuMode = resource.find("/gpu/occa") != std::string::npos;
      bool anyMode = resource.find("/*/occa") != std::string::npos;
      bool validResource = (cpuMode || gpuMode || anyMode);
      std::string resourceProps;

      // Make sure resource is an occa resource
      // Valid:
      //   /cpu/occa
      //   /cpu/occa/
      //   /*/occa/{mode: CUDA, device_id: 0}
      //   /gpu/occa/{mode: CUDA, device_id: 0}
      // Invalid:
      //   /cpu/occa-not
      if (validResource) {
        if (resource.size() > 9) {
          validResource = (resource[9] == '/');
          if (validResource && resource.size() > 10) {
            resourceProps = resource.substr(10);
          }
        }
      }

      if (!validResource) {
        return CeedError(ceed, 1, "OCCA backend cannot use resource: %s", c_resource);
      }

      ::occa::properties deviceProps(resourceProps);
      if (!deviceProps.has("mode")) {
        const std::string defaultMode = (
          ceed::occa::getDefaultDeviceMode(cpuMode || anyMode,
                                           gpuMode || anyMode)
        );
        if (!defaultMode.size()) {
          return CeedError(ceed, 1,
                           "No available OCCA mode for the given resource: %s",
                           c_resource);
        }
        deviceProps["mode"] = defaultMode;
      }

      ceed::occa::Context *context;
      int ierr;
      ierr = CeedCalloc(1, &context); CeedChk(ierr);
      ierr = CeedSetData(ceed, (void**) &context); CeedChk(ierr);

      context->device = ::occa::device(deviceProps);

      return 0;
    }

    static int registerCeedFunction(Ceed ceed, const char *fname, ceed::occa::ceedFunction f) {
      return CeedSetBackendFunction(ceed, "Ceed", ceed, fname, f);
    }

    static int getPreferredMemType(CeedMemType *type) {
      *type = CEED_MEM_DEVICE;
      return 0;
    }

    static int registerMethods(Ceed ceed) {
      int ierr;

      ierr = registerCeedFunction(ceed, "GetPreferredMemType",
                                  (ceed::occa::ceedFunction) ceed::occa::getPreferredMemType);
      CeedChk(ierr);

      ierr = registerCeedFunction(ceed, "VectorCreate",
                                  (ceed::occa::ceedFunction) ceed::occa::Vector::ceedCreate);
      CeedChk(ierr);

#if 0
      ierr = registerCeedFunction(ceed, "BasisCreateTensorH1",
                                  (ceed::occa::ceedFunction) CeedBasisCreateTensorH1_Cuda);
      CeedChk(ierr);

      ierr = registerCeedFunction(ceed, "BasisCreateH1",
                                  (ceed::occa::ceedFunction) CeedBasisCreateH1_Cuda);
      CeedChk(ierr);

      ierr = registerCeedFunction(ceed, "ElemRestrictionCreate",
                                  (ceed::occa::ceedFunction) CeedElemRestrictionCreate_Cuda);
      CeedChk(ierr);

      ierr = registerCeedFunction(ceed, "ElemRestrictionCreateBlocked",
                                  (ceed::occa::ceedFunction) CeedElemRestrictionCreateBlocked_Cuda);
      CeedChk(ierr);
#endif

      ierr = registerCeedFunction(ceed, "QFunctionCreate",
                                  (ceed::occa::ceedFunction) ceed::occa::QFunction::ceedCreate);
      CeedChk(ierr);

#if 0
      ierr = registerCeedFunction(ceed, "OperatorCreate",
                                  (ceed::occa::ceedFunction) CeedOperatorCreate_Cuda);
      CeedChk(ierr);

      ierr = registerCeedFunction(ceed, "CompositeOperatorCreate",
                                  (ceed::occa::ceedFunction) CeedCompositeOperatorCreate_Cuda);
      CeedChk(ierr);
#endif

      return 0;
    }

    static int registerBackend(const char *resource, Ceed ceed) {
      int ierr;

      try {
        ierr = ceed::occa::initCeed(resource, ceed); CeedChk(ierr);
        ierr = ceed::occa::registerMethods(ceed); CeedChk(ierr);
      } catch (::occa::exception exc) {
        std::string error = exc.toString();
        return CeedError(ceed, 1, error.c_str());
      }

      return 0;
    }
  }
}

__attribute__((constructor))
static void Register(void) {
  CeedRegister("/*/occa", ceed::occa::registerBackend, 20);
  CeedRegister("/gpu/occa", ceed::occa::registerBackend, 20);
  CeedRegister("/cpu/occa", ceed::occa::registerBackend, 20);
}