/*
 * Copyright (C) 2019 Samsung Electronics Co. LTD
 *
 * This software is proprietary of Samsung Electronics.
 * No part of this software, either material or conceptual may be copied or distributed, transmitted,
 * transcribed, stored in a retrieval system or translated into any human or computer language in any form by any means,
 * electronic, mechanical, manual or otherwise, or disclosed
 * to third parties without the express written permission of Samsung Electronics.
 */

/**
 * @file    EdenServiceDelegatorService.h
 * @brief   This is EdenServiceDelegatorService class file.
 * @details This header defines EdenServiceDelegator class.
 *          This class is implementing proxy role for Eden Runtime service.
 * @author  minsu.jeon (minsu.jeon@samsung.com)
 *          yeongjun.kim (yj0576.kim@samsung.com)
 */

#ifndef DRIVER_EDENSERVICEDELEGATORSERVICE_H_
#define DRIVER_EDENSERVICEDELEGATORSERVICE_H_

#include <cstdint>  // int32_t

#include "../include/eden_model.h"

using namespace eden::nn;

namespace android {
namespace nn {
namespace eden_driver {

class EdenServiceDelegatorService : public EdenServiceDelegator {
 public:
    int32_t Init() override;
    int32_t OpenModel(EdenModel* model, uint32_t* modelId, ModelPreference preference) override;
    int32_t OpenModelFromMemory(ModelTypeInMemory modelTypeInMemory, int8_t* addr, int32_t size,
                                bool encrypted, uint32_t* modelId, ModelPreference preference) override;
    int32_t ExecuteReq(EdenRequest* req, EdenEvent** evt, RequestPreference preference) override;
    int32_t ExecuteRequest(EdenRequest* request, RequestOptions requestOptions) override;
    int32_t CloseModel(uint32_t modelId) override;
    int32_t Shutdown(void) override;

    int32_t AllocateInputBuffers(uint32_t modelId, EdenBuffer** buffers, int32_t* numOfBuffers) override;
    int32_t AllocateOutputBuffers(uint32_t modelId, EdenBuffer** buffers, int32_t* numOfBuffers) override;
    int32_t FreeBuffers(uint32_t modelId, EdenBuffer* buffers) override;
    int32_t GetState(EdenState* state) override;
    int32_t GetInputBufferShape(uint32_t modelId, int32_t inputIndex,
                                int32_t* width, int32_t* height, int32_t* channel, int32_t* number) override;
    int32_t GetOutputBufferShape(uint32_t modelId, int32_t outputIndex,
                                 int32_t* width, int32_t* height, int32_t* channel, int32_t* number) override;
};

}  // namespace eden_driver
}  // namespace nn
}  // namespace android

#endif  // DRIVER_EDENSERVICEDELEGATORSERVICE_H_

