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
 * @file    EdenModelConvertLib.cpp
 * @brief   This is EdenModelConvertLib class file.
 * @details This header defines functions to convert Android NN Model to Eden NN Model.
 * @author  minsu.jeon (minsu.jeon@samsung.com)
 */

#include <iostream>
#include <algorithm>
#include <limits>      // numeric_limits
#include <cstring>     // strlen
#include <sys/mman.h>  // mmap, munmap
#include <cmath>       // floor

#include "log.h"
#include "Utils.h"               // convertToV1_0, convertToV1_1, android::nn::initVLogMask, logModelToInfo, DRIVER

#include "NeuralNetworks.h"     // Operation, Operand, ANEURALNETWORKS_ADD etc
#include "ActivationFunctor.h"  // kActivationNone, kActivationRelu, kActivationTanh etc

#include "Common.h"             // DATA_TYPE, OperationInfo
#include "MyUtils.h"
#include "EdenModelConvertLib.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "EdenDriver::EdenModelConvertLib"

using namespace eden::nn;

namespace android {
namespace nn {
namespace eden_driver {

enum PaddingScheme {
    kPaddingUnknown = 0,
    kPaddingSame = 1,
    kPaddingValid = 2,
};

static const char* edenOperandNames[] = {
    "IFM",
    "Tensor_000",
    "Kernel_000",
    "Bias_000",

    // SVDF
    "WeightsFeature",
    "WeightsTime",
    "State",

    // LSH_PROJECTION
    "Hash",
    "Weight",

    // BATCH_TO_SPACE_ND
    "BlockShape",

    // PAD
    "Paddings",

    // STRIDED_SLICE
    "Begin",
    "End",
    "Strides",

    // SPACE_TO_BATCH_ND
    "Block",
    "Pad",
    "Axis",
    "KeepDims",

    // HASHTABLE_LOOKUP, EMBEDDING_LOOKUP
    "Lookups",
    "Keys",
    "Values",
    "Hits",

    // RNN, UNIDIRECTIONAL_SEQUENCE_RNN
    "Weights",
    "RecurrentWeights",
    "HiddenState",

    // LSTM
    "InputToInputWeights",
    "InputToForgetWeights",
    "InputToCellWeights",
    "InputToOutputWeights",

    "RecurrentToInputWeights",
    "RecurrentToForgetWeights",
    "RecurrentToCellWeights",
    "RecurrentToOutputWeights",

    "CellToInputWeights",
    "CellToForgetWeights",
    "CellToOutputWeights",

    "InputGateBias",
    "ForgetGateBias",
    "CellGateBias",
    "OutputGateBias",

    "ProjectionWeights",
    "ProjectionBias",

    "InputActivationState",
    "InputCellState",

    "ScratchBuffer",
    "OutputState",
    "CellState",

    "InputLayerNormWeights",
    "ForgetLayerNormWeights",
    "CellLayerNormWeights",
    "OutputLayerNormWeights",

    // BIDIRECTIONAL_SEQUENCE_LSTM
    "FwInputToInputWeights",
    "FwInputToForgetWeights",
    "FwInputToCellWeights",
    "FwInputToOutputWeights",

    "FwRecurrentToInputWeights",
    "FwRecurrentToForgetWeights",
    "FwRecurrentToCellWeights",
    "FwRecurrentToOutputWeights",

    "FwCellToInputWeights",
    "FwCellToForgetWeights",
    "FwCellToOutputWeights",

    "FwInputGateBias",
    "FwForgetGateBias",
    "FwCellGateBias",
    "FwOutputGateBias",
    "FwProjectionWeights",
    "FwProjectionBias",

    "BwInputToInputWeights",
    "BwInputToForgetWeights",
    "BwInputToCellWeights",
    "BwInputToOutputWeights",

    "BwRecurrentToInputWeights",
    "BwRecurrentToForgetWeights",
    "BwRecurrentToCellWeights",
    "BwRecurrentToOutputWeights",

    "BwCellToInputWeights",
    "BwCellToForgetWeights",
    "BwCellToOutputWeights",

    "BwInputGateBias",
    "BwForgetGateBias",
    "BwCellGateBias",
    "BwOutputGateBias",
    "BwProjectionWeights",
    "BwProjectionBias",

    "FwInputActivationState",
    "FwInputCellState",
    "BwInputActivationState",
    "BwInputCellState",

    "FwAuxInputToInputWeights",
    "FwAuxInputToForgetWeights",
    "FwAuxInputToCellWeights",
    "FwAuxInputToOutputWeights",

    "BwAuxInputToInputWeights",
    "BwAuxInputToForgetWeights",
    "BwAuxInputToCellWeights",
    "BwAuxInputToOutputWeights",

    "FwInputLayerNormWeights",
    "FwForgetLayerNormWeights",
    "FwCellLayerNormWeights",
    "FwOutputLayerNormWeights",

    "BwInputLayerNormWeights",
    "BwForgetLayerNormWeights",
    "BwCellLayerNormWeights",
    "BwOuputLayerNormWeights",

    // UNIDIRECTIONAL_SEQUENCE_LSTM
    "LSTM_IntoIn",
    "LSTM_IntoForget",
    "LSTM_IntoCell",
    "LSTM_IntoOut",
    "LSTM_RetoIn",
    "LSTM_RetoForget",
    "LSTM_RetoCell",
    "LSTM_RetoOut",
    "LSTM_CelltoIn",
    "LSTM_CelltoForget",
    "LSTM_CelltoOut",
    "LSTM_InGate",
    "LSTM_ForgetGate",
    "LSTM_CellGate",
    "LSTM_OutGate",
    "LSTM_ProjWeight",
    "LSTM_ProjBias",
    "LSTM_InAct",
    "LSTM_InCell",
    "LSTM_Input_Layer_Norm_Weights",
    "LSTM_Forget_Layer_Norm_Weights",
    "LSTM_Cell_Layer_Norm_Weights",
    "LSTM_Output_Layer_Norm_Weights",

    // PAD_V2
    "PaddingShape",
    "PaddingValue",

    // DETECTION_POSTPROCESSING
    "ANCHORS",

    "PReLU",

    "Indices",
    "Gather",

    "Split",

    "ExpandDims",
    "Size",
    "Multipliers",

    // BIDIRECTIONAL_SEQUENCE_RNN
    "FwWeights",
    "FwRecurrentWeights",
    "FwBias",
    "FwHiddenState",

    "BwWeights",
    "BwRecurrentWeights",
    "BwBias",
    "BwHiddenState",

    "FwAuxWeights",
    "BwAuxWeights",
};

static const char* edenOperationNames[] = {
    "ADD",
    "AVERAGE_POOL_2D",
    "CONCATENATION",
    "CONV_2D",
    "DEPTHWISE_CONV_2D",

    "DEPTH_TO_SPACE",
    "DEQUANTIZE",
    "EMBEDDING_LOOKUP",
    "FLOOR",
    "FULLY_CONNECTED",

    "HASHTABLE_LOOKUP",
    "L2_NORMALIZATION",
    "L2_POOL_2D",
    "LOCAL_RESPONSE_NORMALIZATION",
    "LOGISTIC",

    "LSH_PROJECTION",
    "LSTM",
    "MAX_POOL_2D",
    "MUL",
    "RELU",

    "RELU1",
    "RELU6",
    "RESHAPE",
    "RESIZE_BILINEAR",
    "RNN",

    "SOFTMAX",
    "SPACE_TO_DEPTH",
    "SVDF",
    "TANH",
    "BATCH_TO_SPACE_ND",

    "DIV",
    "MEAN",
    "PAD",
    "SPACE_TO_BATCH_ND",
    "SQUEEZE",

    "STRIDED_SLICE",
    "SUB",
    "TRANSPOSE",
    "ABS",      // V1.2
    "ARGMAX", // 39

    "ARGMIN",
    "AXIS_ALIGNED_BBOX_TRANSFORM",
    "BIDIRECTIONAL_SEQUENCE_LSTM",
    "BIDIRECTIONAL_SEQUENCE_RNN",
    "BOX_WITH_NMS_LIMIT", // 44

    "CAST",
    "CHANNEL_SHUFFLE",
    "DETECTION_POSTPROCESSING",
    "EQUAL",
    "EXP", //49

    "EXPAND_DIMS",
    "GATHER",
    "GENERATE_PROPOSALS",
    "GREATER",
    "GREATER_EQUAL", //55

    "GROUPED_CONV_2D",
    "HEATMAP_MAX_KEYPOINT",
    "INSTANCE_NORMALIZATION",
    "LESS",
    "LESS_EQUAL", // 59

    "LOG",
    "LOGICAL_AND",
    "LOGICAL_NOT",
    "LOGICAL_OR",
    "LOG_SOFTMAX", // 64

    "MAXIMUM",
    "MINIMUM",
    "NEG",
    "NOT_EQUAL",
    "PAD_V2", // 69

    "POW",
    "PRELU",
    "QUANTIZE",
    "QUANTIZED_16BIT_LSTM",
    "RANDOM_MULTINOMIAL", // 74

    "REDUCE_ALL",
    "REDUCE_ANY",
    "REDUCE_MAX",
    "REDUCE_MIN",
    "REDUCE_PROD", // 79

    "REDUCE_SUM",
    "ROI_ALIGN",
    "ROI_POOLING",
    "RSQRT",
    "SELECT", // 84

    "SIN",
    "SLICE",
    "SPLIT",
    "SQRT",
    "TILE", // 89

    "TOPK_V2",
    "TRANSPOSE_CONV_2D",
    "UNIDIRECTIONAL_SEQUENCE_LSTM",
    "UNIDIRECTIONAL_SEQUENCE_RNN",
    "RESIZE_NEAREST_NEIGHBOR", // 94
};

static const char* edenOptionNames[] = {
    "AddOptions",
    "AveragePool2DOptions",
    "ConcatenationOptions",
    "Conv2DOptions",
    "DepthwiseConv2DOptions",

    "DepthToSpaceOptions",
    "DequantizeOptions",
    "EmbeddingLookupOptions",
    "FloorOptions",
    "FullyConnectedOptions",

    "HashtableLookupOptions",
    "L2NormalizationOptions",
    "L2Pool2DOptions",
    "LocalResponseNormalization",
    "LogisticOptions",

    "LshProjectionOptions",
    "LSTMOptions",
    "MaxPool2DOptions",
    "MulOptions",
    "RELUOptions",

    "RELU1Options",
    "RELU6Options",
    "ReshapeOptions",
    "ResizeBilinearOptions",
    "RNNOptions",

    "SoftmaxOptions",
    "SpaceToDepthOptions",
    "SVDFOptions",
    "TanhOptions",
    "BatchToSpaceNDOptions",

    "DivOptions",
    "MeanOptions",
    "CustomOptions",
    "SpaceToBatchNDOptions",
    "SqueezeOptions",

    "StridedSliceOptions",
    "SubOptions",
    "TransposeOptions",
    "AbsOptions", // v1.2
    "ArgmaxOptions",

    "ArgminOptions",
    "AxisAlignedBboxTransfromOptions",
    "BidirectionalSequenceLSTMOptions",
    "BidirectionalRNNOptions",
    "BoxWithNmsLimitOptions", // 44

    "CastOptions",
    "ChannelShuffleOptions",
    "DetectionPostprocessingOptions",
    "EqualOptions",
    "ExpOptions", // 49

    "ExpandDimsOptions",
    "GatherOptions",
    "GenerateProposalOptions",
    "GreaterOptions",
    "GreaterEqualOptional", // 55

    "Conv2DOptions",
    "HeatMapMaxKeyPointOptions",
    "InstanceNormalizationOptions",
    "LessOptions",
    "LessEqualOptions", // 59

    "LogOptions",
    "LogicalAndOptions",
    "LogicalNotOptions",
    "LogicalOrOptions",
    "LogSoftmaxOptions", // 64

    "ElementwiseMaxOptions",
    "MinimumOptions",
    "NegOptions",
    "NotEqualOptions",
    "PadV2Options", // 69

    "PowerOptions",
    "PReLUOptions",
    "QuantizeOptions",
    "Quanzed16bitLstmOptions",
    "RandomMultinomialOptions", // 74

    "ReduceOptions",
    "ReduceOptions",
    "ReduceOptions",
    "ReduceOptions",
    "ReduceOptions", // 79

    "ReduceOptions",
    "RoiAlignOptions",
    "TFliteRoiPoolOptions",
    "RsqrtOptions",
    "SelectOptions", // 84

    "SinOptions",
    "SliceOptions",
    "SplitOptions",
    "SqrtOptions",
    "TileOptions", // 89

    "TopK_V2Options",
    "Deconv2DOptions",
    "UnidirectionalSequenceLstmOptions",
    "UnidirectionalSequenceRNNOptions",
    "ResizeNearestNeighborOptions",
};

const char* androidNNTypeNames[] = {
    "FLOAT32",                         // 0
    "INT32",                           // 1
    "UINT32",                          // 2
    "TENSOR_FLOAT32",                  // 3
    "TENSOR_INT32",                    // 4
    "TENSOR_QUANT8_ASYMM",             // 5
    "BOOL",                            // 6
    "TENSOR_QUANT16_SYMM",             // 7
    "TENSOR_FLOAT16",                  // 8
    "TENSOR_BOOL8",                    // 9
    "FLOAT16",                         // 10
    "TENSOR_QUANT8_SYMM_PER_CHANNEL",  // 11
    "TENSOR_QUANT16_ASYMM",            // 12
    "TENSOR_QUANT8_SYMM",              // 13

    // @todo Below should not be used directly
    "OEM",                             // 10000
    "TENSOR_OEM_BYTE",                 // 10001
};

enum EDEN_CUSTOM_OPERAND_NAME_IDX {
    NCP_BINARY,
    NCP_NAME,
};

static const char* edenCustomOperandNames[] = {
    "NCP_BINARY",
    "NCP_NAME",
};

enum EDEN_CUSTOM_OPERATION_NAME_IDX {
    CUSTOM_OP_NCP,
    CUSTOM_OP_NORMALIZATION,
    CUSTOM_OP_QUANTIZATION,
    CUSTOM_OP_DEQUANTIZATION,
};

static const char* edenCustomOperationNames[] = {
    "NCP",
    "Normalization",
    "Quantization",
    "Dequantization",
};

HwPreference getHwPreference(const ModelInfo& modelInfo) {
    bool targetDevices[4] = {false, false, false, false};
    for (const NNSubOperationList& subOperationList : modelInfo.vecOpGroup) {
        targetDevices[static_cast<int32_t>(subOperationList.targetDevice)] = true;
    }

    // @todo in fact, we need to find more grace way to explain target devices
    HwPreference hwPreference = HWCOUNT;
    do {
        if (targetDevices[static_cast<int32_t>(NN_TARGET_DEVICE::NPU)]) {
            LOGD(EDEN_DRIVER, "targetDevice=NPU\n");
#if 0
            if (hwPreference == HWCOUNT) {
                hwPreference = NPU_ONLY;
            } else {
                hwPreference = ALL_HW;
                break;
            }
#else
            // NOTICE Above code was intended code but MCD's Prevent tool report it as error.
            // To avoid this error, above code was replaced to below code.
            // But if this code snippet order was changed, please reconsider above code.
            hwPreference = NPU_ONLY;
#endif
        }
        if (targetDevices[static_cast<int32_t>(NN_TARGET_DEVICE::GPU)]) {
            LOGD(EDEN_DRIVER, "targetDevice=GPU\n");
            if (hwPreference == HWCOUNT) {
                hwPreference = GPU_ONLY;
            } else {
                hwPreference = ALL_HW;
                break;
            }
        }
        if (targetDevices[static_cast<int32_t>(NN_TARGET_DEVICE::CPU)]) {
            LOGD(EDEN_DRIVER, "targetDevice=CPU\n");
            if (hwPreference == HWCOUNT) {
                hwPreference = CPU_ONLY;
            } else {
                hwPreference = ALL_HW;
                break;
            }
        }
    } while (0);

    LOGD(EDEN_DRIVER, "Selected hwPreference is %d (0=ALL_HW, 1=NPU_ONLY, 2=GPU_ONLY, 3=CPU_ONLY)\n", hwPreference);
    return hwPreference;
}

const char* getAndroidNNOperandTypeName(V1_2::OperandType type) {
    constexpr uint32_t numOfNameArray = sizeof(androidNNTypeNames) / sizeof(char*);
    uint32_t typeIdx = static_cast<uint32_t>(type);
    if (typeIdx >= numOfNameArray) {
        typeIdx = (typeIdx - 10000) + (numOfNameArray - 2);
        if (typeIdx >= numOfNameArray) {
            LOGE(EDEN_DRIVER, "Error, invalid type was delivered! (typeIdx=%d), set it to last index(TENSOR_OEM_BYTE) as default!\n", typeIdx);
            typeIdx = numOfNameArray - 1;
        }
    }
    return androidNNTypeNames[typeIdx];
}

void copyName(const char* targetName, char*& name, int32_t& length) {
    int32_t lengthOfName = strlen(targetName);
    char* newName = new char[lengthOfName + 1];  // +1 for \0
    strncpy(newName, targetName, lengthOfName);
    newName[lengthOfName] = '\0';

    // Return to caller
    name = newName;
    length = lengthOfName;
}

void releaseName(int8_t*& name) {
    if (name) {
        delete[] name;
    }
    name = nullptr;
}

void getMatchedEdenOperandName(int32_t nameIndex, int8_t*& name, int32_t& length) {
    const char* edenOperandName = edenOperandNames[nameIndex];

    copyName(edenOperandName, reinterpret_cast<char*&>(name), length);
    LOGD(EDEN_DRIVER, "copyName edenOperandName -> %s\n", name);
}

void getMatchedEdenOperationName(int32_t nameIndex, int8_t*& name, int32_t& length) {
    const char* edenOperationName = edenOperationNames[nameIndex];

    copyName(edenOperationName, reinterpret_cast<char*&>(name), length);
}

void getEdenCustomOperandName(int32_t nameIndex, int8_t*& name, int32_t& length) {
    const char* edenCustomOperandName = edenCustomOperandNames[nameIndex];

    copyName(edenCustomOperandName, reinterpret_cast<char*&>(name), length);
}

void getEdenCustomOperationName(int32_t nameIndex, int8_t*& name, int32_t& length) {
    const char* edenCustomOperationName = edenCustomOperationNames[nameIndex];

    copyName(edenCustomOperationName, reinterpret_cast<char*&>(name), length);
}

void getEdenCustomOperationNameForNCP(int8_t*& name, int32_t& length) {
    const char* edenCustomOperationName = edenCustomOperationNames[CUSTOM_OP_NCP];

    copyName(edenCustomOperationName, reinterpret_cast<char*&>(name), length);
}

void getOptionsName(uint32_t type, char*& name, int32_t& length) {
    const char* opName = edenOptionNames[type];

    copyName(opName, reinterpret_cast<char*&>(name), length);
}

void getModelNameForNCP(int8_t*& name, int32_t& length) {
    static int32_t uniqueNumber = 1;
    char strNum[4] = { 0, };
    std::string ncpName("SubModelForNCP_");
    std::sprintf(strNum, "%d", uniqueNumber++);
    std::string strUniqueNumber(strNum);
    ncpName += strUniqueNumber;

    copyName(ncpName.c_str(), reinterpret_cast<char*&>(name), length);
}

EdenDataType getEdenDataType(V1_2::OperandType androidDataType) {
    EdenDataType edenDataType = DATA_TYPE_FLOAT32;

    // at this moment only float32 is supported
    switch (androidDataType) {
    case V1_2::OperandType::FLOAT32:               // 0
        edenDataType = DATA_TYPE_FLOAT32;
        break;
    case V1_2::OperandType::INT32:                 // 1
        edenDataType = DATA_TYPE_INT32;
        break;
    case V1_2::OperandType::UINT32:                // 2
        // @todo There is no DATA_TYPE_UINT32 on EdenModel
        edenDataType = DATA_TYPE_INT32;
        break;
    case V1_2::OperandType::TENSOR_FLOAT32:        // 3
        edenDataType = DATA_TYPE_FLOAT32;
        break;
    case V1_2::OperandType::TENSOR_INT32:          // 4
        edenDataType = DATA_TYPE_INT32;
        break;
    case V1_2::OperandType::TENSOR_QUANT8_ASYMM:   // 5
        edenDataType = DATA_TYPE_UINT8;
        break;
    case V1_2::OperandType::BOOL:                  // 6
        edenDataType = DATA_TYPE_BOOL8;
        break;
    case V1_2::OperandType::TENSOR_QUANT16_SYMM:   // 7
        edenDataType = DATA_TYPE_INT16;
        break;
    case V1_2::OperandType::TENSOR_FLOAT16:        // 8
        edenDataType = DATA_TYPE_FLOAT16;
        break;
    case V1_2::OperandType::TENSOR_BOOL8:          // 9
        edenDataType = DATA_TYPE_INT8;
        break;
    case V1_2::OperandType::FLOAT16:               // 10
        edenDataType = DATA_TYPE_FLOAT16;
        break;
    case V1_2::OperandType::TENSOR_QUANT8_SYMM_PER_CHANNEL:  // 11
        edenDataType = DATA_TYPE_INT8;
        break;
    case V1_2::OperandType::TENSOR_QUANT16_ASYMM:  // 12
        edenDataType = DATA_TYPE_INT16;
        break;
    case V1_2::OperandType::TENSOR_QUANT8_SYMM:    // 13
        edenDataType = DATA_TYPE_INT8;
        break;
    default:
        LOGE(EDEN_DRIVER, "Not supported Operand Type (type=%d)", static_cast<int32_t>(androidDataType));
        break;
    }

    return edenDataType;
}

DATA_TYPE getDataType(V1_2::OperandType androidDataType) {
    DATA_TYPE dataType = DATA_TYPE::FLOAT32;

    if (androidDataType == V1_2::OperandType::FLOAT32) {
        dataType = DATA_TYPE::FLOAT32;
    } else if (androidDataType == V1_2::OperandType::INT32) {
        dataType = DATA_TYPE::INT32;
    } else if (androidDataType == V1_2::OperandType::UINT32) {
        // @todo There is no DATA_TYPE::UINT32
        dataType = DATA_TYPE::INT32;
    } else if (androidDataType == V1_2::OperandType::TENSOR_FLOAT32) {
        dataType = DATA_TYPE::FLOAT32;
    } else if (androidDataType == V1_2::OperandType::TENSOR_INT32) {
        dataType = DATA_TYPE::INT32;
    } else if (androidDataType == V1_2::OperandType::TENSOR_QUANT8_ASYMM) {
        dataType = DATA_TYPE::QUANT8;
    } else if (androidDataType == V1_2::OperandType::BOOL) {
        dataType = DATA_TYPE::BOOL8;
    } else if (androidDataType == V1_2::OperandType::TENSOR_QUANT16_SYMM) {
        dataType = DATA_TYPE::INT16;
    } else if (androidDataType == V1_2::OperandType::TENSOR_FLOAT16) {
        dataType = DATA_TYPE::FLOAT16;
    } else if (androidDataType == V1_2::OperandType::TENSOR_BOOL8) {
        dataType = DATA_TYPE::INT8;
    } else if (androidDataType == V1_2::OperandType::FLOAT16) {
        dataType = DATA_TYPE::FLOAT16;
    } else if (androidDataType == V1_2::OperandType::TENSOR_QUANT8_SYMM_PER_CHANNEL) {
        dataType = DATA_TYPE::INT8;
    } else if (androidDataType == V1_2::OperandType::TENSOR_QUANT16_ASYMM) {
        dataType = DATA_TYPE::INT16;
    } else if (androidDataType == V1_2::OperandType::TENSOR_QUANT8_SYMM) {
        dataType = DATA_TYPE::INT8;
    } else {
        LOGE(EDEN_DRIVER, "Error, type=%d is not supported, set it default type (INT32)!\n",  static_cast<int32_t>(androidDataType));
    }

    LOGD(EDEN_DRIVER, "androidDataType=%d, dataType=%d (0:FLOAT32, 1:QUANT8, 2:RELAXED_FLOAT32, 3:INT32)\n",
                       static_cast<int32_t>(androidDataType), static_cast<int32_t>(dataType));

    return dataType;
}

DATA_TYPE getDataTypeFromEden(EdenDataType edenDataType) {
    DATA_TYPE dataType = DATA_TYPE::FLOAT32;

    if (edenDataType == DATA_TYPE_INT8) {
        dataType = DATA_TYPE::QUANT8;
    } else if (edenDataType == DATA_TYPE_UINT8) {
        dataType = DATA_TYPE::UINT8;
    } else if (edenDataType == DATA_TYPE_INT16) {
        dataType = DATA_TYPE::INT16;
    } else if (edenDataType == DATA_TYPE_UINT16) {
        dataType = DATA_TYPE::UINT16;
    } else if (edenDataType == DATA_TYPE_INT32) {
        dataType = DATA_TYPE::INT32;
    } else if (edenDataType == DATA_TYPE_INT64) {
        dataType = DATA_TYPE::INT64;
    } else if (edenDataType == DATA_TYPE_FLOAT32) {
        dataType = DATA_TYPE::FLOAT32;
    } else if (edenDataType == DATA_TYPE_FLOAT16) {
        dataType = DATA_TYPE::FLOAT16;
    } else if (edenDataType == DATA_TYPE_BOOL8) {
        dataType = DATA_TYPE::BOOL8;
    } else {
        LOGE(EDEN_DRIVER, "Error, type=%d is not supported, set it default type (DATA_TYPE::FLOAT32)!\n",  static_cast<int32_t>(edenDataType));
    }

    LOGD(EDEN_DRIVER, "edenDataType=%d, dataType=%d\n",
         static_cast<int32_t>(edenDataType), static_cast<int32_t>(dataType));

    return dataType;
}

void getEdenDimensions(const hidl_vec<uint32_t>& androidDimensions,
                       int32_t* edenDimensions,
                       int32_t& numOfDims,
                       bool isNCHW,
                       bool noChange) {
    int32_t numOfAndroidDims = static_cast<int32_t>(androidDimensions.size());
    if (noChange) {
        numOfDims = 4;
        for (int i = 0; i < 4; i++) {
            edenDimensions[i] = i < numOfAndroidDims ? static_cast<int32_t>(androidDimensions[i]) : 1;
        }
    } else if (numOfAndroidDims == 0) {  // Convert dimensions based on data layout
        // Unknown rank supported by Android Q
        numOfDims = 0;
        edenDimensions[0] = 0;
        edenDimensions[1] = 0;
        edenDimensions[2] = 0;
        edenDimensions[3] = 0;
    } else if (numOfAndroidDims == 1) {  // (number)
        // Since # of dimention is 1, there is no meaning on data layout like NCHW, NHWC.
        numOfDims = 1;
        if (androidDimensions[0] != 0) {
            edenDimensions[0] = static_cast<int32_t>(androidDimensions[0]);
            edenDimensions[1] = 1;
            edenDimensions[2] = 1;
            edenDimensions[3] = 1;
        } else {
            edenDimensions[0] = 0;
            edenDimensions[1] = 0;
            edenDimensions[2] = 0;
            edenDimensions[3] = 0;
        }
    } else /* if (numOfAndroidDims > 1) */ {
        numOfDims = 4;
        if (numOfAndroidDims == 2) {  // (height, width)
            edenDimensions[N_NCHW] = 1;
            edenDimensions[C_NCHW] = 1;
            edenDimensions[H_NCHW] = static_cast<int32_t>(androidDimensions[0]);
            edenDimensions[W_NCHW] = static_cast<int32_t>(androidDimensions[1]);
        } else if (numOfAndroidDims == 3) {  // (number, height, width)
            if (isNCHW) {
                edenDimensions[N_NCHW] = static_cast<int32_t>(androidDimensions[0]);
                edenDimensions[C_NCHW] = static_cast<int32_t>(androidDimensions[1]);
                edenDimensions[H_NCHW] = static_cast<int32_t>(androidDimensions[2]);
                edenDimensions[W_NCHW] = 1;
            } else {
                edenDimensions[N_NCHW] = static_cast<int32_t>(androidDimensions[0]);
                edenDimensions[C_NCHW] = 1;
                edenDimensions[H_NCHW] = static_cast<int32_t>(androidDimensions[1]);
                edenDimensions[W_NCHW] = static_cast<int32_t>(androidDimensions[2]);
            }
        } else {  // (number, channel, width, height)
            numOfDims = 4;
            if (isNCHW) {
                edenDimensions[N_NCHW] = static_cast<int32_t>(androidDimensions[0]);
                edenDimensions[C_NCHW] = static_cast<int32_t>(androidDimensions[1]);
                edenDimensions[H_NCHW] = static_cast<int32_t>(androidDimensions[2]);
                edenDimensions[W_NCHW] = static_cast<int32_t>(androidDimensions[3]);
            } else {
                edenDimensions[N_NCHW] = static_cast<int32_t>(androidDimensions[N_NHWC]);  // 0
                edenDimensions[C_NCHW] = static_cast<int32_t>(androidDimensions[C_NHWC]);  // 3
                edenDimensions[H_NCHW] = static_cast<int32_t>(androidDimensions[H_NHWC]);  // 1
                edenDimensions[W_NCHW] = static_cast<int32_t>(androidDimensions[W_NHWC]);  // 2
            }
        }
    }

    // Show androidDimentions and edenDimensions
    LOGD(EDEN_DRIVER, "androidDimensions.size()=%d\n", numOfAndroidDims);
    LOGD(EDEN_DRIVER, "dimensions=(\n");
    for (auto& dimension : androidDimensions) {
        LOGD(EDEN_DRIVER, "%d \n", dimension);
    }
    LOGD(EDEN_DRIVER, ")\n");
    LOGD(EDEN_DRIVER, "edenDimensions.size()=%d\n", numOfDims);
    LOGD(EDEN_DRIVER, "dimensions=(\n");
    for (int32_t idx = 0; idx < numOfDims; idx++) {
        LOGD(EDEN_DRIVER, "%d \n", edenDimensions[idx]);
    }
    LOGD(EDEN_DRIVER, ")\n");
}

// a hack for OPs whose input/output dims > 4
// (not a permanent solution, but a pain-free patch to existing code)
void reduceDimensions(const V1_2::Operation& androidOperation, const V1_2::Model& model, const int32_t& androidOperandIdx,
                      const int32_t& edenOperandId, EdenModel* edenModel) {
    const int32_t opType = static_cast<int32_t>(androidOperation.type);
    const V1_2::Operand& androidOperand = model.operands[androidOperandIdx];
    EdenOperand* edenOperand = getEdenOperand(edenModel, edenOperandId);
    const size_t numOfAndroidDims = androidOperand.dimensions.size();
    switch (opType) {
        case ANEURALNETWORKS_ABS:  // 38
        case ANEURALNETWORKS_CAST:  // 45
        case ANEURALNETWORKS_EXP:  // 49
        case ANEURALNETWORKS_LOG:  // 60
        case ANEURALNETWORKS_NEG:  // 67
        case ANEURALNETWORKS_QUANTIZE:  // 72
        case ANEURALNETWORKS_LOGICAL_NOT:
        case ANEURALNETWORKS_SIN:
        case ANEURALNETWORKS_SQRT:
        case ANEURALNETWORKS_RSQRT:
        case ANEURALNETWORKS_SELECT:
            // reorder dims for elementwise OPs: dims exceeding `W_NCHW` is multiplied to `N_NCHW`
        {
            if (numOfAndroidDims < 5)
                return;
            LOGD(EDEN_DRIVER, "opType[%d]: Input/output operand's dimensions[%zu] exceed 4. Execute dim reduction.\n",
                 opType, numOfAndroidDims);
            for (size_t i = 4; i < numOfAndroidDims; ++i) {
                edenOperand->shapeInfo->dims[0] *= static_cast<int32_t>(androidOperand.dimensions[i]);
            }
            break;
        }
        case ANEURALNETWORKS_LOGICAL_AND:
        case ANEURALNETWORKS_LOGICAL_OR:
        {
            if (numOfAndroidDims < 5)
                return;
            LOGD(EDEN_DRIVER, "opType[%d]: Input/output operand's dimensions[%zu] exceed 4. Execute dim reduction.\n",
                opType, numOfAndroidDims);
            edenOperand->shapeInfo->dims[0] = static_cast<int32_t>(androidOperand.dimensions[1]);
            edenOperand->shapeInfo->dims[1] = static_cast<int32_t>(androidOperand.dimensions[2]);
            edenOperand->shapeInfo->dims[2] = static_cast<int32_t>(androidOperand.dimensions[3]);
            edenOperand->shapeInfo->dims[3] = static_cast<int32_t>(androidOperand.dimensions[4]);
            break;
        }
        case ANEURALNETWORKS_ARGMIN:
        case ANEURALNETWORKS_ARGMAX:
        case ANEURALNETWORKS_LOG_SOFTMAX:  // 64
        {
            if (numOfAndroidDims < 5)
                return;
            LOGD(EDEN_DRIVER, "opType[%d]: Input/output operand's dimensions[%zu] exceed 4. Execute dim reduction.\n",
                 opType, numOfAndroidDims);

            int32_t batch, channel, height, width;
            batch = static_cast<int32_t>(androidOperand.dimensions[numOfAndroidDims - 4]);
            height = static_cast<int32_t>(androidOperand.dimensions[numOfAndroidDims - 3]);
            width = static_cast<int32_t>(androidOperand.dimensions[numOfAndroidDims - 2]);
            channel = static_cast<int32_t>(androidOperand.dimensions[numOfAndroidDims - 1]);
            edenOperand->shapeInfo->dims[0] = batch;
            edenOperand->shapeInfo->dims[1] = channel;
            edenOperand->shapeInfo->dims[2] = height;
            edenOperand->shapeInfo->dims[3] = width;
            edenOperand->shapeInfo->numOfDims = 4;
            break;
        }
        case ANEURALNETWORKS_MINIMUM:
        {
            if (numOfAndroidDims < 5)
                return;
            LOGD(EDEN_DRIVER, "opType[%d]: Input/output operand's dimensions[%zu] exceed 4. Execute dim reduction.\n",
                 opType, numOfAndroidDims);

            int32_t batch, channel, height, width;
            batch = static_cast<int32_t>(androidOperand.dimensions[numOfAndroidDims - 4]);
            height = static_cast<int32_t>(androidOperand.dimensions[numOfAndroidDims - 3]);
            width = static_cast<int32_t>(androidOperand.dimensions[numOfAndroidDims - 2]);
            channel = static_cast<int32_t>(androidOperand.dimensions[numOfAndroidDims - 1]);

            edenOperand->shapeInfo->dims[0] = batch;
            edenOperand->shapeInfo->dims[1] = height;
            edenOperand->shapeInfo->dims[2] = width;
            edenOperand->shapeInfo->dims[3] = channel;
            edenOperand->shapeInfo->numOfDims = 4;

            break;
        }
        case ANEURALNETWORKS_GATHER:
        {
            LOGD(EDEN_DRIVER, "opType[%d]: Input/output operand's dimensions[%zu] exceed 4. Execute dim reduction.\n",
                 opType, numOfAndroidDims);

            int axis = getValue<int32_t>(model, androidOperation, 1);
            if (axis < 0) axis += numOfAndroidDims;
            std::vector<int> input_shapes;
            input_shapes.clear();

            for (size_t i = 0; i < numOfAndroidDims; ++i) input_shapes.push_back(androidOperand.dimensions[i]);

            int32_t inner_size = 1, outer_size = 1, axis_size = 1;
            int32_t batch, channel, height, width;
            if (0 <= axis && axis < (int)numOfAndroidDims) {
                for (int i = 0; i < axis; ++i) outer_size *= input_shapes[i];
                for (int i = axis + 1; i < (int)numOfAndroidDims; ++i) inner_size *= input_shapes[i];
                axis_size = input_shapes[axis];
                batch = outer_size;
                channel = axis_size;
                height = inner_size;
                width = 1;
            } else {
                int total_size = 1;
                for (size_t i = 0; i < numOfAndroidDims; ++i) total_size *= input_shapes[i];
                batch = total_size;
                channel = 1;
                height = 1;
                width = 1;
            }

            edenOperand->shapeInfo->dims[0] = batch;
            edenOperand->shapeInfo->dims[1] = channel;
            edenOperand->shapeInfo->dims[2] = height;
            edenOperand->shapeInfo->dims[3] = width;
            edenOperand->shapeInfo->numOfDims = 4;
            break;
        }
        default:
            return;
    }
    LOGD(EDEN_DRIVER, "edenDimensions after reduction=(\n");
    for (int32_t idx = 0; idx < 4; idx++) {
        LOGD(EDEN_DRIVER, "#[%d]: %d ", idx, edenOperand->shapeInfo->dims[idx]);
    }
    LOGD(EDEN_DRIVER, ")\n");
}

int32_t getActivationFn(int32_t type) {
    /*
    * Android NN activation Fn
    *  enum ActivationFn {
    *      kActivationNone = 0,
    *      kActivationRelu,
    *      kActivationRelu1,
    *      kActivationRelu6,
    *      kActivationTanh,
    *      kActivationSignBit,
    *      kActivationSigmoid,
    *};
    *
    * eden activation Fn
    *  typedef enum {
    *      EDEN_FUSED_ACT_NONE = 0,
    *      EDEN_FUSED_ACT_RELU = 1,
    *      EDEN_FUSED_ACT_RELU1 = 2,
    *      EDEN_FUSED_ACT_RELU6 = 3,
    *      EDEN_FUSED_ACT_TANH = 4,
    *      EDEN_FUSED_ACT_LOGISTIC = 5,
    *} FusedActivation;
    */

    int32_t activationFnNum;

    switch (type) {
    case kActivationNone:
        activationFnNum = EDEN_FUSED_ACT_NONE;
        break;
    case kActivationRelu:
        activationFnNum = EDEN_FUSED_ACT_RELU;
        break;
    case kActivationRelu1:
        activationFnNum = EDEN_FUSED_ACT_RELU1;
        break;
    case kActivationRelu6:
        activationFnNum = EDEN_FUSED_ACT_RELU6;
        break;
    case kActivationTanh:
        activationFnNum = EDEN_FUSED_ACT_TANH;
        break;
    case kActivationSignBit:
        /* not matched with Android NN and eden act fn definition */
        LOGD(EDEN_DRIVER, "kActivationSignBit and kActivationSigmoid is not supported at this moment: %d\n", type);
        activationFnNum = EDEN_FUSED_ACT_NONE;
        break;
    case kActivationSigmoid:
        activationFnNum = EDEN_FUSED_ACT_LOGISTIC;
        break;
    default:
        activationFnNum = EDEN_FUSED_ACT_NONE;
        break;
    }

    return activationFnNum;
}

int32_t getMatchedEdenOperationType(V1_2::OperationType androidOpType) {
    // todo There is no matched Android Operation for below EDEN_OP_XXX
    /*
    EDEN_OP_ELEMENTWISE_MAX = 39,
    EDEN_OP_SCALE = 41,
    EDEN_OP_CROP = 42,
    EDEN_OP_FLATTEN = 43,
    EDEN_OP_PERMUTE = 44,

    EDEN_OP_PRIORBOX = 46,

    EDEN_OP_DETECTION = 50,
    EDEN_OP_ROIPOOL = 51,

    EDEN_OP_LAYER_NORM_LSTM = 56,

    EDEN_OP_TFDETECTION = 57,
    */
    int32_t opType = static_cast<int32_t>(androidOpType);
    switch (opType) {
        case ANEURALNETWORKS_ADD:
            return EDEN_OP_ADD;
        case ANEURALNETWORKS_AVERAGE_POOL_2D:
            return EDEN_OP_AVERAGE_POOL_2D;
        case ANEURALNETWORKS_CONCATENATION:
            return EDEN_OP_CONCATENATION;
        case ANEURALNETWORKS_CONV_2D:
            return EDEN_OP_CONV_2D;
        case ANEURALNETWORKS_DEPTHWISE_CONV_2D:
            return EDEN_OP_DEPTHWISE_CONV_2D;

        case ANEURALNETWORKS_DEPTH_TO_SPACE:
            return EDEN_OP_DEPTH_TO_SPACE;
        case ANEURALNETWORKS_DEQUANTIZE:
            return EDEN_OP_DEQUANTIZE;
        case ANEURALNETWORKS_EMBEDDING_LOOKUP:
            return EDEN_OP_EMBEDDING_LOOKUP;
        case ANEURALNETWORKS_FLOOR:
            return EDEN_OP_FLOOR;
        case ANEURALNETWORKS_FULLY_CONNECTED:
            return EDEN_OP_FULLY_CONNECTED;

        case ANEURALNETWORKS_HASHTABLE_LOOKUP:
            return EDEN_OP_HASHTABLE_LOOKUP;
        case ANEURALNETWORKS_L2_NORMALIZATION:
            return EDEN_OP_L2_NORMALIZATION;
        case ANEURALNETWORKS_L2_POOL_2D:
            return EDEN_OP_L2_POOL_2D;
        case ANEURALNETWORKS_LOCAL_RESPONSE_NORMALIZATION:
            return EDEN_OP_LOCAL_RESPONSE_NORMALIZATION;
        case ANEURALNETWORKS_LOGISTIC:
            return EDEN_OP_LOGISTIC;

        case ANEURALNETWORKS_LSH_PROJECTION:
            return EDEN_OP_LSH_PROJECTION;
        case ANEURALNETWORKS_LSTM:
            return EDEN_OP_LSTM;
        case ANEURALNETWORKS_MAX_POOL_2D:
            return EDEN_OP_MAX_POOL_2D;
        case ANEURALNETWORKS_MUL:
            return EDEN_OP_MUL;
        case ANEURALNETWORKS_RELU:
            return EDEN_OP_RELU;

        case ANEURALNETWORKS_RELU1:
            return EDEN_OP_RELU1;
        case ANEURALNETWORKS_RELU6:
            return EDEN_OP_RELU6;
        case ANEURALNETWORKS_RESHAPE:
            return EDEN_OP_RESHAPE;
        case ANEURALNETWORKS_RESIZE_BILINEAR:
            return EDEN_OP_RESIZE_BILINEAR;
        case ANEURALNETWORKS_RNN:
            return EDEN_OP_RNN;

        case ANEURALNETWORKS_SOFTMAX:
            return EDEN_OP_SOFTMAX;
        case ANEURALNETWORKS_SPACE_TO_DEPTH:
            return EDEN_OP_SPACE_TO_DEPTH;
        case ANEURALNETWORKS_SVDF:
            return EDEN_OP_SVDF;
        case ANEURALNETWORKS_TANH:
            return EDEN_OP_TANH;
        case ANEURALNETWORKS_BATCH_TO_SPACE_ND:
            return EDEN_OP_BATCH_TO_SPACE_ND;

        case ANEURALNETWORKS_DIV:
            return EDEN_OP_DIV;
        case ANEURALNETWORKS_MEAN:
            return EDEN_OP_MEAN;
        case ANEURALNETWORKS_PAD:
            return EDEN_OP_PAD;
        case ANEURALNETWORKS_SPACE_TO_BATCH_ND:
            return EDEN_OP_SPACE_TO_BATCH_ND;
        case ANEURALNETWORKS_SQUEEZE:
            return EDEN_OP_SQUEEZE;

        case ANEURALNETWORKS_STRIDED_SLICE:
            return EDEN_OP_STRIDED_SLICE;
        case ANEURALNETWORKS_SUB:
            return EDEN_OP_SUB;
        case ANEURALNETWORKS_TRANSPOSE:
            return EDEN_OP_TRANSPOSE;

        case ANEURALNETWORKS_ABS:
            return EDEN_OP_ABS;
        case ANEURALNETWORKS_ARGMAX:
            return EDEN_OP_ARGMAX;
        case ANEURALNETWORKS_ARGMIN:
            return EDEN_OP_ARGMIN;
        case ANEURALNETWORKS_AXIS_ALIGNED_BBOX_TRANSFORM:
            return EDEN_OP_AXIS_ALIGNED_BBOX_TRANSFORM;
        case ANEURALNETWORKS_BIDIRECTIONAL_SEQUENCE_LSTM:
            return EDEN_OP_BIDIRECTIONAL_SEQUENCE_LSTM;
        case ANEURALNETWORKS_BIDIRECTIONAL_SEQUENCE_RNN:
            return EDEN_OP_BIDIRECTIONAl_RNN;
        case ANEURALNETWORKS_BOX_WITH_NMS_LIMIT:
            return EDEN_OP_BOX_WITH_NMS_LIMIT;

        case ANEURALNETWORKS_CAST:
            return EDEN_OP_CAST;
        case ANEURALNETWORKS_CHANNEL_SHUFFLE:
            return EDEN_OP_CHANNEL_SHUFFLE;
        case ANEURALNETWORKS_DETECTION_POSTPROCESSING:
            return EDEN_OP_DETECTION_POSTPROCESSING;
        case ANEURALNETWORKS_EQUAL:
            return EDEN_OP_EQUAL;
        case ANEURALNETWORKS_EXP:
            return EDEN_OP_EXP;

        case ANEURALNETWORKS_EXPAND_DIMS:
            return EDEN_OP_EXPAND_DIMS;
        case ANEURALNETWORKS_GATHER:
            return EDEN_OP_GATHER;
        case ANEURALNETWORKS_GENERATE_PROPOSALS:
            return EDEN_OP_GENERATE_PROPOSALS;
        case ANEURALNETWORKS_GREATER:
            return EDEN_OP_GREATER;
        case ANEURALNETWORKS_GREATER_EQUAL:
            return EDEN_OP_GREATER_EQUAL;

        case ANEURALNETWORKS_GROUPED_CONV_2D:
            return EDEN_OP_CONV_2D;
        case ANEURALNETWORKS_HEATMAP_MAX_KEYPOINT:
            return EDEN_OP_HEATMAP_MAX_KEYPOINT_OP;
        case ANEURALNETWORKS_INSTANCE_NORMALIZATION:
            return EDEN_OP_INSTANCE_NORMALIZATION;
        case ANEURALNETWORKS_LESS:
            return EDEN_OP_LESS;
        case ANEURALNETWORKS_LESS_EQUAL:
            return EDEN_OP_LESS_EQUAL;

        case ANEURALNETWORKS_LOG:
            return EDEN_OP_LOG;
        case ANEURALNETWORKS_LOGICAL_AND:
            return EDEN_OP_LOGICAL_AND;
        case ANEURALNETWORKS_LOGICAL_NOT:
            return EDEN_OP_LOGICAL_NOT;
        case ANEURALNETWORKS_LOGICAL_OR:
            return EDEN_OP_LOGICAL_OR;
        case ANEURALNETWORKS_LOG_SOFTMAX:
            return EDEN_OP_LOG_SOFTMAX;

        case ANEURALNETWORKS_MAXIMUM:
            return EDEN_OP_ELEMENTWISE_MAX;
        case ANEURALNETWORKS_MINIMUM:
            return EDEN_OP_MINIMUM;
        case ANEURALNETWORKS_NEG:
            return EDEN_OP_NEG;
        case ANEURALNETWORKS_NOT_EQUAL:
            return EDEN_OP_NOT_EQUAL;
        case ANEURALNETWORKS_PAD_V2:
            return EDEN_OP_PAD_V2;

        case ANEURALNETWORKS_POW:
            return EDEN_OP_POW;
        case ANEURALNETWORKS_PRELU:
            return EDEN_OP_PRELU;
        case ANEURALNETWORKS_QUANTIZE:
            return EDEN_OP_QUANTIZE;
        case ANEURALNETWORKS_QUANTIZED_16BIT_LSTM:
            return EDEN_OP_QUANTIZED_16BIT_LSTM;
        case ANEURALNETWORKS_RANDOM_MULTINOMIAL:
            return EDEN_OP_RANDOM_MULTINOMIAL;

        case ANEURALNETWORKS_REDUCE_ALL:
            return EDEN_OP_REDUCE_ALL;
        case ANEURALNETWORKS_REDUCE_ANY:
            return EDEN_OP_REDUCE_ANY;
        case ANEURALNETWORKS_REDUCE_MAX:
            return EDEN_OP_REDUCE_MAX;
        case ANEURALNETWORKS_REDUCE_MIN:
            return EDEN_OP_REDUCE_MIN;
        case ANEURALNETWORKS_REDUCE_PROD:
            return EDEN_OP_REDUCE_PROD;

        case ANEURALNETWORKS_REDUCE_SUM:
            return EDEN_OP_REDUCE_SUM;
        case ANEURALNETWORKS_ROI_ALIGN:
            return EDEN_OP_ROI_ALIGN;
        case ANEURALNETWORKS_ROI_POOLING:
            return EDEN_OP_TFLITEROIPOOL;
        case ANEURALNETWORKS_RSQRT:
            return EDEN_OP_RSQRT;
        case ANEURALNETWORKS_SELECT:
            return EDEN_OP_SELECT;

        case ANEURALNETWORKS_SIN:
            return EDEN_OP_SIN;
        case ANEURALNETWORKS_SLICE:
            return EDEN_OP_TF_SLICE;
        case ANEURALNETWORKS_SPLIT:
            return EDEN_OP_SPLIT;
        case ANEURALNETWORKS_SQRT:
            return EDEN_OP_SQRT;
        case ANEURALNETWORKS_TILE:
            return EDEN_OP_TILE;

        case ANEURALNETWORKS_TOPK_V2:
            return EDEN_OP_TOPK_V2;
        case ANEURALNETWORKS_TRANSPOSE_CONV_2D:
            return EDEN_OP_DECONV_2D;
        case ANEURALNETWORKS_UNIDIRECTIONAL_SEQUENCE_LSTM:
            return EDEN_OP_UNIDIRECTIONAL_SEQUENCE_LSTM;
        case ANEURALNETWORKS_UNIDIRECTIONAL_SEQUENCE_RNN:
            return EDEN_OP_UNIDIRECTIONAL_SEQUENCE_RNN;
        case ANEURALNETWORKS_RESIZE_NEAREST_NEIGHBOR:
            return EDEN_OP_RESIZE_NEAREST_NEIGHBOR;
    }

    // Not supported androidOperation
    return EDEN_OP_NUM_MAXIMUM;
}

EdenOperand* getEdenOperand(EdenModel* edenModel, int32_t edenOperandIdx) {
    auto mapToOperand = edenModel->GetMapToOperand();
    EdenOperand* edenOperand = mapToOperand[edenOperandIdx];
    return edenOperand;
}

EdenOperation* getEdenOperation(EdenModel* edenModel, int32_t edenOperationIdx) {
    auto mapToOperation = edenModel->GetMapToOperation();
    EdenOperation* edenOperation = mapToOperation[edenOperationIdx];
    return edenOperation;
}

template <typename T>
bool getValue2(const V1_2::Model& model, const V1_2::Operation& androidOperation, const int op_id, T& value) {
    V1_2::Operand operand = model.operands[androidOperation.inputs[op_id]];
    if (operand.lifetime == OperandLifeTime::CONSTANT_COPY) {
        T* data = (T*)(model.operandValues.data() + operand.location.offset);
        LOGD(EDEN_DRIVER, "getValue) op_id=%d, operandlifetime == CONSTANT_COPY, value=%f\n", op_id, (float)data[0]);
        value = data[0];
        return true;
    } else if (operand.lifetime == OperandLifeTime::CONSTANT_REFERENCE) {
        sp<IMemory> buffer_addr = mapMemory(model.pools[operand.location.poolIndex]);
        char* mappedPtr = reinterpret_cast<char*>(static_cast<void*>(buffer_addr->getPointer()));
        T* data = reinterpret_cast<T*>(mappedPtr + operand.location.offset);
        LOGD(EDEN_DRIVER, "getValue) op_id=%d, operandlifetime == CONSTANT_REFERENCE, value=%f\n", op_id, (float)data[0]);
        value = data[0];
        return true;
    } else {
        if (operand.lifetime == OperandLifeTime::MODEL_INPUT) {
            LOGE(EDEN_DRIVER, "operandliftime == MODEL_INPUT!\n");
        } else if (operand.lifetime == OperandLifeTime::MODEL_OUTPUT) {
            LOGE(EDEN_DRIVER, "operandliftime == MODEL_OUTPUT!\n");
        }
        LOGE(EDEN_DRIVER, "getValue) op_id=%d, operandliftime == %d, Using Internal buffer only\n", op_id, static_cast<int32_t>(operand.lifetime));
        return false;
    }
}

template <typename T>
T getValue(const V1_2::Model& model, const V1_2::Operation& androidOperation, const int op_id) {
    V1_2::Operand operand = model.operands[androidOperation.inputs[op_id]];
    if (operand.lifetime == OperandLifeTime::CONSTANT_COPY) {
        T* data = (T*)(model.operandValues.data() + operand.location.offset);
        LOGD(EDEN_DRIVER, "getValue) op_id=%d, operandlifetime == CONSTANT_COPY, value=%f\n", op_id, (float)data[0]);
        return data[0];
    } else if (operand.lifetime == OperandLifeTime::CONSTANT_REFERENCE) {
        sp<IMemory> buffer_addr = mapMemory(model.pools[operand.location.poolIndex]);
        char* mappedPtr = reinterpret_cast<char*>(static_cast<void*>(buffer_addr->getPointer()));
        T* data = reinterpret_cast<T*>(mappedPtr + operand.location.offset);
        LOGD(EDEN_DRIVER, "getValue) op_id=%d, operandlifetime == CONSTANT_REFERENCE, value=%f\n", op_id, (float)data[0]);
        return data[0];
    } else {
        if (operand.lifetime == OperandLifeTime::MODEL_INPUT) {
            LOGE(EDEN_DRIVER, "operandliftime == MODEL_INPUT!");
        } else if (operand.lifetime == OperandLifeTime::MODEL_OUTPUT) {
            LOGE(EDEN_DRIVER, "operandliftime == MODEL_OUTPUT!");
        }
        LOGE(EDEN_DRIVER, "getValue) op_id=%d, operandliftime == %d, Using Internal buffer only", op_id, static_cast<int32_t>(operand.lifetime));
        return -1;
    }
}

template <typename T>
T getPtr(const V1_2::Model& model, const V1_2::Operation& androidOperation,
         const int op_id, const ModelInfo* modelInfo, sp<IMemory>& buffer_addr) {
    V1_2::Operand operand = model.operands[androidOperation.inputs[op_id]];
    T data = nullptr;
    if (operand.lifetime == OperandLifeTime::CONSTANT_COPY) {
        data = (T)(model.operandValues.data() + operand.location.offset);
        LOGD(EDEN_DRIVER, "getPtr) op_id=%d, operandlifetime == CONSTANT_COPY, addr=%p\n", op_id, data);
    } else if (operand.lifetime == OperandLifeTime::CONSTANT_REFERENCE) {
        if (model.pools[operand.location.poolIndex].name() == "ashmem") {
            buffer_addr = mapMemory(model.pools[operand.location.poolIndex]);
            char* mappedPtr = reinterpret_cast<char*>(static_cast<void*>(buffer_addr->getPointer()));
            data = reinterpret_cast<T>(mappedPtr + operand.location.offset);
            LOGD(EDEN_DRIVER, "getPtr) op_id=%d, operandlifetime == CONSTANT_REFERENCE, addr=%p\n", op_id, data);
        } else {
            if (modelInfo == nullptr) {
                LOGE(EDEN_DRIVER, "modelInfo is nullptr!");
                return nullptr;
            }
            VirtualAddressInfo vInfo = modelInfo->vecVirtualAddressOnPools[operand.location.poolIndex];
            data = reinterpret_cast<T>(vInfo.addr + operand.location.offset);
            LOGD(EDEN_DRIVER, "getPtr) op_id=%d, operandlifetime == CONSTANT_REFERENCE, addr=%p\n", op_id, data);
        }
    } else {
        if (operand.lifetime == OperandLifeTime::MODEL_INPUT) {
            LOGE(EDEN_DRIVER, "operandliftime == MODEL_INPUT!");
        } else if (operand.lifetime == OperandLifeTime::MODEL_OUTPUT) {
            LOGE(EDEN_DRIVER, "operandliftime == MODEL_OUTPUT!");
        }
        LOGE(EDEN_DRIVER, "getValue) op_id=%d, operandliftime == %d, Using Internal buffer only", op_id, static_cast<int32_t>(operand.lifetime));
        return nullptr;
    }
    return data;
}

void showModelInfo(ModelInfo& modelInfo) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    for (auto& virtualAddressOnPools : modelInfo.vecVirtualAddressOnPools) {
        LOGD(EDEN_DRIVER, "type:%d\n", virtualAddressOnPools.type);
        LOGD(EDEN_DRIVER, "addr:%p\n", virtualAddressOnPools.addr);
        LOGD(EDEN_DRIVER, "size:%d\n", virtualAddressOnPools.size);
        //DumpToStdio(virtualAddressOnPools.addr, virtualAddressOnPools.size);
    }
    if (modelInfo.operandValues == nullptr) {
        LOGD(EDEN_DRIVER, "operandValues is NULL\n");
    } else {
        LOGD(EDEN_DRIVER, "operandValues:%p\n", modelInfo.operandValues.get());
        LOGD(EDEN_DRIVER, "operandValues.get():%p\n", reinterpret_cast<void*>(modelInfo.operandValues.get()));
    }
    LOGD(EDEN_DRIVER, "modelInfo.vecOpGroup.size():%zu\n", static_cast<size_t>(modelInfo.vecOpGroup.size()));
    for (auto& opGroup : modelInfo.vecOpGroup) {
        LOGD(EDEN_DRIVER, "operationList...\n");
        for (auto& opIdx : opGroup.operationList) {
            LOGD(EDEN_DRIVER, "%d \n", opIdx);
        }
        LOGD(EDEN_DRIVER, "inputOperandIndexes...\n");
        for (auto& opIdx : opGroup.inputOperandIndexes) {
            LOGD(EDEN_DRIVER, "%d \n", opIdx);
        }
        LOGD(EDEN_DRIVER, "outputOperandIndexes...\n");
        for (auto& opIdx : opGroup.outputOperandIndexes) {
            LOGD(EDEN_DRIVER, "%d \n", opIdx);
        }
        LOGD(EDEN_DRIVER, "targetDevice:%d\n", static_cast<int32_t>(opGroup.targetDevice));
    }
    LOGD(EDEN_DRIVER, "modelInputIndexes...\n");
    for (auto& idx : modelInfo.modelInputIndexes) {
        LOGD(EDEN_DRIVER, "%d \n", idx);
    }
    LOGD(EDEN_DRIVER, "modelOutputIndexes...\n");
    for (auto& idx : modelInfo.modelOutputIndexes) {
        LOGD(EDEN_DRIVER, "%d \n", idx);
    }
    LOGD(EDEN_DRIVER, "mapOperandIdFromAToE...\n");
    for (auto it = modelInfo.mapOperandIdFromAToE.begin(); it != modelInfo.mapOperandIdFromAToE.end(); ++it) {
        LOGD(EDEN_DRIVER, "(%d, %d)\n", it->first, it->second);
    }
    LOGD(EDEN_DRIVER, "mapOperationIdFromAToE...\n");
    for (auto it = modelInfo.mapOperationIdFromAToE.begin(); it != modelInfo.mapOperationIdFromAToE.end(); ++it) {
        LOGD(EDEN_DRIVER, "(%d, %d)\n", it->first, it->second);
    }
    LOGD(EDEN_DRIVER, "inputConsumers.size():%zu\n", static_cast<size_t>(modelInfo.inputConsumers.size()));
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

int32_t prepareModelInfo(const V1_2::Model& model, ModelInfo& modelInfo) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);
    int32_t retCode = RET_OK;

    // Get virtual address on pools
    retCode = getVirtualAddressOnPools(model, modelInfo.vecVirtualAddressOnPools, modelInfo.vecIMemoryOnAshmems);
    if (retCode != RET_OK) {
        LOGE(EDEN_DRIVER, "getVirtualAddressOnPools() is failed.\n");
        return retCode;
    }

    // Load operandValues on model
    int32_t size = model.operandValues.size();
    if (size == 0) {
        LOGD(EDEN_DRIVER, "model.operandValues.size() is zero!\n");
        modelInfo.operandValues = nullptr;
    } else {
        LOGD(EDEN_DRIVER, "model.operandValues.size() is %zu\n", model.operandValues.size());
        auto spOperandValues = std::make_unique<uint8_t[]>(size);
        LOGD(EDEN_DRIVER, "spOperandValues.get(): %p\n", reinterpret_cast<void*>(spOperandValues.get()));
        modelInfo.operandValues = std::move(spOperandValues);
        std::memcpy(reinterpret_cast<void*>(modelInfo.operandValues.get()), model.operandValues.data(), size);
        LOGD(EDEN_DRIVER, "modelInfo.operandValues.get(): %p\n", reinterpret_cast<void*>(modelInfo.operandValues.get()));
        LOGD(EDEN_DRIVER, "model.operandValues.data(): %p\n", (void*)(model.operandValues.data()));
    }

    // Set up model input/output
    for (int32_t inputIndex : model.inputIndexes) {
        modelInfo.modelInputIndexes.push_back(inputIndex);
    }
    for (int32_t outputIndex : model.outputIndexes) {
        modelInfo.modelOutputIndexes.push_back(outputIndex);
    }

    // Init inputConsumers. This information is updated during converting.
    modelInfo.inputConsumers.resize(model.inputIndexes.size());

    modelInfo.vecConverted.resize(model.operands.size(), false);
    modelInfo.vecIsNCHW.resize(model.operands.size(), false);

    showModelInfo(modelInfo);

    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
    return RET_OK;
}

int32_t getVirtualAddressOnPools(const V1_2::Model& model, std::vector<VirtualAddressInfo>& vecVirtualAddressOnPools,
                                 std::vector<sp<IMemory>>& vecIMemoryOnAshmems) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    for (size_t idx = 0; idx < model.pools.size(); idx++) {
        if (model.pools[idx].name() == "ashmem") {
            sp<IMemory> memoryPtr = mapMemory(model.pools[idx]);
            int32_t size = model.pools[idx].size();
            char* mappedPtr = reinterpret_cast<char*>(static_cast<void*>(memoryPtr->getPointer()));
            VirtualAddressInfo vInfo;
            vInfo.type = 0;  // 0 for ashmem
            vInfo.addr = mappedPtr;
            vInfo.size = size;  // @todo need to fill real size
            LOGD(EDEN_DRIVER, "Adding ashmem, vInfo.addr=%p, size=%d\n", reinterpret_cast<void*>(vInfo.addr), vInfo.size);
            vecVirtualAddressOnPools.push_back(vInfo);
            vecIMemoryOnAshmems.push_back(memoryPtr);
        } else if (model.pools[idx].name() == "mmap_fd") {
            int32_t size = model.pools[idx].size();
            int32_t fd = model.pools[idx].handle()->data[0];
            int32_t prot = model.pools[idx].handle()->data[1];

            size_t offset = (uint32_t)(model.pools[idx].handle()->data[2]) +
                ((uint64_t)(uint32_t)(model.pools[idx].handle()->data[3]) << 32);

            char* mappedAddr = reinterpret_cast<char*>(mmap(nullptr, size, prot, MAP_SHARED, fd, offset));

            // Copy original data into heap since it might be modified on converting NCHW<->NHWC
            char* virtAddr = new char[size];
            std::memcpy(virtAddr, mappedAddr, size);
            munmap(mappedAddr, size);

            VirtualAddressInfo vInfo;
            vInfo.type = 1;  // 1 for mmap_fd
            vInfo.addr = virtAddr;
            vInfo.size = size;
            LOGD(EDEN_DRIVER, "Adding mmap_fd, vInfo.addr=%p, size=%d\n", vInfo.addr, vInfo.size);
            vecVirtualAddressOnPools.push_back(vInfo);
        } else if (model.pools[idx].name() == "hardware_buffer_blob") {
            auto handle = model.pools[idx].handle();
            auto format = AHARDWAREBUFFER_FORMAT_BLOB;
            auto usage = AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN | AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN;
            const size_t width = model.pools[idx].size();
            const uint32_t height = 1;  // height is always 1 for BLOB mode AHardwareBuffer.
            const uint32_t layers = 1;  // layers is always 1 for BLOB mode AHardwareBuffer.
            const size_t stride = model.pools[idx].size();
            sp<GraphicBuffer> graphicBuffer = new GraphicBuffer(handle, GraphicBuffer::HandleWrapMethod::CLONE_HANDLE,
                                                                width, height, format, layers, usage, stride);
            void* gBuffer = nullptr;
            int32_t outBytesPerPixel, outBytesPerStride;
            graphicBuffer->lock(usage, &gBuffer, &outBytesPerPixel, &outBytesPerStride);

            char* virtAddr = new char[width];
            std::memcpy(virtAddr, gBuffer, width);
            VirtualAddressInfo vInfo;
            vInfo.type = 0;  // 0 for ashmem
            vInfo.addr = virtAddr;
            vInfo.size = model.pools[idx].size();
            LOGD(EDEN_DRIVER, "Adding hardware_buffer_blob, vInfo.addr=%p, size=%d\n", reinterpret_cast<void*>(vInfo.addr), vInfo.size);
            vecVirtualAddressOnPools.push_back(vInfo);
        } else {
            LOGE(EDEN_DRIVER, "Oops, unsupported hidl_memory type! (type=%s)", model.pools[idx].name().c_str());
            return SUPPORTED_HIDL_MEMORY_TYPE;
        }
    }
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
    return RET_OK;
}

int32_t getVirtualAddressOnPool(const hidl_memory& memory, bool needToWrite, VirtualAddressInfo& vInfo,
                                sp<IMemory>& spIMemoryOnAshmem) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    if (memory.name() == "ashmem") {
        sp<IMemory> memoryPtr = mapMemory(memory);
        int32_t size = memory.size();
        char* mappedPtr = reinterpret_cast<char*>(static_cast<void*>(memoryPtr->getPointer()));
        vInfo.type = 0;  // 0 for ashmem
        vInfo.addr = mappedPtr;
        vInfo.size = size;  // @todo need to fill real size
        LOGD(EDEN_DRIVER, "ashmem, vInfo.addr=%p, size=%d\n", reinterpret_cast<void*>(vInfo.addr), vInfo.size);
        spIMemoryOnAshmem = memoryPtr;
    } else if (memory.name() == "mmap_fd") {
        int32_t size = memory.size();
        int32_t fd = memory.handle()->data[0];
        int32_t prot = memory.handle()->data[1];

        size_t offset = (uint32_t)(memory.handle()->data[2]) +
            ((uint64_t)(uint32_t)(memory.handle()->data[3]) << 32);

        char* mappedAddr = reinterpret_cast<char*>(mmap(nullptr, size, prot, MAP_SHARED, fd, offset));

        if (needToWrite == false) {
            char* virtAddr = mappedAddr;

            vInfo.type = 2;  // 2 for mmap_fd, need to call munmap(..) later
            vInfo.addr = virtAddr;
            vInfo.size = size;
        } else {
            if (prot & PROT_WRITE) {
                char* virtAddr = mappedAddr;

                vInfo.type = 2;  // 2 for mmap_fd, need to call munmap(..) later
                vInfo.addr = virtAddr;
                vInfo.size = size;
            } else {
                char* virtAddr = new char[size];
                std::memcpy(virtAddr, mappedAddr, size);
                munmap(mappedAddr, size);

                vInfo.type = 1;  // 1 for mmap_fd, need to call delete[] later
                vInfo.addr = virtAddr;
                vInfo.size = size;
            }
        }

        LOGD(EDEN_DRIVER, "mmap_fd, vInfo.addr=%p, size=%d\n", vInfo.addr, vInfo.size);
    } else {
        LOGE(EDEN_DRIVER, "Oops, unsupported hidl_memory type! (type=%s)", memory.name().c_str());
        return SUPPORTED_HIDL_MEMORY_TYPE;
    }

    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
    return RET_OK;
}

/**
 * @brief Create empty EdenModel
 * @details This function creates an empty EdenModel.
 * @param[in] model Android NN Model
 * @param[out] edenModel empty EdenModel
 * @returns return code
 */
int32_t createEmptyEdenModel(EdenModel*& edenModel, EdenOperand*& edenOperands, int32_t numOfEdenOperands,
                                             EdenOperation*& edenOperations, int32_t numOfEdenOperations) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);
    LOGD(EDEN_DRIVER, "numOfEdenOperands:%d, numOfEdenOperations:%d\n", numOfEdenOperands, numOfEdenOperations);

    edenModel = new EdenModel();
    edenOperands = new EdenOperand[numOfEdenOperands];
    edenOperations = new EdenOperation[numOfEdenOperations];

    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
    return RET_OK;
}

void showEdenOperand(EdenOperand* edenOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    if (edenOperand == nullptr) {
        LOGE(EDEN_DRIVER, "Oops, edenOperand is nullptr!");
        return;
    }

    if (edenOperand->name.name != nullptr) {
        EdenString* operandName = &(edenOperand->name);
        LOGD(EDEN_DRIVER, "  name->name=[%s]\n", operandName->name);
        LOGD(EDEN_DRIVER, "  name->length=[%d]\n", operandName->length);
    } else {
        LOGD(EDEN_DRIVER, "  name is NULL\n");
    }

    if ((edenOperand->buffer != nullptr) && (edenOperand->buffer->addr != nullptr)) {
        EdenBuffer* operandBuffer = edenOperand->buffer;
        LOGD(EDEN_DRIVER, "  buffer->addr=[%p]\n", operandBuffer->addr);
        LOGD(EDEN_DRIVER, "  buffer->size=[%d]\n", operandBuffer->size);
    } else {
        LOGD(EDEN_DRIVER, "  buffer is NULL\n");
    }

    if (edenOperand->shapeInfo != nullptr) {
        EdenShapeInfo* operandShapeInfo = edenOperand->shapeInfo;
        LOGD(EDEN_DRIVER, "  shapeInfo->dataType=[%d] (0:INT8,1:INT16,2:INT32,3:INT64,4:FLOAT16,5:FLOAT32)\n", operandShapeInfo->dataType);
        LOGD(EDEN_DRIVER, "  shapeInfo->bufferLayout=[%d]\n", operandShapeInfo->bufferLayout);
        LOGD(EDEN_DRIVER, "  shapeInfo->numOfDims=[%d]\n", operandShapeInfo->numOfDims);
        LOGD(EDEN_DRIVER, "  shapeInfo->dims[3]=[%d]\n", operandShapeInfo->dims[3]);
        LOGD(EDEN_DRIVER, "  shapeInfo->dims[2]=[%d]\n", operandShapeInfo->dims[2]);
        LOGD(EDEN_DRIVER, "  shapeInfo->dims[1]=[%d]\n", operandShapeInfo->dims[1]);
        LOGD(EDEN_DRIVER, "  shapeInfo->dims[0]=[%d]\n", operandShapeInfo->dims[0]);
    } else {
        LOGD(EDEN_DRIVER, "  shapeInfo is NULL\n");
    }
    if (edenOperand->quantInfo) {
        EdenQuantInfo* operandQuantInfo = edenOperand->quantInfo;
        LOGD(EDEN_DRIVER, "  quantInfo->quantWay=[%d]\n", operandQuantInfo->quantWay);
        LOGD(EDEN_DRIVER, "  quantInfo->data=[%p]\n", operandQuantInfo->data);
        if (operandQuantInfo->quantWay == SCALE_ZEROPOINT) {
            EdenScaleQuantInfo* scaleQuantInfo = reinterpret_cast<EdenScaleQuantInfo*>(operandQuantInfo->data);
            LOGD(EDEN_DRIVER, "    scale=%f\n", scaleQuantInfo->scale);
            LOGD(EDEN_DRIVER, "    zeroPoint=%d\n", scaleQuantInfo->zeroPoint);
            LOGD(EDEN_DRIVER, "    realValueMax=%f\n", scaleQuantInfo->realValueMax);
            LOGD(EDEN_DRIVER, "    realValueMin=%f\n", scaleQuantInfo->realValueMin);
            LOGD(EDEN_DRIVER, "    bitLength=%d\n", scaleQuantInfo->bitLength);
        }
    } else {
        LOGD(EDEN_DRIVER, "  quantInfo is NULL\n");
    }
    if (edenOperand->extraParams) {
        EdenExtraParams* extraParams = edenOperand->extraParams;
        LOGD(EDEN_NN, "  extraParams->type=[%d]\n", extraParams->type);
        if (extraParams->type == SYMM_PER_CHANNEL_QUANT) {
            EdenExtraParamsSymmPerChannelQuant* perChannelQuant = reinterpret_cast<EdenExtraParamsSymmPerChannelQuant*>(extraParams->data);
            for (size_t idx = 0; idx < perChannelQuant->scales.size(); idx++) {
                LOGD(EDEN_NN, "    scale[%zu]=[%f]\n", idx, perChannelQuant->scales[idx]);
            }
            LOGD(EDEN_NN, "    channelDim=[%d]\n", perChannelQuant->channelDim);
        }
    } else {
        LOGD(EDEN_NN, "  extraParams is NULL\n");
    }

    LOGD(EDEN_DRIVER, "  reserved[2]=[%d]\n", edenOperand->reserved[2]);
    LOGD(EDEN_DRIVER, "  reserved[1]=[%d]\n", edenOperand->reserved[1]);
    LOGD(EDEN_DRIVER, "  reserved[0]=[%d]\n", edenOperand->reserved[0]);

    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

/**
 * @brief Create EdenOperand using a given Android NN Operand
 * @details This function creates an EdenOperand using a given Android NN Operand.
 *          Android NN Operand should be changed properly without breaking any original information.
 * @param[in] androidOperand Android Operand to be converted
 * @param[out] edenOperand Created EdenOperand converted from Android Operand
 * @returns return code
 */
int32_t createEdenOperand(ModelInfo& modelInfo, V1_2::Operand androidOperand, EdenOperand*& edenOperand, int32_t edenOpType, bool isNCHW) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);
    // Create name
    EdenString& newName = edenOperand->name;
    if (androidOperand.lifetime == OperandLifeTime::MODEL_INPUT) {
        LOGD(EDEN_DRIVER, "createEdenOperand() : OperandLifeTime::MODEL_INPUT\n");
        getMatchedEdenOperandName(IFM, newName.name, newName.length);
    } else if (androidOperand.lifetime == OperandLifeTime::MODEL_OUTPUT) {
        LOGD(EDEN_DRIVER, "createEdenOperand() : OperandLifeTime::MODEL_OUTPUT\n");
        getMatchedEdenOperandName(TENSOR, newName.name, newName.length);
    } else {
        LOGD(EDEN_DRIVER, "createEdenOperand() : OperandLifeTime::others\n");
        // Postponed until which operation uses this operand
        newName.name = nullptr;
        newName.length = 0;
    }

    // Create buffer
    EdenBuffer* newBuffer = new EdenBuffer();

    // OperandLifeTime = {TEMPORARY_VARIABLE, MODEL_INPUT, MODEL_OUTPUT, CONSTANT_COPY, CONSTANT_REFERENCE, NO_VALUE}
    if (androidOperand.lifetime == OperandLifeTime::CONSTANT_REFERENCE) {
        LOGD(EDEN_DRIVER, "createEdenOperand() : CONSTANT_REFERENCE\n");
        size_t poolIndex = androidOperand.location.poolIndex;
        char* virtAddr = modelInfo.vecVirtualAddressOnPools[poolIndex].addr;
        newBuffer->addr = virtAddr + androidOperand.location.offset;
        newBuffer->size = androidOperand.location.length;
        LOGD(EDEN_DRIVER, "OperandLifeTime::CONSTANT_REFERENCE, addr:%p, size:%d\n", newBuffer->addr, newBuffer->size);
    } else if (androidOperand.lifetime == OperandLifeTime::CONSTANT_COPY) {
        LOGD(EDEN_DRIVER, "createEdenOperand() : CONSTANT_COPY\n");
        newBuffer->addr = (modelInfo.operandValues.get() + androidOperand.location.offset);
        newBuffer->size = androidOperand.location.length;  // MODEL_INPUT, MODEL_OUTPUT = 0, unit -> Byte
        LOGD(EDEN_DRIVER, "OperandLifeTime::CONSTANT_COPY, addr:%p, size:%d\n", newBuffer->addr, newBuffer->size);
    } else {
        LOGD(EDEN_DRIVER, "createEdenOperand() : MODEL_INPUT | MODEL_OUTPUT | TEMPORARY_VARIABLE | NO_VALUE\n");
        // MODEL_INPUT, MODEL_OUTPUT, TEMPORARY_VARIABLE, NO_VALUE
        // Nothing to set for buffer
        newBuffer->addr = nullptr;
        newBuffer->size = 0;
    }

    edenOperand->buffer = newBuffer;

    // Create shapeInfo
    EdenShapeInfo* newShapeInfo = new EdenShapeInfo();

    newShapeInfo->dataType = getEdenDataType(androidOperand.type);  // get data type
    newShapeInfo->bufferLayout = BUFFER_LAYOUT_NCHW;

    switch (edenOpType) {
        case EDEN_OP_UNIDIRECTIONAL_SEQUENCE_RNN:
        case EDEN_OP_UNIDIRECTIONAL_SEQUENCE_LSTM:
        case EDEN_OP_REDUCE_SUM:  // 97
        case EDEN_OP_REDUCE_MIN:  // 98
        case EDEN_OP_REDUCE_MAX:  // 99
        case EDEN_OP_REDUCE_PROD:  // 100
        case EDEN_OP_REDUCE_ALL:  // 101
        case EDEN_OP_REDUCE_ANY:  // 102
        case EDEN_OP_TILE:  // 103
        case EDEN_OP_TF_SLICE:  // 104
        case EDEN_OP_HASHTABLE_LOOKUP: //10
            //some op doesn't need change dim order
            getEdenDimensions(androidOperand.dimensions, newShapeInfo->dims, newShapeInfo->numOfDims, isNCHW, true);
            break;
        default:
            getEdenDimensions(androidOperand.dimensions, newShapeInfo->dims, newShapeInfo->numOfDims, isNCHW, false);
            break;
    }

    edenOperand->shapeInfo = newShapeInfo;

    // Create quantInfo
    edenOperand->quantInfo = nullptr;
    if ((androidOperand.type == V1_2::OperandType::TENSOR_INT32)         ||
        (androidOperand.type == V1_2::OperandType::TENSOR_QUANT8_ASYMM)  ||
        (androidOperand.type == V1_2::OperandType::TENSOR_QUANT8_SYMM)   ||
        (androidOperand.type == V1_2::OperandType::TENSOR_QUANT16_ASYMM) ||
        (androidOperand.type == V1_2::OperandType::TENSOR_QUANT16_SYMM)  ||
        (androidOperand.type == V1_2::OperandType::TENSOR_QUANT8_SYMM_PER_CHANNEL)) {
        EdenQuantInfo* newQuantInfo = new EdenQuantInfo();
        newQuantInfo->quantWay = SCALE_ZEROPOINT;

        EdenScaleQuantInfo* newScaleQuantInfo = new EdenScaleQuantInfo();
        newScaleQuantInfo->scale = androidOperand.scale;
        newScaleQuantInfo->zeroPoint = androidOperand.zeroPoint;
        if (androidOperand.type == OperandType::TENSOR_INT32) {
            newScaleQuantInfo->realValueMax = std::numeric_limits<int>::max();
            newScaleQuantInfo->realValueMin = std::numeric_limits<int>::min();
            newScaleQuantInfo->bitLength = 32;
        } else if (androidOperand.type == V1_2::OperandType::TENSOR_QUANT8_ASYMM) {
            newScaleQuantInfo->realValueMax = std::numeric_limits<unsigned char>::max();
            newScaleQuantInfo->realValueMin = std::numeric_limits<unsigned char>::min();
            newScaleQuantInfo->bitLength = 8;
        } else if (androidOperand.type == OperandType::TENSOR_QUANT8_SYMM) {
            newScaleQuantInfo->realValueMax = std::numeric_limits<unsigned char>::max();
            newScaleQuantInfo->realValueMin = std::numeric_limits<unsigned char>::min();
            newScaleQuantInfo->bitLength = 8;
        } else if (androidOperand.type == OperandType::TENSOR_QUANT16_ASYMM) {
            newScaleQuantInfo->realValueMax = std::numeric_limits<unsigned short>::max();
            newScaleQuantInfo->realValueMin = std::numeric_limits<unsigned short>::min();
            newScaleQuantInfo->bitLength = 16;
        } else if (androidOperand.type == OperandType::TENSOR_QUANT16_SYMM) {
            newScaleQuantInfo->realValueMax = std::numeric_limits<unsigned short>::max();
            newScaleQuantInfo->realValueMin = std::numeric_limits<unsigned short>::min();
            newScaleQuantInfo->bitLength = 16;
        } else if (androidOperand.type == OperandType::TENSOR_QUANT8_SYMM_PER_CHANNEL) {
            newScaleQuantInfo->realValueMax = std::numeric_limits<unsigned char>::max();
            newScaleQuantInfo->realValueMin = std::numeric_limits<unsigned char>::min();
            newScaleQuantInfo->bitLength = 8;
        }

        newQuantInfo->data = newScaleQuantInfo;
        edenOperand->quantInfo = newQuantInfo;
    }

    // Set opType
    edenOperand->opType = edenOpType;
    edenOperand->isNCHW = isNCHW;

    // Create EdenExtraParams
    edenOperand->extraParams = nullptr;
    if (androidOperand.type == V1_2::OperandType::TENSOR_QUANT8_SYMM_PER_CHANNEL) {
        EdenExtraParams* extraParams = new EdenExtraParams();
        extraParams->type = SYMM_PER_CHANNEL_QUANT;

        EdenExtraParamsSymmPerChannelQuant* newExtraParamsSymmPerChannelQuant = new EdenExtraParamsSymmPerChannelQuant();
        for (size_t idx = 0; idx < androidOperand.extraParams.channelQuant().scales.size(); idx++) {
            newExtraParamsSymmPerChannelQuant->scales.push_back(androidOperand.extraParams.channelQuant().scales[idx]);
        }
        newExtraParamsSymmPerChannelQuant->channelDim = androidOperand.extraParams.channelQuant().channelDim;
        extraParams->data = newExtraParamsSymmPerChannelQuant;
        edenOperand->extraParams = extraParams;
    }

    edenOperand->reserved[0] = 0;
    edenOperand->reserved[1] = 0;
    edenOperand->reserved[2] = 0;

    //showEdenOperand(edenOperand);

    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
    return RET_OK;
}

void showEdenOperation(EdenOperation* edenOperation) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    if (edenOperation == nullptr) {
        LOGE(EDEN_DRIVER, "Oops, edenOperation is nullptr!\n");
        return;
    }

    LOGD(EDEN_DRIVER, "  opType=[%d]\n", edenOperation->opType);

    if (edenOperation->opName.name) {
        EdenString* operandName = &(edenOperation->opName);
        LOGD(EDEN_DRIVER, "  name->name=[%s]\n", operandName->name);
        LOGD(EDEN_DRIVER, "  name->length=[%d]\n", operandName->length);
    } else {
        LOGD(EDEN_DRIVER, "  name is NULL\n");
    }

    LOGD(EDEN_DRIVER, "  numOfInputs=[%d]\n", edenOperation->numOfInputs);
    for (int32_t idx = 0; idx < edenOperation->numOfInputs; idx++) {
        LOGD(EDEN_DRIVER, "  idx[%d]=[%d]\n", idx, edenOperation->inputOperandIndexes[idx]);
    }

    LOGD(EDEN_DRIVER, "  numOfOutputs=[%d]\n", edenOperation->numOfOutputs);
    for (int32_t idx = 0; idx < edenOperation->numOfOutputs; idx++) {
        LOGD(EDEN_DRIVER, "  idx[%d]=[%d]\n", idx, edenOperation->outputOperandIndexes[idx]);
    }

    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

/**
 * @brief Create EdenOperation using a given Android NN Operation
 * @details This function creates an EdenOperation using a given Android NN Operation.
 *          Android NN Operation should be changed properly without breaking any original information.
 * @param[in] androidOperation Android Operation to be converted
 * @param[out] edenOperation Created EdenOperation converted from Android Operation
 * @returns return code
 */
int32_t createEdenOperation(const V1_2::Model& model, ModelInfo& modelInfo, const V1_2::Operation& androidOperation, EdenOperation*& edenOperation,
                            EdenModel* edenModel) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    int32_t opType = getMatchedEdenOperationType(androidOperation.type);
    if (opType == EDEN_OP_NUM_MAXIMUM) {
        LOGE(EDEN_DRIVER, "Error, [%d] is not supported!\n", static_cast<int32_t>(androidOperation.type));
        return UNSUPPORTED_OPERATION;
    }

    // Create EdenOperand for options if it is required
    edenOperation->isNCHW = false;

    int32_t configOperandId = -1;
    EdenOperand* configOperand = nullptr;
    createEdenOperandForConfig(model, androidOperation, &modelInfo, &configOperand);
    if (configOperand != nullptr) {
        edenModel->AddOperandForOptions(configOperand, &configOperandId);
        edenOperation->isNCHW = configOperand->isNCHW;
        LOGD(EDEN_DRIVER, "Config is created, configOperandId=%d\n", configOperandId);
    } else {
        //some op doesn't have configOperand set isNCHW
        int32_t opType = static_cast<int32_t>(androidOperation.type);
        switch (opType) {
            case ANEURALNETWORKS_REDUCE_SUM:  // 97
            case ANEURALNETWORKS_REDUCE_MIN:  // 98
            case ANEURALNETWORKS_REDUCE_MAX:  // 99
            case ANEURALNETWORKS_REDUCE_PROD:  // 100
            case ANEURALNETWORKS_REDUCE_ALL:  // 101
            case ANEURALNETWORKS_REDUCE_ANY:  // 102
            case ANEURALNETWORKS_TILE:  // 103
            case ANEURALNETWORKS_SLICE:  // 104
            case ANEURALNETWORKS_HASHTABLE_LOOKUP: // 10
            case ANEURALNETWORKS_MEAN:
                edenOperation->isNCHW = true;
                break;
        }
    }

    edenOperation->opType = opType;

    getMatchedEdenOperationName((int32_t)androidOperation.type, edenOperation->opName.name, edenOperation->opName.length);
    int32_t status = getInputOutputForEdenOperation(model, modelInfo, androidOperation, configOperandId, edenOperation, edenModel);
    if (RET_OK != status) {
        return status;
    }

    setEdenOperandNames(edenModel, edenOperation);
    adjustOperandDims(model, androidOperation, modelInfo, edenOperation, edenModel);

    // Now add generated edenOperation to model
    int32_t edenOperationId = -1;
    edenModel->AddOperation(edenOperation, &edenOperationId);
    LOGD(EDEN_DRIVER, "Operation converting is complete! opType=%d, edenOperationId=%d\n", edenOperation->opType, edenOperationId);

    //showEdenOperation(edenOperation);

    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
    return RET_OK;
}

void createEmptyEdenOperand(EdenOperand** edenOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    EdenOperand* emptyEdenOperand = new EdenOperand();

    emptyEdenOperand->buffer = new EdenBuffer();
    emptyEdenOperand->buffer->addr = nullptr;
    emptyEdenOperand->buffer->size = 0;

    emptyEdenOperand->shapeInfo = nullptr;
    emptyEdenOperand->quantInfo = nullptr;
    emptyEdenOperand->opType = EDEN_OP_NUM_MAXIMUM;
    emptyEdenOperand->extraParams = nullptr;

    emptyEdenOperand->reserved[0] = 0;
    emptyEdenOperand->reserved[1] = 0;
    emptyEdenOperand->reserved[2] = 0;

    *edenOperand = emptyEdenOperand;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void createEdenCustomOperandForNcpBinary(void* bufferForNCP, int32_t bufferSizeForNCP, EdenOperand** edenOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    EdenOperand* edenOperandForNcpBinary = nullptr;
    createEmptyEdenOperand(&edenOperandForNcpBinary);

    getEdenCustomOperandName(NCP_BINARY, edenOperandForNcpBinary->name.name, edenOperandForNcpBinary->name.length);

    edenOperandForNcpBinary->buffer->addr = bufferForNCP;
    edenOperandForNcpBinary->buffer->size = bufferSizeForNCP;

    edenOperandForNcpBinary->shapeInfo = nullptr;
    edenOperandForNcpBinary->quantInfo = nullptr;
    edenOperandForNcpBinary->extraParams = nullptr;
    edenOperandForNcpBinary->reserved[0] = 0;
    edenOperandForNcpBinary->reserved[1] = 0;
    edenOperandForNcpBinary->reserved[2] = 0;

    *edenOperand = edenOperandForNcpBinary;
}

void createEdenCustomOperandForNcpName(EdenOperand** edenOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    EdenOperand* edenOperandForNcpName = nullptr;
    createEmptyEdenOperand(&edenOperandForNcpName);

    getEdenCustomOperandName(NCP_NAME, edenOperandForNcpName->name.name, edenOperandForNcpName->name.length);

    int8_t* bufferForModelName = nullptr;
    int32_t bufferSizeForModelName = 0;
    getModelNameForNCP(bufferForModelName, bufferSizeForModelName);
    edenOperandForNcpName->buffer->addr = static_cast<void*>(bufferForModelName);
    edenOperandForNcpName->buffer->size = bufferSizeForModelName;

    edenOperandForNcpName->shapeInfo = nullptr;
    edenOperandForNcpName->quantInfo = nullptr;
    edenOperandForNcpName->extraParams = nullptr;
    edenOperandForNcpName->reserved[0] = 0;
    edenOperandForNcpName->reserved[1] = 0;
    edenOperandForNcpName->reserved[2] = 0;

    *edenOperand = edenOperandForNcpName;
}

int32_t getEdenOperandIdx(const V1_2::Model& model, ModelInfo& modelInfo, EdenModel* edenModel, int32_t androidOperandIdx,
                          int32_t edenOpType, bool isNCHW) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    auto edenOperandIter = modelInfo.mapOperandIdFromAToE.find(androidOperandIdx);
    if (edenOperandIter != modelInfo.mapOperandIdFromAToE.end()) {
        return edenOperandIter->second;
    }

    const V1_2::Operand& androidOperand = model.operands[androidOperandIdx];

    LOGD(EDEN_DRIVER, "Try to convert androidOperandIdx=%d...\n", androidOperandIdx);
    EdenOperand* edenOperand = new EdenOperand();

    createEdenOperand(modelInfo, androidOperand, edenOperand, edenOpType, isNCHW);

    int32_t edenOperandId = -1;
    edenModel->AddOperandForOptions(edenOperand, &edenOperandId);

    // Use edenOperandId instead of edenOperandIdx
    keepOperandIdMap(modelInfo, androidOperandIdx, edenOperandId);
    LOGD(EDEN_DRIVER, "Converting complete, (androidOperandIdx=%d, edenOperandId=%d\n", androidOperandIdx, edenOperandId);

    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
    return edenOperandId;
}

/**
 * @brief Map android operand index to eden operand index
 * @details This function maps an android operand index to an eden operand index.
 * @param[in] modelInfo
 * @param[in] androidOperandIdx
 * @param[in] edenOperandIdx
 * @returns return code
 */
int32_t keepOperandIdMap(ModelInfo& modelInfo, int32_t androidOperandIdx, int32_t edenOperandIdx) {
    LOGD(EDEN_DRIVER, "%s(+), androidOperandIdx=%d, edenOperandIdx=%d\n", __func__, androidOperandIdx, edenOperandIdx);

    modelInfo.mapOperandIdFromAToE.insert(std::pair<int32_t, int32_t>(androidOperandIdx, edenOperandIdx));

    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
    return RET_OK;
}

/**
 * @brief Map android operation index to eden operation index
 * @details This function maps an android operation index to an eden operation index.
 * @param[in] modelInfo
 * @param[in] androidOperationIdx
 * @param[in] edenOperationIdx
 * @returns return code
 */
int32_t keepOperationIdMap(ModelInfo& modelInfo, int32_t androidOperationIdx, int32_t edenOperationIdx) {
    LOGD(EDEN_DRIVER, "%s(+), androidOperationIdx=%d, edenOperationIdx=%d\n", __func__, androidOperationIdx, edenOperationIdx);

    modelInfo.mapOperationIdFromAToE.insert(std::pair<int32_t, int32_t>(androidOperationIdx, edenOperationIdx));

    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
    return RET_OK;
}

/**
 * @brief Map android operation address to android operation index
 * @details This function maps an android operation address to android operation index.
 * @param[in] modelInfo
 * @param[in] androidOperation
 * @param[in] androidOperationIdx
 * @returns return code
 */
int32_t keepOperationToOperationIdMap(ModelInfo& modelInfo, void* androidOperation, int32_t androidOperationIdx) {
    LOGD(EDEN_DRIVER, "%s(+), androidOperationIdx=%d\n", __func__, androidOperationIdx);

    modelInfo.mapOperationToOperationId.insert(std::pair<void*, int32_t>(androidOperation, androidOperationIdx));

    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
    return RET_OK;
}

/**
 * @brief Keep EdenCustomOperationId
 * @details This function keeps EdenCustom operation index.
 * @param[in] modelInfo
 * @param[in] edenOperationIdx
 * @returns return code
 */
int32_t keepCustomOperationId(ModelInfo& modelInfo, int32_t edenOperationIdx) {
    LOGD(EDEN_DRIVER, "%s(+), edenOperationIdx=%d\n", __func__, edenOperationIdx);

    modelInfo.vecCustomOperationIds.push_back(edenOperationIdx);

    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
    return RET_OK;
}

/**
 * @brief Map android operation address to android operation index
 * @details This function maps an android operation address to android operation index.
 * @param[in] androidOperationIdx
 * @param[in] edenOperationIdx
 * @returns return code
 */
int32_t keepInputConsumers(const V1_2::Model& model, ModelInfo& modelInfo, int32_t androidOperandIdx, int32_t androidOperationIdx) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    const V1_2::Operand& androidOperand = model.operands[androidOperandIdx];
    if (androidOperand.lifetime != OperandLifeTime::MODEL_INPUT) {
        LOGE(EDEN_DRIVER, "Oops, androidOperandIdx=%d.lifetime is not MODEL_INPUT! Please check it!\n", androidOperandIdx);
        return LIFETIME_IS_NOT_MODEL_INPUT;
    }

    // First, find proper index that is matched to a given androidOperandIdx on model.inputs
    for (size_t idx = 0; idx < modelInfo.modelInputIndexes.size(); idx++) {
        if (modelInfo.modelInputIndexes[idx] == androidOperandIdx) {
            modelInfo.inputConsumers[idx] = androidOperationIdx;
            LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
            return RET_OK;
        }
    }
    LOGE(EDEN_DRIVER, "Error, given androidOperandIdx=%d is not existing on model.inputs... Please check the model's input indexes and a given androidOperandIdx.\n", androidOperandIdx);

    return FAIL_TO_FIND_A_INPUT_OPERAND_INDEX;
}

bool createEdenOperandForConfig(const V1_2::Model& model, const V1_2::Operation& androidOperation, const ModelInfo* modelInfo, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);
    bool generated = false;

    int32_t opType = static_cast<int32_t>(androidOperation.type);

    switch (opType) {
    case ANEURALNETWORKS_ADD:  // 0
        configAdd(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_AVERAGE_POOL_2D:  // 1
        configAveragePool2d(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_CONCATENATION:  // 2
        configConcatenation(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_CONV_2D:  // 3
        configConv2d(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_DEPTHWISE_CONV_2D:  // 4
        configDepthwiseConv2d(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_DEPTH_TO_SPACE:  // 5
        configDepthToSpace(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_DEQUANTIZE:  // 6
        // No options
        break;
    case ANEURALNETWORKS_EMBEDDING_LOOKUP:  // 7
        // No options
        break;
    case ANEURALNETWORKS_FLOOR:  // 8
        // No options
        break;
    case ANEURALNETWORKS_FULLY_CONNECTED:  // 9
        configFullyConnected(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_HASHTABLE_LOOKUP:  // 10
        // No options
        break;
    case ANEURALNETWORKS_L2_NORMALIZATION:  // 11
        configL2Normalization(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_L2_POOL_2D:  // 12
        configL2Pool2d(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_LOCAL_RESPONSE_NORMALIZATION:  // 13
        configLocalResponseNormalization(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_LOGISTIC:  // 14
        // No options
        break;
    case ANEURALNETWORKS_LSH_PROJECTION:  // 15
        configLshProjection(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_LSTM:  // 16
        configLSTM(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_MAX_POOL_2D:  // 17
        configMaxpool2d(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_MUL:  // 18
        configMul(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_RELU:  // 19
        // No options
        break;
    case ANEURALNETWORKS_RELU1:  // 20
        // No options
        break;
    case ANEURALNETWORKS_RELU6:  // 21
        // No options
        break;
    case ANEURALNETWORKS_RESHAPE:  // 22
        configReshape(model, androidOperation, modelInfo, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_RESIZE_BILINEAR:  // 23
        configResizeBilinear(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_RNN:  // 24
        configRNN(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_SOFTMAX:  // 25
        configSoftmax(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_SPACE_TO_DEPTH:  // 26
        configSpaceToDepth(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_SVDF:  // 27
        configSVDF(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_TANH:  // 28
        // No options
        break;
    case ANEURALNETWORKS_BATCH_TO_SPACE_ND:  // 29
        configBatchToSpaceND(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_DIV:  // 30
        configDiv(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_MEAN:  // 31
        // No options
        break;
    case ANEURALNETWORKS_PAD:  // 32
        // No options
        break;
    case ANEURALNETWORKS_SPACE_TO_BATCH_ND:  // 33
        configSpaceToBatchND(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_SQUEEZE:  // 34
        configSqueeze(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_STRIDED_SLICE:  // 35
        configStridedSlice(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_SUB:  // 36
        configSub(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_TRANSPOSE:  // 37
        configTranspose(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_BIDIRECTIONAL_SEQUENCE_LSTM : // 42
        configBidirectionalSequenceLstm(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_GENERATE_PROPOSALS: // 52
        configGenerateProposals(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_UNIDIRECTIONAL_SEQUENCE_LSTM:  // 53
        configUnidirectionalSequenceLstm(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_GROUPED_CONV_2D:  // 55
        configGroupConv2d(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_LOGICAL_NOT:  // 62
        // No options
        break;
    case ANEURALNETWORKS_NEG:  // 67
        // No options
        break;
    case ANEURALNETWORKS_ROI_ALIGN : // 81
        configRoiAlign(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_RESIZE_NEAREST_NEIGHBOR : // 94
        configResizeNearestNeighbor(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_ARGMAX:  // 40
        configArgMax(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_TRANSPOSE_CONV_2D:  // 49
        configDeConv2d(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_BIDIRECTIONAL_SEQUENCE_RNN:  // 54
        configBidriectionalRNN(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_UNIDIRECTIONAL_SEQUENCE_RNN:  // 55
        configUnidirectionalRNN(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_SIN:  // 75
        // No options
        break;
    case ANEURALNETWORKS_RSQRT:  // 76
        // No options
        break;
    case ANEURALNETWORKS_PAD_V2:  // 77
        // No options
        break;
    case ANEURALNETWORKS_TOPK_V2:  // 78
        configTopKV2(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_ROI_POOLING:  // 79
        configTFLiteRoiPool(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_ARGMIN:  // 80
        configArgMin(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_SQRT:  // 81
        // No options
        break;
    case ANEURALNETWORKS_BOX_WITH_NMS_LIMIT:  // 82
        configBoxWithNmsLimit(model, androidOperation, configOperand);
        generated = true;
        break;
   case ANEURALNETWORKS_EQUAL: // 48
        // No options;
        break;
    case ANEURALNETWORKS_EXPAND_DIMS: // 50
        configExpandDims(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_GATHER: // 51
        configGather(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_GREATER: // 53
        // No options;
        break;
    case ANEURALNETWORKS_GREATER_EQUAL: // 54
        // No options;
        break;
    case ANEURALNETWORKS_LESS: // 58
        // No options;
        break;
    case ANEURALNETWORKS_LESS_EQUAL: // 59
        // No options;
        break;
    case ANEURALNETWORKS_NOT_EQUAL: // 68
        // No options;
        break;
    case ANEURALNETWORKS_POW: // 69
        // No options;
        break;
    case ANEURALNETWORKS_PRELU: // 71
        // No options;
        break;
    case ANEURALNETWORKS_QUANTIZED_16BIT_LSTM: // 73
        // No options;
        break;
    case ANEURALNETWORKS_SELECT: // 84
        // No options;
        break;
    case ANEURALNETWORKS_SPLIT: // 87
        configSplit(model, androidOperation, configOperand);
        generated = true;
        break;
  case ANEURALNETWORKS_CHANNEL_SHUFFLE:  // 46
        configChannelShuffle(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_DETECTION_POSTPROCESSING:  //47
        configDetectionPostprocessing(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_INSTANCE_NORMALIZATION:  // 57
        configInstanceNormalization(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_LOG_SOFTMAX:  // 64
        configLogSoftmax(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_RANDOM_MULTINOMIAL:  // 74
        configRandomMultinomial(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_HEATMAP_MAX_KEYPOINT:  // 56
        configHeatmapMaxKeypoint(model, androidOperation, configOperand);
        generated = true;
        break;
    case ANEURALNETWORKS_ABS:  // 38
    case ANEURALNETWORKS_CAST:  // 45
    case ANEURALNETWORKS_EXP:  // 49
    case ANEURALNETWORKS_LOG:  // 60
    case ANEURALNETWORKS_LOGICAL_AND:  // 61
    case ANEURALNETWORKS_LOGICAL_OR:  // 63
    case ANEURALNETWORKS_QUANTIZE:  // 72
        // No options
        break;
    default:
        LOGE(EDEN_DRIVER, "opType=%d is not yet supported", opType);
        break;
    }

    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
    return generated;
}

void configAdd(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_ADD_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;
    LOGD(EDEN_DRIVER, "Add new operand name : %s\n", newOperandForOptions->name.name);

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(AddOptions);
    bufferForOptions->addr = new uint32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // inputs operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    AddOptions* options = new (bufferForOptions->addr) AddOptions();

    /*
     0: A tensor.
     1: A tensor of the same OperandCode, and compatible dimensions as input0.
        For a ANEURALNETWORKS_TENSOR_QUANT8_ASYMM tensor, the scales and zeroPoint can be different from input0 scale and zeroPoint.
     2: An ANEURALNETWORKS_INT32 scalar, and has to be one of the FuseCode values.
        Specifies the activation to invoke on the result.
     */
    options->numOfCoeffs = 0;
    options->coeffs = nullptr;
    int32_t activation = getValue<int32_t>(model, androidOperation, 2);  // 2 to get FuseCode
    options->fusedActivation = static_cast<FusedActivation>(getActivationFn(activation));
    LOGD(EDEN_DRIVER, "activation=%d\n", activation);

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configAveragePool2d(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_AVERAGE_POOL_2D_EXPLICIT_1_1 && numOfInputs != NUM_OF_INPUTS_ON_AVERAGE_POOL_2D_IMPLICIT_1_1 &&
        numOfInputs != NUM_OF_INPUTS_ON_AVERAGE_POOL_2D_EXPLICIT_1_2 && numOfInputs != NUM_OF_INPUTS_ON_AVERAGE_POOL_2D_IMPLICIT_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }

    // Check unknown dimension, unknown rank
    if ((model.operands[androidOperation.inputs[0]].dimensions.size() == 0) ||   // unknown rank
        (model.operands[androidOperation.inputs[0]].dimensions[W_NHWC] == 0) ||  // unknown dimension
        (model.operands[androidOperation.inputs[0]].dimensions[H_NHWC] == 0)) {  // unknown dimension
        LOGD(EDEN_DRIVER, "Unknown dimension, unknown rank is detected... skip it\n");
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    auto inputOperandIndex = androidOperation.inputs[0];
    auto width = model.operands[inputOperandIndex].dimensions[W_NHWC];
    auto height = model.operands[inputOperandIndex].dimensions[H_NHWC];

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(Pool2DOptions);
    bufferForOptions->addr = new uint32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    Pool2DOptions* options = new (bufferForOptions->addr) Pool2DOptions();

    int32_t paddingLeft, paddingRight, paddingTop, paddingBottom;
    int32_t paddingImplicit;
    int32_t strideWidth, strideHeight;
    int32_t filterWidth, filterHeight;
    int32_t activation;
    bool NCHWDataLayout = false;

    /*
     Inputs (explicit padding):
     0: A 4-D tensor, of shape [batches, height, width, depth], specifying the input.
        Since API level 29, zero batches is supported for this tensor.
     1: An ANEURALNETWORKS_INT32 scalar, specifying the padding on the left, in the width dimension.
     2: An ANEURALNETWORKS_INT32 scalar, specifying the padding on the right, in the width dimension.
     3: An ANEURALNETWORKS_INT32 scalar, specifying the padding on the top, in the height dimension.
     4: An ANEURALNETWORKS_INT32 scalar, specifying the padding on the bottom, in the height dimension.
     5: An ANEURALNETWORKS_INT32 scalar, specifying the stride when walking through input in the width dimension.
     6: An ANEURALNETWORKS_INT32 scalar, specifying the stride when walking through input in the height dimension.
     7: An ANEURALNETWORKS_INT32 scalar, specifying the filter width.
     8: An ANEURALNETWORKS_INT32 scalar, specifying the filter height.
     9: An ANEURALNETWORKS_INT32 scalar, and has to be one of the FuseCode values. Specifies the activation to invoke on the result.
     10: An optional ANEURALNETWORKS_BOOL scalar, default to false. Set to true to specify NCHW data layout for input0 and output0.
         Available since API level 29.

     Inputs (implicit padding):
     0: A 4-D tensor, of shape [batches, height, width, depth], specifying the input.
        Since API level 29, zero batches is supported for this tensor.
     1: An ANEURALNETWORKS_INT32 scalar, specifying the implicit padding scheme, has to be one of the PaddingCode values.
     2: An ANEURALNETWORKS_INT32 scalar, specifying the stride when walking through input in the width dimension.
     3: An ANEURALNETWORKS_INT32 scalar, specifying the stride when walking through input in the height dimension.
     4: An ANEURALNETWORKS_INT32 scalar, specifying the filter width.
     5: An ANEURALNETWORKS_INT32 scalar, specifying the filter height.
     6: An ANEURALNETWORKS_INT32 scalar, and has to be one of the FuseCode values. Specifies the activation to invoke on the result.
     7: An optional ANEURALNETWORKS_BOOL scalar, default to false. Set to true to specify NCHW data layout for input0 and output0.
        Available since API level 29.
     */
    LOGD(EDEN_DRIVER, "numOfInputs: %zu\n", numOfInputs);
    if (numOfInputs == NUM_OF_INPUTS_ON_AVERAGE_POOL_2D_EXPLICIT_1_1) {
        paddingLeft = getValue<int32_t>(model, androidOperation, 1);
        paddingRight = getValue<int32_t>(model, androidOperation, 2);
        paddingTop = getValue<int32_t>(model, androidOperation, 3);
        paddingBottom = getValue<int32_t>(model, androidOperation, 4);
        strideWidth = getValue<int32_t>(model, androidOperation, 5);
        strideHeight = getValue<int32_t>(model, androidOperation, 6);
        filterWidth = getValue<int32_t>(model, androidOperation, 7);
        filterHeight = getValue<int32_t>(model, androidOperation, 8);
        activation = getValue<int32_t>(model, androidOperation, 9);
    } else if (numOfInputs == NUM_OF_INPUTS_ON_AVERAGE_POOL_2D_IMPLICIT_1_1) {
        paddingImplicit = getValue<int32_t>(model, androidOperation, 1);
        strideWidth = getValue<int32_t>(model, androidOperation, 2);
        strideHeight = getValue<int32_t>(model, androidOperation, 3);
        filterWidth = getValue<int32_t>(model, androidOperation, 4);
        filterHeight = getValue<int32_t>(model, androidOperation, 5);
        activation = getValue<int32_t>(model, androidOperation, 6);

        getPadding(width, strideWidth, filterWidth, paddingImplicit, &paddingLeft, &paddingRight);
        getPadding(height, strideHeight, filterHeight, paddingImplicit, &paddingTop, &paddingBottom);
    } else if (numOfInputs == NUM_OF_INPUTS_ON_AVERAGE_POOL_2D_EXPLICIT_1_2) {
        paddingLeft = getValue<int32_t>(model, androidOperation, 1);
        paddingRight = getValue<int32_t>(model, androidOperation, 2);
        paddingTop = getValue<int32_t>(model, androidOperation, 3);
        paddingBottom = getValue<int32_t>(model, androidOperation, 4);
        strideWidth = getValue<int32_t>(model, androidOperation, 5);
        strideHeight = getValue<int32_t>(model, androidOperation, 6);
        filterWidth = getValue<int32_t>(model, androidOperation, 7);
        filterHeight = getValue<int32_t>(model, androidOperation, 8);
        activation = getValue<int32_t>(model, androidOperation, 9);
        NCHWDataLayout = getValue<bool>(model, androidOperation, 10);
    } else {
        paddingImplicit = getValue<int32_t>(model, androidOperation, 1);
        strideWidth = getValue<int32_t>(model, androidOperation, 2);
        strideHeight = getValue<int32_t>(model, androidOperation, 3);
        filterWidth = getValue<int32_t>(model, androidOperation, 4);
        filterHeight = getValue<int32_t>(model, androidOperation, 5);
        activation = getValue<int32_t>(model, androidOperation, 6);
        NCHWDataLayout = getValue<bool>(model, androidOperation, 7);

        if (NCHWDataLayout) {
            width = model.operands[inputOperandIndex].dimensions[W_NCHW];
            height = model.operands[inputOperandIndex].dimensions[H_NCHW];
        } else {
            width = model.operands[inputOperandIndex].dimensions[W_NHWC];
            height = model.operands[inputOperandIndex].dimensions[H_NHWC];
        }

        getPadding(width, strideWidth, filterWidth, paddingImplicit, &paddingLeft, &paddingRight);
        getPadding(height, strideHeight, filterHeight, paddingImplicit, &paddingTop, &paddingBottom);
    }

    options->padLeft = paddingLeft;
    options->padRight = paddingRight;
    options->padTop = paddingTop;
    options->padBottom = paddingBottom;

    options->strideWidth = strideWidth;
    options->strideHeight = strideHeight;
    options->kernelWidth = filterWidth;
    options->kernelHeight = filterHeight;
    options->NCHWDataLayout = NCHWDataLayout;

    options->fusedActivation = static_cast<FusedActivation>(getActivationFn(activation));

    newOperandForOptions->isNCHW = NCHWDataLayout;

    LOGD(EDEN_DRIVER, "paddingLeft=%d, paddingRight=%d\n", paddingLeft, paddingRight);
    LOGD(EDEN_DRIVER, "paddingTop=%d, paddingBottom=%d\n", paddingTop, paddingBottom);
    LOGD(EDEN_DRIVER, "strideWidth=%d, strideHeight=%d\n", strideWidth, strideHeight);
    LOGD(EDEN_DRIVER, "filterWidth=%d, filterHeight=%d\n", filterWidth, filterHeight);
    LOGD(EDEN_DRIVER, "activation=%d\n", activation);
    LOGD(EDEN_DRIVER, "NCHWDataLayout=%d\n", NCHWDataLayout);

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configConcatenation(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // Check unknown dimension, unknown rank
    if (model.operands[androidOperation.inputs[0]].dimensions.size() == 0) {  // unknown rank
        LOGD(EDEN_DRIVER, "Unknown dimension, unknown rank is detected... skip it\n");
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    auto numOfInputs = inputs.size();

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(ConcatenationOptions);
    bufferForOptions->addr = new uint32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    ConcatenationOptions* options = new (bufferForOptions->addr) ConcatenationOptions();

    /*
     0 ~ n-1: The list of n input tensors, of shape [D0, D1, ..., Daxis(i), ..., Dm].
              Before API level 29, all input tensors of ANEURALNETWORKS_TENSOR_QUANT8_ASYMM must have the same scale and zeroPoint as the output tensor.
              Since API level 29, zero-sized tensors are supported.
     n: An ANEURALNETWORKS_INT32 scalar, specifying the concatenation axis.
     */
    int32_t axisValue = getValue<int32_t>(model, androidOperation, numOfInputs - 1);
    LOGD(EDEN_DRIVER, "axisValue=%d\n", axisValue);

    //assume that all the inputs' dimention of concate are same
    auto operandId = androidOperation.inputs[0];
    auto operand = model.operands[operandId];
    size_t rank = operand.dimensions.size();

    if (rank == 1) {
        options->axis = axisValue;
    } else if (rank == 2) {
        options->axis = axisValue + 2;  /* VTS has open 2 axis, 0 for h, 1 for w */
    } else if (rank == 3) {
        if (axisValue == H_NHWC || axisValue == W_NHWC) {
            options->axis = axisValue + 1;
        } else {
            options->axis = axisValue;
        }
    } else {
        if (axisValue == C_NHWC) {
            options->axis = C_NCHW;
        } else if (axisValue == W_NHWC) {
            options->axis = W_NCHW;
        } else if (axisValue == H_NHWC) {
            options->axis = H_NCHW;
        } else {
            options->axis = axisValue;
        }
    }
    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configConv2d(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_CONV_2D_EXPLICIT_1_1 && numOfInputs != NUM_OF_INPUTS_ON_CONV_2D_IMPLICIT_1_1 &&
        numOfInputs != NUM_OF_INPUTS_ON_CONV_2D_EXPLICIT_1_2_ALL && numOfInputs != NUM_OF_INPUTS_ON_CONV_2D_IMPLICIT_1_2_ALL &&
        numOfInputs != NUM_OF_INPUTS_ON_CONV_2D_EXPLICIT_1_2_LAYOUT && numOfInputs != NUM_OF_INPUTS_ON_CONV_2D_IMPLICIT_1_2_LAYOUT) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }

    // Check unknown dimension, unknown rank
    if ((model.operands[androidOperation.inputs[0]].dimensions.size() == 0) ||   // unknown rank
        (model.operands[androidOperation.inputs[0]].dimensions[W_NHWC] == 0) ||  // unknown dimension
        (model.operands[androidOperation.inputs[0]].dimensions[H_NHWC] == 0)) {  // unknown dimension
        LOGD(EDEN_DRIVER, "Unknown dimension, unknown rank is detected... skip it\n");
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    auto inputOperandIndex = androidOperation.inputs[0];
    auto width = model.operands[inputOperandIndex].dimensions[W_NHWC];
    auto height = model.operands[inputOperandIndex].dimensions[H_NHWC];

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(Conv2DOptions);
    bufferForOptions->addr = new uint32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    Conv2DOptions* options = new (bufferForOptions->addr)Conv2DOptions();

    int32_t paddingLeft, paddingRight, paddingTop, paddingBottom;
    int32_t paddingImplicit;
    int32_t strideWidth, strideHeight;
    int32_t filterWidth, filterHeight;
    int32_t activation;
    bool NCHWDataLayout = false;
    int32_t dilationWidthFactor = 1;
    int32_t dilationHeightFactor = 1;

    /*
     Inputs (explicit padding):
     0: A 4-D tensor, of shape [batches, height, width, depth_in], specifying the input.
     1: A 4-D tensor, of shape [depth_out, filter_height, filter_width, depth_in], specifying the filter.
        For tensor of type ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL the channel dimension (extraParams.channelQuant.channelDim) must be set to 0.
     2: A 1-D tensor, of shape [depth_out], specifying the bias.
        For input tensor of type ANEURALNETWORKS_TENSOR_FLOAT32 or ANEURALNETWORKS_TENSOR_FLOAT16, the bias must be of the same type.
        For filter tensor of ANEURALNETWORKS_TENSOR_QUANT8_ASYMM, the bias should be of ANEURALNETWORKS_TENSOR_INT32, with zeroPoint of 0 and bias_scale == input_scale * filter_scale.
        For filter tensor of ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL, the bias should be of ANEURALNETWORKS_TENSOR_INT32, with zeroPoint of 0 and bias_scale of 0.
        The actual scale of each value 'i' is equal to bias_scale[i] = input_scale * filter_scale[i].
     3: An ANEURALNETWORKS_INT32 scalar, specifying the padding on the left, in the width dimension.
     4: An ANEURALNETWORKS_INT32 scalar, specifying the padding on the right, in the width dimension.
     5: An ANEURALNETWORKS_INT32 scalar, specifying the padding on the top, in the height dimension.
     6: An ANEURALNETWORKS_INT32 scalar, specifying the padding on the bottom, in the height dimension.
     7: An ANEURALNETWORKS_INT32 scalar, specifying the stride when walking through input in the width dimension.
     8: An ANEURALNETWORKS_INT32 scalar, specifying the stride when walking through input in the height dimension.
     9: An ANEURALNETWORKS_INT32 scalar, and has to be one of the FuseCode values. Specifies the activation to invoke on the result.
     10: An optional ANEURALNETWORKS_BOOL scalar, default to false. Set to true to specify NCHW data layout for input0 and output0.
         Available since API level 29.
     11: An optional ANEURALNETWORKS_INT32 scalar, specifying the dilation factor for width. Defaults to 1.
         If set to k > 1, there will be k-1 skipped cells between each filter element on width dimension.
         If this input is set, input 12 (dilation factor for height) must be specified as well. Available since API level 29.
     12: An optional ANEURALNETWORKS_INT32 scalar, specifying the dilation factor for height. Defaults to 1.
         If set to k > 1, there will be k-1 skipped cells between each filter element on height dimension.
         If this input is set, input 11 (dilation factor for width) must be specified as well. Available since API level 29.

     Inputs (implicit padding):
     0: A 4-D tensor, of shape [batches, height, width, depth_in], specifying the input.
     1: A 4-D tensor, of shape [depth_out, filter_height, filter_width, depth_in], specifying the filter.
        For tensor of type ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL the channel dimension (extraParams.channelQuant.channelDim) must be set to 0.
     2: A 1-D tensor, of shape [depth_out], specifying the bias.
        For input tensor of type ANEURALNETWORKS_TENSOR_FLOAT32 or ANEURALNETWORKS_TENSOR_FLOAT16, the bias must be of the same type.
        For filter tensor of ANEURALNETWORKS_TENSOR_QUANT8_ASYMM, the bias should be of ANEURALNETWORKS_TENSOR_INT32, with zeroPoint of 0 and bias_scale == input_scale * filter_scale.
        For filter tensor of ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL, the bias should be of ANEURALNETWORKS_TENSOR_INT32, with zeroPoint of 0 and bias_scale of 0.
        The actual scale of each value 'i' is equal to bias_scale[i] = input_scale * filter_scale[i].
     3: An ANEURALNETWORKS_INT32 scalar, specifying the implicit padding scheme, has to be one of the PaddingCode values.
     4: An ANEURALNETWORKS_INT32 scalar, specifying the stride when walking through input in the width dimension.
     5: An ANEURALNETWORKS_INT32 scalar, specifying the stride when walking through input in the height dimension.
     6: An ANEURALNETWORKS_INT32 scalar, and has to be one of the FuseCode values. Specifies the activation to invoke on the result.
     7: An optional ANEURALNETWORKS_BOOL scalar, default to false. Set to true to specify NCHW data layout for input0 and output0. Available since API level 29.
     8: An optional ANEURALNETWORKS_INT32 scalar, specifying the dilation factor for width. Defaults to 1.
        If set to k > 1, there will be k-1 skipped cells between each filter element on width dimension.
        If this input is set, input 9 (dilation factor for height) must be specified as well. Available since API level 29.
     9: An optional ANEURALNETWORKS_INT32 scalar, specifying the dilation factor for height. Defaults to 1.
        If set to k > 1, there will be k-1 skipped cells between each filter element on height dimension.
        If this input is set, input 8 (dilation factor for width) must be specified as well. Available since API level 29.
     */
    LOGD(EDEN_DRIVER, "numOfInputs: %zu\n", numOfInputs);
    bool isV1_2 = false;
    if (numOfInputs >= 8) {
        auto index = androidOperation.inputs[7];
        isV1_2 = model.operands[index].type == OperandType::BOOL ? true : false;
    }

    if ((numOfInputs >= 8 && isV1_2) || numOfInputs == 7) {
        paddingImplicit = getValue<int32_t>(model, androidOperation, 3);
        strideWidth = getValue<int32_t>(model, androidOperation, 4);
        strideHeight = getValue<int32_t>(model, androidOperation, 5);
        activation = getValue<int32_t>(model, androidOperation, 6);
        if (numOfInputs >= 8) {
            NCHWDataLayout = getValue<bool>(model, androidOperation, 7);
        }
        if (numOfInputs == 10) {
            dilationWidthFactor = getValue<int32_t>(model, androidOperation, 8);
            dilationHeightFactor = getValue<int32_t>(model, androidOperation, 9);
        }

        if (NCHWDataLayout) {
            width = model.operands[inputOperandIndex].dimensions[3];
            height = model.operands[inputOperandIndex].dimensions[2];
        } else {
            width = model.operands[inputOperandIndex].dimensions[2];
            height = model.operands[inputOperandIndex].dimensions[1];
        }

        auto filterIndex = androidOperation.inputs[1];
        auto filterOperand = model.operands[filterIndex];
        filterWidth = filterOperand.dimensions[W_NHWC];
        filterHeight = filterOperand.dimensions[H_NHWC];

        filterWidth = dilationWidthFactor * (filterWidth - 1) + 1;
        filterHeight = dilationHeightFactor * (filterHeight - 1) + 1;

        getPadding(width, strideWidth, filterWidth, paddingImplicit, &paddingLeft, &paddingRight);
        getPadding(height, strideHeight, filterHeight, paddingImplicit, &paddingTop, &paddingBottom);
    } else {
        paddingLeft = getValue<int32_t>(model, androidOperation, 3);
        paddingRight = getValue<int32_t>(model, androidOperation, 4);
        paddingTop = getValue<int32_t>(model, androidOperation, 5);
        paddingBottom = getValue<int32_t>(model, androidOperation, 6);
        strideWidth = getValue<int32_t>(model, androidOperation, 7);
        strideHeight = getValue<int32_t>(model, androidOperation, 8);
        activation = getValue<int32_t>(model, androidOperation, 9);
        if (numOfInputs >= 11) {
            NCHWDataLayout = getValue<bool>(model, androidOperation, 10);
        }
        if (numOfInputs == 13) {
            dilationWidthFactor = getValue<int32_t>(model, androidOperation, 11);
            dilationHeightFactor = getValue<int32_t>(model, androidOperation, 12);
        }
    }

    options->padLeft = paddingLeft;
    options->padRight = paddingRight;
    options->padTop = paddingTop;
    options->padBottom = paddingBottom;

    options->strideWidth = strideWidth;
    options->strideHeight = strideHeight;

    options->NCHWDataLayout = NCHWDataLayout;
    options->dilationWidthFactor = dilationWidthFactor;
    options->dilationHeightFactor = dilationHeightFactor;

    options->fusedActivation = static_cast<FusedActivation>(getActivationFn(activation));
    newOperandForOptions->isNCHW = NCHWDataLayout;

    LOGD(EDEN_DRIVER, "paddingLeft=%d, paddingRight=%d\n", paddingLeft, paddingRight);
    LOGD(EDEN_DRIVER, "paddingTop=%d, paddingBottom=%d\n", paddingTop, paddingBottom);
    LOGD(EDEN_DRIVER, "strideWidth=%d, strideHeight=%d\n", strideWidth, strideHeight);
    LOGD(EDEN_DRIVER, "activation=%d\n", activation);
    LOGD(EDEN_DRIVER, "NCHWDataLayout=%d\n", NCHWDataLayout);
    LOGD(EDEN_DRIVER, "dilationWidthFactor=%d, dilationHeightFactor=%d\n", dilationWidthFactor, dilationHeightFactor);

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configGroupConv2d(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_GROUP_CONV_2D_EXPLICIT_1_2 && numOfInputs != NUM_OF_INPUTS_ON_GROUP_CONV_2D_IMPLICIT_1_2 ) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }

    // Check unknown dimension, unknown rank
    if ((model.operands[androidOperation.inputs[0]].dimensions.size() == 0) ||   // unknown rank
        (model.operands[androidOperation.inputs[0]].dimensions[W_NHWC] == 0) ||  // unknown dimension
        (model.operands[androidOperation.inputs[0]].dimensions[H_NHWC] == 0)) {  // unknown dimension
        LOGD(EDEN_DRIVER, "Unknown dimension, unknown rank is detected... skip it\n");
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(Conv2DOptions);
    bufferForOptions->addr = new uint32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    Conv2DOptions* options = new (bufferForOptions->addr)Conv2DOptions();

    int32_t paddingLeft, paddingRight, paddingTop, paddingBottom;
    int32_t paddingImplicit;
    int32_t strideWidth, strideHeight;
    int32_t filterWidth, filterHeight;
    int32_t activation;
    int32_t group;
    bool NCHWDataLayout = false;

    LOGD(EDEN_DRIVER, "numOfInputs: %zu\n", numOfInputs);
    if (numOfInputs == NUM_OF_INPUTS_ON_GROUP_CONV_2D_EXPLICIT_1_2) {
        paddingLeft = getValue<int32_t>(model, androidOperation, 3);
        paddingRight = getValue<int32_t>(model, androidOperation, 4);
        paddingTop = getValue<int32_t>(model, androidOperation, 5);
        paddingBottom = getValue<int32_t>(model, androidOperation, 6);
        strideWidth = getValue<int32_t>(model, androidOperation, 7);
        strideHeight = getValue<int32_t>(model, androidOperation, 8);
        group = getValue<int32_t>(model, androidOperation, 9);
        activation = getValue<int32_t>(model, androidOperation, 10);
        NCHWDataLayout = getValue<bool>(model, androidOperation, 11);

    } else {
        paddingImplicit = getValue<int32_t>(model, androidOperation, 3);
        strideWidth = getValue<int32_t>(model, androidOperation, 4);
        strideHeight = getValue<int32_t>(model, androidOperation, 5);
        group = getValue<int32_t>(model, androidOperation, 6);
        activation = getValue<int32_t>(model, androidOperation, 7);
        NCHWDataLayout = getValue<bool>(model, androidOperation, 8);

        auto filterIndex = androidOperation.inputs[1];
        auto filterOperand = model.operands[filterIndex];
        filterWidth = filterOperand.dimensions[W_NHWC];
        filterHeight = filterOperand.dimensions[H_NHWC];

        auto inputOperandIndex = androidOperation.inputs[0];
        int32_t width, height;

        if (NCHWDataLayout) {
            width = model.operands[inputOperandIndex].dimensions[3];
            height = model.operands[inputOperandIndex].dimensions[2];
        } else {
            width = model.operands[inputOperandIndex].dimensions[2];
            height = model.operands[inputOperandIndex].dimensions[1];
        }

        getPadding(width, strideWidth, filterWidth, paddingImplicit, &paddingLeft, &paddingRight);
        getPadding(height, strideHeight, filterHeight, paddingImplicit, &paddingTop, &paddingBottom);
    }

    options->padLeft = paddingLeft;
    options->padRight = paddingRight;
    options->padTop = paddingTop;
    options->padBottom = paddingBottom;

    options->strideWidth = strideWidth;
    options->strideHeight = strideHeight;

    options->NCHWDataLayout = NCHWDataLayout;
    options->dilationWidthFactor = 1;
    options->dilationHeightFactor = 1;
    newOperandForOptions->isNCHW = NCHWDataLayout;

    options->fusedActivation = static_cast<FusedActivation>(getActivationFn(activation));

    LOGD(EDEN_DRIVER, "paddingLeft=%d, paddingRight=%d\n", paddingLeft, paddingRight);
    LOGD(EDEN_DRIVER, "paddingTop=%d, paddingBottom=%d\n", paddingTop, paddingBottom);
    LOGD(EDEN_DRIVER, "strideWidth=%d, strideHeight=%d\n", strideWidth, strideHeight);
    LOGD(EDEN_DRIVER, "group=%d\n", group);
    LOGD(EDEN_DRIVER, "activation=%d\n", activation);
    LOGD(EDEN_DRIVER, "NCHWDataLayout=%d\n", NCHWDataLayout);

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configFullyConnected(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_FULLY_CONNECTED_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }
    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(FusedActivationOptions);
    bufferForOptions->addr = new uint32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    FusedActivationOptions* options = new (bufferForOptions->addr) FusedActivationOptions();

    /*
     Inputs:
     0: A tensor of at least rank 2, specifying the input. If rank is greater than 2, then it gets flattened to a 2-D Tensor.
        The (flattened) 2-D Tensor is reshaped (if necessary) to [batch_size, input_size], where "input_size" corresponds to the number of inputs to the layer,
        matching the second dimension of weights, and "batch_size" is calculated by dividing the number of elements by "input_size".
     1: A 2-D tensor, specifying the weights, of shape [num_units, input_size], where "num_units" corresponds to the number of output nodes.
     2: A 1-D tensor, of shape [num_units], specifying the bias.
        For input tensor of ANEURALNETWORKS_TENSOR_FLOAT32, the bias should also be of ANEURALNETWORKS_TENSOR_FLOAT32.
        For input tensor of ANEURALNETWORKS_TENSOR_QUANT8_ASYMM, the bias should be of ANEURALNETWORKS_TENSOR_INT32, with zeroPoint of 0 and bias_scale == input_scale * filter_scale.
     3: An ANEURALNETWORKS_INT32 scalar, and has to be one of the FuseCode values. Specifies the activation to invoke on the result.
     */
    int32_t activation = getValue<int32_t>(model, androidOperation, 3);
    options->fusedActivation = static_cast<FusedActivation>(getActivationFn(activation));
    LOGD(EDEN_DRIVER, "activation=%d\n", activation);

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configDepthwiseConv2d(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_DEPTHWISE_CONV_2D_EXPLICIT_1_1 && numOfInputs != NUM_OF_INPUTS_ON_DEPTHWISE_CONV_2D_IMPLICIT_1_1 &&
        numOfInputs != NUM_OF_INPUTS_ON_DEPTHWISE_CONV_2D_EXPLICIT_1_2 && numOfInputs != NUM_OF_INPUTS_ON_DEPTHWISE_CONV_2D_IMPLICIT_1_2 &&
        numOfInputs != 12 && numOfInputs != 9) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }

    // Check unknown dimension, unknown rank
    if ((model.operands[androidOperation.inputs[0]].dimensions.size() == 0) ||   // unknown rank
        (model.operands[androidOperation.inputs[0]].dimensions[W_NHWC] == 0) ||  // unknown dimension
        (model.operands[androidOperation.inputs[0]].dimensions[H_NHWC] == 0)) {  // unknown dimension
        LOGD(EDEN_DRIVER, "Unknown dimension, unknown rank is detected... skip it\n");
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();
    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    auto inputOperandIndex = androidOperation.inputs[0];
    auto width = model.operands[inputOperandIndex].dimensions[W_NHWC];
    auto height = model.operands[inputOperandIndex].dimensions[H_NHWC];

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(DepthwiseConv2DOptions);
    bufferForOptions->addr = new uint32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    DepthwiseConv2DOptions* options = new (bufferForOptions->addr) DepthwiseConv2DOptions();

    int32_t paddingLeft=0, paddingRight=0, paddingTop=0, paddingBottom=0;
    int32_t paddingImplicit = 0;
    int32_t strideWidth, strideHeight;
    int32_t filterWidth, filterHeight;
    int32_t activation;
    int32_t depthMultiplier;
    bool NCHWDataLayout = false;
    int32_t dilationWidthFactor = 1;
    int32_t dilationHeightFactor = 1;

    /*
     Inputs (explicit padding):

     0: A 4-D tensor, of shape [batches, height, width, depth_in], specifying the input.
     1: A 4-D tensor, of shape [1, filter_height, filter_width, depth_out], specifying the filter.
        For tensor of type ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL the channel dimension (extraParams.channelQuant.channelDim) must be set to 3.
     2: A 1-D tensor, of shape [depth_out], specifying the bias.
        For input tensor of type ANEURALNETWORKS_TENSOR_FLOAT32 or ANEURALNETWORKS_TENSOR_FLOAT16, the bias must be of the same type.
        For filter tensor of ANEURALNETWORKS_TENSOR_QUANT8_ASYMM, the bias should be of ANEURALNETWORKS_TENSOR_INT32, with zeroPoint of 0 and bias_scale == input_scale * filter_scale.
        For filter tensor of ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL, the bias should be of ANEURALNETWORKS_TENSOR_INT32, with zeroPoint of 0 and bias_scale of 0.
        The actual scale of each value 'i' is equal to bias_scale[i] = input_scale * filter_scale[i].
     3: An ANEURALNETWORKS_INT32 scalar, specifying the padding on the left, in the width dimension.
     4: An ANEURALNETWORKS_INT32 scalar, specifying the padding on the right, in the width dimension.
     5: An ANEURALNETWORKS_INT32 scalar, specifying the padding on the top, in the height dimension.
     6: An ANEURALNETWORKS_INT32 scalar, specifying the padding on the bottom, in the height dimension.
     7: An ANEURALNETWORKS_INT32 scalar, specifying the stride when walking through input in the width dimension.
     8: An ANEURALNETWORKS_INT32 scalar, specifying the stride when walking through input in the height dimension.
     9: An ANEURALNETWORKS_INT32 scalar, specifying the depthwise multiplier.
     10: An ANEURALNETWORKS_INT32 scalar, and has to be one of the FuseCode values. Specifies the activation to invoke on the result.
     11: An optional ANEURALNETWORKS_BOOL scalar, default to false. Set to true to specify NCHW data layout for input0 and output0. Available since API level 29.
     12: An optional ANEURALNETWORKS_INT32 scalar, specifying the dilation factor for width. Defaults to 1.
         If set to k > 1, there will be k-1 skipped cells between each filter element on width dimension.
         If this input is set, input 13 (dilation factor for height) must be specified as well. Available since API level 29.
     13: An optional ANEURALNETWORKS_INT32 scalar, specifying the dilation factor for height.
         Defaults to 1. If set to k > 1, there will be k-1 skipped cells between each filter element on height dimension.
         If this input is set, input 12 (dilation factor for width) must be specified as well. Available since API level 29.

     Inputs (implicit padding):

     0: A 4-D tensor, of shape [batches, height, width, depth_in], specifying the input.
     1: A 4-D tensor, of shape [1, filter_height, filter_width, depth_out], specifying the filter.
     2: A 1-D tensor, of shape [depth_out], specifying the bias.
        For input tensor of type ANEURALNETWORKS_TENSOR_FLOAT32 or ANEURALNETWORKS_TENSOR_FLOAT16, the bias must be of the same type.
        For filter tensor of ANEURALNETWORKS_TENSOR_QUANT8_ASYMM, the bias should be of ANEURALNETWORKS_TENSOR_INT32, with zeroPoint of 0 and bias_scale == input_scale * filter_scale.
        For filter tensor of ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL, the bias should be of ANEURALNETWORKS_TENSOR_INT32, with zeroPoint of 0 and bias_scale of 0.
        The actual scale of each value 'i' is equal to bias_scale[i] = input_scale * filter_scale[i].
     3: An ANEURALNETWORKS_INT32 scalar, specifying the implicit padding scheme, has to be one of the PaddingCode values.
     4: An ANEURALNETWORKS_INT32 scalar, specifying the stride when walking through input in the width dimension.
     5: An ANEURALNETWORKS_INT32 scalar, specifying the stride when walking through input in the height dimension.
     6: An ANEURALNETWORKS_INT32 scalar, specifying the depthwise multiplier.
     7: An ANEURALNETWORKS_INT32 scalar, and has to be one of the FuseCode values. Specifies the activation to invoke on the result.
     8: An optional ANEURALNETWORKS_BOOL scalar, default to false. Set to true to specify NCHW data layout for input0 and output0. Available since API level 29.
     9: An optional ANEURALNETWORKS_INT32 scalar, specifying the dilation factor for width.
        Defaults to 1. If set to k > 1, there will be k-1 skipped cells between each filter element on width dimension.
        If this input is set, input 10 (dilation factor for height) must be specified as well. Available since API level 29.
     10: An optional ANEURALNETWORKS_INT32 scalar, specifying the dilation factor for height.
         Defaults to 1. If set to k > 1, there will be k-1 skipped cells between each filter element on height dimension.
         If this input is set, input 9 (dilation factor for width) must be specified as well. Available since API level 29.
    */
    LOGD(EDEN_DRIVER, "numOfInputs: %zu\n", numOfInputs);

    if ((numOfInputs >= 9 && model.operands[androidOperation.inputs[8]].type == OperandType::BOOL) || numOfInputs == 8) {
        paddingImplicit = getValue<int32_t>(model, androidOperation, 3);
        strideWidth = getValue<int32_t>(model, androidOperation, 4);
        strideHeight = getValue<int32_t>(model, androidOperation, 5);
        depthMultiplier = getValue<int32_t>(model, androidOperation, 6);
        activation = getValue<int32_t>(model, androidOperation, 7);
        if (numOfInputs >= 9) {
            NCHWDataLayout = getValue<bool>(model, androidOperation, 8);
        }
        if (numOfInputs == 11) {
            dilationWidthFactor = getValue<int32_t>(model, androidOperation, 9);
            dilationHeightFactor = getValue<int32_t>(model, androidOperation, 10);
        }

        if (NCHWDataLayout) {
            width = model.operands[inputOperandIndex].dimensions[3];
            height = model.operands[inputOperandIndex].dimensions[2];
        } else {
            width = model.operands[inputOperandIndex].dimensions[2];
            height = model.operands[inputOperandIndex].dimensions[1];
        }

        auto filterIndex = androidOperation.inputs[1];
        auto filterOperand = model.operands[filterIndex];
        filterWidth = filterOperand.dimensions[W_NHWC];
        filterHeight = filterOperand.dimensions[H_NHWC];

        filterWidth = dilationWidthFactor * (filterWidth - 1) + 1;
        filterHeight = dilationHeightFactor * (filterHeight - 1) + 1;

        getPadding(width, strideWidth, filterWidth, paddingImplicit, &paddingLeft, &paddingRight);
        getPadding(height, strideHeight, filterHeight, paddingImplicit, &paddingTop, &paddingBottom);

    } else {
        paddingLeft = getValue<int32_t>(model, androidOperation, 3);
        paddingRight = getValue<int32_t>(model, androidOperation, 4);
        paddingTop = getValue<int32_t>(model, androidOperation, 5);
        paddingBottom = getValue<int32_t>(model, androidOperation, 6);
        strideWidth = getValue<int32_t>(model, androidOperation, 7);
        strideHeight = getValue<int32_t>(model, androidOperation, 8);
        depthMultiplier = getValue<int32_t>(model, androidOperation, 9);
        activation = getValue<int32_t>(model, androidOperation, 10);
        if (numOfInputs >= 12) {
            NCHWDataLayout = getValue<bool>(model, androidOperation, 11);
        }
        if (numOfInputs == 14) {
            dilationWidthFactor = getValue<int32_t>(model, androidOperation, 12);
            dilationHeightFactor = getValue<int32_t>(model, androidOperation, 13);
        }
    }

    options->padLeft = paddingLeft;
    options->padRight = paddingRight;
    options->padTop = paddingTop;
    options->padBottom = paddingBottom;

    options->strideWidth = strideWidth;
    options->strideHeight = strideHeight;
    options->depthMultiplier = depthMultiplier;

    options->NCHWDataLayout = NCHWDataLayout;
    options->dilationWidthFactor = dilationWidthFactor;
    options->dilationHeightFactor = dilationHeightFactor;

    options->fusedActivation = static_cast<FusedActivation>(getActivationFn(activation));

    newOperandForOptions->isNCHW = options->NCHWDataLayout;

    LOGD(EDEN_DRIVER, "paddingLeft=%d, paddingRight=%d\n", paddingLeft, paddingRight);
    LOGD(EDEN_DRIVER, "paddingTop=%d, paddingBottom=%d\n", paddingTop, paddingBottom);
    LOGD(EDEN_DRIVER, "strideWidth=%d, strideHeight=%d\n", strideWidth, strideHeight);
    LOGD(EDEN_DRIVER, "depthMultiplier=%d\n", depthMultiplier);
    LOGD(EDEN_DRIVER, "activation=%d\n", activation);
    LOGD(EDEN_DRIVER, "NCHWDataLayout=%d\n", NCHWDataLayout);
    LOGD(EDEN_DRIVER, "dilationWidthFactor=%d, dilationHeightFactor=%d\n", dilationWidthFactor, dilationHeightFactor);

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configDepthToSpace(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand**  configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_DEPTH_TO_SPACE_1_1 && numOfInputs != NUM_OF_INPUTS_ON_DEPTH_TO_SPACE_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(DepthSpaceOptions);
    bufferForOptions->addr = new int32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    DepthSpaceOptions* options = new(bufferForOptions->addr) DepthSpaceOptions();
    bool NCHWDataLayout = false;

    /*
     0: A 4-D tensor, of shape [batches, height, width, depth_in], specifying the input.
     1: An ANEURALNETWORKS_INT32 scalar, specifying the block_size.
        block_size must be >=1 and block_size * block_size must be a divisor of the input depth.
     2: An optional ANEURALNETWORKS_BOOL scalar, default to false.
        Set to true to specify NCHW data layout for input0 and output0. Available since API level 29.
     */
    options->blockSize = getValue<int32_t>(model, androidOperation, 1);
    if (numOfInputs == NUM_OF_INPUTS_ON_DEPTH_TO_SPACE_1_1) {
        options->NCHWDataLayout = NCHWDataLayout;
    } else {
        options->NCHWDataLayout = getValue<bool>(model, androidOperation, 2);
    }

    newOperandForOptions->isNCHW = options->NCHWDataLayout;
    LOGD(EDEN_DRIVER, "blockSize=%d\n", options->blockSize);
    LOGD(EDEN_DRIVER, "NCHWDataLayout=%d\n", options->blockSize);

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configL2Normalization(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_L2_NORMALIZATION_1_1 && numOfInputs != NUM_OF_INPUTS_ON_L2_NORMALIZATION_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(L2NormalizationOptions);
    bufferForOptions->addr = new uint32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    L2NormalizationOptions* options = new (bufferForOptions->addr) L2NormalizationOptions();
    int32_t axis = -1;

    /*
     0: An n-D tensor, specifying the tensor to be normalized.
     1: An optional ANEURALNETWORKS_INT32 scalar, default to -1, specifying the dimension normalization would be performed on.
        Negative index is used to specify axis from the end (e.g. -1 for the last axis). Must be in the range [-n, n). Available since API level 29.
     */
    auto operandId = androidOperation.inputs[0];
    auto operand = model.operands[operandId];
    int32_t numOfInputDims = operand.dimensions.size();
    if (numOfInputs == NUM_OF_INPUTS_ON_L2_NORMALIZATION_1_2) {
        axis = getValue<int32_t>(model, androidOperation, 1);
        if (!(-numOfInputDims <= axis && axis < numOfInputDims)) {
            LOGE(EDEN_DRIVER, "axis(%d) is too small or too larger! set default value 1\n", axis);
            delete newOperandForOptions;
            delete[] reinterpret_cast<int32_t*>(bufferForOptions->addr);
            delete options;
            return;
        }
    }

    if (axis < 0) axis += numOfInputDims;
    if (numOfInputDims == 1) {
        options->axis = axis;
    } else if (numOfInputDims == 2) {
        options->axis = axis + 2;  /* VTS has open 2 axis, 0 for h, 1 for w */
    } else if (numOfInputDims == 3) {
        if (axis == H_NHWC || axis == W_NHWC) {
            options->axis = axis + 1;
        } else {
            options->axis = axis;
        }
    } else {
        if (axis == C_NHWC) {
            options->axis = C_NCHW;
        } else if (axis == W_NHWC) {
            options->axis = W_NCHW;
        } else if (axis == H_NHWC) {
            options->axis = H_NCHW;
        } else {
            options->axis = axis;
        }
    }

    options->numOfInputDims = numOfInputDims;
    LOGD(EDEN_DRIVER, "axis=%d\n", options->axis);
    LOGD(EDEN_DRIVER, "numOfInputDims=%d\n", options->numOfInputDims);

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configL2Pool2d(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_L2_POOL_2D_EXPLICIT_1_1 && numOfInputs != NUM_OF_INPUTS_ON_L2_POOL_2D_IMPLICIT_1_1 &&
        numOfInputs != NUM_OF_INPUTS_ON_L2_POOL_2D_EXPLICIT_1_2 && numOfInputs != NUM_OF_INPUTS_ON_L2_POOL_2D_IMPLICIT_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }

    // Check unknown dimension, unknown rank
    if ((model.operands[androidOperation.inputs[0]].dimensions.size() == 0) ||   // unknown rank
        (model.operands[androidOperation.inputs[0]].dimensions[W_NHWC] == 0) ||  // unknown dimension
        (model.operands[androidOperation.inputs[0]].dimensions[H_NHWC] == 0)) {  // unknown dimension
        LOGD(EDEN_DRIVER, "Unknown dimension, unknown rank is detected... skip it\n");
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    auto inputOperandIndex = androidOperation.inputs[0];
    auto width = model.operands[inputOperandIndex].dimensions[W_NHWC];
    auto height = model.operands[inputOperandIndex].dimensions[H_NHWC];

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(Pool2DOptions);
    bufferForOptions->addr = new uint32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    Pool2DOptions* options = new (bufferForOptions->addr) Pool2DOptions();

    int32_t paddingLeft, paddingRight, paddingTop, paddingBottom;
    int32_t paddingImplicit;
    int32_t strideWidth, strideHeight;
    int32_t filterWidth, filterHeight;
    int32_t activation;
    bool NCHWDataLayout = false;

    /*
     Inputs (explicit padding):

     0: A 4-D tensor, of shape [batches, height, width, depth], specifying the input.
     1: An ANEURALNETWORKS_INT32 scalar, specifying the padding on the left, in the width dimension.
     2: An ANEURALNETWORKS_INT32 scalar, specifying the padding on the right, in the width dimension.
     3: An ANEURALNETWORKS_INT32 scalar, specifying the padding on the top, in the height dimension.
     4: An ANEURALNETWORKS_INT32 scalar, specifying the padding on the bottom, in the height dimension.
     5: An ANEURALNETWORKS_INT32 scalar, specifying the stride when walking through input in the width dimension.
     6: An ANEURALNETWORKS_INT32 scalar, specifying the stride when walking through input in the height dimension.
     7: An ANEURALNETWORKS_INT32 scalar, specifying the filter width.
     8: An ANEURALNETWORKS_INT32 scalar, specifying the filter height.
     9: An ANEURALNETWORKS_INT32 scalar, and has to be one of the FuseCode values. Specifies the activation to invoke on the result.
     10: An optional ANEURALNETWORKS_BOOL scalar, default to false. Set to true to specify NCHW data layout for input0 and output0. Available since API level 29.
     Inputs (implicit padding):

     0: A 4-D tensor, of shape [batches, height, width, depth], specifying the input.
     1: An ANEURALNETWORKS_INT32 scalar, specifying the implicit padding scheme, has to be one of the PaddingCode values.
     2: An ANEURALNETWORKS_INT32 scalar, specifying the stride when walking through input in the width dimension.
     3: An ANEURALNETWORKS_INT32 scalar, specifying the stride when walking through input in the height dimension.
     4: An ANEURALNETWORKS_INT32 scalar, specifying the filter width.
     5: An ANEURALNETWORKS_INT32 scalar, specifying the filter height.
     6: An ANEURALNETWORKS_INT32 scalar, and has to be one of the FuseCode values. Specifies the activation to invoke on the result.
     7: An optional ANEURALNETWORKS_BOOL scalar, default to false. Set to true to specify NCHW data layout for input0 and output0. Available since API level 29.
     */
    LOGD(EDEN_DRIVER, "numOfInputs: %zu\n", numOfInputs);
    if (numOfInputs == NUM_OF_INPUTS_ON_L2_POOL_2D_EXPLICIT_1_1) {
        paddingLeft = getValue<int32_t>(model, androidOperation, 1);
        paddingRight = getValue<int32_t>(model, androidOperation, 2);
        paddingTop = getValue<int32_t>(model, androidOperation, 3);
        paddingBottom = getValue<int32_t>(model, androidOperation, 4);
        strideWidth = getValue<int32_t>(model, androidOperation, 5);
        strideHeight = getValue<int32_t>(model, androidOperation, 6);
        filterWidth = getValue<int32_t>(model, androidOperation, 7);
        filterHeight = getValue<int32_t>(model, androidOperation, 8);
        activation = getValue<int32_t>(model, androidOperation, 9);
    } else if (numOfInputs == NUM_OF_INPUTS_ON_L2_POOL_2D_IMPLICIT_1_1) {
        paddingImplicit = getValue<int32_t>(model, androidOperation, 1);
        strideWidth = getValue<int32_t>(model, androidOperation, 2);
        strideHeight = getValue<int32_t>(model, androidOperation, 3);
        filterWidth = getValue<int32_t>(model, androidOperation, 4);
        filterHeight = getValue<int32_t>(model, androidOperation, 5);
        activation = getValue<int32_t>(model, androidOperation, 6);

        getPadding(width, strideWidth, filterWidth, paddingImplicit, &paddingLeft, &paddingRight);
        getPadding(height, strideHeight, filterHeight, paddingImplicit, &paddingTop, &paddingBottom);
    } else if (numOfInputs == NUM_OF_INPUTS_ON_L2_POOL_2D_EXPLICIT_1_2) {
        paddingLeft = getValue<int32_t>(model, androidOperation, 1);
        paddingRight = getValue<int32_t>(model, androidOperation, 2);
        paddingTop = getValue<int32_t>(model, androidOperation, 3);
        paddingBottom = getValue<int32_t>(model, androidOperation, 4);
        strideWidth = getValue<int32_t>(model, androidOperation, 5);
        strideHeight = getValue<int32_t>(model, androidOperation, 6);
        filterWidth = getValue<int32_t>(model, androidOperation, 7);
        filterHeight = getValue<int32_t>(model, androidOperation, 8);
        activation = getValue<int32_t>(model, androidOperation, 9);
        NCHWDataLayout = getValue<bool>(model, androidOperation, 10);
    } else {
        paddingImplicit = getValue<int32_t>(model, androidOperation, 1);
        strideWidth = getValue<int32_t>(model, androidOperation, 2);
        strideHeight = getValue<int32_t>(model, androidOperation, 3);
        filterWidth = getValue<int32_t>(model, androidOperation, 4);
        filterHeight = getValue<int32_t>(model, androidOperation, 5);
        activation = getValue<int32_t>(model, androidOperation, 6);
        NCHWDataLayout = getValue<bool>(model, androidOperation, 7);

        if (NCHWDataLayout) {
            width = model.operands[inputOperandIndex].dimensions[W_NCHW];
            height = model.operands[inputOperandIndex].dimensions[H_NCHW];
        } else {
            width = model.operands[inputOperandIndex].dimensions[W_NHWC];
            height = model.operands[inputOperandIndex].dimensions[H_NHWC];
        }
        getPadding(width, strideWidth, filterWidth, paddingImplicit, &paddingLeft, &paddingRight);
        getPadding(height, strideHeight, filterHeight, paddingImplicit, &paddingTop, &paddingBottom);
    }

    options->padLeft = paddingLeft;
    options->padRight = paddingRight;
    options->padTop = paddingTop;
    options->padBottom = paddingBottom;

    options->strideWidth = strideWidth;
    options->strideHeight = strideHeight;
    options->kernelWidth = filterWidth;
    options->kernelHeight = filterHeight;
    options->NCHWDataLayout = NCHWDataLayout;

    options->fusedActivation = static_cast<FusedActivation>(getActivationFn(activation));

    newOperandForOptions->isNCHW = NCHWDataLayout;

    LOGD(EDEN_DRIVER, "paddingLeft=%d, paddingRight=%d\n", paddingLeft, paddingRight);
    LOGD(EDEN_DRIVER, "paddingTop=%d, paddingBottom=%d\n", paddingTop, paddingBottom);
    LOGD(EDEN_DRIVER, "strideWidth=%d, strideHeight=%d\n", strideWidth, strideHeight);
    LOGD(EDEN_DRIVER, "filterWidth=%d, filterHeight=%d\n", filterWidth, filterHeight);
    LOGD(EDEN_DRIVER, "activation=%d\n", activation);
    LOGD(EDEN_DRIVER, "NCHWDataLayout=%d\n", NCHWDataLayout);

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configLocalResponseNormalization(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_LOCAL_RESPONSE_NORMALIZATION_1_1 && numOfInputs != NUM_OF_INPUTS_ON_LOCAL_RESPONSE_NORMALIZATION_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(LocalResponseNormalizationOptions);
    bufferForOptions->addr = new uint32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    LocalResponseNormalizationOptions* options = new (bufferForOptions->addr) LocalResponseNormalizationOptions();
    int32_t axis = -1;

    /*
     0: A 4-D tensor, of shape [batches, height, width, depth], specifying the input.
     1: An ANEURALNETWORKS_INT32 scalar, specifying the radius of the normalization window.
     2: An ANEURALNETWORKS_FLOAT32 scalar, specifying the bias, must not be zero.
     3: An ANEURALNETWORKS_FLOAT32 scalar, specifying the scale factor, alpha.
     4: An ANEURALNETWORKS_FLOAT32 scalar, specifying the exponent, beta.
     5: An optional ANEURALNETWORKS_INT32 scalar, default to -1, specifying the dimension normalization would be performed on.
        Negative index is used to specify axis from the end (e.g. -1 for the last axis). Must be in the range [-n, n). Available since API level 29.
     */
    options->radius = getValue<int32_t>(model, androidOperation, 1);
    options->normRegion = EDEN_NORM_REGION_ACROSS_CHANNELS;  // default

    V1_2::Operand operand2 = model.operands[androidOperation.inputs[2]];
    V1_2::Operand operand3 = model.operands[androidOperation.inputs[3]];
    V1_2::Operand operand4 = model.operands[androidOperation.inputs[4]];
    if (operand2.type == OperandType::FLOAT16) {
        options->bias = getValue<_Float16>(model, androidOperation, 2);
    } else {
        options->bias = getValue<float>(model, androidOperation, 2);
    }

    if (operand2.type == OperandType::FLOAT16) {
        options->alpha = getValue<_Float16>(model, androidOperation, 3);
    } else {
        options->alpha = getValue<float>(model, androidOperation, 3);
    }

    if (operand2.type == OperandType::FLOAT16) {
        options->beta = getValue<_Float16>(model, androidOperation, 4);
    } else {
        options->beta = getValue<float>(model, androidOperation, 4);
    }

    auto operandId = androidOperation.inputs[0];
    auto operand = model.operands[operandId];
    int32_t numOfInputDims = operand.dimensions.size();
    if (numOfInputs == NUM_OF_INPUTS_ON_LOCAL_RESPONSE_NORMALIZATION_1_2) {
        axis = getValue<int32_t>(model, androidOperation, 5);
        if (!(-numOfInputDims <= axis && axis < numOfInputDims)) {
            LOGE(EDEN_DRIVER, "axis(%d) is too small or too larger! set default value 1\n", axis);
            delete newOperandForOptions;
            delete[] reinterpret_cast<int32_t*>(bufferForOptions->addr);
            delete options;
            return;
        }
    }

    if (axis < 0) axis += numOfInputDims;
    if (numOfInputDims == 1) {
        options->axis = axis;
    } else if (numOfInputDims == 2) {
        options->axis = axis + 2;  /* VTS has open 2 axis, 0 for h, 1 for w */
    } else if (numOfInputDims == 3) {
        if (axis == H_NHWC || axis == W_NHWC) {
            options->axis = axis + 1;
        } else {
            options->axis = axis;
        }
    } else {
        if (axis == C_NHWC) {
            options->axis = C_NCHW;
        } else if (axis == W_NHWC) {
            options->axis = W_NCHW;
        } else if (axis == H_NHWC) {
            options->axis = H_NCHW;
        } else {
            options->axis = axis;
        }
    }

    options->numOfInputDims = numOfInputDims;

    LOGD(EDEN_DRIVER, "numOfInputDims=%d\n", options->numOfInputDims);
    LOGD(EDEN_DRIVER, "axis=%d\n", options->axis);
    LOGD(EDEN_DRIVER, "radius=%d\n", options->radius);
    LOGD(EDEN_DRIVER, "bias=%f\n", options->bias);
    LOGD(EDEN_DRIVER, "alpha=%f\n", options->alpha);
    LOGD(EDEN_DRIVER, "beta=%f\n", options->beta);

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configMaxpool2d(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_MAX_POOL_2D_EXPLICIT_1_1 && numOfInputs != NUM_OF_INPUTS_ON_MAX_POOL_2D_IMPLICIT_1_1 &&
        numOfInputs != NUM_OF_INPUTS_ON_MAX_POOL_2D_EXPLICIT_1_2 && numOfInputs != NUM_OF_INPUTS_ON_MAX_POOL_2D_IMPLICIT_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }

    // Check unknown dimension, unknown rank
    if ((model.operands[androidOperation.inputs[0]].dimensions.size() == 0) ||   // unknown rank
        (model.operands[androidOperation.inputs[0]].dimensions[W_NHWC] == 0) ||  // unknown dimension
        (model.operands[androidOperation.inputs[0]].dimensions[H_NHWC] == 0)) {  // unknown dimension
        LOGD(EDEN_DRIVER, "Unknown dimension, unknown rank is detected... skip it\n");
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    auto inputOperandIndex = androidOperation.inputs[0];
    auto width = model.operands[inputOperandIndex].dimensions[W_NHWC];
    auto height = model.operands[inputOperandIndex].dimensions[H_NHWC];

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(Pool2DOptions);
    bufferForOptions->addr = new uint32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    Pool2DOptions* options = new(bufferForOptions->addr) Pool2DOptions();

    int32_t paddingLeft, paddingRight, paddingTop, paddingBottom;
    int32_t paddingImplicit;
    int32_t strideWidth, strideHeight;
    int32_t filterWidth, filterHeight;
    int32_t activation;
    bool NCHWDataLayout = false;

    /*
     Inputs (explicit padding):

     0: A 4-D tensor, of shape [batches, height, width, depth], specifying the input.
     1: An ANEURALNETWORKS_INT32 scalar, specifying the padding on the left, in the width dimension.
     2: An ANEURALNETWORKS_INT32 scalar, specifying the padding on the right, in the width dimension.
     3: An ANEURALNETWORKS_INT32 scalar, specifying the padding on the top, in the height dimension.
     4: An ANEURALNETWORKS_INT32 scalar, specifying the padding on the bottom, in the height dimension.
     5: An ANEURALNETWORKS_INT32 scalar, specifying the stride when walking through input in the width dimension.
     6: An ANEURALNETWORKS_INT32 scalar, specifying the stride when walking through input in the height dimension.
     7: An ANEURALNETWORKS_INT32 scalar, specifying the filter width.
     8: An ANEURALNETWORKS_INT32 scalar, specifying the filter height.
     9: An ANEURALNETWORKS_INT32 scalar, and has to be one of the FuseCode values. Specifies the activation to invoke on the result.
     10: An optional ANEURALNETWORKS_BOOL scalar, default to false. Set to true to specify NCHW data layout for input0 and output0. Available since API level 29.

     Inputs (implicit padding):

     0: A 4-D tensor, of shape [batches, height, width, depth], specifying the input.
     1: An ANEURALNETWORKS_INT32 scalar, specifying the implicit padding scheme, has to be one of the PaddingCode values.
     2: An ANEURALNETWORKS_INT32 scalar, specifying the stride when walking through input in the width dimension.
     3: An ANEURALNETWORKS_INT32 scalar, specifying the stride when walking through input in the height dimension.
     4: An ANEURALNETWORKS_INT32 scalar, specifying the filter width.
     5: An ANEURALNETWORKS_INT32 scalar, specifying the filter height.
     6: An ANEURALNETWORKS_INT32 scalar, and has to be one of the FuseCode values. Specifies the activation to invoke on the result.
     7: An optional ANEURALNETWORKS_BOOL scalar, default to false. Set to true to specify NCHW data layout for input0 and output0. Available since API level 29.
     */
    LOGD(EDEN_DRIVER, "numOfInputs: %zu\n", numOfInputs);
    if (numOfInputs == NUM_OF_INPUTS_ON_MAX_POOL_2D_EXPLICIT_1_1) {
        paddingLeft = getValue<int32_t>(model, androidOperation, 1);
        paddingRight = getValue<int32_t>(model, androidOperation, 2);
        paddingTop = getValue<int32_t>(model, androidOperation, 3);
        paddingBottom = getValue<int32_t>(model, androidOperation, 4);
        strideWidth = getValue<int32_t>(model, androidOperation, 5);
        strideHeight = getValue<int32_t>(model, androidOperation, 6);
        filterWidth = getValue<int32_t>(model, androidOperation, 7);
        filterHeight = getValue<int32_t>(model, androidOperation, 8);
        activation = getValue<int32_t>(model, androidOperation, 9);
    } else if (numOfInputs == NUM_OF_INPUTS_ON_MAX_POOL_2D_IMPLICIT_1_1) {
        paddingImplicit = getValue<int32_t>(model, androidOperation, 1);
        strideWidth = getValue<int32_t>(model, androidOperation, 2);
        strideHeight = getValue<int32_t>(model, androidOperation, 3);
        filterWidth = getValue<int32_t>(model, androidOperation, 4);
        filterHeight = getValue<int32_t>(model, androidOperation, 5);
        activation = getValue<int32_t>(model, androidOperation, 6);

        getPadding(width, strideWidth, filterWidth, paddingImplicit, &paddingLeft, &paddingRight);
        getPadding(height, strideHeight, filterHeight, paddingImplicit, &paddingTop, &paddingBottom);
    } else if (numOfInputs == NUM_OF_INPUTS_ON_MAX_POOL_2D_EXPLICIT_1_2) {
        paddingLeft = getValue<int32_t>(model, androidOperation, 1);
        paddingRight = getValue<int32_t>(model, androidOperation, 2);
        paddingTop = getValue<int32_t>(model, androidOperation, 3);
        paddingBottom = getValue<int32_t>(model, androidOperation, 4);
        strideWidth = getValue<int32_t>(model, androidOperation, 5);
        strideHeight = getValue<int32_t>(model, androidOperation, 6);
        filterWidth = getValue<int32_t>(model, androidOperation, 7);
        filterHeight = getValue<int32_t>(model, androidOperation, 8);
        activation = getValue<int32_t>(model, androidOperation, 9);
        NCHWDataLayout = getValue<bool>(model, androidOperation, 10);
    } else {
        paddingImplicit = getValue<int32_t>(model, androidOperation, 1);
        strideWidth = getValue<int32_t>(model, androidOperation, 2);
        strideHeight = getValue<int32_t>(model, androidOperation, 3);
        filterWidth = getValue<int32_t>(model, androidOperation, 4);
        filterHeight = getValue<int32_t>(model, androidOperation, 5);
        activation = getValue<int32_t>(model, androidOperation, 6);
        NCHWDataLayout = getValue<bool>(model, androidOperation, 7);

        getPadding(width, strideWidth, filterWidth, paddingImplicit, &paddingLeft, &paddingRight);
        getPadding(height, strideHeight, filterHeight, paddingImplicit, &paddingTop, &paddingBottom);
    }

    options->padLeft = paddingLeft;
    options->padRight = paddingRight;
    options->padTop = paddingTop;
    options->padBottom = paddingBottom;

    options->strideWidth = strideWidth;
    options->strideHeight = strideHeight;
    options->kernelWidth = filterWidth;
    options->kernelHeight = filterHeight;
    options->NCHWDataLayout = NCHWDataLayout;

    options->fusedActivation = static_cast<FusedActivation>(getActivationFn(activation));

    LOGD(EDEN_DRIVER, "paddingLeft=%d, paddingRight=%d\n", paddingLeft, paddingRight);
    LOGD(EDEN_DRIVER, "paddingTop=%d, paddingBottom=%d\n", paddingTop, paddingBottom);
    LOGD(EDEN_DRIVER, "strideWidth=%d, strideHeight=%d\n", strideWidth, strideHeight);
    LOGD(EDEN_DRIVER, "filterWidth=%d, filterHeight=%d\n", filterWidth, filterHeight);
    LOGD(EDEN_DRIVER, "activation=%d\n", activation);
    LOGD(EDEN_DRIVER, "NCHWDataLayout=%d\n", NCHWDataLayout);

    newOperandForOptions->isNCHW = NCHWDataLayout;
    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configMul(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_MUL_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(MulOptions);
    bufferForOptions->addr = new int32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    MulOptions* options = new (bufferForOptions->addr) MulOptions();

    /*
     0: A tensor.
     1: A tensor of the same OperandCode, and compatible dimensions as input0.
     2: An ANEURALNETWORKS_INT32 scalar, and has to be one of the FuseCode values. Specifies the activation to invoke on the result.
    */
    options->numOfCoeffs = 0;
    options->coeffs = nullptr;
    int32_t activation = getValue<int32_t>(model, androidOperation, 2);
    options->fusedActivation = static_cast<FusedActivation>(getActivationFn(activation));
    LOGD(EDEN_DRIVER, "activation=%d\n", activation);

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configSoftmax(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_SOFTMAX_1_1 && numOfInputs != NUM_OF_INPUTS_ON_SOFTMAX_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(SoftmaxOptions);
    bufferForOptions->addr = new int32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    SoftmaxOptions* options = new(bufferForOptions->addr) SoftmaxOptions();
    /*
     0: A 2-D or 4-D tensor, specifying the tensor to be reshaped.
     1: An ANEURALNETWORKS_FLOAT32 scalar, specifying the positive scaling factor for the exponent, beta.
     2: An optional ANEURALNETWORKS_INT32 scalar, default to -1, specifying the dimension the activation would be performed on.
        Negative index is used to specify axis from the end (e.g. -1 for the last axis). Must be in the range [-n, n). Available since API level 29.
     */
    if (model.operands[androidOperation.inputs[0]].type == V1_2::OperandType::TENSOR_FLOAT16) {
        options->beta = getValue<_Float16>(model, androidOperation, 1);
    } else {
        options->beta = getValue<float>(model, androidOperation, 1);
    }
    int32_t axis = -1;
    auto operandId = androidOperation.inputs[0];
    auto operand = model.operands[operandId];
    int32_t numOfInputDims = operand.dimensions.size();
    if (numOfInputs == NUM_OF_INPUTS_ON_SOFTMAX_1_2) {
        axis = getValue<int32_t>(model, androidOperation, 2);
    }
    if (axis < 0) {
        axis += numOfInputDims;
    }
    switch (numOfInputDims)
    {
    case 3:
        axis += 1;
        if (axis == 1) axis = 0;
        break;
    case 4:
        if (axis > 0) {
            axis += 1;
        }
        if (axis > 3) {
            axis -= 3;
        }
        break;
    default:
        break;
    }
    options->axis = axis;

    LOGD(EDEN_DRIVER, "axis=%d\n", options->axis);
    LOGD(EDEN_DRIVER, "beta=%f\n", options->beta);

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configSpaceToDepth(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_SPACE_TO_DEPTH_1_1 && numOfInputs != NUM_OF_INPUTS_ON_SPACE_TO_DEPTH_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(DepthSpaceOptions);
    bufferForOptions->addr = new int32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    DepthSpaceOptions* options = new(bufferForOptions->addr) DepthSpaceOptions();
    bool NCHWDataLayout = false;

    /*
     0: A 4-D tensor, of shape [batches, height, width, depth_in], specifying the input.
     1: An ANEURALNETWORKS_INT32 scalar, specifying the block_size.
        block_size must be >=1 and block_size must be a divisor of both the input height and width.
     2: An optional ANEURALNETWORKS_BOOL scalar, default to false.
        Set to true to specify NCHW data layout for input0 and output0. Available since API level 29.
     */
    options->blockSize = getValue<int32_t>(model, androidOperation, 1);
    LOGD(EDEN_DRIVER, "blockSize=%d\n", options->blockSize);

    if (numOfInputs == NUM_OF_INPUTS_ON_SPACE_TO_DEPTH_1_2) {
        options->NCHWDataLayout = getValue<bool>(model, androidOperation, 2);
        newOperandForOptions->isNCHW = getValue<bool>(model, androidOperation, 2);
    } else {
        options->NCHWDataLayout = NCHWDataLayout;
    }
    LOGD(EDEN_DRIVER, "NCHWDataLayout=%d\n", options->NCHWDataLayout);

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configStridedSlice(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_STRIDED_SLICE_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(StridedSliceOptions);
    bufferForOptions->addr = new int32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    StridedSliceOptions* options = new(bufferForOptions->addr) StridedSliceOptions();

    /*
     Inputs:

     0: An n-D tensor, specifying the tensor to be sliced.
     1: begin, a 1-D tensor of ANEURALNETWORKS_TENSOR_INT32.
        The starts of the dimensions of the input tensor to be sliced. The length must be of rank(input0).
     2: end, a 1-D tensor of ANEURALNETWORKS_TENSOR_INT32.
        The ends of the dimensions of the input tensor to be sliced. The length must be of rank(input0).
     3: strides, a 1-D tensor of ANEURALNETWORKS_TENSOR_INT32.
        The strides of the dimensions of the input tensor to be sliced. The length must be of rank(input0). The entries must be non-zero.
     4: begin_mask, an ANEURALNETWORKS_INT32 scalar.
        If the ith bit of begin_mask is set, begin[i] is ignored and the fullest possible range in that dimension is used instead.
     5: end_mask, an ANEURALNETWORKS_INT32 scalar.
        If the ith bit of end_mask is set, end[i] is ignored and the fullest possible range in that dimension is used instead.
     6: shrink_axis_mask, an ANEURALNETWORKS_INT32 scalar.
        If the ith bit of shrink_axis_mask is set, the ith dimension specification shrinks the dimensionality by 1, taking on the value at index begin[i].
        In this case, the ith specification must define a slice of size 1, e.g. begin[i] = x, end[i] = x + 1.
    */
    options->beginMask = getValue<int32_t>(model, androidOperation, 4);
    options->endMask = getValue<int32_t>(model, androidOperation, 5);
    options->ellipsisMask = 0;  // default
    options->newAxisMask = 0;   // default
    options->shrinkAxisMask = getValue<int32_t>(model, androidOperation, 6);

    LOGD(EDEN_DRIVER, "beginMask=%d\n", options->beginMask);
    LOGD(EDEN_DRIVER, "endMask=%d\n", options->endMask);
    LOGD(EDEN_DRIVER, "ellipsisMask=%d\n", options->ellipsisMask);
    LOGD(EDEN_DRIVER, "newAxisMask=%d\n", options->newAxisMask);
    LOGD(EDEN_DRIVER, "shrinkAxisMask=%d\n", options->shrinkAxisMask);

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configLshProjection(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_LSH_PROJECTION_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }

    // Check unknown dimension, unknown rank
    if ((model.operands[androidOperation.inputs[0]].dimensions.size() == 0) ||  // unknown rank
        (model.operands[androidOperation.inputs[0]].dimensions[0] == 0)) {      // unknown dimension
        LOGD(EDEN_DRIVER, "Unknown dimension, unknown rank is detected... skip it\n");
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(LshProjectionOptions);
    bufferForOptions->addr = new int32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    LshProjectionOptions* options = new(bufferForOptions->addr) LshProjectionOptions();

    /*
     0: Hash functions. Dim.size == 2, DataType: Float. Tensor[0].Dim[0]: Number of hash functions.
        Tensor[0].Dim[1]: Number of projected output bits generated by each hash function.
        If the projection type is Sparse: Tensor[0].Dim[1] + ceil(log2(Tensor[0].Dim[0])) <= 32
     1: Input. Dim.size >= 1, no restriction on DataType.
     2: Weight. Optional. Dim.size == 1, DataType: Float.
        If not set, each input element is considered to have the same weight of 1.0. Tensor[1].Dim[0] == Tensor[2].Dim[0]
     3: Type: Sparse: Value LSHProjectionType_SPARSE(=3) (since API level 29).
        Computed bit vector is considered to be sparse. Each output element is an int32 made up of multiple bits computed from hash functions.
        NOTE: To avoid collisions across hash functions, an offset value of k * (1 << Tensor[0].Dim[1]) will be added to each signature,
              where k is the index of the hash function.Value LSHProjectionType_SPARSE_DEPRECATED(=1).
              Legacy behavior that does not include the offset value.Dense: Value LSHProjectionType_DENSE(=2).
              Computed bit vector is considered to be dense. Each output element represents a bit and can take the value of either 0 or 1.
    */
    /*
     * @todo Currently, there's no way to load options for LSH_PROJECTION on Android NN.
     */
    //float* hashInfo = getPtr<float*>(model, androidOperation, 0);
    auto operandId = androidOperation.inputs[0];
    auto dimensions = model.operands[operandId].dimensions;
    options->numOfHashFunctions = dimensions[0];
    options->numOfSeedsPerHashFunction = 0;
    options->seeds = nullptr;
    options->numOfWeight = 0;
    options->weights = nullptr;
    options->lshType = getValue<int32_t>(model, androidOperation, 3);
    LOGD(EDEN_DRIVER, "lshType=%d\n", options->lshType);

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configReshape(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_RESHAPE_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }

    // Check unknown dimension, unknown rank
    if (model.operands[androidOperation.inputs[0]].dimensions.size() == 0) {  // unknown rank
        LOGD(EDEN_DRIVER, "Unknown dimension, unknown rank is detected... skip it\n");
        return;
    }

    /*
     0: A tensor, specifying the tensor to be reshaped.
     1: A 1-D tensor of ANEURALNETWORKS_TENSOR_INT32, defining the shape of the output tensor.
        The number of elements implied by shape must be the same as the number of elements in the input tensor.
     */
    sp<IMemory> sp;
    const ModelInfo* modelInfo = nullptr;
    int32_t* reshapeDim = getPtr<int32_t*>(model, androidOperation, 1, modelInfo, sp);
    auto operandId = androidOperation.inputs[1];
    auto& dims = model.operands[operandId].dimensions;
    size_t rank = dims.size();

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(ReshapeOptions);
    bufferForOptions->addr = new int32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    ReshapeOptions* options = new(bufferForOptions->addr) ReshapeOptions();

    if (reshapeDim == nullptr) {
        LOGE(EDEN_DRIVER, "Fail to load shape.");
        options->numOfDims = 1;
        std::unique_ptr<int32_t[]> spDims(new int32_t[options->numOfDims]);
        options->dims = std::move(spDims);
        options->dims[0] = 1;
    } else {
        size_t dimNum = 1;
        for (size_t i = 0; i < rank; i++)
            dimNum *= dims[i];
        options->numOfDims = dimNum;
        LOGD(EDEN_DRIVER, "options->numOfDims : %d\n", options->numOfDims);
        std::unique_ptr<int32_t[]> spDims(new int32_t[options->numOfDims]);
        options->dims = std::move(spDims);
        for (int32_t idx = 0; idx < options->numOfDims; idx++) {
            options->dims[idx] = reshapeDim[idx];
            LOGD(EDEN_DRIVER, "dims[%d] = %d\n", idx, options->dims[idx]);
        }
    }
    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configReshape(const V1_2::Model& model, const V1_2::Operation& androidOperation, const ModelInfo* modelInfo, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_RESHAPE_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }

    // Check unknown dimension, unknown rank
    if (model.operands[androidOperation.inputs[0]].dimensions.size() == 0) {  // unknown rank
        LOGD(EDEN_DRIVER, "Unknown dimension, unknown rank is detected... skip it\n");
        return;
    }

    /*
     0: A tensor, specifying the tensor to be reshaped.
     1: A 1-D tensor of ANEURALNETWORKS_TENSOR_INT32, defining the shape of the output tensor.
        The number of elements implied by shape must be the same as the number of elements in the input tensor.
     */
    sp<IMemory> sp;
    int32_t* reshapeDim = getPtr<int32_t*>(model, androidOperation, 1, modelInfo, sp);
    auto operandId = androidOperation.inputs[1];
    auto& dims = model.operands[operandId].dimensions;
    size_t rank = dims.size();

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(ReshapeOptions);
    bufferForOptions->addr = new int32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    ReshapeOptions* options = new(bufferForOptions->addr) ReshapeOptions();

    if (reshapeDim == nullptr) {
        LOGE(EDEN_DRIVER, "Fail to load shape.");
        options->numOfDims = 1;
        std::unique_ptr<int32_t[]> spDims(new int32_t[options->numOfDims]);
        options->dims = std::move(spDims);
        options->dims[0] = 1;
    } else {
        size_t dimNum = 1;
        for (size_t i = 0; i < rank; i++)
            dimNum *= dims[i];
        options->numOfDims = dimNum;
        LOGD(EDEN_DRIVER, "options->numOfDims : %d\n", options->numOfDims);
        std::unique_ptr<int32_t[]> spDims(new int32_t[options->numOfDims]);
        options->dims = std::move(spDims);
        for (int32_t idx = 0; idx < options->numOfDims; idx++) {
            options->dims[idx] = reshapeDim[idx];
            LOGD(EDEN_DRIVER, "dims[%d] = %d\n", idx, options->dims[idx]);
        }
    }
    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configResizeBilinear(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_RESIZE_BILINEAR_1_1 && numOfInputs != NUM_OF_INPUTS_ON_RESIZE_BILINEAR_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(ResizeBilinearOptions);
    bufferForOptions->addr = new uint32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    ResizeBilinearOptions* options = new(bufferForOptions->addr) ResizeBilinearOptions();
    bool NCHWDataLayout = false;

    if (numOfInputs == NUM_OF_INPUTS_ON_RESIZE_BILINEAR_1_2) {
        options->NCHWDataLayout = getValue<bool>(model, androidOperation, 3);
    } else {
        options->NCHWDataLayout = NCHWDataLayout;
    }
    LOGD(EDEN_DRIVER, "NCHWDataLayout=%d\n", options->NCHWDataLayout);

    newOperandForOptions->isNCHW = options->NCHWDataLayout;

    /*
     Both resizing by shape and resizing by scale are supported.

     Inputs (resizing by shape):
     0: A 4-D tensor, of shape [batches, height, width, depth], specifying the input.
     1: An ANEURALNETWORKS_INT32 scalar, specifying the output height of the output tensor.
     2: An ANEURALNETWORKS_INT32 scalar, specifying the output width of the output tensor.
     3: An optional ANEURALNETWORKS_BOOL scalar, default to false. Set to true to specify NCHW data layout for input0 and output0. Available since API level 29.

     Inputs (resizing by scale, since API level 29):
     0: A 4-D tensor, of shape [batches, height, width, depth], specifying the input. Zero batches is supported for this tensor.
     1: A scalar, specifying width_scale, the scaling factor of the width dimension from the input tensor to the output tensor.
     The output width is calculated as new_width = floor(width * width_scale).
     The scalar must be of ANEURALNETWORKS_FLOAT16 if input0 is of ANEURALNETWORKS_TENSOR_FLOAT16 and of ANEURALNETWORKS_FLOAT32 otherwise.
     2: A scalar, specifying height_scale, the scaling factor of the height dimension from the input tensor to the output tensor.
     The output height is calculated as new_height = floor(height * height_scale).
     The scalar must be of ANEURALNETWORKS_FLOAT16 if input0 is of ANEURALNETWORKS_TENSOR_FLOAT16 and of ANEURALNETWORKS_FLOAT32 otherwise.
     3: An optional ANEURALNETWORKS_BOOL scalar, default to false. Set to true to specify NCHW data layout for input0 and output0.
     */

    // check 2nd operandId's type
    std::string type = getAndroidNNOperandTypeName(model.operands[androidOperation.inputs[1]].type);
    if (type.find("INT32") != std::string::npos) {
        options->outputWidth = getValue<int32_t>(model, androidOperation, 1);
        options->outputHeight = getValue<int32_t>(model, androidOperation, 2);
    } else {
        auto inputOperandIndex = androidOperation.inputs[0];
        int width;
        int height;
        if (options->NCHWDataLayout) {
            width = model.operands[inputOperandIndex].dimensions[W_NCHW];
            height = model.operands[inputOperandIndex].dimensions[H_NCHW];
        } else {
            width = model.operands[inputOperandIndex].dimensions[W_NHWC];
            height = model.operands[inputOperandIndex].dimensions[H_NHWC];
        }

        float widthScale = 0.0;
        float heightScale = 0.0;
        if (model.operands[androidOperation.inputs[1]].type == OperandType::FLOAT32) {
            widthScale = getValue<float>(model, androidOperation, 1);
            heightScale = getValue<float>(model, androidOperation, 2);
        } else if (model.operands[androidOperation.inputs[1]].type == OperandType::FLOAT16) {
            widthScale = getValue<_Float16>(model, androidOperation, 1);
            heightScale = getValue<_Float16>(model, androidOperation, 2);
        }
        int32_t outputWidth = floor(width * widthScale);
        int32_t outputHeight = floor(height * heightScale);
        options->outputWidth = outputWidth;
        options->outputHeight = outputHeight;
    }
    /* ANN do not have alignCorners param */
    options->alignCorners = 0;  // default
    LOGD(EDEN_DRIVER, "outputWidth=%d\n", options->outputWidth);
    LOGD(EDEN_DRIVER, "outputHeight=%d\n", options->outputHeight);

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configLSTM(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_LSTM_1_1 && numOfInputs != NUM_OF_INPUTS_ON_LSTM_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(LSTMOptions);
    bufferForOptions->addr = new uint32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    LSTMOptions* options = new(bufferForOptions->addr) LSTMOptions();

    /*
     20: The activation function ( $g$). A value indicating the activation function:
         0: None;
         1: Relu;
         3: Relu6;
         4: Tanh;
         6: Sigmoid.
     21: The clipping threshold ( $t_{cell}$) for the cell state, such that values are bound within [-cell_clip, cell_clip].
         If set to 0.0 then clipping is disabled. Until API level 29 this scalar must be of type ANEURALNETWORKS_FLOAT32.
         Since API level 29, if all the input tensors have type ANEURALNETWORKS_TENSOR_FLOAT32,this scalar must be of the type ANEURALNETWORKS_FLOAT32,
         otherwise if all the input tensors have the type ANEURALNETWORKS_TENSOR_FLOAT16, this scalar must be of type ANEURALNETWORKS_FLOAT16.
     22: The clipping threshold ( $t_{proj}$) for the output from the projection layer, such that values are bound within [-proj_clip, proj_clip].
         If set to 0.0 then clipping is disabled. Until API level 29 this scalar must be of type ANEURALNETWORKS_FLOAT32.
         Since API level 29, if all the input tensors have type ANEURALNETWORKS_TENSOR_FLOAT32, this scalar must be of the type ANEURALNETWORKS_FLOAT32,
         otherwise if all the input tensors have the type ANEURALNETWORKS_TENSOR_FLOAT16, this scalar must be of type ANEURALNETWORKS_FLOAT16.
         Since API level 29 there are additional inputs to this op:
    */
    int32_t activation = getValue<int32_t>(model, androidOperation, 20);
    options->fusedActivation = static_cast<FusedActivation>(getActivationFn(activation));

    if (model.operands[androidOperation.inputs[21]].type == OperandType::FLOAT32) {
        options->cellClip = getValue<float>(model, androidOperation, 21);
        options->projClip = getValue<float>(model, androidOperation, 22);
    } else if (model.operands[androidOperation.inputs[1]].type == OperandType::FLOAT16) {
        options->cellClip = (float)getValue<_Float16>(model, androidOperation, 21);
        options->projClip = (float)getValue<_Float16>(model, androidOperation, 22);
    }

    if (numOfInputs == 27) {
        options->kernelType = EDEN_LSTM_KERNEL_TYPE_LAYER_NORM;
    } else {
        options->kernelType = EDEN_LSTM_KERNEL_TYPE_FULL;
    }

    LOGD(EDEN_DRIVER, "activation=%d\n", activation);
    LOGD(EDEN_DRIVER, "cellClip=%f\n", options->cellClip);
    LOGD(EDEN_DRIVER, "kernelType=%d\n", options->kernelType);

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configGenerateProposals(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_GENERATE_PROPOSALS_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(GenerateProposalOptions);
    bufferForOptions->addr = new uint32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 , TENSOR_INT32, TENSOR_QUANT8_ASYM or TENSOR_QUANT16_SYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    GenerateProposalOptions* options = new(bufferForOptions->addr) GenerateProposalOptions();
    if (model.operands[androidOperation.inputs[4]].type == OperandType::FLOAT16) {
        options->height_stride = getValue<_Float16>(model, androidOperation, 4);
        options->width_stride = getValue<_Float16>(model, androidOperation, 5);
        options->IoU_threshold  = getValue<_Float16>(model, androidOperation, 8);
        options->min_size  = getValue<_Float16>(model, androidOperation, 9);
    } else {
        options->height_stride = getValue<float>(model, androidOperation, 4);
        options->width_stride = getValue<float>(model, androidOperation, 5);
        options->IoU_threshold  = getValue<float>(model, androidOperation, 8);
        options->min_size  = getValue<float>(model, androidOperation, 9);
    }

    options->pre_nms = getValue<int>(model, androidOperation, 6);
    options->post_nms = getValue<int>(model, androidOperation, 7);
    options->layout  = getValue<bool>(model, androidOperation, 10);
    newOperandForOptions->isNCHW = getValue<bool>(model, androidOperation, 10);

    LOGD(EDEN_DRIVER, "height_stride=%f\n", options->height_stride);
    LOGD(EDEN_DRIVER, "width_stride=%f\n", options->width_stride);
    LOGD(EDEN_DRIVER, "pre_nms=%d\n", options->pre_nms);
    LOGD(EDEN_DRIVER, "post_nms=%d\n", options->post_nms);
    LOGD(EDEN_DRIVER, "IoU_threshold=%f\n", options->IoU_threshold);
    LOGD(EDEN_DRIVER, "min_size=%f\n", options->min_size);
    LOGD(EDEN_DRIVER, "layout=%d\n", options->layout);

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configResizeNearestNeighbor(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_RESIZE_NEAREST_NEIGHBOR_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(GenerateProposalOptions);
    bufferForOptions->addr = new uint32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 , TENSOR_INT32, TENSOR_QUANT8_ASYM or TENSOR_QUANT16_SYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    ResizeNearestNeighborOptions* options = new(bufferForOptions->addr) ResizeNearestNeighborOptions();
    if (model.operands[androidOperation.inputs[1]].type == OperandType::FLOAT32) {
        options->type = 1;
        options->width_scale = getValue<float>(model, androidOperation, 1);
        options->height_scale = getValue<float>(model, androidOperation, 2);
    } else if (model.operands[androidOperation.inputs[1]].type == OperandType::FLOAT16) {
        options->type = 1;
        options->width_scale = getValue<_Float16>(model, androidOperation, 1);
        options->height_scale = getValue<_Float16>(model, androidOperation, 2);
    } else {
        options->type = 0;
        options->width_scale = (float) (getValue<int>(model, androidOperation, 1));
        options->height_scale = (float) (getValue<int>(model, androidOperation, 2));
    }
    newOperandForOptions->isNCHW = getValue<bool>(model, androidOperation, 3);

    LOGD(EDEN_DRIVER, "type=%d\n", options->type);
    LOGD(EDEN_DRIVER, "height_scale=%f\n", options->height_scale);
    LOGD(EDEN_DRIVER, "width_scale=%f\n", options->width_scale);

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configBidirectionalSequenceLstm(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_BIDIRECTIONAL_SEQUENCE_LSTM_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(GenerateProposalOptions);
    bufferForOptions->addr = new uint32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 , TENSOR_INT32, TENSOR_QUANT8_ASYM or TENSOR_QUANT16_SYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    BidirectionalSequenceLSTMOptions* options = new(bufferForOptions->addr) BidirectionalSequenceLSTMOptions();
    int32_t activation = getValue<int32_t>(model, androidOperation, 48);
    options->fusedActivation = static_cast<FusedActivation>(getActivationFn(activation));
    options->cellClip = getValue<float>(model, androidOperation, 49);
    options->projClip = getValue<float>(model, androidOperation, 50);
    options->merge_outputs = getValue<bool>(model, androidOperation, 51);
    options->time_major = getValue<bool>(model, androidOperation, 52);

    LOGD(EDEN_DRIVER, "activation=%d\n", activation);
    LOGD(EDEN_DRIVER, "cellClip=%f\n", options->cellClip );
    LOGD(EDEN_DRIVER, "projClip=%f\n", options->projClip);
    LOGD(EDEN_DRIVER, "merge_outputs=%d\n", options->merge_outputs);
    LOGD(EDEN_DRIVER, "time_major=%d\n", options->time_major);

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configUnidirectionalSequenceLstm(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_UNIDIRECTIONAL_SEQUENCE_LSTM_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();
    newOperandForOptions->isNCHW = true;
    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(GenerateProposalOptions);
    bufferForOptions->addr = new uint32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 , TENSOR_INT32, TENSOR_QUANT8_ASYM or TENSOR_QUANT16_SYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    UnidirectionalSequenceLSTMOptions* options = new(bufferForOptions->addr) UnidirectionalSequenceLSTMOptions();
    int32_t activation = getValue<int32_t>(model, androidOperation, 20);
    options->fusedActivation = static_cast<FusedActivation>(getActivationFn(activation));
    options->cellClip = getValue<float>(model, androidOperation, 21);
    options->projClip = getValue<float>(model, androidOperation, 22);
    options->time_major = getValue<bool>(model, androidOperation, 23);

    LOGD(EDEN_DRIVER, "activation=%d\n", activation);
    LOGD(EDEN_DRIVER, "cellClip=%f\n", options->cellClip );
    LOGD(EDEN_DRIVER, "projClip=%f\n", options->projClip);
    LOGD(EDEN_DRIVER, "time_major=%d\n", options->time_major);

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configRoiAlign(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();

    if (numOfInputs != NUM_OF_INPUTS_ON_ROI_ALIGN_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(GenerateProposalOptions);
    bufferForOptions->addr = new uint32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 , TENSOR_INT32, TENSOR_QUANT8_ASYM or TENSOR_QUANT16_SYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }
    // Fill newOperandForOptions with parsed information
    RoiAlignOptions* options = new(bufferForOptions->addr) RoiAlignOptions();
    options->output_height = getValue<int>(model, androidOperation, 3);
    options->output_width = getValue<int>(model, androidOperation, 4);

    if (model.operands[androidOperation.inputs[5]].type == OperandType::FLOAT32) {
        options->stride_height = getValue<float>(model, androidOperation, 5);
        options->stride_width = getValue<float>(model, androidOperation, 6);
    } else {
        options->stride_height = getValue<_Float16>(model, androidOperation, 5);
        options->stride_width = getValue<_Float16>(model, androidOperation, 6);
    }

    options->ratio_height = getValue<int>(model, androidOperation, 7);
    options->ratio_width = getValue<int>(model, androidOperation, 8);
    bool layout = getValue<bool>(model, androidOperation, 9);
    newOperandForOptions->isNCHW = layout;

    LOGD(EDEN_DRIVER, "output_height=%d\n", options->output_height);
    LOGD(EDEN_DRIVER, "output_width=%d\n", options->output_width);
    LOGD(EDEN_DRIVER, "stride_height=%f\n", options->stride_height);
    LOGD(EDEN_DRIVER, "stride_width=%f\n", options->stride_width);
    LOGD(EDEN_DRIVER, "ratio_height=%d\n", options->ratio_height);
    LOGD(EDEN_DRIVER, "ratio_width=%d\n", options->ratio_width);
    LOGD(EDEN_DRIVER, "layout=%d\n", layout);

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configSqueeze(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_SQUEEZE_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }

    // Check unknown dimension, unknown rank
    if ((model.operands[androidOperation.inputs[0]].dimensions.size() == 0) ||  // unknown rank
        (model.operands[androidOperation.inputs[0]].dimensions[0] == 0)) {      // unknown dimension
        LOGD(EDEN_DRIVER, "Unknown dimension, unknown rank is detected... skip it\n");
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(SqueezeOptions);
    bufferForOptions->addr = new int32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    SqueezeOptions* options = new(bufferForOptions->addr) SqueezeOptions();

    /*
     0: An n-D tensor, the tensor to be squeezed.
     1: An optional 1-D tensor of ANEURALNETWORKS_TENSOR_INT32. The dimensions to squeeze.
        If specified only squeezes the dimensions listed. Otherwise, squeezes all dimensions.
        The dimension index starts at 0. An error must be reported if squeezing a dimension that is not 1.
     */
    if (numOfInputs < 2) {
        // Since operand #1 might be omitted since it is an Optional.
        LOGD(EDEN_DRIVER, "operand #1 is omitted, so set numOfSqueezeDims=0, squeezeDims=nullptr!\n");
        options->numOfSqueezeDims = 0;
        options->squeezeDims = nullptr;
    } else {
        sp<IMemory> sp;
        const ModelInfo* modelInfo = nullptr;
        int32_t* squeezeDim = getPtr<int32_t*>(model, androidOperation, 1, modelInfo, sp);
        auto operandId = androidOperation.inputs[1];
        auto dimensions = model.operands[operandId].dimensions;
        uint32_t rank = dimensions[0];

        options->numOfSqueezeDims = rank;

        std::unique_ptr<int32_t[]> spDims(new int32_t[options->numOfSqueezeDims]);
        options->squeezeDims = std::move(spDims);
        for (int32_t idx = 0; idx < options->numOfSqueezeDims; idx++) {
            options->squeezeDims[idx] = squeezeDim[idx];
            LOGD(EDEN_DRIVER, " squeeze value : %d\n", options->squeezeDims[idx]);
        }
    }

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configTranspose(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    auto numOfInputs = inputs.size();
    auto operandId = androidOperation.inputs[numOfInputs - 1];
    auto dimensions = model.operands[operandId].dimensions;
    uint32_t rank = dimensions[0];

    if (rank > 4) {
        LOGE(EDEN_DRIVER, "Oops, currently more than 4-rank is not supported...");
        rank = 4;

        // @todo Here we have two choise, just stop further processing or uses forcely reduced rank-4 number
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(TransposeOptions);
    bufferForOptions->addr = new int32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    TransposeOptions* options = new(bufferForOptions->addr) TransposeOptions();

    /*
     0: An n-D tensor, specifying the tensor to be transposed.
     1: An optional 1-D Tensor of ANEURALNETWORKS_TENSOR_INT32, the permutation of the dimensions of the input tensor.
     */
    sp<IMemory> sp;
    const ModelInfo* modelInfo = nullptr;
    int32_t* perms = getPtr<int32_t*>(model, androidOperation, numOfInputs - 1, modelInfo, sp);
    options->numOfPermIndex = rank;

    std::unique_ptr<int32_t[]> spDims(new int32_t[options->numOfPermIndex]);
    options->permIndex = std::move(spDims);
    for (int32_t idx = 0; idx < options->numOfPermIndex; idx++) {
        LOGD(EDEN_DRIVER, " perms value : %d\n", perms[idx]);
    }

    // Since perm array has indexes in NHWC, so options->permIndex[] should be modified to NCHW properly.
    int32_t permInNHWC[4];
    for (int32_t idx = 0; idx < options->numOfPermIndex; idx++) {
        permInNHWC[idx] = perms[idx];
    }
    for (int32_t idx = options->numOfPermIndex; idx < 4; idx++) {
        permInNHWC[idx] = idx;
    }

    for (int32_t idx = 0; idx < 4; idx++) {
        int32_t permIndex = permInNHWC[idx];
        if (permIndex == N_NHWC) {
            permIndex = N_NCHW;
        } else if (permIndex == C_NHWC) {
            permIndex = C_NCHW;
        } else if (permIndex == H_NHWC) {
            permIndex = H_NCHW;
        } else if (permIndex == W_NHWC) {
            permIndex = W_NCHW;
        }
        permInNHWC[idx] = permIndex;
    }
    switch (rank)  // prevent index out of range
    {
        case 2:
            options->permIndex[N_NCHW] = permInNHWC[N_NHWC];
            options->permIndex[C_NCHW] = permInNHWC[C_NHWC];
            break;
        case 3:
            options->permIndex[N_NCHW] = permInNHWC[N_NHWC];
            options->permIndex[C_NCHW] = permInNHWC[C_NHWC];
            options->permIndex[H_NCHW] = permInNHWC[H_NHWC];
            break;
        case 4:
            options->permIndex[N_NCHW] = permInNHWC[N_NHWC];
            options->permIndex[C_NCHW] = permInNHWC[C_NHWC];
            options->permIndex[H_NCHW] = permInNHWC[H_NHWC];
            options->permIndex[W_NCHW] = permInNHWC[W_NHWC];
            break;
        default:
            break;
    }

    LOGD(EDEN_DRIVER, "permIndex = {%d, %d, %d, %d}\n", options->permIndex[N_NCHW], options->permIndex[C_NCHW],
                                                        options->permIndex[H_NCHW], options->permIndex[W_NCHW]);

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configRNN(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_RNN_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(RNNOptions);
    bufferForOptions->addr = new uint32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    RNNOptions* options = new(bufferForOptions->addr) RNNOptions();

    /*
     0: input. A 2-D tensor of shape [batch_size, input_size], where batch_size corresponds to the batching dimension, and input_size is the size of the input.
     1: weights. A 2-D tensor of shape [num_units, input_size], where num_units corresponds to the number of units.
     2: recurrent_weights. A 2-D tensor of shape [num_units, num_units], with columns corresponding to the weights from each unit.
     3: bias. A 1-D tensor of shape [num_units].
     4: hidden state (in). A 2-D tensor of shape [batch_size, num_units].
     5: fused_activation_function. An optional FuseCode value indicating the activation function. If NONE is specified then it results in a linear activation.
     */
    int32_t activation = getValue<int32_t>(model, androidOperation, 5);
    options->fusedActivation = static_cast<FusedActivation>(getActivationFn(activation));
    LOGD(EDEN_DRIVER, "activation=%d\n", activation);

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configSVDF(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_SVDF_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(SVDFOptions);
    bufferForOptions->addr = new uint32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    SVDFOptions* options = new(bufferForOptions->addr) SVDFOptions();

    /*
     0: input. A 2-D tensor of shape [batch_size, input_size], where batch_size corresponds to the batching dimension, and input_size is the size of the input.
     1: weights_feature. A 2-D tensor of shape [num_units, input_size], where num_units corresponds to the number of units.
     2: weights_time. A 2-D tensor of shape [num_units, memory_size], where memory_size corresponds to the fixed-size of the memory.
     3: bias. An optional 1-D tensor of shape [num_units].
     4: state (in). A 2-D tensor of shape [batch_size, (memory_size - 1) * num_units * rank].
     5: rank. The rank of the SVD approximation.
     6: fused_activation_function. An optional FuseCode value indicating the activation function. If NONE is specified then it results in a linear activation.
     */
    int32_t rank = getValue<int32_t>(model, androidOperation, 5);
    int32_t activation = getValue<int32_t>(model, androidOperation, 6);
    options->rank = rank;
    options->fusedActivation = static_cast<FusedActivation>(getActivationFn(activation));

    LOGD(EDEN_DRIVER, "rank=%d\n", rank);
    LOGD(EDEN_DRIVER, "activation=%d\n", activation);

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configDiv(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_DIV_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;
    LOGD(EDEN_DRIVER, "Div new operand name : %s\n", newOperandForOptions->name.name);

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(DivOptions);
    bufferForOptions->addr = new uint32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    DivOptions* options = new (bufferForOptions->addr) DivOptions();

    /*
    0: A tensor.
    1: A tensor of the same OperandCode, and compatible dimensions as input0.
    2: An ANEURALNETWORKS_INT32 scalar, and has to be one of the FuseCode values. Specifies the activation to invoke on the result.
    */
    options->numOfCoeffs = 0;
    options->coeffs = nullptr;
    int32_t activation = getValue<int32_t>(model, androidOperation, 2);  // 2 to get FuseCode
    options->fusedActivation = static_cast<FusedActivation>(getActivationFn(activation));
    LOGD(EDEN_DRIVER, "activation=%d\n", activation);

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configSub(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_SUB_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu:\n", numOfInputs);
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;
    LOGD(EDEN_DRIVER, "Sub new operand name : %s\n", newOperandForOptions->name.name);

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(SubOptions);
    bufferForOptions->addr = new uint32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    SubOptions* options = new (bufferForOptions->addr) SubOptions();

    /*
    0: A tensor.
    1: A tensor of the same OperandCode, and compatible dimensions as input0.
    2: An ANEURALNETWORKS_INT32 scalar, and has to be one of the FuseCode values. Specifies the activation to invoke on the result.
    */
    options->numOfCoeffs = 0;
    options->coeffs = nullptr;
    int32_t activation = getValue<int32_t>(model, androidOperation, 2);  // 2 to get FuseCode
    options->fusedActivation = static_cast<FusedActivation>(getActivationFn(activation));
    LOGD(EDEN_DRIVER, "activation=%d\n", activation);

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configBatchToSpaceND(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_BATCH_TO_SPACE_ND_1_1 && numOfInputs != NUM_OF_INPUTS_ON_BATCH_TO_SPACE_ND_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu:\n", numOfInputs);
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;
    LOGD(EDEN_DRIVER, "Sub new operand name : %s\n", newOperandForOptions->name.name);

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(BatchToSpaceNDOptions);
    bufferForOptions->addr = new uint32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    BatchToSpaceNDOptions* options = new (bufferForOptions->addr) BatchToSpaceNDOptions();
    bool NCHWDataLayout = false;

    /*
    0: An n-D tensor, specifying the tensor to be reshaped
    1: A 1-D Tensor of ANEURALNETWORKS_TENSOR_INT32, the block sizes for each spatial dimension of the input tensor. All values must be >= 1.
    2: An optional ANEURALNETWORKS_BOOL scalar, default to false. Set to true to specify NCHW data layout for input0 and output0.
       Available since API level 29.
    */
    if (numOfInputs == NUM_OF_INPUTS_ON_BATCH_TO_SPACE_ND_1_2) {
        options->NCHWDataLayout = getValue<bool>(model, androidOperation, 2);
    } else {
        options->NCHWDataLayout = NCHWDataLayout;
    }
    LOGD(EDEN_DRIVER, "NCHWDataLayout=%d\n", options->NCHWDataLayout);

    newOperandForOptions->isNCHW = options->NCHWDataLayout;
    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configSpaceToBatchND(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_SPACE_TO_BATCH_ND_1_1 && numOfInputs != NUM_OF_INPUTS_ON_SPACE_TO_BATCH_ND_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu:\n", numOfInputs);
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;
    LOGD(EDEN_DRIVER, "Sub new operand name : %s\n", newOperandForOptions->name.name);

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(SpaceToBatchNDOptions);
    bufferForOptions->addr = new uint32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    SpaceToBatchNDOptions* options = new (bufferForOptions->addr) SpaceToBatchNDOptions();
    bool NCHWDataLayout = false;

    /*
    0: An n-D tensor, specifying the input.
    1: A 1-D Tensor of ANEURALNETWORKS_TENSOR_INT32, the block sizes for each spatial dimension of the input tensor. All values must be >= 1.
    2: A 2-D Tensor of ANEURALNETWORKS_TENSOR_INT32, the paddings for each spatial dimension of the input tensor. All values must be >= 0.
       The shape of the tensor must be {M, 2}, where M is the number of spatial dimensions.
       padding[i, 0] specifies the number of element to be padded in the front of dimension i.
       padding[i, 1] specifies the number of element to be padded after the end of dimension i.
    3: An optional ANEURALNETWORKS_BOOL scalar, default to false. Set to true to specify NCHW data layout for input0 and output0.
       Available since API level 29.
    */
    if (numOfInputs == NUM_OF_INPUTS_ON_SPACE_TO_BATCH_ND_1_2) {
        options->NCHWDataLayout = getValue<bool>(model, androidOperation, 3);
    } else {
        options->NCHWDataLayout = NCHWDataLayout;
    }
    LOGD(EDEN_DRIVER, "NCHWDataLayout=%d\n", options->NCHWDataLayout);

    newOperandForOptions->isNCHW = options->NCHWDataLayout;
    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configArgMax(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_ARGMAX_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu:\n", numOfInputs);
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(ArgmaxOptions);
    bufferForOptions->addr = new uint32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    ArgmaxOptions* options = new (bufferForOptions->addr) ArgmaxOptions();

    int32_t axisValue = getValue<int32_t>(model, androidOperation, 1);
    LOGD(EDEN_DRIVER, "axisValue=%d\n", axisValue);

    //assume that all the inputs' dimention of argmax are same
    auto operandId = androidOperation.inputs[0];
    auto operand = model.operands[operandId];
    size_t rank = operand.dimensions.size();

    if (axisValue < 0) axisValue += rank;

    if (rank == 1) {
        options->axis = axisValue;
    } else if (rank == 2) {
        options->axis = axisValue + 2;  /* VTS has open 2 axis, 0 for h, 1 for w */
    } else if (rank == 3) {
        if (axisValue == H_NHWC || axisValue == W_NHWC) {
            options->axis = axisValue + 1;
        } else {
            options->axis = axisValue;
        }
    } else if (rank == 4) {
        if (axisValue == C_NHWC) {
            options->axis = C_NCHW;
        } else if (axisValue == W_NHWC) {
            options->axis = W_NCHW;
        } else if (axisValue == H_NHWC) {
            options->axis = H_NCHW;
        } else {
            options->axis = axisValue;
        }
    } else {
        LOGD(EDEN_DRIVER, "rank=%zu is not support \n", rank);
    }

    LOGD(EDEN_DRIVER, "axis=%d\n", options->axis);

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configDeConv2d(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_DECONV_2D_EXPLICIT_1_2 &&
        numOfInputs != NUM_OF_INPUTS_ON_DECONV_2D_IMPLICIT_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }

    // Check unknown dimension, unknown rank
    if ((model.operands[androidOperation.inputs[0]].dimensions.size() == 0) ||   // unknown rank
        (model.operands[androidOperation.inputs[0]].dimensions[W_NHWC] == 0) ||  // unknown dimension
        (model.operands[androidOperation.inputs[0]].dimensions[H_NHWC] == 0)) {  // unknown dimension
        LOGD(EDEN_DRIVER, "Unknown dimension, unknown rank is detected... skip it\n");
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    auto outputOperandIndex = androidOperation.outputs[0];
    auto outputWidth = model.operands[outputOperandIndex].dimensions[W_NHWC];
    auto outputHeight = model.operands[outputOperandIndex].dimensions[H_NHWC];

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(Deconv2DOptions);
    bufferForOptions->addr = new uint32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    Deconv2DOptions* options = new (bufferForOptions->addr)Deconv2DOptions();

    int32_t paddingLeft, paddingRight, paddingTop, paddingBottom;
    int32_t paddingImplicit;
    int32_t strideWidth, strideHeight;
    int32_t filterWidth, filterHeight;
    int32_t activation;
    bool NCHWDataLayout = false;

    LOGD(EDEN_DRIVER, "numOfInputs: %zu\n", numOfInputs);
    if (numOfInputs == NUM_OF_INPUTS_ON_DECONV_2D_EXPLICIT_1_2) {
        paddingLeft = getValue<int32_t>(model, androidOperation, 3);
        paddingRight = getValue<int32_t>(model, androidOperation, 4);
        paddingTop = getValue<int32_t>(model, androidOperation, 5);
        paddingBottom = getValue<int32_t>(model, androidOperation, 6);
        strideWidth = getValue<int32_t>(model, androidOperation, 7);
        strideHeight = getValue<int32_t>(model, androidOperation, 8);
        activation = getValue<int32_t>(model, androidOperation, 9);
        NCHWDataLayout = getValue<bool>(model, androidOperation, 10);
    } else {
        paddingImplicit = getValue<int32_t>(model, androidOperation, 4);
        strideWidth = getValue<int32_t>(model, androidOperation, 5);
        strideHeight = getValue<int32_t>(model, androidOperation, 6);
        activation = getValue<int32_t>(model, androidOperation, 7);
        NCHWDataLayout = getValue<bool>(model, androidOperation, 8);

        if (NCHWDataLayout) {
            outputWidth = model.operands[outputOperandIndex].dimensions[W_NCHW];
            outputHeight = model.operands[outputOperandIndex].dimensions[H_NCHW];
        }

        auto filterIndex = androidOperation.inputs[1];
        auto filterOperand = model.operands[filterIndex];
        filterWidth = filterOperand.dimensions[W_NHWC];
        filterHeight = filterOperand.dimensions[H_NHWC];

        // paddingRight and paddingBottom in transpose conv may be less than 0 to resolve the
        // ambiguous output shape issue in the case of stride > 1.
        getPadding(outputWidth, strideWidth, filterWidth, paddingImplicit, &paddingLeft, &paddingRight, true);
        getPadding(outputHeight, strideHeight, filterHeight, paddingImplicit, &paddingTop, &paddingBottom, true);
    }

    options->padLeft = paddingLeft;
    options->padRight = paddingRight;
    options->padTop = paddingTop;
    options->padBottom = paddingBottom;

    options->strideWidth = strideWidth;
    options->strideHeight = strideHeight;

    options->fusedActivation = static_cast<FusedActivation>(getActivationFn(activation));
    options->group = 1;

    newOperandForOptions->isNCHW = NCHWDataLayout;

    LOGD(EDEN_DRIVER, "paddingLeft=%d, paddingRight=%d\n", paddingLeft, paddingRight);
    LOGD(EDEN_DRIVER, "paddingTop=%d, paddingBottom=%d\n", paddingTop, paddingBottom);
    LOGD(EDEN_DRIVER, "strideWidth=%d, strideHeight=%d\n", strideWidth, strideHeight);
    LOGD(EDEN_DRIVER, "activation=%d\n", activation);

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configUnidirectionalRNN(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_UNIDIRECTIONAL_RNN_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu:\n", numOfInputs);
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();
    newOperandForOptions->isNCHW = true;
    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(BidirectionalRNNOptions);
    bufferForOptions->addr = new uint32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    SequenceRNNOptions* options = new (bufferForOptions->addr) SequenceRNNOptions();
    int32_t activation = getValue<int32_t>(model, androidOperation, 5);
    options->fusedActivation = static_cast<FusedActivation>(getActivationFn(activation));
    options->time_major = getValue<bool>(model, androidOperation, 6);

    LOGD(EDEN_DRIVER, "fusedActivation=%d\n", activation);
    LOGD(EDEN_DRIVER, "time_major=%d\n", options->time_major);

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configBidriectionalRNN(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_BIDIRECTIONAl_RNN_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu:\n", numOfInputs);
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(BidirectionalRNNOptions);
    bufferForOptions->addr = new uint32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    BidirectionalRNNOptions* options = new (bufferForOptions->addr) BidirectionalRNNOptions();
    int32_t activation = getValue<int32_t>(model, androidOperation, 12);
    options->fusedActivation = static_cast<FusedActivation>(getActivationFn(activation));
    options->time_major = getValue<bool>(model, androidOperation, 13);
    options->merge_outputs = getValue<bool>(model, androidOperation, 14);

    LOGD(EDEN_DRIVER, "fusedActivation=%d\n", activation);
    LOGD(EDEN_DRIVER, "time_major=%d\n", options->time_major);
    LOGD(EDEN_DRIVER, "merge_outputs=%d\n", options->merge_outputs);

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configTopKV2(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_TOPK_V2_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu:\n", numOfInputs);
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(TopK_V2Options);
    bufferForOptions->addr = new uint32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    TopK_V2Options* options = new (bufferForOptions->addr) TopK_V2Options();
    options->k = getValue<int32_t>(model, androidOperation, 1);
    LOGD(EDEN_DRIVER, "k=%d\n", options->k);

    newOperandForOptions->isNCHW = true;
    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configTFLiteRoiPool(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_TFLITEROIPOOL_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu:\n", numOfInputs);
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(TFliteRoiPoolOptions);
    bufferForOptions->addr = new uint32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    TFliteRoiPoolOptions* options = new (bufferForOptions->addr) TFliteRoiPoolOptions();
    options->output_height = getValue<int32_t>(model, androidOperation, 3);
    options->output_width = getValue<int32_t>(model, androidOperation, 4);

    if (model.operands[androidOperation.inputs[5]].type == OperandType::FLOAT16) {
        options->height_stride = getValue<_Float16>(model, androidOperation, 5);
        options->width_stride = getValue<_Float16>(model, androidOperation, 6);
    } else {
        options->height_stride = getValue<float>(model, androidOperation, 5);
        options->width_stride = getValue<float>(model, androidOperation, 6);
    }
    bool layout = getValue<bool>(model, androidOperation, 7);
    newOperandForOptions->isNCHW = layout;

    LOGD(EDEN_DRIVER, "output_height=%d\n", options->output_height);
    LOGD(EDEN_DRIVER, "output_width=%d\n", options->output_width);
    LOGD(EDEN_DRIVER, "height_stride=%f\n", options->height_stride);
    LOGD(EDEN_DRIVER, "width_stride=%f\n", options->width_stride);
    LOGD(EDEN_DRIVER, "layout=%d\n", layout);

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configArgMin(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_ARGMIN_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu:\n", numOfInputs);
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(ArgminOptions);
    bufferForOptions->addr = new uint32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    ArgminOptions* options = new (bufferForOptions->addr) ArgminOptions();

    int32_t axisValue = getValue<int32_t>(model, androidOperation, 1);
    LOGD(EDEN_DRIVER, "axisValue=%d\n", axisValue);

    //assume that all the inputs' dimention of argmin are same
    auto operandId = androidOperation.inputs[0];
    auto operand = model.operands[operandId];
    size_t rank = operand.dimensions.size();

    if (axisValue < 0) axisValue += rank;

    if (rank == 1) {
        options->axis = axisValue;
    } else if (rank == 2) {
        options->axis = axisValue + 2;  /* VTS has open 2 axis, 0 for h, 1 for w */
    } else if (rank == 3) {
        if (axisValue == H_NHWC || axisValue == W_NHWC) {
            options->axis = axisValue + 1;
        } else {
            options->axis = axisValue;
        }
    } else if (rank == 4) {
        if (axisValue == C_NHWC) {
            options->axis = C_NCHW;
        } else if (axisValue == W_NHWC) {
            options->axis = W_NCHW;
        } else if (axisValue == H_NHWC) {
            options->axis = H_NCHW;
        } else {
            options->axis = axisValue;
        }
    } else {
        LOGD(EDEN_DRIVER, "rank=%zu is not support \n", rank);
    }

    LOGD(EDEN_DRIVER, "axis=%d\n", options->axis);

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configBoxWithNmsLimit(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_BOX_WITH_NMS_LIMIT_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu:\n", numOfInputs);
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(BoxWithNmsLimitOptions);
    bufferForOptions->addr = new uint32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    BoxWithNmsLimitOptions* options = new (bufferForOptions->addr) BoxWithNmsLimitOptions();
    if (model.operands[androidOperation.inputs[3]].type == OperandType::FLOAT32) {
        options->scoreThreshold = getValue<float>(model, androidOperation, 3);
        options->ioUThreshold = getValue<float>(model, androidOperation, 6);
        options->sigma = getValue<float>(model, androidOperation, 7);
        options->nmsScoreThreshold = getValue<float>(model, androidOperation, 8);
    } else if (model.operands[androidOperation.inputs[3]].type == OperandType::FLOAT16) {
        options->scoreThreshold = getValue<_Float16>(model, androidOperation, 3);
        options->ioUThreshold = getValue<_Float16>(model, androidOperation, 6);
        options->sigma = getValue<_Float16>(model, androidOperation, 7);
        options->nmsScoreThreshold = getValue<_Float16>(model, androidOperation, 8);
    }
    options->maxNumDetection = getValue<int32_t>(model, androidOperation, 4);
    options->nmsKernel = getValue<int32_t>(model, androidOperation, 5);

    LOGD(EDEN_DRIVER, "scoreThreshold=%f\n", options->scoreThreshold);
    LOGD(EDEN_DRIVER, "maxNumDetection=%d\n", options->maxNumDetection);
    LOGD(EDEN_DRIVER, "nmsKernel=%d\n", options->nmsKernel);
    LOGD(EDEN_DRIVER, "ioUThreshold=%f\n", options->ioUThreshold);
    LOGD(EDEN_DRIVER, "sigma=%f\n", options->sigma);
    LOGD(EDEN_DRIVER, "nmsScoreThreshold=%f\n", options->nmsScoreThreshold);

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}
void configGather(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_GATHER_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(GatherOptions);
    bufferForOptions->addr = new int32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    GatherOptions* options = new (bufferForOptions->addr) GatherOptions();

    /*
     0: An n-D tensor, specifying the input.
     1: An ANEURALNETWORKS_INT32 scalar.
    */
    int32_t axisValue = getValue<int32_t>(model, androidOperation, 1);

    auto inputOperandId = androidOperation.inputs[0];
    auto inputOperand = model.operands[inputOperandId];
    int32_t numOfInputDims = inputOperand.dimensions.size();
    if (!(-numOfInputDims <= axisValue && axisValue < numOfInputDims)) {
        LOGE(EDEN_DRIVER, "axis(%d) is too small or too larger! set default value 1\n", axisValue);
        delete newOperandForOptions;
        delete[] reinterpret_cast<int32_t*>(bufferForOptions->addr);
        delete options;
        return;
    }

    options->numOfInputDims = 4;
    options->numOfIndicesDims = 4; // fixed to 4 to match with Eden
    options->axis = 1; // fixed to 1. input dims are set to {outer_size, axis_size, inner_size, 1}

    newOperandForOptions->isNCHW = true;
    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configSplit(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_SPLIT_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(SplitOptions);
    bufferForOptions->addr = new int32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    SplitOptions* options = new (bufferForOptions->addr) SplitOptions();

    /*
     0: An n-D tensor, specifying the input.
     1: An ANEURALNETWORKS_INT32 scalar.
     2: An ANEURALNETWORKS_INT32 scalar.
    */
    int32_t axis = getValue<int32_t>(model, androidOperation, 1);
    int32_t num_splits = getValue<int32_t>(model, androidOperation, 2);
    options->axis = axis;
    options->num_outputs = num_splits;

    auto inputOperandId = androidOperation.inputs[0];
    auto inputOperand = model.operands[inputOperandId];
    size_t rank = inputOperand.dimensions.size();
    if (axis < 0) axis += rank;
    if (rank == 1) {
        options->axis = axis;
    } else if (rank == 2) {
        options->axis = axis + 2;
    } else if (rank == 3) {
        if (axis == H_NHWC || axis == W_NHWC) {
            options->axis = axis + 1;
        } else {
            options->axis = axis;
        }
    } else {
        if (axis == C_NHWC) {
            options->axis = C_NCHW;
        } else if (axis == W_NHWC) {
            options->axis = W_NCHW;
        } else if (axis == H_NHWC) {
            options->axis = H_NCHW;
        } else {
            options->axis = axis;
        }
    }

    int32_t numOfInputDims = inputOperand.dimensions.size();
    if (!(-numOfInputDims <= axis && axis < numOfInputDims)) {
        LOGE(EDEN_DRIVER, "axis(%d) is too small or too larger! set default value 1\n", axis);
        delete newOperandForOptions;
        delete[] reinterpret_cast<int32_t*>(bufferForOptions->addr);
        delete options;
        return;
    }

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configExpandDims(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_EXPAND_DIMS_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }

    EdenOperand* newOperandForOptions = new EdenOperand();

    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;

    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(ExpandDimsOptions);
    bufferForOptions->addr = new int32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;

    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;

    // quantInfo
    newOperandForOptions->quantInfo = nullptr;

    // extraParams
    newOperandForOptions->extraParams = nullptr;

    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }

    // Fill newOperandForOptions with parsed information
    ExpandDimsOptions* options = new (bufferForOptions->addr) ExpandDimsOptions();

    /*
     0: An n-D tensor, specifying the input.
     1: An ANEURALNETWORKS_INT32 scalar.
    */
    int32_t axis = getValue<int32_t>(model, androidOperation, 1);
    options->axis = axis;

    auto inputOperandId = androidOperation.inputs[0];
    auto inputOperand = model.operands[inputOperandId];
    int32_t numOfInputDims = inputOperand.dimensions.size();
    if (!(-(numOfInputDims + 1) <= axis && axis < (numOfInputDims + 1))) {
        LOGE(EDEN_DRIVER, "axis(%d) is too small or too larger! set default value 1\n", axis);
        delete newOperandForOptions;
        delete[] reinterpret_cast<int32_t*>(bufferForOptions->addr);
        delete options;
        return;
    }
    options->nums = numOfInputDims;

    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void configChannelShuffle(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);
    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_CHANNEL_SHUFFLE_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }
    // Check unknown dimension, unknown rank
    if ((model.operands[androidOperation.inputs[0]].dimensions.size() == 0) ||  // unknown rank
        (model.operands[androidOperation.inputs[0]].dimensions[0] == 0)) {      // unknown dimension
        LOGD(EDEN_DRIVER, "Unknown dimension, unknown rank is detected... skip it\n");
        return;
    }
    EdenOperand* newOperandForOptions = new EdenOperand();
    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;
    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(ChannelShuffleOptions);
    bufferForOptions->addr = new int32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;
    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;
    // quantInfo
    newOperandForOptions->quantInfo = nullptr;
    // extraParams
    newOperandForOptions->extraParams = nullptr;
    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }
    // Fill newOperandForOptions with parsed information
    ChannelShuffleOptions* options = new(bufferForOptions->addr) ChannelShuffleOptions();
    /*
     * Inputs:
     * * 0: An n-D tensor, specifying the tensor to be shuffled.
     * * 1: An {@link ANEURALNETWORKS_INT32} scalar, specifying the number of
     *      groups.
     * * 2: An {@link ANEURALNETWORKS_INT32} scalar, specifying the dimension
     *      channel shuffle would be performed on. Negative index is used to
     *      specify axis from the end (e.g. -1 for the last axis). Must be in
     *      the range [-n, n).
     */
    int32_t group = getValue<int32_t>(model, androidOperation, 1);
    int32_t axis = getValue<int32_t>(model, androidOperation, 2);
    const V1_2::Operand& androidInputOperand = model.operands[androidOperation.inputs[0]];
    const size_t numOfAndroidDims = androidInputOperand.dimensions.size();
    if (axis < 0) axis += static_cast<int32_t>(numOfAndroidDims);
    // calibrate `axis` because `NHWC -> NCHW` issue
    if (numOfAndroidDims == 2) {  // dim2: [H, W] -> [1, 1, H, W]
        axis += 2;
    } else {
        // dim3: [N, H, W] -> [N, 1, H, W]
        // dim4: [N, H, W, C] -> [N, C, H, W]
        if (axis != 0) axis += 1;
        if (axis > 3) axis -= 3;
    }
    options->group = group;
    options->axis = axis;
    LOGD(EDEN_DRIVER, "group=%d\n", options->group);
    LOGD(EDEN_DRIVER, "axis=%d\n", options->axis);
    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}
void configDetectionPostprocessing(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);
    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_DETECTION_POSTPROCESSING_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }
    // Check unknown dimension, unknown rank
    if ((model.operands[androidOperation.inputs[0]].dimensions.size() == 0) ||  // unknown rank
        (model.operands[androidOperation.inputs[0]].dimensions[0] == 0)) {      // unknown dimension
        LOGD(EDEN_DRIVER, "Unknown dimension, unknown rank is detected... skip it\n");
        return;
    }
    EdenOperand* newOperandForOptions = new EdenOperand();
    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;
    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(DetectionPostprocessingOptions);
    bufferForOptions->addr = new int32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;
    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;
    // quantInfo
    newOperandForOptions->quantInfo = nullptr;
    // extraParams
    newOperandForOptions->extraParams = nullptr;
    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }
    // Fill newOperandForOptions with parsed information
    DetectionPostprocessingOptions* options = new(bufferForOptions->addr) DetectionPostprocessingOptions();
    /*
     * Inputs:
     * * 0: A 3-D Tensor of shape [batches, num_anchors, num_classes], specifying
     *      the score of each anchor with each class. Class 0 for each
     *      [batches, num_anchors, 0] is background and will be ignored.
     * * 1: A 3-D Tensor of shape [batches, num_anchors, length_box_encoding], with
     *      the first four values in length_box_encoding specifying the bounding
     *      box deltas. The box deltas are encoded in the order of [dy, dx, dh, dw],
     *      where dy and dx is the linear-scale relative correction factor for the
     *      center position of the bounding box with respect to the width and height,
     *      dh and dw is the log-scale relative correction factor for the width and
     *      height. All the entries in length_box_encoding beyond the first four
     *      values are ignored in this operation.
     * * 2: A 2-D Tensor of shape [num_anchors, 4], specifying the shape of each
     *      predefined anchor, with format [ctr_y, ctr_x, h, w], where ctr_y and
     *      ctr_x are the center position of the box, and h and w are the height
     *      and the width.
     * * 3: An {@link ANEURALNETWORKS_FLOAT32} scalar, specifying the scaling
     *      factor for dy in bounding box deltas.
     * * 4: An {@link ANEURALNETWORKS_FLOAT32} scalar, specifying the scaling
     *      factor for dx in bounding box deltas.
     * * 5: An {@link ANEURALNETWORKS_FLOAT32} scalar, specifying the scaling
     *      factor for dh in bounding box deltas.
     * * 6: An {@link ANEURALNETWORKS_FLOAT32} scalar, specifying the scaling
     *      factor for dw in bounding box deltas.
     * * 7: An {@link ANEURALNETWORKS_BOOL} scalar, set to true to use regular
     *      multi-class NMS algorithm that do NMS separately for each class,
     *      set to false for a faster algorithm that only do one single NMS
     *      using the highest class score..
     * * 8: An {@link ANEURALNETWORKS_INT32} scalar, max_num_detections, specifying
     *      the maximum number of boxes for the output. Boxes with the lowest
     *      scores are discarded to meet the limit.
     * * 9: An {@link ANEURALNETWORKS_INT32} scalar, only used when input7 is
     *      set to false, specifying the maximum number of classes per detection.
     * * 10: An {@link ANEURALNETWORKS_INT32} scalar, only used when input7 is
     *       set to true, specifying the maximum number of detections when
     *       applying NMS algorithm for each single class.
     * * 11: A scalar, score_threshold. Boxes with scores lower than the
     *       threshold are filtered before sending to the NMS algorithm. The
     *       scalar must be of {@link ANEURALNETWORKS_FLOAT16} if input0 is of
     *       {@link ANEURALNETWORKS_TENSOR_FLOAT16} and of {@link
     *       ANEURALNETWORKS_FLOAT32} if input0 is of {@link
     *       ANEURALNETWORKS_TENSOR_FLOAT32}.
     * * 12: A scalar, specifying the IoU threshold for hard NMS. The scalar
     *       must be of {@link ANEURALNETWORKS_FLOAT16} if input0 is of {@link
     *       ANEURALNETWORKS_TENSOR_FLOAT16} and of {@link
     *       ANEURALNETWORKS_FLOAT32} if input0 is of {@link
     *       ANEURALNETWORKS_TENSOR_FLOAT32}.
     * * 13: An {@link ANEURALNETWORKS_BOOL} scalar, set to true to include
     *       background class in the list of label map for the output, set
     *       to false to not include the background. When the background
     *       class is included, it has label 0 and the output classes start
     *       at 1 in the label map, otherwise, the output classes start at 0.
     */
    auto inputOperandIndex = androidOperation.inputs[0];
    // `2` is the last dim of [batches, num_anchors, num_classes]
    int num_classes = model.operands[inputOperandIndex].dimensions[2];
    options->max_detections = getValue<int32_t >(model, androidOperation, 8);
    options->max_classes_per_detection = getValue<int32_t >(model, androidOperation, 9);
    options->detections_per_class = getValue<int32_t>(model, androidOperation, 10);
    options->num_classes = num_classes - 1;  // coz it will +1 in `userdriver_model`
    options->useRegularNms = getValue<bool>(model, androidOperation, 7);;
    options->isBGInLabel = getValue<bool>(model, androidOperation, 13);
    if (model.operands[inputOperandIndex].type == V1_2::OperandType::TENSOR_FLOAT16) {
        options->nms_score_threshold = getValue<_Float16>(model, androidOperation, 11);
        options->nms_iou_threshold = getValue<_Float16>(model, androidOperation, 12);
        options->y_scale = getValue<_Float16>(model, androidOperation, 3);
        options->x_scale = getValue<_Float16>(model, androidOperation, 4);
        options->h_scale = getValue<_Float16>(model, androidOperation, 5);
        options->w_scale = getValue<_Float16>(model, androidOperation, 6);
    } else {
        options->nms_score_threshold = getValue<float>(model, androidOperation, 11);
        options->nms_iou_threshold = getValue<float>(model, androidOperation, 12);
        options->y_scale = getValue<float>(model, androidOperation, 3);
        options->x_scale = getValue<float>(model, androidOperation, 4);
        options->h_scale = getValue<float>(model, androidOperation, 5);
        options->w_scale = getValue<float>(model, androidOperation, 6);
    }
    LOGD(EDEN_DRIVER, "max_detections=%d\n", options->max_detections);
    LOGD(EDEN_DRIVER, "max_classes_per_detection=%d\n", options->max_classes_per_detection);
    LOGD(EDEN_DRIVER, "detections_per_class=%d\n", options->detections_per_class);
    LOGD(EDEN_DRIVER, "num_classes=%d\n", options->num_classes);
    LOGD(EDEN_DRIVER, "nms_score_threshold=%f\n", options->nms_score_threshold);
    LOGD(EDEN_DRIVER, "nms_iou_threshold=%f\n", options->nms_iou_threshold);
    LOGD(EDEN_DRIVER, "(dy, dx, dh, dw,)=(%f, %f, %f, %f)\n",
        options->y_scale, options->x_scale, options->h_scale, options->w_scale);
    LOGD(EDEN_DRIVER, "useRegularNms=%d\n", options->useRegularNms);
    LOGD(EDEN_DRIVER, "isBGInLabel=%d\n", options->isBGInLabel);
    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}
void configInstanceNormalization(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);
    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_INSTANCE_NORMALIZATION_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }
    // Check unknown dimension, unknown rank
    if ((model.operands[androidOperation.inputs[0]].dimensions.size() == 0) ||  // unknown rank
        (model.operands[androidOperation.inputs[0]].dimensions[0] == 0)) {      // unknown dimension
        LOGD(EDEN_DRIVER, "Unknown dimension, unknown rank is detected... skip it\n");
        return;
    }
    EdenOperand* newOperandForOptions = new EdenOperand();
    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;
    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(InstanceNormalizationOptions);
    bufferForOptions->addr = new int32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;
    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;
    // quantInfo
    newOperandForOptions->quantInfo = nullptr;
    // extraParams
    newOperandForOptions->extraParams = nullptr;
    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }
    // Fill newOperandForOptions with parsed information
    InstanceNormalizationOptions* options = new(bufferForOptions->addr) InstanceNormalizationOptions();
    /*
     * Inputs:
     * * 0: An n-D tensor, specifying the tensor to be normalized.
     * * 1: A scalar, specifying gamma, the scale applied to the normalized
     *      tensor. The scalar must be of {@link ANEURALNETWORKS_FLOAT16} if
     *      input0 is of {@link ANEURALNETWORKS_TENSOR_FLOAT16} and of {@link
     *      ANEURALNETWORKS_FLOAT32} if input0 is of {@link
     *      ANEURALNETWORKS_TENSOR_FLOAT32}.
     * * 2: A scalar, specifying beta, the offset applied to the normalized
     *      tensor. The scalar must be of {@link ANEURALNETWORKS_FLOAT16} if
     *      input0 is of {@link ANEURALNETWORKS_TENSOR_FLOAT16} and of {@link
     *      ANEURALNETWORKS_FLOAT32} if input0 is of {@link
     *      ANEURALNETWORKS_TENSOR_FLOAT32}.
     * * 3: A scalar, specifying epsilon, the small value added to variance to
     *      avoid dividing by zero. The scalar must be of {@link ANEURALNETWORKS_FLOAT16} if
     *      input0 is of {@link ANEURALNETWORKS_TENSOR_FLOAT16} and of {@link
     *      ANEURALNETWORKS_FLOAT32} if input0 is of {@link
     *      ANEURALNETWORKS_TENSOR_FLOAT32}.
     * * 4: An {@link ANEURALNETWORKS_BOOL} scalar, set to true to specify
     *      NCHW data layout for input0 and output0. Set to false for NHWC.
     */
    if (model.operands[androidOperation.inputs[0]].type == V1_2::OperandType::TENSOR_FLOAT16) {
        options->gamma = getValue<_Float16>(model, androidOperation, 1);
        options->beta = getValue<_Float16>(model, androidOperation, 2);
        options->epsilon = getValue<_Float16>(model, androidOperation, 3);
    } else {
        options->gamma = getValue<float>(model, androidOperation, 1);
        options->beta = getValue<float>(model, androidOperation, 2);
        options->epsilon = getValue<float>(model, androidOperation, 3);
    }
    newOperandForOptions->isNCHW = getValue<bool>(model, androidOperation, 4);
    LOGD(EDEN_DRIVER, "gamma=%f\n", (float)options->gamma);
    LOGD(EDEN_DRIVER, "beta=%f\n", (float)options->beta);
    LOGD(EDEN_DRIVER, "epsilon=%f\n", (float)options->epsilon);
    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}
void configRandomMultinomial(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);
    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_RANDOM_MULTINOMIAL_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }
    // Check unknown dimension, unknown rank
    if ((model.operands[androidOperation.inputs[0]].dimensions.size() == 0) ||  // unknown rank
        (model.operands[androidOperation.inputs[0]].dimensions[0] == 0)) {      // unknown dimension
        LOGD(EDEN_DRIVER, "Unknown dimension, unknown rank is detected... skip it\n");
        return;
    }
    EdenOperand* newOperandForOptions = new EdenOperand();
    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;
    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(RandomMultinomialOptions);
    bufferForOptions->addr = new int32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;
    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;
    // quantInfo
    newOperandForOptions->quantInfo = nullptr;
    // extraParams
    newOperandForOptions->extraParams = nullptr;
    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }
    // Fill newOperandForOptions with parsed information
    RandomMultinomialOptions* options = new(bufferForOptions->addr) RandomMultinomialOptions();
    /*
     * Inputs:
     * * 0: A 2-D tensor with shape [batches, classes], specifying the
     *      unnormalized log-probabilities for all classes.
     * * 1: A scalar {@link ANEURALNETWORKS_INT32}, specifying the number of
     *      independent samples to draw for each row slice.
     * * 2: A 1-D {@link ANEURALNETWORKS_TENSOR_INT32} tensor with shape [2],
     *      specifying seeds used to initialize the random distribution.
     */
    int32_t sample_count = getValue<int32_t >(model, androidOperation, 1);
    sp<IMemory> sp;
    const ModelInfo* modelInfo = nullptr;
    int32_t* seeds = getPtr<int32_t*>(model, androidOperation, 2, modelInfo, sp);
    std::unique_ptr<int32_t[]> seed_dims(new int32_t[2]);
    options->seeds = std::move(seed_dims);
    options->seeds[0] = seeds[0];
    options->seeds[1] = seeds[1];
    options->sample_count = sample_count;
    LOGD(EDEN_DRIVER, "sample_count=%d\n", options->sample_count);
    LOGD(EDEN_DRIVER, "seeds=(%d, %d)\n", options->seeds[0], options->seeds[1]);
    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}
void configLogSoftmax(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);
    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_LOG_SOFTMAX_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }
    // Check unknown dimension, unknown rank
    if ((model.operands[androidOperation.inputs[0]].dimensions.size() == 0) ||  // unknown rank
        (model.operands[androidOperation.inputs[0]].dimensions[0] == 0)) {      // unknown dimension
        LOGD(EDEN_DRIVER, "Unknown dimension, unknown rank is detected... skip it\n");
        return;
    }
    EdenOperand* newOperandForOptions = new EdenOperand();
    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;
    // buffer
    EdenBuffer* bufferForOptions = new EdenBuffer();
    bufferForOptions->size = sizeof(LogSoftmaxOptions);
    bufferForOptions->addr = new int32_t[bufferForOptions->size];
    newOperandForOptions->buffer = bufferForOptions;
    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;
    // quantInfo
    newOperandForOptions->quantInfo = nullptr;
    // extraParams
    newOperandForOptions->extraParams = nullptr;
    // ins operand type ; TENSOR_FLOAT32 or TENSOR_QUANT8_ASYMM
    for (size_t idx = 0; idx < numOfInputs; idx++) {
        auto operandId = androidOperation.inputs[idx];
        LOGD(EDEN_DRIVER, "Operand[%zu] type : %s\n", idx, getAndroidNNOperandTypeName(model.operands[operandId].type));
    }
    // Fill newOperandForOptions with parsed information
    LogSoftmaxOptions* options = new(bufferForOptions->addr) LogSoftmaxOptions();
    /*
     * Inputs:
     * * 0: A tensor specifying the input logits.
     * * 1: A scalar, specifying the positive scaling factor for the exponent,
     *      beta.
     *      For input tensor of {@link ANEURALNETWORKS_TENSOR_FLOAT16}, the beta
     *      value must be of {@link ANEURALNETWORKS_FLOAT16}.
     *      For input tensor of {@link ANEURALNETWORKS_TENSOR_FLOAT32}, the beta
     *      value must be of {@link ANEURALNETWORKS_FLOAT32}.
     * * 2: An {@link ANEURALNETWORKS_INT32} scalar specifying the axis to
     *      reduce across. Negative index is used to specify axis from the
     *      end (e.g. -1 for the last axis). Must be in the range [-n, n).
     */
    if (model.operands[androidOperation.inputs[0]].type == V1_2::OperandType::TENSOR_FLOAT16) {
        options->beta = getValue<_Float16>(model, androidOperation, 1);
    } else {
        options->beta = getValue<float>(model, androidOperation, 1);
    }
    int32_t axis = -1;
    auto operandId = androidOperation.inputs[0];
    auto operand = model.operands[operandId];
    int32_t numOfInputDims = operand.dimensions.size();
    if (numOfInputs == NUM_OF_INPUTS_ON_LOG_SOFTMAX_1_2) {
        axis = getValue<int32_t>(model, androidOperation, 2);
    }
    if (axis < 0) axis += numOfInputDims;
    switch (numOfInputDims)
    {
    case 3:
        axis += 1;
        if (axis == 1) axis = 0;
        break;
    case 5:  // for VTS1.2 cases
        axis -= 1;  // 1NHWC -> NHWC
        [[fallthrough]];
    case 4:
        if (axis > 0) {
            axis += 1;
        }
        if (axis > 3) {
            axis -= 3;
        }
        break;
    default:
        break;
    }
    options->axis = axis;
    LOGD(EDEN_DRIVER, "beta=%f\n", options->beta);
    LOGD(EDEN_DRIVER, "axis=%d\n", options->axis);
    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}
void configHeatmapMaxKeypoint(const V1_2::Model& model, const V1_2::Operation& androidOperation, EdenOperand** configOperand) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);
    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != NUM_OF_INPUTS_ON_HEATMAP_MAX_KEYPOINT_1_2) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }
    // Check unknown dimension, unknown rank
    if ((model.operands[androidOperation.inputs[0]].dimensions.size() == 0) ||  // unknown rank
        (model.operands[androidOperation.inputs[0]].dimensions[0] == 0)) {      // unknown dimension
        LOGD(EDEN_DRIVER, "Unknown dimension, unknown rank is detected... skip it\n");
        return;
    }
    EdenOperand* newOperandForOptions = new EdenOperand();
    // name
    char* operandForOptionsName = nullptr;
    int32_t operandForOptionsLength = 0;
    getOptionsName(static_cast<uint32_t>(androidOperation.type), operandForOptionsName, operandForOptionsLength);
    newOperandForOptions->name.name = reinterpret_cast<int8_t*>(operandForOptionsName);
    newOperandForOptions->name.length = operandForOptionsLength;
    // buffer
    newOperandForOptions->buffer = nullptr;
    // shapeInfo
    newOperandForOptions->shapeInfo = nullptr;
    // quantInfo
    newOperandForOptions->quantInfo = nullptr;
    // extraParams
    newOperandForOptions->extraParams = nullptr;
    newOperandForOptions->isNCHW = getValue<bool>(model, androidOperation, 2);
    *configOperand = newOperandForOptions;
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void readConv2DParams(const V1_2::Model& model, const V1_2::Operation& androidOperation, Conv2DParams& conv2DParams) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    if (numOfInputs != 10 && numOfInputs != 7) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }

    // Check unknown dimension, unknown rank
    if ((model.operands[androidOperation.inputs[0]].dimensions.size() == 0) ||   // unknown rank
        (model.operands[androidOperation.inputs[0]].dimensions[W_NHWC] == 0) ||  // unknown dimension
        (model.operands[androidOperation.inputs[0]].dimensions[H_NHWC] == 0)) {  // unknown dimension
        LOGD(EDEN_DRIVER, "Unknown dimension, unknown rank is detected... skip it\n");
        return;
    }

    int32_t paddingLeft, paddingRight, paddingTop, paddingBottom;
    int32_t paddingImplicit;
    int32_t strideWidth, strideHeight;
    int32_t filterWidth, filterHeight;

    int32_t inputOperandIndex = androidOperation.inputs[0];
    int32_t width = model.operands[inputOperandIndex].dimensions[W_NHWC];
    int32_t height = model.operands[inputOperandIndex].dimensions[H_NHWC];

    int32_t filterOperandIndex = androidOperation.inputs[1];
    filterWidth = model.operands[filterOperandIndex].dimensions[W_NHWC];
    filterHeight = model.operands[filterOperandIndex].dimensions[H_NHWC];

    if (numOfInputs == 10) {
        paddingLeft = getValue<int32_t>(model, androidOperation, 3);
        paddingRight = getValue<int32_t>(model, androidOperation, 4);
        paddingTop = getValue<int32_t>(model, androidOperation, 5);
        paddingBottom = getValue<int32_t>(model, androidOperation, 6);
        strideWidth = getValue<int32_t>(model, androidOperation, 7);
        strideHeight = getValue<int32_t>(model, androidOperation, 8);
    } else {
        paddingImplicit = getValue<int32_t>(model, androidOperation, 3);
        strideWidth = getValue<int32_t>(model, androidOperation, 4);
        strideHeight = getValue<int32_t>(model, androidOperation, 5);

        getPadding(width, strideWidth, filterWidth, paddingImplicit, &paddingLeft, &paddingRight);
        getPadding(height, strideHeight, filterHeight, paddingImplicit, &paddingTop, &paddingBottom);
    }

    int32_t kernelSize = 0;
    int32_t paddingSize = 0;
    int32_t strideSize = 0;
    int32_t zeroPoint = 0;
    kernelSize = std::max(filterWidth, filterHeight);
    paddingSize = std::max(paddingLeft, paddingRight);
    paddingSize = std::max(paddingSize, paddingTop);
    paddingSize = std::max(paddingSize, paddingBottom);
    strideSize = std::max(strideWidth, strideHeight);
    zeroPoint = model.operands[inputOperandIndex].zeroPoint;

    // Update params to return
    conv2DParams.kernelSize = kernelSize;
    conv2DParams.paddingSize = paddingSize;
    conv2DParams.strideSize = strideSize;
    conv2DParams.zeroPoint = zeroPoint;

    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void showConv2DParams(const Conv2DParams& conv2DParams) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);
    LOGD(EDEN_DRIVER, "conv2DParams.kernelSize=%d\n", conv2DParams.kernelSize);
    LOGD(EDEN_DRIVER, "conv2DParams.paddingSize=%d\n", conv2DParams.paddingSize);
    LOGD(EDEN_DRIVER, "conv2DParams.strideSize=%d\n", conv2DParams.strideSize);
    LOGD(EDEN_DRIVER, "conv2DParams.zeroPoint=%d\n", conv2DParams.zeroPoint);
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void readPool2DParams(const V1_2::Model& model, const Operation& androidOperation, Pool2DParams& pool2DParams) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    LOGD(EDEN_DRIVER, "POOL parameter count : %zu\n", numOfInputs);
    if (numOfInputs != 10 && numOfInputs != 7) {
        LOGE(EDEN_DRIVER, "Exceed supported input parameter count : %zu\n", numOfInputs);
        return;
    }

    // Check unknown dimension, unknown rank
    if ((model.operands[androidOperation.inputs[0]].dimensions.size() == 0) ||   // unknown rank
        (model.operands[androidOperation.inputs[0]].dimensions[W_NHWC] == 0) ||  // unknown dimension
        (model.operands[androidOperation.inputs[0]].dimensions[H_NHWC] == 0)) {  // unknown dimension
        LOGD(EDEN_DRIVER, "Unknown dimension, unknown rank is detected... skip it\n");
        return;
    }

    int32_t paddingLeft, paddingRight, paddingTop, paddingBottom;
    int32_t paddingImplicit;
    int32_t strideWidth, strideHeight;
    int32_t filterWidth, filterHeight;

    int32_t inputOperandIndex = androidOperation.inputs[0];
    int32_t width = model.operands[inputOperandIndex].dimensions[W_NHWC];
    int32_t height = model.operands[inputOperandIndex].dimensions[H_NHWC];

    if (numOfInputs == 10) {
        paddingLeft = getValue<int32_t>(model, androidOperation, 1);
        paddingRight = getValue<int32_t>(model, androidOperation, 2);
        paddingTop = getValue<int32_t>(model, androidOperation, 3);
        paddingBottom = getValue<int32_t>(model, androidOperation, 4);
        strideWidth = getValue<int32_t>(model, androidOperation, 5);
        strideHeight = getValue<int32_t>(model, androidOperation, 6);
        filterWidth = getValue<int32_t>(model, androidOperation, 7);
        filterHeight = getValue<int32_t>(model, androidOperation, 8);
    } else {
        paddingImplicit = getValue<int32_t>(model, androidOperation, 1);
        strideWidth = getValue<int32_t>(model, androidOperation, 2);
        strideHeight = getValue<int32_t>(model, androidOperation, 3);
        filterWidth = getValue<int32_t>(model, androidOperation, 4);
        filterHeight = getValue<int32_t>(model, androidOperation, 5);
        LOGD(EDEN_DRIVER, "NPUC_ONDEVICE: filterWidth=%d\n", filterWidth);
        LOGD(EDEN_DRIVER, "NPUC_ONDEVICE: filterHeight=%d\n", filterHeight);
        getPadding(width, strideWidth, filterWidth, paddingImplicit, &paddingLeft, &paddingRight);
        getPadding(height, strideHeight, filterHeight, paddingImplicit, &paddingTop, &paddingBottom);
    }

    int32_t kernelSize = 0;
    int32_t paddingSize = 0;
    int32_t strideSize = 0;
    int32_t zeroPoint = 0;
    kernelSize = std::max(filterWidth, filterHeight);
    paddingSize = std::max(paddingLeft, paddingRight);
    paddingSize = std::max(paddingSize, paddingTop);
    paddingSize = std::max(paddingSize, paddingBottom);
    strideSize = std::max(strideWidth, strideHeight);
    zeroPoint = model.operands[inputOperandIndex].zeroPoint;

    // Update params to return
    pool2DParams.kernelSize = kernelSize;
    pool2DParams.paddingSize = paddingSize;
    pool2DParams.strideSize = strideSize;
    pool2DParams.zeroPoint = zeroPoint;

    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void showPool2DParams(const Pool2DParams& pool2DParams) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);
    LOGD(EDEN_DRIVER, "pool2DParams.kernelSize=%d\n", pool2DParams.kernelSize);
    LOGD(EDEN_DRIVER, "pool2DParams.paddingSize=%d\n", pool2DParams.paddingSize);
    LOGD(EDEN_DRIVER, "pool2DParams.strideSize=%d\n", pool2DParams.strideSize);
    LOGD(EDEN_DRIVER, "pool2DParams.zeroPoint=%d\n", pool2DParams.zeroPoint);
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void readConcatParams(const V1_2::Model& model, const Operation& androidOperation, ConcatParams& concatParams) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    size_t numOfInputs = inputs.size();
    LOGD(EDEN_DRIVER, "Concat parameter count : %zu\n", numOfInputs);

    int32_t axisValue = getValue<int32_t>(model, androidOperation, numOfInputs-1);

    auto operandId = androidOperation.inputs[0];
    auto operand = model.operands[operandId];
    uint32_t rank = operand.dimensions.size();

    if (rank == 2) {
        axisValue = axisValue + 2;  /* currently, VTS cases has 2 dimensions, 0 for h, 1 for w */
    } else {
        if (axisValue == C_NHWC) {
            axisValue = C_NCHW;
        } else if (axisValue == W_NHWC) {
            axisValue = W_NCHW;
        } else if (axisValue == H_NHWC) {
            axisValue = H_NCHW;
        }
    }
    LOGD(EDEN_DRIVER, "Concat axis : %d\n", axisValue);
    // Update params to return
    concatParams.axis = axisValue;

    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void showConcatParams(const ConcatParams& concatParams) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);
    LOGD(EDEN_DRIVER, "concatParams.axis=%d\n", concatParams.axis);
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

int32_t readOperandType(const V1_2::Model& model, const V1_2::Operation& androidOperation, int32_t inputIndex) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // get user inputs from operation
    const hidl_vec<uint32_t>& inputs = androidOperation.inputs;
    int32_t numOfInputs = static_cast<int32_t>(inputs.size());

    if (inputIndex >= numOfInputs) {
        LOGE(EDEN_DRIVER, "Error, inputIndex is not valid. (inputIndex=%d, numOfInputs=%d)", inputIndex, numOfInputs);
        return -1;
    }

    int32_t operandId = androidOperation.inputs[inputIndex];
    V1_2::OperandType type = model.operands[operandId].type;
    LOGD(EDEN_DRIVER, "Operand[%d] type : %s\n", inputIndex, getAndroidNNOperandTypeName(type));

    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
    return static_cast<int32_t>(type);
}

void getPadding(int32_t inputSize, int32_t stride, int32_t filterSize, int32_t paddingImplicit,
                int32_t* paddingHead, int32_t* paddingTail, bool isTransposeConv) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    /*
     * PADDING_SAME: Padding on both ends are the "same"
     * PADDING_VALID: No padding
     */
    *paddingHead = 0;
    *paddingTail = 0;

    if (stride != 0) {
        if (paddingImplicit == kPaddingSame) {
            int32_t outSize = (inputSize + stride - 1) / stride;
            int32_t neededInput = (outSize - 1) * stride + filterSize;
            if (neededInput > inputSize) {
                *paddingHead = (neededInput - inputSize) / 2;
                *paddingTail = (neededInput - inputSize + 1) / 2;
            }
            // For transpose conv, make padding tail fit tightly to the end of the last stride.
            if (isTransposeConv) {
                *paddingTail = (neededInput - inputSize) - *paddingHead;
            }
        }
    } else {
        LOGE(EDEN_DRIVER, "stride is zero, DIVIDE_BY_ZERO!");
    }
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

int32_t loadParamsForEdenOperation(const int32_t edenOpType, const bool hasOptions, const V1_2::Operation& androidOperation,
                                   int32_t& numOfInputTensors, int32_t& numOfInputs, int32_t& numOfOutputs, std::vector<bool>& needToConvertOperands) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    switch (edenOpType) {
    case EDEN_OP_ADD:  // 0
        // Determine # of inputs and outputs
        numOfInputTensors = 2;
        numOfInputs = numOfInputTensors + 1;  // +1 for AddOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A tensor(input)
        needToConvertOperands[1] = true;  // A tensor(input)

        break;

    case EDEN_OP_AVERAGE_POOL_2D:  // 1
        // Determine # of inputs and outputs
        numOfInputTensors = 1;
        numOfInputs = numOfInputTensors + 1;  // +1 for AveragePool2DOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A 4-D tensor(input)

        break;

    case EDEN_OP_CONCATENATION:  // 2
        // Determine # of inputs and outputs
        numOfInputTensors = androidOperation.inputs.size() - 1;
        numOfInputs = numOfInputTensors + 1;  // +1 for ConcatenationOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        for (int32_t idx = 0; idx < numOfInputTensors; idx++) {
            needToConvertOperands[idx] = true;  // A tensor(input)
        }

        break;

    case EDEN_OP_CONV_2D:  // 3
        // Determine # of inputs and outputs
        numOfInputTensors = 3;
        numOfInputs = numOfInputTensors + 1;  // +1 for Conv2DOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A 4-D tensor(input)
        needToConvertOperands[1] = true;  // A 4-D tensor(filter)
        needToConvertOperands[2] = true;  // A 1-D tensor(bias)

        break;

    case EDEN_OP_DEPTHWISE_CONV_2D:  // 4
        // Determine # of inputs and outputs
        numOfInputTensors = 3;
        numOfInputs = numOfInputTensors + 1;  // +1 for DepthwiseConv2DOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A 4-D tensor(input)
        needToConvertOperands[1] = true;  // A 4-D tensor(filter)
        needToConvertOperands[2] = true;  // A 1-D tensor(bias)

        break;

    case EDEN_OP_DEPTH_TO_SPACE:  // 5
        // Determine # of inputs and outputs
        numOfInputTensors = 1;
        numOfInputs = numOfInputTensors + 1;  // +1 for DepthwiseConv2DOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A 4-D tensor(input)

        break;

    case EDEN_OP_DEQUANTIZE:  // 6
        // Determine # of inputs and outputs
        // @todo Currently there is no specification for EdenModel
        // now just follow the specification of Android NN Model
        numOfInputTensors = androidOperation.inputs.size();
        numOfInputs = numOfInputTensors + 0;  // No XXXOptions
        if (hasOptions == true) return NO_REQUIRED_OPTION_OPERAND_BUT_THERE_IS;
        numOfOutputs = androidOperation.outputs.size();

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        for (int32_t idx = 0; idx < numOfInputTensors; idx++) {
            needToConvertOperands[idx] = true;  // A tensor(input)
        }

        break;

    case EDEN_OP_EMBEDDING_LOOKUP:  // 7
        // Determine # of inputs and outputs
        numOfInputTensors = 2;
        numOfInputs = numOfInputTensors + 0;  // No XXXOptions
        if (hasOptions == true) return NO_REQUIRED_OPTION_OPERAND_BUT_THERE_IS;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A 1-D tensor(Lookups)
        needToConvertOperands[1] = true;  // A n-D tensor(Values)

        break;

    case EDEN_OP_FLOOR:  // 8
        // Determine # of inputs and outputs
        numOfInputTensors = 1;
        numOfInputs = numOfInputTensors + 0;  // No XXXOptions
        if (hasOptions == true) return NO_REQUIRED_OPTION_OPERAND_BUT_THERE_IS;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A tensor(input)

        break;

    case EDEN_OP_FULLY_CONNECTED:  // 9
        // Determine # of inputs and outputs
        numOfInputTensors = 3;
        numOfInputs = numOfInputTensors + 1;  // +1 for FullyConnectedOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A tensor at least rank 2(input)
        needToConvertOperands[1] = true;  // A 2-D tensor(weights)
        needToConvertOperands[2] = true;  // A 1-D tensor(bias)

        break;

    case EDEN_OP_HASHTABLE_LOOKUP:  // 10
        // Determine # of inputs and outputs
        numOfInputTensors = 3;
        numOfInputs = numOfInputTensors + 0;  // No XXXOptions
        if (hasOptions == true) return NO_REQUIRED_OPTION_OPERAND_BUT_THERE_IS;
        numOfOutputs = 2;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A [k] 1-D tensor(Lookups)
        needToConvertOperands[1] = true;  // A [n] 1-D tensor(Keys)
        needToConvertOperands[2] = true;  // A [n,...] tensor(Values)

        break;

    case EDEN_OP_L2_NORMALIZATION:  // 11
        // Determine # of inputs and outputs
        numOfInputTensors = 1;
        numOfInputs = numOfInputTensors + 1;  // +1 for L2NormalizationOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A n-D tensor(input)

        break;

    case EDEN_OP_L2_POOL_2D:  // 12
        // Determine # of inputs and outputs
        numOfInputTensors = 1;
        numOfInputs = numOfInputTensors + 1;  // +1 for L2Pool2DOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A 4-D tensor(input)

        break;

    case EDEN_OP_LOCAL_RESPONSE_NORMALIZATION:  // 13
        // Determine # of inputs and outputs
        numOfInputTensors = 1;
        numOfInputs = numOfInputTensors + 1;  // +1 for LocalResponseNormalizationOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A 4-D tensor(input)

        break;

    case EDEN_OP_LOGISTIC:  // 14
        // Determine # of inputs and outputs
        numOfInputTensors = 1;
        numOfInputs = numOfInputTensors + 0;  // No XXXOptions
        if (hasOptions == true) return NO_REQUIRED_OPTION_OPERAND_BUT_THERE_IS;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A tensor(input)

        break;

    case EDEN_OP_LSH_PROJECTION:  // 15
        // Determine # of inputs and outputs
        numOfInputTensors = 3;
        numOfInputs = numOfInputTensors + 1;  // +1 for LshProjectionOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A tensor(Hash functions)
        needToConvertOperands[1] = true;  // A tensor(input)
        needToConvertOperands[2] = true;  // A tensor(weight)

        break;

    case EDEN_OP_LSTM:  // 16
        // Determine # of inputs and outputs
        // @todo Currently there is no specification for EdenModel
        // now just follow the specification of Android NN Model
        numOfInputTensors = androidOperation.inputs.size() - 3;
        numOfInputs = numOfInputTensors + 1;  // +! for LSTMOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND_BUT_THERE_IS;
        numOfOutputs = androidOperation.outputs.size();

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;   // input, 2-D
        needToConvertOperands[1] = true;   // input-to-input weights, 2-D
        needToConvertOperands[2] = true;   // input-to-forget weights, 2-D
        needToConvertOperands[3] = true;   // input-to-cell weights, 2-D
        needToConvertOperands[4] = true;   // input-to-output weights, 2-D
        needToConvertOperands[5] = true;   // recurrent-to-input weights, 2-D
        needToConvertOperands[6] = true;   // recurrent-to-forget weights, 2-D
        needToConvertOperands[7] = true;   // recurrent-to-cell weights, 2-D
        needToConvertOperands[8] = true;   // recurrent-to-output weights, 2-D
        needToConvertOperands[9] = true;   // cell-to-input, 1-D
        needToConvertOperands[10] = true;  // cell-to-forget, 1-D
        needToConvertOperands[11] = true;  // cell-to-output, 1-D
        needToConvertOperands[12] = true;  // input gate bias, 1-D
        needToConvertOperands[13] = true;  // forget gate bias, 1-D
        needToConvertOperands[14] = true;  // cell bias, 1-D
        needToConvertOperands[15] = true;  // output gate bias, 1-D
        needToConvertOperands[16] = true;  // projection weights, 2-D
        needToConvertOperands[17] = true;  // projection bias, 1-D
        needToConvertOperands[18] = true;  // output state, 2-D
        needToConvertOperands[19] = true;  // cell state, 2-D

        // for layer_norm_lstm
        if ((int)androidOperation.inputs.size() == 27) {
            for (int i = 23; i < (int)androidOperation.inputs.size(); i++) {
                needToConvertOperands[i] = true;
            }
        }

        break;

    case EDEN_OP_MAX_POOL_2D:  // 17
        // Determine # of inputs and outputs
        numOfInputTensors = 1;
        numOfInputs = numOfInputTensors + 1;  // +1 for MaxPool2DOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A 4-D tensor(input)

        break;

    case EDEN_OP_MUL:  // 18
        // Determine # of inputs and outputs
        numOfInputTensors = 2;
        numOfInputs = numOfInputTensors + 1;  // +1 for MulOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A tensor(input)
        needToConvertOperands[1] = true;  // A tensor(input)

        break;

    case EDEN_OP_RELU:  // 19
        // Determine # of inputs and outputs
        numOfInputTensors = 1;
        numOfInputs = numOfInputTensors + 0;  // No XXXOptions
        if (hasOptions == true) return NO_REQUIRED_OPTION_OPERAND_BUT_THERE_IS;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A tensor(input)

        break;

    case EDEN_OP_RELU1:  // 20
        // Determine # of inputs and outputs
        numOfInputTensors = 1;
        numOfInputs = numOfInputTensors + 0;  // No XXXOptions
        if (hasOptions == true) return NO_REQUIRED_OPTION_OPERAND_BUT_THERE_IS;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A tensor(input)

        break;

    case EDEN_OP_RELU6:  // 21
        // Determine # of inputs and outputs
        numOfInputTensors = 1;
        numOfInputs = numOfInputTensors + 0;  // No XXXOptions
        if (hasOptions == true) return NO_REQUIRED_OPTION_OPERAND_BUT_THERE_IS;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A tensor(input)

        break;

    case EDEN_OP_RESHAPE:  // 22
        // Determine # of inputs and outputs
        numOfInputTensors = 1;
        numOfInputs = numOfInputTensors + 1;  // +1 for ReshapeOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A tensor(input)

        break;

    case EDEN_OP_RESIZE_BILINEAR:  // 23
        // Determine # of inputs and outputs
        numOfInputTensors = 1;
        numOfInputs = numOfInputTensors + 1;  // +1 for ResizeBilinearOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A 4-D tensor(input)

        break;

    case EDEN_OP_RNN:  // 24
        // Determine # of inputs and outputs
        // @todo Currently there is no specification for EdenModel
        // now just follow the specification of Android NN Model
        numOfInputTensors = 5;
        numOfInputs = numOfInputTensors + 1;  // +1 for RNNOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND_BUT_THERE_IS;
        numOfOutputs = androidOperation.outputs.size();

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // input, 2-D
        needToConvertOperands[1] = true;  // weights, 2-D
        needToConvertOperands[2] = true;  // recurrent_weights, 2-D
        needToConvertOperands[3] = true;  // bias, 1-D
        needToConvertOperands[4] = true;  // hiddne_state, 2-D

        break;

    case EDEN_OP_SOFTMAX:  // 25
        // Determine # of inputs and outputs
        numOfInputTensors = 1;
        numOfInputs = numOfInputTensors + 1;  // +1 for SoftmaxOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A 2-D or 4-D tensor(input)

        break;

    case EDEN_OP_SPACE_TO_DEPTH:  // 26
        // Determine # of inputs and outputs
        numOfInputTensors = 1;
        numOfInputs = numOfInputTensors + 1;  // +1 for SpaceToDepthOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A 4-D tensor(input)

        break;

    case EDEN_OP_SVDF:  // 27
        // Determine # of inputs and outputs
        numOfInputTensors = 5;
        numOfInputs = numOfInputTensors + 1;  // +1 for SVDFOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = androidOperation.outputs.size();

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A 2-D tensor(input)
        needToConvertOperands[1] = true;  // A 2-D tensor(weights_feature)
        needToConvertOperands[2] = true;  // A 2-D tensor(weight_time)
        needToConvertOperands[3] = true;  // A 1-D tensor(bias)
        needToConvertOperands[4] = true;  // A 1-D tensor(state)

        break;

    case EDEN_OP_TANH:  // 28
        // Determine # of inputs and outputs
        numOfInputTensors = 1;
        numOfInputs = numOfInputTensors + 0;  // No XXXOptions
        if (hasOptions == true) return NO_REQUIRED_OPTION_OPERAND_BUT_THERE_IS;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A tensor(input)

        break;

    case EDEN_OP_BATCH_TO_SPACE_ND:  // 29
        // Determine # of inputs and outputs
        // @todo Currently there is no specification for EdenModel
        // now just follow the specification of Android NN Model
        numOfInputTensors = androidOperation.inputs.size();
        numOfInputs = numOfInputTensors + 1;  // +1 for BatchToSpaceNDOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = androidOperation.outputs.size();

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        for (int32_t idx = 0; idx < numOfInputTensors; idx++) {
            needToConvertOperands[idx] = true;  // A tensor
        }

        break;

    case EDEN_OP_DIV:  // 30
        // Determine # of inputs and outputs
        numOfInputTensors = 2;
        numOfInputs = numOfInputTensors + 1;  // +1 for DivOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A tensor(input)
        needToConvertOperands[1] = true;  // A tensor(input)

        break;

    case EDEN_OP_MEAN:  // 31
        // Determine # of inputs and outputs
        // @todo Currently there is no specification for EdenModel
        // now just follow the specification of Android NN Model
        numOfInputTensors = androidOperation.inputs.size();
        numOfInputs = numOfInputTensors + 0;  // No XXXOptions
        if (hasOptions == true) return NO_REQUIRED_OPTION_OPERAND_BUT_THERE_IS;
        numOfOutputs = androidOperation.outputs.size();

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        for (int32_t idx = 0; idx < numOfInputTensors; idx++) {
            needToConvertOperands[idx] = true;  // A tensor
        }

        break;

    case EDEN_OP_CUSTOM:  // 32
        // @todo below separate path is the best? can it be integrated here?
        LOGD(EDEN_DRIVER, "EDEN_OP_CUSTOM should be handled in separate path...\n");
        return RET_OK;

    case EDEN_OP_SPACE_TO_BATCH_ND:  // 33
        // Determine # of inputs and outputs
        // @todo Currently there is no specification for EdenModel
        // now just follow the specification of Android NN Model
        numOfInputTensors = androidOperation.inputs.size();
        numOfInputs = numOfInputTensors + 1;  // +1 for SpaceToBatchNDOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = androidOperation.outputs.size();

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        for (int32_t idx = 0; idx < numOfInputTensors; idx++) {
            needToConvertOperands[idx] = true;  // A tensor
        }

        break;

    case EDEN_OP_SQUEEZE:  // 34
        // Determine # of inputs and outputs
        numOfInputTensors = 1;
        numOfInputs = numOfInputTensors + 1;  // +1 for SqueezeOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A n-D tensor(input)

        break;

    case EDEN_OP_STRIDED_SLICE:  // 35
        // Determine # of inputs and outputs
        numOfInputTensors = 4;
        numOfInputs = numOfInputTensors + 1;  // +1 for StridedSliceOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A n-D tensor(input)
        needToConvertOperands[1] = true;  // A 1-D tensor(begin)
        needToConvertOperands[2] = true;  // A 1-D tensor(end)
        needToConvertOperands[3] = true;  // A 1-D tensor(strides)

        break;

    case EDEN_OP_SUB:  // 36
        // Determine # of inputs and outputs
        numOfInputTensors = 2;
        numOfInputs = numOfInputTensors + 1;  // +1 for SubOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A n-D tensor(input)
        needToConvertOperands[1] = true;  // A n-D tensor(input)

        break;

    case EDEN_OP_TRANSPOSE:  // 37
        // Determine # of inputs and outputs
        numOfInputTensors = 1;
        numOfInputs = numOfInputTensors + 1;  // +1 for TransposeOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A n-D tensor(input)

        break;
    case EDEN_OP_PRELU:  // 38
        // Determine # of inputs and outputs
        numOfInputTensors = 2;
        numOfInputs = numOfInputTensors + 0;  // No PReluOptions
        if (hasOptions == true) return NO_REQUIRED_OPTION_OPERAND_BUT_THERE_IS;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A n-D tensor(input)
        needToConvertOperands[1] = true;  // A n-D tensor(alpha)
        break;


    case EDEN_OP_BIDIRECTIONAL_SEQUENCE_LSTM:  // 52
        // Determine # of inputs and outputs
        numOfInputTensors = androidOperation.inputs.size() - 5;
        numOfInputs = numOfInputTensors + 1;
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = androidOperation.outputs.size();

        for (int i = 0; i< 48; i++) {
            needToConvertOperands[i] = true;  // A tensor(input)
        }
        for (int i = 53; i< (int)androidOperation.inputs.size(); i++) {
            needToConvertOperands[i] = true;  // A tensor(input)
        }
        break;
    case EDEN_OP_UNIDIRECTIONAL_SEQUENCE_LSTM:  // 53
        // Determine # of inputs and outputs
        numOfInputTensors = androidOperation.inputs.size() - 4;
        numOfInputs = numOfInputTensors + 1;
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = androidOperation.outputs.size();
        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        for (int i = 0; i< 20; i++) {
            needToConvertOperands[i] = true;  // A tensor(input)
        }
        for (int i = 24; i< (int)androidOperation.inputs.size(); i++) {
            needToConvertOperands[i] = true;  // A tensor(input)
        }
        break;
    case EDEN_OP_LOGICAL_NOT:  // 58
        // Determine # of inputs and outputs
        numOfInputTensors = 1;
        numOfInputs = numOfInputTensors;
        if (hasOptions == true) return NO_REQUIRED_OPTION_OPERAND_BUT_THERE_IS;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A tensor(input)

        break;
    case EDEN_OP_ROI_ALIGN:  // 59
        // Determine # of inputs and outputs
        numOfInputTensors = 3;
        numOfInputs = numOfInputTensors + 1;
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A tensor(input)
        needToConvertOperands[1] = true;  // A tensor(roi)
        needToConvertOperands[2] = true;  // A tensor(batch)

        break;
    case EDEN_OP_GENERATE_PROPOSALS:  // 60
        // Determine # of inputs and outputs
        numOfInputTensors = 4;
        numOfInputs = numOfInputTensors + 1;  // +1 for MulOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 3;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A tensor(scores)
        needToConvertOperands[1] = true;  // A tensor(bboxDeltas)
        needToConvertOperands[2] = true;  // A tensor(anchors)
        needToConvertOperands[3] = true;  // A tensor(imageInfo)

        break;
    case EDEN_OP_RESIZE_NEAREST_NEIGHBOR:  // 61
        // Determine # of inputs and outputs
        numOfInputTensors = 1;
        numOfInputs = numOfInputTensors + 1;
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A tensor(input)
        break;
    case EDEN_OP_AXIS_ALIGNED_BBOX_TRANSFORM:  // 82
        // Determine # of inputs and outputs
        // @todo Currently there is no specification for EdenModel
        // now just follow the specification of Android NN Model
        numOfInputTensors = androidOperation.inputs.size();
        numOfInputs = numOfInputTensors + 0;  // No XXXOptions
        if (hasOptions == true) return NO_REQUIRED_OPTION_OPERAND_BUT_THERE_IS;
        numOfOutputs = androidOperation.outputs.size();

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        for (int32_t idx = 0; idx < numOfInputTensors; idx++) {
            needToConvertOperands[idx] = true;  // A tensor
        }

        break;
    case EDEN_OP_ARGMAX:  // 40
        // Determine # of inputs and outputs
        numOfInputTensors = 1;
        numOfInputs = numOfInputTensors + 1;  // +1 for ArgmaxOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A n-D tensor(input)

        break;

    case EDEN_OP_DECONV_2D:  // 49
        // Determine # of inputs and outputs
        numOfInputTensors = 3;
        numOfInputs = numOfInputTensors + 1;  // +1 for Deconv2DOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A 4-D tensor(input)
        needToConvertOperands[1] = true;  // A 4-D tensor(filter)
        needToConvertOperands[2] = true;  // A 1-D tensor(bias)

        break;
    case EDEN_OP_BIDIRECTIONAl_RNN:  // 54
        // Determine # of inputs and outputs
        numOfInputTensors = 12;
        numOfInputs = numOfInputTensors + 1;  // +1 for BidirectionalRNNOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = androidOperation.outputs.size();

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        for (int i = 0; i< numOfInputTensors; i++) {
            needToConvertOperands[i] = true;  // A tensor(input)
        }

        break;
    case EDEN_OP_UNIDIRECTIONAL_SEQUENCE_RNN:  // 55
        // Determine # of inputs and outputs
        numOfInputTensors = androidOperation.inputs.size()- 2;
        numOfInputs = numOfInputTensors + 1;  // +1 for BidirectionalRNNOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = androidOperation.outputs.size();
        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        for (int i = 0; i< numOfInputTensors; i++) {
            needToConvertOperands[i] = true;  // A tensor(input)
        }
        break;
    case EDEN_OP_NEG:  // 68
        // Determine # of inputs and outputs
        numOfInputTensors = 1;
        numOfInputs = numOfInputTensors + 0;  // No NegOptions
        if (hasOptions == true) return NO_REQUIRED_OPTION_OPERAND_BUT_THERE_IS;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A n-D tensor(input)

        break;

    case EDEN_OP_SIN:  // 75
        // Determine # of inputs and outputs
        numOfInputTensors = 1;
        numOfInputs = numOfInputTensors + 0;  // No XXXOptions
        if (hasOptions == true) return NO_REQUIRED_OPTION_OPERAND_BUT_THERE_IS;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A n-D tensor(input)

        break;

    case EDEN_OP_RSQRT:  // 76
        // Determine # of inputs and outputs
        numOfInputTensors = 1;
        numOfInputs = numOfInputTensors + 0;  // No XXXOptions
        if (hasOptions == true) return NO_REQUIRED_OPTION_OPERAND_BUT_THERE_IS;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A n-D tensor(input)

        break;

    case EDEN_OP_PAD_V2:  // 77
        // Determine # of inputs and outputs
        numOfInputTensors = 3;
        numOfInputs = numOfInputTensors + 0;  // No XXXOptions
        if (hasOptions == true) return NO_REQUIRED_OPTION_OPERAND_BUT_THERE_IS;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A n-D tensor(input)
        needToConvertOperands[1] = true;  // A n-D tensor(padding)
        needToConvertOperands[2] = true;  // A scalar(padvalue)

        break;

    case EDEN_OP_TOPK_V2:  // 78
        // Determine # of inputs and outputs
        numOfInputTensors = 1;
        numOfInputs = numOfInputTensors + 1;  // +1 for TopK_V2Options
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 2;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A n-D tensor(input)

        break;

    case EDEN_OP_TFLITEROIPOOL:  // 79
        // Determine # of inputs and outputs
        numOfInputTensors = 3;
        numOfInputs = numOfInputTensors + 1;  // +1 for TFliteRoiPoolOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A n-D tensor(input)
        needToConvertOperands[1] = true;  // A n-D tensor(input)
        needToConvertOperands[2] = true;  // A n-D tensor(input)

        break;

    case EDEN_OP_ARGMIN:  // 80
        // Determine # of inputs and outputs
        numOfInputTensors = 1;
        numOfInputs = numOfInputTensors + 1;  // +1 for ArgminOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A n-D tensor(input)

        break;

    case EDEN_OP_SQRT:  // 81
        // Determine # of inputs and outputs
        numOfInputTensors = 1;
        numOfInputs = numOfInputTensors + 0;  // No XXXOptions
        if (hasOptions == true) return NO_REQUIRED_OPTION_OPERAND_BUT_THERE_IS;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A n-D tensor(input)

        break;

    case EDEN_OP_BOX_WITH_NMS_LIMIT:  // 82
        // Determine # of inputs and outputs
        numOfInputTensors = 3;
        numOfInputs = numOfInputTensors + 1;  // +1 for BoxWithNmsLimitOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 4;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A n-D tensor(input)
        needToConvertOperands[1] = true;  // A n-D tensor(input)
        needToConvertOperands[2] = true;  // A n-D tensor(input)

        break;

#if 0
    // @todo
    case EDEN_OP_PRELU:  // 38
    case EDEN_OP_ELEMENTWISE_MAX:  // 39
    case EDEN_OP_SCALE:  // 41
    case EDEN_OP_CROP:  // 42
    case EDEN_OP_FLATTEN:  // 43
    case EDEN_OP_PERMUTE:  // 44
    case EDEN_OP_SLICE:  // 45
    case EDEN_OP_PRIORBOX:  // 46
    case EDEN_OP_POWER:  // 47
#endif
    case EDEN_OP_PAD:  // 48
        // Determine # of inputs and outputs
        numOfInputTensors = 2;
        numOfInputs = numOfInputTensors + 0;  // No XXXOptions
        if (hasOptions == true) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A n-D tensor(input)
        needToConvertOperands[1] = true;  // A 2-D tensor(padding)

        break;

#if 0
    case EDEN_OP_DETECTION:  // 50
    case EDEN_OP_ROIPOOL:  // 51
    case EDEN_OP_LAYER_NORM_LSTM:  // 56
    case EDEN_OP_TFDETECTION:  // 57
#endif
    case EDEN_OP_GREATER:  // 63
    case EDEN_OP_GREATER_EQUAL:  // 64
    case EDEN_OP_EQUAL:  // 65
    case EDEN_OP_NOT_EQUAL:  // 66
    case EDEN_OP_LOGICAL_AND:  // 89
    case EDEN_OP_LOGICAL_OR:  // 90
    case EDEN_OP_LESS_EQUAL:  // 96
    case EDEN_OP_LESS:  // 105
        // Determine # of inputs and outputs
        numOfInputTensors = 2;
        numOfInputs = numOfInputTensors + 0;  // No NotEqualOptions
        if (hasOptions == true) return NO_REQUIRED_OPTION_OPERAND_BUT_THERE_IS;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A n-D tensor(input)
        needToConvertOperands[1] = true;  // A n-D tensor(input)

        break;

    case EDEN_OP_EXPAND_DIMS:  // 69
        // Determine # of inputs and outputs
        numOfInputTensors = 1;
        numOfInputs = numOfInputTensors + 1;  // +1 for ExpandDimsOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A n-D tensor(input)

        break;

    case EDEN_OP_GATHER:  // 70
        // Determine # of inputs and outputs
        numOfInputTensors = 2;
        numOfInputs = numOfInputTensors + 1;  // +1 for GatherOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A n-D tensor(input)
        needToConvertOperands[2] = true;  // A n-D tensor(input)

        break;

    case EDEN_OP_SELECT:  // 71
        // Determine # of inputs and outputs
        numOfInputTensors = 3;
        numOfInputs = numOfInputTensors + 0;  // No SelectOptions
        if (hasOptions == true) return NO_REQUIRED_OPTION_OPERAND_BUT_THERE_IS;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A n-D tensor(input)
        needToConvertOperands[1] = true;  // A n-D tensor(input)
        needToConvertOperands[2] = true;  // A n-D tensor(input)

        break;

    case EDEN_OP_SPLIT:  // 72
        // Determine # of inputs and outputs
        numOfInputTensors = 1;
        numOfInputs = numOfInputTensors + 1;  // +1 for SplitOptions
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = androidOperation.outputs.size();

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A n-D tensor(input)

        break;

    case EDEN_OP_POW:  // 73
        // Determine # of inputs and outputs
        numOfInputTensors = 2;
        numOfInputs = numOfInputTensors + 0;  // No PowOptions
        if (hasOptions == true) return NO_REQUIRED_OPTION_OPERAND_BUT_THERE_IS;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A n-D tensor(input)
        needToConvertOperands[1] = true;  // A n-D tensor(input)

        break;

    case EDEN_OP_QUANTIZED_16BIT_LSTM:  // 86
        // Determine # of inputs and outputs
        numOfInputTensors = 15;
        numOfInputs = numOfInputTensors + 0;  // No Quantized16BitLstmOptions
        if (hasOptions == true) return NO_REQUIRED_OPTION_OPERAND_BUT_THERE_IS;
        numOfOutputs = 2;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        for (int32_t idx = 0; idx < numOfInputTensors; idx++) {
            needToConvertOperands[idx] = true;  // A tensor
        }
        break;
    case EDEN_OP_ELEMENTWISE_MAX: // 39
    case EDEN_OP_MINIMUM:  // 67
        // Determine # of inputs and outputs
        numOfInputTensors = 2;
        numOfInputs = numOfInputTensors + 0;  // +1 for ElementwiseMaxOptions
        if (hasOptions == true) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;
        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A n-D tensor(input)
        needToConvertOperands[1] = true;  // A n-D tensor(input)
        break;
    case EDEN_OP_REDUCE_SUM:  // 97
    case EDEN_OP_REDUCE_MIN:  // 98
    case EDEN_OP_REDUCE_MAX:  // 99
    case EDEN_OP_REDUCE_PROD:  // 100
    case EDEN_OP_REDUCE_ALL:  // 101
    case EDEN_OP_REDUCE_ANY:  // 102
        // Determine # of inputs and outputs
        numOfInputTensors = androidOperation.inputs.size();
        numOfInputs = numOfInputTensors + 0;  // No XXXOptions
        if (hasOptions == true) return NO_REQUIRED_OPTION_OPERAND_BUT_THERE_IS;
        numOfOutputs = 1;
        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        for (int32_t idx = 0; idx < numOfInputTensors; idx++) {
            needToConvertOperands[idx] = true;  // A tensor
        }
        break;
    case EDEN_OP_TILE:  // 103
        // Determine # of inputs and outputs
        numOfInputTensors = 2;
        numOfInputs = numOfInputTensors + 0;  // No XXXOptions
        if (hasOptions == true) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;
        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A n-D tensor(input)
        needToConvertOperands[1] = true;  // A n-D tensor(multipliers)
        break;
    case EDEN_OP_TF_SLICE:  // 104
        // Determine # of inputs and outputs
        numOfInputTensors = 3;
        numOfInputs = numOfInputTensors + 0;  // No XXXOptions
        if (hasOptions == true) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;
        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // A n-D tensor(input)
        needToConvertOperands[1] = true;  // A 1-D tensor(begin)
        needToConvertOperands[2] = true;  // A 1-D tensor(size)
        break;
    case EDEN_OP_CHANNEL_SHUFFLE:  // 94
        // Determine # of inputs and outputs
        numOfInputTensors = 1;
        // +1 for builtin options
        numOfInputs = numOfInputTensors + 1;
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;

        break;

    case EDEN_OP_DETECTION_POSTPROCESSING:  // 94
        // Tensor and ANCHORS
        numOfInputTensors = 3;
        // +1 for builtin options
        numOfInputs = numOfInputTensors + 1;
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 4;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // 3-D Tensor: [batches, num_anchors, num_classes]
        needToConvertOperands[1] = true;  // 3-D Tensor: [batches, num_anchors, length_box_encoding]
        needToConvertOperands[2] = true;  // 2-D Tensor: [num_anchors, 4]

        break;

    case EDEN_OP_HEATMAP_MAX_KEYPOINT_OP:  // 92
        // Determine # of inputs and outputs
        numOfInputTensors = 2;
        // +1 for builtin options
        numOfInputs = numOfInputTensors + 1;
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 2;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;
        needToConvertOperands[1] = true;

        break;

    case EDEN_OP_INSTANCE_NORMALIZATION:  // 85
        // Tensor
        numOfInputTensors = 1;
        // +1 for builtin options
        numOfInputs = numOfInputTensors + 1;
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // n-D Tensor

        break;

    case EDEN_OP_RANDOM_MULTINOMIAL:  // 95
        // Tensor
        numOfInputTensors = 1;
        // +1 for builtin options
        numOfInputs = numOfInputTensors + 1;
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // 2-D Tensor

        break;
    case EDEN_OP_LOG_SOFTMAX:  // 93
        // Tensor
        numOfInputTensors = 1;
        // +1 for builtin options
        numOfInputs = numOfInputTensors + 1;
        if (hasOptions == false) return NO_REQUIRED_OPTION_OPERAND;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;  // n-D Tensor

        break;

    // unary OPs without builtin options follow the same settings.
    case EDEN_OP_ABS:  // 62
    case EDEN_OP_LOG:  // 74
    case EDEN_OP_EXP:  // 83
    case EDEN_OP_CAST:  // 91
    case EDEN_OP_QUANTIZE:  // 87
        // Determine # of inputs and outputs
        numOfInputTensors = 1;
        // no builtin options
        numOfInputs = numOfInputTensors;
        if (hasOptions == true) return NO_REQUIRED_OPTION_OPERAND_BUT_THERE_IS;
        numOfOutputs = 1;

        // Determine which Android Operands are converted to Eden Operands as inputs
        // if needToConvertOperands[idx] == true,
        // it means androidOperation->inputs[idx] would be converted to EdenOperands
        needToConvertOperands[0] = true;

        break;
    default:
        LOGD(EDEN_DRIVER, "Converting to edenOpType=%d is not yet fully supported", edenOpType);
        numOfInputTensors = androidOperation.inputs.size();
        numOfInputs = numOfInputTensors + 0;  // No XXXOptions
        numOfOutputs = androidOperation.outputs.size();
        break;
    }

    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
    return RET_OK;
}

int32_t getInputOutputForEdenOperation(const V1_2::Model& model, ModelInfo& modelInfo, const V1_2::Operation& androidOperation, int32_t configOperandId,
                                       EdenOperation* edenOperation, EdenModel* edenModel) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    int32_t& numOfInputs = edenOperation->numOfInputs;
    int32_t*& inputOperandIndexes = edenOperation->inputOperandIndexes;
    int32_t& numOfOutputs = edenOperation->numOfOutputs;
    int32_t*& outputOperandIndexes = edenOperation->outputOperandIndexes;
    bool& hasOptions = edenOperation->hasOptions;
    hasOptions = (configOperandId != -1);

    int32_t edenOpType = edenOperation->opType;
    bool isNCHW = edenOperation->isNCHW;

    int32_t numOfInputTensors = 0;
    std::vector<bool> needToConvertOperands(androidOperation.inputs.size(), false);

    int32_t ret = loadParamsForEdenOperation(edenOpType, hasOptions, androidOperation, numOfInputTensors, numOfInputs, numOfOutputs, needToConvertOperands);
    if (ret != RET_OK) return ret;

    LOGD(EDEN_DRIVER, "edenOpType:%d, numOfInputs:%d, numOfOutputs:%d\n", edenOpType, numOfInputs, numOfOutputs);
    // show Android NN Operation
    {
        LOGD(EDEN_DRIVER, "Show Android NN Operation...\n");
        LOGD(EDEN_DRIVER, "type:%d\n", static_cast<int32_t>(androidOperation.type));
        LOGD(EDEN_DRIVER, "androidOperation.inputs.size():%zu\n", androidOperation.inputs.size());
        for (size_t idx = 0; idx < androidOperation.inputs.size(); idx++) {
            int32_t androidInputOperandIndex = androidOperation.inputs[idx];
            LOGD(EDEN_DRIVER, "androidInputOperandIndex:%d\n", androidInputOperandIndex);
        }
        LOGD(EDEN_DRIVER, "androidOperation.outputs.size():%zu\n", androidOperation.outputs.size());
        for (size_t idx = 0; idx < androidOperation.outputs.size(); idx++) {
            int32_t androidOutputOperandIndex = androidOperation.outputs[idx];
            LOGD(EDEN_DRIVER, "androidOutputOperandIndex:%d\n", androidOutputOperandIndex);
        }
        LOGD(EDEN_DRIVER, "Show Android NN Operation...Done!\n");
    }

    // Update input indexes
    std::vector<int32_t> vecInputOperandIndexes;
    for (size_t idx = 0; idx < needToConvertOperands.size(); idx++) {
        int32_t androidInputOperandIndex = androidOperation.inputs[idx];
        int32_t edenOpType = getMatchedEdenOperationType(androidOperation.type);
        if (needToConvertOperands[idx]) {
            int32_t edenInputOperandId = getEdenOperandIdx(model, modelInfo, edenModel, androidInputOperandIndex, edenOpType, isNCHW);
            reduceDimensions(androidOperation, model, androidInputOperandIndex, edenInputOperandId, edenModel);
            vecInputOperandIndexes.push_back(edenInputOperandId);

            // Update input consumer
            auto iter = std::find(modelInfo.modelInputIndexes.begin(), modelInfo.modelInputIndexes.end(), androidInputOperandIndex);
            if (iter != modelInfo.modelInputIndexes.end()) {
                // Since lifetime is MODEL_INPUT, real data is delivered via Request at execution time,
                // Below mapping should be kept to load it at execution time.
                int32_t androidOperationIndex = modelInfo.mapOperationToOperationId.at((void*)&androidOperation);
                ret = keepInputConsumers(model, modelInfo, androidInputOperandIndex, androidOperationIndex);
                if (ret != RET_OK) return ret;
            }
        } else {
            LOGD(EDEN_DRIVER, "(androidInputOperandIndex=%d, No need to convert androidInputOperand", androidInputOperandIndex);
            continue;
        }
    }
    if (hasOptions) {
        vecInputOperandIndexes.push_back(configOperandId);
    }

    if (static_cast<int32_t>(vecInputOperandIndexes.size()) != numOfInputs) {
        LOGE(EDEN_DRIVER, "Oops, # of inputs from converted and configured are different!! Please check it!");
        return CONVERTED_NUM_OF_OPERAND_IS_DIFFERENT;
    }

    // Now load converted input indexes on EdenOperation.inputOperandIndexes
    inputOperandIndexes = new int32_t[numOfInputs];  // numOfInputs contains +1 for options if it exists
    for (int32_t idx = 0; idx < numOfInputs; idx++) {
        inputOperandIndexes[idx] = vecInputOperandIndexes[idx];
    }

    // Update output indexes
    outputOperandIndexes = new int32_t[numOfOutputs];
    for (int32_t idx = 0; idx < numOfOutputs; idx++) {
        int32_t androidOutputOperandIndex = androidOperation.outputs[idx];
        int32_t edenOpType = getMatchedEdenOperationType(androidOperation.type);
        int32_t edenOutputOperandIndex = getEdenOperandIdx(model, modelInfo, edenModel, androidOutputOperandIndex, edenOpType, isNCHW);
        reduceDimensions(androidOperation, model, androidOutputOperandIndex, edenOutputOperandIndex, edenModel);

        outputOperandIndexes[idx] = edenOutputOperandIndex;
    }

    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
    return RET_OK;
}

void setEdenOperandNamesForNull(EdenModel* edenModel) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    auto edenOperands = edenModel->GetOperands();
    for (EdenOperand* edenOperand : edenOperands) {
        if (edenOperand->name.name == nullptr) {
            getMatchedEdenOperandName(TENSOR, edenOperand->name.name, edenOperand->name.length);
            LOGD(EDEN_DRIVER, "> Named to %s\n", edenOperand->name.name);
        }
    }

    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void setEdenOperandNamesForInput(EdenModel* edenModel, EdenOperation* edenOperation) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    int32_t edenOpType = edenOperation->opType;
    std::vector<int32_t> nameIndexesForInput(edenOperation->numOfInputs, TENSOR);
    int32_t* indexesForInput = nullptr;
    int32_t numOfIndexesForInput = 0;
    bool bForceUpdate = false;

    LOGD(EDEN_DRIVER, "edenOpType:%d, edenOperation->numOfInputs:%d\n", edenOpType, edenOperation->numOfInputs);

    switch (edenOpType) {
    case EDEN_OP_ADD:  // 0
    case EDEN_OP_MUL:  // 18
    case EDEN_OP_DIV:  // 30
    case EDEN_OP_SUB:  // 36
    case EDEN_OP_GREATER: // 63
    case EDEN_OP_GREATER_EQUAL: // 64
    case EDEN_OP_EQUAL: // 65
    case EDEN_OP_NOT_EQUAL: // 66
    case EDEN_OP_POW: // 73
    case EDEN_OP_LESS_EQUAL: // 96
    case EDEN_OP_LESS: // 105
    case EDEN_OP_LOGICAL_AND:  // 89
    case EDEN_OP_LOGICAL_OR:  // 90
    case EDEN_OP_HEATMAP_MAX_KEYPOINT_OP:  // 92
    case EDEN_OP_ELEMENTWISE_MAX:  // 39
    case EDEN_OP_MINIMUM: // 67
    {
        // 2 inputs
        static int32_t operandNameIndexesForInput[] = { TENSOR, TENSOR };
        indexesForInput = operandNameIndexesForInput;
        numOfIndexesForInput = (sizeof(operandNameIndexesForInput) / sizeof(int32_t));
        break;
    }
    case EDEN_OP_AVERAGE_POOL_2D:   // 1
    case EDEN_OP_DEPTH_TO_SPACE:    // 5
    case EDEN_OP_FLOOR:             // 8
    case EDEN_OP_L2_NORMALIZATION:  // 11
    case EDEN_OP_L2_POOL_2D:        // 12
    case EDEN_OP_LOCAL_RESPONSE_NORMALIZATION:  // 13
    case EDEN_OP_LOGISTIC:          // 14
    case EDEN_OP_MAX_POOL_2D:       // 17
    case EDEN_OP_RELU:              // 19
    case EDEN_OP_RELU1:             // 20
    case EDEN_OP_RELU6:             // 21
    case EDEN_OP_RESHAPE:           // 22
    case EDEN_OP_RESIZE_BILINEAR:   // 23
    case EDEN_OP_SOFTMAX:           // 25
    case EDEN_OP_SPACE_TO_DEPTH:    // 26
    case EDEN_OP_TANH:              // 28
    case EDEN_OP_SQUEEZE:           // 34
    case EDEN_OP_TRANSPOSE:         // 37
    case EDEN_OP_LOGICAL_NOT:       // 58
    case EDEN_OP_NEG:               // 68
    case EDEN_OP_RESIZE_NEAREST_NEIGHBOR:       // 94
    case EDEN_OP_ARGMAX:            // 40
    case EDEN_OP_SIN:               // 75
    case EDEN_OP_RSQRT:             // 76
    case EDEN_OP_TOPK_V2:           // 78
    case EDEN_OP_ARGMIN:            // 80
    case EDEN_OP_SQRT:              // 81
    case EDEN_OP_ABS:               // 62
    case EDEN_OP_CAST:              // 91
    case EDEN_OP_CHANNEL_SHUFFLE:   // 94
    case EDEN_OP_INSTANCE_NORMALIZATION:  // 85
    case EDEN_OP_RANDOM_MULTINOMIAL:  // 95
    case EDEN_OP_EXP:               // 83
    case EDEN_OP_LOG:               // 74
    case EDEN_OP_LOG_SOFTMAX:       // 93
    case EDEN_OP_QUANTIZE:          // 87
    {
        // 1 input
        static int32_t operandNameIndexesForInput[] = { TENSOR };
        indexesForInput = operandNameIndexesForInput;
        numOfIndexesForInput = (sizeof(operandNameIndexesForInput) / sizeof(int32_t));
        break;
    }
    case EDEN_OP_CONCATENATION:  // 2
    case EDEN_OP_DEQUANTIZE:     // 6
    case EDEN_OP_CUSTOM:         // 32
    case EDEN_OP_SELECT:         // 71
    {
        // n inputs
        break;
    }
    case EDEN_OP_CONV_2D:            // 3
    case EDEN_OP_DEPTHWISE_CONV_2D:  // 4
    case EDEN_OP_DECONV_2D:          // 49
    {
        // 3 inputs, 1 option, 1 output
        bForceUpdate = true;
        static int32_t operandNameIndexesForInput[] = { TENSOR, FILTER, BIAS };
        indexesForInput = operandNameIndexesForInput;
        numOfIndexesForInput = (sizeof(operandNameIndexesForInput) / sizeof(int32_t));
        break;
    }
    case EDEN_OP_EMBEDDING_LOOKUP:  // 7
    {
        LOGD(EDEN_DRIVER, "EDEN_OP_EMBEDDING_LOOKUP\n");
        bForceUpdate = true;
        static int32_t operandNameIndexesForInput[] = { LOOKUPS, VALUES };
        indexesForInput = operandNameIndexesForInput;
        numOfIndexesForInput = (sizeof(operandNameIndexesForInput) / sizeof(int32_t));
        break;
    }
    case EDEN_OP_FULLY_CONNECTED:  // 9
    {
        // 3 inputs, 1 option, 1 output
        // @todo WEIGHT is the right name for second operand but now userdriver use FILTER instead
        bForceUpdate = true;
        LOGD(EDEN_DRIVER, "EDEN_OP_FULLY_CONNECTED\n");
        static int32_t operandNameIndexesForInput[] = { TENSOR, FILTER, BIAS };
        indexesForInput = operandNameIndexesForInput;
        numOfIndexesForInput = (sizeof(operandNameIndexesForInput) / sizeof(int32_t));
        break;
    }
    case EDEN_OP_HASHTABLE_LOOKUP:  // 10
    {
        // 3 inputs
        LOGD(EDEN_DRIVER, "EDEN_OP_HASHTABLE_LOOKUP\n");
        bForceUpdate = true;
        static int32_t operandNameIndexesForInput[] = { LOOKUPS, KEYS, VALUES };
        indexesForInput = operandNameIndexesForInput;
        numOfIndexesForInput = (sizeof(operandNameIndexesForInput) / sizeof(int32_t));
        break;
    }
    case EDEN_OP_LSH_PROJECTION:  // 15
    {
        // 3 inputs
        LOGD(EDEN_DRIVER, "EDEN_OP_LSH_PROJECTION\n");
        bForceUpdate = true;
        static int32_t operandNameIndexesForInput[] = { HASH, TENSOR, WEIGHT };
        indexesForInput = operandNameIndexesForInput;
        numOfIndexesForInput = (sizeof(operandNameIndexesForInput) / sizeof(int32_t));
        break;
    }
    case EDEN_OP_LSTM:  // 16
    {
        // 20 inputs
        LOGD(EDEN_DRIVER, "EDEN_OP_LSTM\n");
        bForceUpdate = true;

        if (edenOperation->numOfInputs == 21) {

            static int32_t operandNameIndexesForInput[] = {
                TENSOR,                            // input, 2-D
                INPUT_TO_INPUT_WEIGTHS,       // input-to-input weights, 2-D
                INPUT_TO_FORGET_WEIGTHS,      // input-to-forget weights, 2-D
                INPUT_TO_CELL_WEIGTHS,        // input-to-cell weights, 2-D
                INPUT_TO_OUTPUT_WEIGTHS,      // input-to-output weights, 2-D

                RECURRENT_TO_INPUT_WEIGTHS,   // recurrent-to-input weights, 2-D
                RECURRENT_TO_FORGET_WEIGTHS,  // recurrent-to-forget weights, 2-D
                RECURRENT_TO_CELL_WEIGTHS,    // recurrent-to-cell weights, 2-D
                RECURRENT_TO_OUTPUT_WEIGTHS,  // recurrent-to-output weights, 2-D

                CELL_TO_INPUT_WEIGTHS,        // cell-to-input, 2-D
                CELL_TO_FORGET_WEIGTHS,       // cell-to-forget, 1-D
                CELL_TO_OUTPUT_WEIGTHS,       // cell-to-output, 1-D

                INPUT_GATE_BIAS,              // input gate bias, 1-D
                FORGET_GATE_BIAS,             // forget gate bias, 1-D
                CELL_GATE_BIAS,               // cell bias, 1-D
                OUTPUT_GATE_BIAS,             // output gate bias, 1-D

                PROJECTION_WEIGHTS,           // projection weights, 2-D
                PROJECTION_BIAS,              // projection bias, 1-D

                INPUT_ACTIVATION_STATE,       // output state, 2-D
                INPUT_CELL_STATE,              // cell state, 2-D
            };

            indexesForInput = operandNameIndexesForInput;
            numOfIndexesForInput = (sizeof(operandNameIndexesForInput) / sizeof(int32_t));

        } else if (edenOperation->numOfInputs == 25) {

            static int32_t operandNameIndexesForInput[] = {
                TENSOR,                            // input, 2-D
                INPUT_TO_INPUT_WEIGTHS,       // input-to-input weights, 2-D
                INPUT_TO_FORGET_WEIGTHS,      // input-to-forget weights, 2-D
                INPUT_TO_CELL_WEIGTHS,        // input-to-cell weights, 2-D
                INPUT_TO_OUTPUT_WEIGTHS,      // input-to-output weights, 2-D

                RECURRENT_TO_INPUT_WEIGTHS,   // recurrent-to-input weights, 2-D
                RECURRENT_TO_FORGET_WEIGTHS,  // recurrent-to-forget weights, 2-D
                RECURRENT_TO_CELL_WEIGTHS,    // recurrent-to-cell weights, 2-D
                RECURRENT_TO_OUTPUT_WEIGTHS,  // recurrent-to-output weights, 2-D

                CELL_TO_INPUT_WEIGTHS,        // cell-to-input, 2-D
                CELL_TO_FORGET_WEIGTHS,       // cell-to-forget, 1-D
                CELL_TO_OUTPUT_WEIGTHS,       // cell-to-output, 1-D

                INPUT_GATE_BIAS,              // input gate bias, 1-D
                FORGET_GATE_BIAS,             // forget gate bias, 1-D
                CELL_GATE_BIAS,               // cell bias, 1-D
                OUTPUT_GATE_BIAS,             // output gate bias, 1-D

                PROJECTION_WEIGHTS,           // projection weights, 2-D
                PROJECTION_BIAS,              // projection bias, 1-D

                INPUT_ACTIVATION_STATE,       // output state, 2-D
                INPUT_CELL_STATE,              // cell state, 2-D

                INPUT_LAYER_NORM_WEIGHTS,
                FORGET_LAYER_NORM_WEIGHTS,
                CELL_LAYER_NORM_WEIGHTS,
                OUTPUT_LAYER_NORM_WEIGHTS,

            };

            indexesForInput = operandNameIndexesForInput;
            numOfIndexesForInput = (sizeof(operandNameIndexesForInput) / sizeof(int32_t));

        } else {
            LOGE(EDEN_DRIVER, "Oops, Cannot Support Such Configuration!! Please check it!");
        }

        break;
    }
    case EDEN_OP_RNN:  // 24
    {
        // 4 inputs
        bForceUpdate = true;
        static int32_t operandNameIndexesForInput[] = { TENSOR, WEIGHTS, RECURRENT_WEIGHTS, BIAS, HIDDEN_STATE };
        indexesForInput = operandNameIndexesForInput;
        numOfIndexesForInput = (sizeof(operandNameIndexesForInput) / sizeof(int32_t));
        break;
    }
    case EDEN_OP_SVDF:  // 27
    {
        // 4 inputs
        bForceUpdate = true;
        static int32_t operandNameIndexesForInput[] = { TENSOR, WEIGHTS_FEATURE, WEIGHTS_TIME, BIAS, STATE };
        indexesForInput = operandNameIndexesForInput;
        numOfIndexesForInput = (sizeof(operandNameIndexesForInput) / sizeof(int32_t));
        break;
    }
    case EDEN_OP_BATCH_TO_SPACE_ND:  // 29
    {
        // @todo Currently there is no specification for EdenModel
        // now just follow the specification of Android NN Model
        // 2 inputs
        static int32_t operandNameIndexesForInput[] = { TENSOR, BLOCKSHAPE };
        indexesForInput = operandNameIndexesForInput;
        numOfIndexesForInput = (sizeof(operandNameIndexesForInput) / sizeof(int32_t));
        break;
    }
    case EDEN_OP_MEAN:  // 31
    {
        // @todo Currently there is no specification for EdenModel
        // now just follow the specification of Android NN Model
        // 2 inputs
        // @todo Currently Android NN specifies 3 inputs, but userdriver only uses 2 inputs.
        // keepDims is ignored
        static int32_t operandNameIndexesForInput[] = { TENSOR, AXIS, KEEP_DIMS };
        indexesForInput = operandNameIndexesForInput;
        numOfIndexesForInput = (sizeof(operandNameIndexesForInput) / sizeof(int32_t));
        break;
    }
    case EDEN_OP_SPACE_TO_BATCH_ND:  // 33
    {
        bForceUpdate = true;
        // @todo Currently there is no specification for EdenModel
        // now just follow the specification of Android NN Model
        // 3 inputs
        static int32_t operandNameIndexesForInput[] = { TENSOR, BLOCK, PAD };
        indexesForInput = operandNameIndexesForInput;
        numOfIndexesForInput = (sizeof(operandNameIndexesForInput) / sizeof(int32_t));
        break;
    }
    case EDEN_OP_STRIDED_SLICE:  // 35
    {
        // @todo Currently there is no specification for EdenModel
        // now just follow the specification of Android NN Model
        // 4 inputs
        static int32_t operandNameIndexesForInput[] = { TENSOR, BEGIN, END, STRIDES };
        indexesForInput = operandNameIndexesForInput;
        numOfIndexesForInput = (sizeof(operandNameIndexesForInput) / sizeof(int32_t));
        break;
    }
    case EDEN_OP_PRELU:  // 38
    {
        // @todo Currently there is no specification for EdenModel
        // now just follow the specification of Android NN Model
        // 2 inputs
        static int32_t operandNameIndexesForInput[] = { TENSOR, TENSOR };
        indexesForInput = operandNameIndexesForInput;
        numOfIndexesForInput = (sizeof(operandNameIndexesForInput) / sizeof(int32_t));
        break;
    }
    case EDEN_OP_BIDIRECTIONAL_SEQUENCE_LSTM:  // 52
    {
        // 61 inputs
        LOGD(EDEN_DRIVER, "EDEN_OP_BIDIRECTIONAL_SEQUENCE_LSTM\n");
        bForceUpdate = true;
        static int32_t operandNameIndexesForInput[] = {
                TENSOR,                            // input, 3-D

                FW_INPUT_TO_INPUT_WEIGHTS,
                FW_INPUT_TO_FORGET_WEIGHTS,
                FW_INPUT_TO_CELL_WEIGHTS,
                FW_INPUT_TO_OUT_WEIGHTS,

                FW_RECURRENT_TO_INPUT_WEIGHTS,
                FW_RECURRENT_TO_FORGET_WEIGHTS,
                FW_RECURRENT_TO_CELL_WEIGHTS,
                FW_RECURRENT_TO_OUTPUT_WEIGHTS,

                FW_CELL_TO_INPUT_WEIGHTS,
                FW_CELL_TO_FORGET_WEIGHTS,
                FW_CELL_TO_OUT_WEIGHTS,

                FW_INPUT_GATE_BIAS,
                FW_FORGET_GATE_BIAS,
                FW_CELL_GATE_BIAS,
                FW_OUTPUT_GATE_BIAS,

                FW_PROJECTION_WEIGHTS,
                FW_PROJECTION_BIAS,

                BW_INPUT_TO_INPUT_WEIGHTS,
                BW_INPUT_TO_FORGET_WEIGHTS,
                BW_INPUT_TO_CELL_WEIGHTS,
                BW_INPUT_TO_OUTPUT_WEIGHTS,

                BW_RECURRENT_TO_INPUT_WEIGHTS,
                BW_RECURRENT_TO_FORGET_WEIGHTS,
                BW_RECURRENT_TO_CELL_WEIGHTS,
                BW_RECURRENT_TO_OUTPUT_WEIGHTS,

                BW_CELL_TO_INPUT_WEIGHTS,
                BW_CELL_TO_FORGET_WEIGHTS,
                BW_CELL_TO_OUTPUT_WEIGHTS,

                BW_INPUT_GATE_BIAS,
                BW_GORGET_GATE_BIAS,
                BW_CELL_GATE_BIAS,
                BW_OUTPUT_GATE_BIAS,

                BW_PROJECTION_WEIGHTS,
                BW_PROJECTION_BIAS,

                FW_INPUT_ACTIVATION_STATE,
                FW_INPUT_CELL_STATE,
                BW_INPUT_ACTIVATION_STATE,
                BW_INPUT_CELL_STATE,

                TENSOR,

                FW_AUX_INPUT_TO_INPUT_WEIGHTS,
                FW_AUX_INPUT_TO_FORGET_WEIGHTS,
                FW_AUX_INPUT_TO_CELL_WEIGHTS,
                FW_AUX_INPUT_TO_OUTPUT_WEIGHTS,

                BW_AUX_INPUT_TO_INPUT_WEIGHTS,
                BW_AUX_INPUT_TO_FORGET_WEIGHTS,
                BW_AUX_INPUT_TO_CELL_WEIGHTS,
                BW_AUX_INPUT_TO_OUTPUT_WEIGHTS,

                FW_INPUT_LAYER_NORM_WEIGHTS,
                FW_FORGET_LAYER_NORM_WEIGHTS,
                FW_CELL_LAYER_NORM_WEIGHTS,
                FW_OUTPUT_LAYER_NORM_WEIGHTS,

                BW_INPUT_LAYER_NORM_WEIGHTS,
                BW_FORGET_LAYER_NORM_WEIGHTS,
                BW_CELL_LAYER_NORM_WEIGHTS,
                BW_OUTPUT_LAYER_NORM_WEIGHTS,
        };
        indexesForInput = operandNameIndexesForInput;
        numOfIndexesForInput = (sizeof(operandNameIndexesForInput) / sizeof(int32_t));
        break;
    }
    case EDEN_OP_UNIDIRECTIONAL_SEQUENCE_LSTM:  // 53
    {
        bForceUpdate = true;
        //24 input
        static int32_t operandNameIndexesForInput[] = {
            TENSOR,

            LSTM_INTOIN,//input_to_input_weights
            LSTM_INTOFORGET,//input_to_forget_weights
            LSTM_INTOCELL,//input_to_cell_weights
            LSTM_INTOOUT,//input_to_output_weights

            LSTM_RETOIN,//recurrent_to_input_weights
            LSTM_RETOFORGET,//recurrent_to_forget_weights
            LSTM_RETOCELL,//recurrent_to_cell_weights
            LSTM_RETOOUT,//recurrent_to_output_weights

            LSTM_CELLTOIN,//cell_to_input_weights
            LSTM_CELLTOFORGET,//cell_to_forget_weights
            LSTM_CELLTOOUT,//cell_to_output_weights

            LSTM_INGATE,//input_gate_bias
            LSTM_FORGETGATE,//forget_gate_bias
            LSTM_CELLGATE,//cell_gate_bias
            LSTM_OUTGATE,//output_gate_bias

            LSTM_PROJWEIGHT,//projection_weights
            LSTM_PROJBIAS,//projection_bias

            LSTM_INACT, //output_state_in?
            LSTM_INCELL,//cell_state_in?

            //activation_param
            //cell_clip_param
            //proj_clip_param
            //time_major_param

            LSTM_INPUT_LAYER_NORM_WEIGHTS,//input_layer_norm_weights
            LSTM_FORGET_LAYER_NORM_WEIGHTS,//forget_layer_norm_weights
            LSTM_CELL_LAYER_NORM_WEIGHTS,//cell_layer_norm_weights
            LSTM_OUTPUT_LAYER_NORM_WEIGHTS,//output_layer_norm_weights

        };
        indexesForInput = operandNameIndexesForInput;
        numOfIndexesForInput = (sizeof(operandNameIndexesForInput) / sizeof(int32_t));
        break;
    }
    case EDEN_OP_ROI_ALIGN:  // 59
    {
        bForceUpdate = true;
        static int32_t operandNameIndexesForInput[] = { TENSOR, TENSOR, TENSOR};
        indexesForInput = operandNameIndexesForInput;
        numOfIndexesForInput = (sizeof(operandNameIndexesForInput) / sizeof(int32_t));
        break;
    }
    case EDEN_OP_GENERATE_PROPOSALS:  // 60
    case EDEN_OP_AXIS_ALIGNED_BBOX_TRANSFORM: // 82
    {
        static int32_t operandNameIndexesForInput[] = { TENSOR, TENSOR, TENSOR, TENSOR };
        indexesForInput = operandNameIndexesForInput;
        numOfIndexesForInput = (sizeof(operandNameIndexesForInput) / sizeof(int32_t));
        break;
    }
    case EDEN_OP_BIDIRECTIONAl_RNN:  // 54
    {
        bForceUpdate = true;
        // 12 inputs
        static int32_t operandNameIndexesForInput[] = {
                TENSOR, // kInputTensor
                FW_WEIGHTS,
                FW_RECURRENT_WEIGHTS,
                FW_BIAS,
                FW_HIDDEN_STATE,

                BW_WEIGHTS,
                BW_RECURRENT_WEIGHTS,
                BW_BIAS,
                BW_HIDDEN_STATE,

                TENSOR, // kAuxInputTensor
                FW_AUX_WEIGHTS,
                BW_AUX_WEIGHTS,
        };
        indexesForInput = operandNameIndexesForInput;
        numOfIndexesForInput = (sizeof(operandNameIndexesForInput) / sizeof(int32_t));
        break;
    }
    case EDEN_OP_UNIDIRECTIONAL_SEQUENCE_RNN:  // 55
    {
        bForceUpdate = true;
        static int32_t operandNameIndexesForInput[] = {
            TENSOR, // kInputTensor
            FILTER,
            RECURRENT_WEIGHTS,
            BIAS,
            HIDDEN_STATE
        };
        indexesForInput = operandNameIndexesForInput;
        numOfIndexesForInput = (sizeof(operandNameIndexesForInput) / sizeof(int32_t));
        break;
    }
    case EDEN_OP_PAD_V2:  // 77
    {
        bForceUpdate = true;
        // 3 inputs
        static int32_t operandNameIndexesForInput[] = { TENSOR, PADDING_SHAPE, PADDING_VALUE };
        indexesForInput = operandNameIndexesForInput;
        numOfIndexesForInput = (sizeof(operandNameIndexesForInput) / sizeof(int32_t));
        break;
    }
    case EDEN_OP_TFLITEROIPOOL:  // 79
    case EDEN_OP_BOX_WITH_NMS_LIMIT:  // 82
    {
        // 3 inputs
        static int32_t operandNameIndexesForInput[] = { TENSOR, TENSOR, TENSOR };
        indexesForInput = operandNameIndexesForInput;
        numOfIndexesForInput = (sizeof(operandNameIndexesForInput) / sizeof(int32_t));
        break;
    }
    case EDEN_OP_DETECTION_POSTPROCESSING:  // 88
    {
        // @todo Currently there is no specification for EdenModel
        // now just follow the specification of Android NN Model
        // 3 inputs
        bForceUpdate = true;  // be forced to update operand name
        static int32_t operandNameIndexesForInput[] = { TENSOR, TENSOR, ANCHORS };
        indexesForInput = operandNameIndexesForInput;
        numOfIndexesForInput = (sizeof(operandNameIndexesForInput) / sizeof(int32_t));
        break;
    }

#if 0
        // @todo
    case EDEN_OP_PRELU:  // 38
    case EDEN_OP_ARGMAX:  // 40
    case EDEN_OP_SCALE:  // 41
    case EDEN_OP_CROP:  // 42
    case EDEN_OP_FLATTEN:  // 43
    case EDEN_OP_PERMUTE:  // 44
    case EDEN_OP_SLICE:  // 45
    case EDEN_OP_PRIORBOX:  // 46
    case EDEN_OP_POWER:  // 47
#endif
    case EDEN_OP_PAD:  // 48
    {
        bForceUpdate = true;
        // 1 input
        static int32_t operandNameIndexesForInput[] = { TENSOR, PADDINGS };
        indexesForInput = operandNameIndexesForInput;
        numOfIndexesForInput = (sizeof(operandNameIndexesForInput) / sizeof(int32_t));
        break;
    }
#if 0
    case EDEN_OP_DECONV_2D:  // 49
    case EDEN_OP_DETECTION:  // 50
    case EDEN_OP_ROIPOOL:  // 51
    case EDEN_OP_LAYER_NORM_LSTM:  // 56
    case EDEN_OP_TFDETECTION:  // 57
#endif

    case EDEN_OP_EXPAND_DIMS:  // 69
    {
        // 1 input
        LOGD(EDEN_DRIVER, "EDEN_OP_EXPAND_DIMS\n");
        bForceUpdate = true;
        static int32_t operandNameIndexesForInput[] = { TENSOR, EXPAND_DIMS };
        indexesForInput = operandNameIndexesForInput;
        numOfIndexesForInput = (sizeof(operandNameIndexesForInput) / sizeof(int32_t));
        break;
    }

    case EDEN_OP_GATHER:  // 70
    {
        // 1 input
        LOGD(EDEN_DRIVER, "EDEN_OP_GATHER\n");
        bForceUpdate = true;
        static int32_t operandNameIndexesForInput[] = { TENSOR, INDICES, GATHER };
        indexesForInput = operandNameIndexesForInput;
        numOfIndexesForInput = (sizeof(operandNameIndexesForInput) / sizeof(int32_t));
        break;
    }

    case EDEN_OP_SPLIT:  // 72
    {
        // 1 input
        LOGD(EDEN_DRIVER, "EDEN_OP_SPLIT\n");
        bForceUpdate = true;
        static int32_t operandNameIndexesForInput[] = { TENSOR, SPLIT };
        indexesForInput = operandNameIndexesForInput;
        numOfIndexesForInput = (sizeof(operandNameIndexesForInput) / sizeof(int32_t));
        break;
    }

    case EDEN_OP_QUANTIZED_16BIT_LSTM:  // 86
    {
        // 1 input
        LOGD(EDEN_DRIVER, "EDEN_OP_QUANTIZED_16BIT_LSTM\n");
        bForceUpdate = true;
        static int32_t operandNameIndexesForInput[] = {
            TENSOR,
            INPUT_TO_INPUT_WEIGTHS,
            INPUT_TO_FORGET_WEIGTHS,
            INPUT_TO_CELL_WEIGTHS,
            INPUT_TO_OUTPUT_WEIGTHS,

            RECURRENT_TO_INPUT_WEIGTHS,
            RECURRENT_TO_FORGET_WEIGTHS,
            RECURRENT_TO_CELL_WEIGTHS,
            RECURRENT_TO_OUTPUT_WEIGTHS,

            INPUT_GATE_BIAS,
            FORGET_GATE_BIAS,
            CELL_GATE_BIAS,
            OUTPUT_GATE_BIAS,

            TENSOR,
            TENSOR
        };
        indexesForInput = operandNameIndexesForInput;
        numOfIndexesForInput = (sizeof(operandNameIndexesForInput) / sizeof(int32_t));
        break;
    }
    case EDEN_OP_REDUCE_SUM:  // 97
    case EDEN_OP_REDUCE_MIN:  // 98
    case EDEN_OP_REDUCE_MAX:  // 9
    case EDEN_OP_REDUCE_PROD:  // 100
    case EDEN_OP_REDUCE_ALL:  // 101
    case EDEN_OP_REDUCE_ANY:  // 102
    {
        // 2 input
        // keepDims is ignored
        static int32_t operandNameIndexesForInput[] = { TENSOR, AXIS, KEEP_DIMS };
        indexesForInput = operandNameIndexesForInput;
        numOfIndexesForInput = (sizeof(operandNameIndexesForInput) / sizeof(int32_t));
        break;
    }
    case EDEN_OP_TILE:  // 103
    {
        bForceUpdate = true;
        // 2 input
        static int32_t operandNameIndexesForInput[] = { TENSOR, MULTIPLIERS };
        indexesForInput = operandNameIndexesForInput;
        numOfIndexesForInput = (sizeof(operandNameIndexesForInput) / sizeof(int32_t));
        break;
    }
    case EDEN_OP_TF_SLICE:  // 104
    {
        bForceUpdate = true;
        // 3 input
        static int32_t operandNameIndexesForInput[] = { TENSOR, BEGIN, SIZE };
        indexesForInput = operandNameIndexesForInput;
        numOfIndexesForInput = (sizeof(operandNameIndexesForInput) / sizeof(int32_t));
        break;
    }

    default:
        break;
    }

    LOGD(EDEN_DRIVER, "numOfIndexesForInput:%d\n", numOfIndexesForInput);
    LOGD(EDEN_DRIVER, "Show operand name indexesForInput like, 0=IFM, 1=TENSOR, 2=FILTER etc\n");
    nameIndexesForInput.resize(numOfIndexesForInput);
    for (int32_t idx = 0; idx < numOfIndexesForInput; idx++) {
        nameIndexesForInput.at(idx) = indexesForInput[idx];
        LOGD(EDEN_DRIVER, "operandNameIndexesForInput[%d]:%d\n", idx, indexesForInput[idx]);
    }

    //setEdenOperandNamesOnInputs(edenModel, edenOperation, nameIndexesForInput, bForceUpdate);
    updateEdenOperandNames(edenModel, edenOperation->inputOperandIndexes, edenOperation->numOfInputs, nameIndexesForInput, bForceUpdate);
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void setEdenOperandNamesForOutput(EdenModel* edenModel, EdenOperation* edenOperation) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    int32_t edenOpType = edenOperation->opType;
    std::vector<int32_t> nameIndexesForOutput(edenOperation->numOfOutputs, TENSOR);
    int32_t* indexesForOutput = nullptr;
    int32_t numOfIndexesForOutput = 0;
    bool bForceUpdate = false;

    LOGD(EDEN_DRIVER, "edenOpType:%d, edenOperation->numOfOutputs:%d\n", edenOpType, edenOperation->numOfOutputs);

    switch (edenOpType) {
#if 0
    case EDEN_OP_ADD:                // 0
    case EDEN_OP_AVERAGE_POOL_2D:    // 1
    case EDEN_OP_CONCATENATION:      // 2
    case EDEN_OP_CONV_2D:            // 3
    case EDEN_OP_DEPTHWISE_CONV_2D:  // 4
    case EDEN_OP_DEPTH_TO_SPACE:     // 5
    case EDEN_OP_DEQUANTIZE:         // 6
    case EDEN_OP_EMBEDDING_LOOKUP:   // 7
    case EDEN_OP_FLOOR:              // 8
    case EDEN_OP_FULLY_CONNECTED:    // 9
#endif

    case EDEN_OP_HASHTABLE_LOOKUP:   // 10
    {
        // 4 outputs
        bForceUpdate = true;
        static int32_t operandNameIndexesForOutput[] = { TENSOR, HITS };
        indexesForOutput = operandNameIndexesForOutput;
        numOfIndexesForOutput = (sizeof(operandNameIndexesForOutput) / sizeof(int32_t));
        break;
    }

#if 0
    case EDEN_OP_L2_NORMALIZATION:   // 11
    case EDEN_OP_L2_POOL_2D:         // 12
    case EDEN_OP_LOCAL_RESPONSE_NORMALIZATION:  // 13
    case EDEN_OP_LOGISTIC:           // 14
    case EDEN_OP_LSH_PROJECTION:     // 15
#endif

    case EDEN_OP_LSTM:               // 16
    {
        // 4 outputs
        bForceUpdate = true;
        static int32_t operandNameIndexesForOutput[] = { SCRATCH_BUFFER, OUTPUT_STATE, CELL_STATE, TENSOR };
        indexesForOutput = operandNameIndexesForOutput;
        numOfIndexesForOutput = (sizeof(operandNameIndexesForOutput) / sizeof(int32_t));
        break;
    }

#if 0
    case EDEN_OP_MAX_POOL_2D:        // 17
    case EDEN_OP_MUL:                // 18
    case EDEN_OP_RELU:               // 19
    case EDEN_OP_RELU1:              // 20
    case EDEN_OP_RELU6:              // 21
    case EDEN_OP_RESHAPE:            // 22
    case EDEN_OP_RESIZE_BILINEAR:    // 23
#endif

    case EDEN_OP_RNN:                // 24
    {
        // 2 outputs
        bForceUpdate = true;
        static int32_t operandNameIndexesForOutput[] = { HIDDEN_STATE, TENSOR };
        indexesForOutput = operandNameIndexesForOutput;
        numOfIndexesForOutput = (sizeof(operandNameIndexesForOutput) / sizeof(int32_t));
        break;
    }

#if 0
    case EDEN_OP_SOFTMAX:            // 25
    case EDEN_OP_SPACE_TO_DEPTH:     // 26
#endif

    case EDEN_OP_SVDF:               // 27
    {
        // 2 outputs
        bForceUpdate = true;
        static int32_t operandNameIndexesForOutput[] = { STATE, TENSOR };
        indexesForOutput = operandNameIndexesForOutput;
        numOfIndexesForOutput = (sizeof(operandNameIndexesForOutput) / sizeof(int32_t));
        break;
    }
#if 0
    case EDEN_OP_TANH:               // 28
    case EDEN_OP_BATCH_TO_SPACE_ND:  // 29
    case EDEN_OP_DIV:                // 30
    case EDEN_OP_MEAN:               // 31
    case EDEN_OP_CUSTOM:             // 32
    case EDEN_OP_SPACE_TO_BATCH_ND:  // 33
    case EDEN_OP_SQUEEZE:            // 34
    case EDEN_OP_STRIDED_SLICE:      // 35
    case EDEN_OP_SUB:                // 36
    case EDEN_OP_TRANSPOSE:          // 37
#endif
#if 0
    // @todo
    case EDEN_OP_PRELU:  // 38
    case EDEN_OP_ARGMAX:  // 40
    case EDEN_OP_SCALE:  // 41
    case EDEN_OP_CROP:  // 42
    case EDEN_OP_FLATTEN:  // 43
    case EDEN_OP_PERMUTE:  // 44
    case EDEN_OP_SLICE:  // 45
    case EDEN_OP_PRIORBOX:  // 46
    case EDEN_OP_POWER:  // 47
#endif
#if 0
    case EDEN_OP_PAD:  // 48
    case EDEN_OP_DECONV_2D:  // 49
    case EDEN_OP_DETECTION:  // 50
    case EDEN_OP_ROIPOOL:  // 51
    case EDEN_OP_BIDIRECTIONAL_SEQUENCE_LSTM:  // 52
    case EDEN_OP_UNIDIRECTIONAL_SEQUENCE_LSTM:  // 53
    case EDEN_OP_BIDIRECTIONAl_RNN:  // 54
    case EDEN_OP_UNIDIRECTIONAL_SEQUENCE_RNN:  // 55
    case EDEN_OP_LAYER_NORM_LSTM:  // 56
    case EDEN_OP_TFDETECTION:  // 57
    case EDEN_OP_TFDETECTION:       // 57
#endif
    default:
        break;
    }

    LOGD(EDEN_DRIVER, "numOfIndexesForOutput:%d\n", numOfIndexesForOutput);
    LOGD(EDEN_DRIVER, "Show operand name indexesForOutput like, 0=IFM, 1=TENSOR, 2=FILTER etc\n");
    nameIndexesForOutput.resize(numOfIndexesForOutput);
    for (int32_t idx = 0; idx < numOfIndexesForOutput; idx++) {
        nameIndexesForOutput.at(idx) = indexesForOutput[idx];
        LOGD(EDEN_DRIVER, "operandNameIndexesForOutput[%d]:%d\n", idx, indexesForOutput[idx]);
    }

    updateEdenOperandNames(edenModel, edenOperation->outputOperandIndexes, edenOperation->numOfOutputs, nameIndexesForOutput, bForceUpdate);
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void setEdenOperandNames(EdenModel* edenModel, EdenOperation* edenOperation) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    setEdenOperandNamesForInput(edenModel, edenOperation);
    setEdenOperandNamesForOutput(edenModel, edenOperation);

    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void setEdenOperandNamesOnInputs(EdenModel* edenModel, EdenOperation* edenOperation, std::vector<int32_t>& vecNameIndexes, bool bForceUpdate) {
    const auto& edenOperands = edenModel->GetMapToOperand();
    LOGD(EDEN_DRIVER, "%s(+), edenOperands.size() :%zu, vecNameIndexes.size() :%zu\n", __func__, static_cast<size_t>(edenOperands.size()), static_cast<size_t>(vecNameIndexes.size()));

    for (size_t idx = 0; idx < vecNameIndexes.size(); idx++) {
        int32_t inputIndex = edenOperation->inputOperandIndexes[idx];
        LOGD(EDEN_DRIVER, "[%zu] inputIndex : %d\n", idx, inputIndex);
        EdenOperand* edenOperand = edenOperands.at(inputIndex);
        if (edenOperand->name.name != nullptr) {
            LOGD(EDEN_DRIVER, "> EdenOperand : %s\n", edenOperand->name.name);
        } else {
            LOGD(EDEN_DRIVER, "> EdenOperand : nullptr\n");
        }
        if ((bForceUpdate == true) && (edenOperand->name.name != nullptr)) {
            LOGD(EDEN_DRIVER, "Force Update -> EdenOperand : %s\n", edenOperand->name.name);
            delete edenOperand->name.name;
            edenOperand->name.name = nullptr;
        }
        if (edenOperand->name.name == nullptr) {
            getMatchedEdenOperandName(vecNameIndexes[idx], edenOperand->name.name, edenOperand->name.length);
            LOGD(EDEN_DRIVER, "> Named to %s\n", edenOperand->name.name);
        }
    }
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

int32_t updateEdenOperandNames(EdenModel* edenModel, int32_t* operandIndexes, int32_t numOfIndexes, std::vector<int32_t>& vecNameIndexes, bool bForceUpdate) {
    const auto& edenOperands = edenModel->GetMapToOperand();
    LOGD(EDEN_DRIVER, "%s(+), edenOperands.size() :%zu, vecNameIndexes.size() :%zu\n", __func__, static_cast<size_t>(edenOperands.size()), static_cast<size_t>(vecNameIndexes.size()));

    if (numOfIndexes < static_cast<int32_t>(vecNameIndexes.size())) {
        LOGE(EDEN_DRIVER, "Oops, vecNameIndexes.size() is greater than numOfIndexes!! Please check it!");
        return NUM_OF_INDEX_IS_NOT_MATCHED;
    }

    for (size_t idx = 0; idx < vecNameIndexes.size(); idx++) {
        int32_t operandIndex = operandIndexes[idx];
        LOGD(EDEN_DRIVER, "[%zu] operandIndex : %d\n", idx, operandIndex);
        EdenOperand* edenOperand = edenOperands.at(operandIndex);
        if (edenOperand->name.name != nullptr) {
            LOGD(EDEN_DRIVER, "> EdenOperand : %s\n", edenOperand->name.name);
        } else {
            LOGD(EDEN_DRIVER, "> EdenOperand : nullptr\n");
        }
        if ((bForceUpdate == true) && (edenOperand->name.name != nullptr)) {
            LOGD(EDEN_DRIVER, "Force Update -> EdenOperand : %s\n", edenOperand->name.name);
            delete edenOperand->name.name;
            edenOperand->name.name = nullptr;
        }
        if (edenOperand->name.name == nullptr) {
            getMatchedEdenOperandName(vecNameIndexes[idx], edenOperand->name.name, edenOperand->name.length);
            LOGD(EDEN_DRIVER, "> Named to %s\n", edenOperand->name.name);
        }
    }
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
    return RET_OK;
}

void showDimensions(std::vector<int32_t>& inputIndexes, EdenModel* edenModel) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    auto& operands = edenModel->GetMapToOperand();
    for (int32_t inputOperandIdx : inputIndexes) {
        EdenOperand* operand = operands.at(inputOperandIdx);

        LOGD(EDEN_DRIVER, "dims[N_NCHW]=%d, dims[C_NCHW]=%d, dims[H_NCHW]=%d, dims[W_NCHW]=%d\n",
                           operand->shapeInfo->dims[N_NCHW], operand->shapeInfo->dims[C_NCHW],
                           operand->shapeInfo->dims[H_NCHW], operand->shapeInfo->dims[W_NCHW]);
    }
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void reorderFromHWToNC(std::vector<int32_t>& inputIndexes, EdenModel* edenModel) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // DEBUG
    if (0) {
        LOGD(EDEN_DRIVER, "Before reorderFromHWToNC...\n");
        //showDimensions(inputIndexes, edenModel);
        LOGD(EDEN_DRIVER, "Before reorderFromHWToNC...Done!\n");
    }

    auto& operands = edenModel->GetMapToOperand();
    for (int32_t inputOperandIdx : inputIndexes) {
        EdenOperand* operand = operands.at(inputOperandIdx);
        if (operand->shapeInfo->numOfDims == 0) continue;
        operand->shapeInfo->dims[N_NCHW] = operand->shapeInfo->dims[H_NCHW];
        operand->shapeInfo->dims[C_NCHW] = operand->shapeInfo->dims[W_NCHW];
        operand->shapeInfo->dims[H_NCHW] = 1;
        operand->shapeInfo->dims[W_NCHW] = 1;
    }

    // DEBUG
    if (0) {
        LOGD(EDEN_DRIVER, "After reorderFromHWToNC...\n");
        //showDimensions(inputIndexes, edenModel);
        LOGD(EDEN_DRIVER, "After reorderFromHWToNC...Done!\n");
    }
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void reorderFromNXHWToNHWX(std::vector<int32_t>& inputIndexes, EdenModel* edenModel) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // DEBUG
    if (0)
    {
        LOGD(EDEN_DRIVER, "Before reorderFromNXHWToNHWX...\n");
        //showDimensions(inputIndexes, edenModel);
        LOGD(EDEN_DRIVER, "Before reorderFromNXHWToNHWX...Done!\n");
    }

    auto& operands = edenModel->GetMapToOperand();
    for (int32_t inputOperandIdx : inputIndexes) {
        EdenOperand* operand = operands.at(inputOperandIdx);
        if (operand->shapeInfo->numOfDims == 0) continue;

        operand->shapeInfo->dims[C_NCHW] = operand->shapeInfo->dims[H_NCHW];
        operand->shapeInfo->dims[H_NCHW] = operand->shapeInfo->dims[W_NCHW];
        operand->shapeInfo->dims[W_NCHW] = 1;
    }

    // DEBUG
    if (0)
    {
        LOGD(EDEN_DRIVER, "After reorderFromHWToNC...\n");
        //showDimensions(inputIndexes, edenModel);
        LOGD(EDEN_DRIVER, "After reorderFromHWToNC...Done!\n");
    }
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void reorderFromNHWCToNCHW(std::vector<int32_t>& inputIndexes, EdenModel* edenModel) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // DEBUG
    if (0)
    {
        LOGD(EDEN_DRIVER, "Before reorderFromNHWCToNCHW...\n");
        //showDimensions(inputIndexes, edenModel);
        LOGD(EDEN_DRIVER, "Before reorderFromNHWCToNCHW...Done!\n");
    }

    auto& operands = edenModel->GetMapToOperand();
    for (int32_t inputOperandIdx : inputIndexes) {
        EdenOperand* operand = operands.at(inputOperandIdx);
        if (operand->shapeInfo->numOfDims == 0) continue;
        uint32_t C = operand->shapeInfo->dims[C_NCHW];
        uint32_t H = operand->shapeInfo->dims[H_NCHW];

        operand->shapeInfo->dims[C_NCHW] = operand->shapeInfo->dims[W_NCHW];
        operand->shapeInfo->dims[H_NCHW] = C;
        operand->shapeInfo->dims[W_NCHW] = H;
    }

    // DEBUG
    if (0)
    {
        LOGD(EDEN_DRIVER, "After reorderFromNHWCToNCHW...\n");
        //showDimensions(inputIndexes, edenModel);
        LOGD(EDEN_DRIVER, "After reorderFromNHWCToNCHW...Done!\n");
    }
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void reorderFromHWNCToNCHW(std::vector<int32_t>& inputIndexes, EdenModel* edenModel) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // DEBUG
    if (0) {
        LOGD(EDEN_DRIVER, "Before reorderFromHWNCToNCHW...\n");
        //showDimensions(inputIndexes, edenModel);
        LOGD(EDEN_DRIVER, "Before reorderFromHWNCToNCHW...Done!\n");
    }

    auto& operands = edenModel->GetMapToOperand();
    for (int32_t inputOperandIdx : inputIndexes) {
        EdenOperand* operand = operands.at(inputOperandIdx);
        if (operand->shapeInfo->numOfDims == 0) continue;

        int32_t batch = operand->shapeInfo->dims[N_NCHW];
        int32_t channel = operand->shapeInfo->dims[C_NCHW];
        int32_t height = operand->shapeInfo->dims[H_NCHW];
        int32_t width = operand->shapeInfo->dims[W_NCHW];
        operand->shapeInfo->dims[N_NCHW] = height;
        operand->shapeInfo->dims[C_NCHW] = width;
        operand->shapeInfo->dims[H_NCHW] = batch;
        operand->shapeInfo->dims[W_NCHW] = channel;
    }

    // DEBUG
    if (0) {
        LOGD(EDEN_DRIVER, "After reorderFromHWNCToNCHW...\n");
        //showDimensions(inputIndexes, edenModel);
        LOGD(EDEN_DRIVER, "After reorderFromHWNCToNCHW...Done!\n");
    }
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void reorderFromNToC(std::vector<int32_t>& inputIndexes, EdenModel* edenModel) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    // DEBUG
    if (0) {
        LOGD(EDEN_DRIVER, "Before reorderFromNToC...\n");
        //showDimensions(inputIndexes, edenModel);
        LOGD(EDEN_DRIVER, "Before reorderFromNToC...Done!\n");
    }

    auto& operands = edenModel->GetMapToOperand();
    for (int32_t inputOperandIdx : inputIndexes) {
        EdenOperand* operand = operands.at(inputOperandIdx);
        if (operand->shapeInfo->numOfDims == 0) continue;

        if (operand->shapeInfo->dims[N_NCHW] != 1 &&
            operand->shapeInfo->dims[H_NCHW] == 1 &&
            operand->shapeInfo->dims[W_NCHW] == 1 &&
            operand->shapeInfo->dims[C_NCHW] == 1) {
            operand->shapeInfo->dims[C_NCHW] = operand->shapeInfo->dims[N_NCHW];
            operand->shapeInfo->dims[N_NCHW] = 1;
            operand->shapeInfo->dims[H_NCHW] = 1;
            operand->shapeInfo->dims[W_NCHW] = 1;
        }
    }

    // DEBUG
    if (0) {
        LOGD(EDEN_DRIVER, "After reorderFromNToC...\n");
        //showDimensions(inputIndexes, edenModel);
        LOGD(EDEN_DRIVER, "After reorderFromNToC...Done!\n");
    }
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void extendRankToFour(ModelInfo& modelInfo, EdenOperand* edenOperand, int32_t rank, int32_t defaultValue, int32_t& mask) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    int32_t* originalValuePtr = reinterpret_cast<int32_t*>(edenOperand->buffer->addr);

    // Update dimensions
    std::unique_ptr<int32_t[]> newDims(new int32_t[4]);
    if (rank == 1) {
        newDims[N_NCHW] = originalValuePtr[0];
        newDims[C_NCHW] = defaultValue;
        newDims[H_NCHW] = defaultValue;
        newDims[W_NCHW] = defaultValue;
    } else if (rank == 2) {
        newDims[N_NCHW] = defaultValue;
        newDims[C_NCHW] = defaultValue;
        newDims[H_NCHW] = originalValuePtr[0];
        newDims[W_NCHW] = originalValuePtr[1];
    } else if (rank == 3) {
        newDims[N_NCHW] = originalValuePtr[0];
        newDims[C_NCHW] = defaultValue;
        newDims[H_NCHW] = originalValuePtr[1];
        newDims[W_NCHW] = originalValuePtr[2];
    } else /* if (rank == 4) */ {
        newDims[N_NCHW] = originalValuePtr[0];
        newDims[C_NCHW] = originalValuePtr[3];
        newDims[H_NCHW] = originalValuePtr[1];
        newDims[W_NCHW] = originalValuePtr[2];
    }

    // Update mask
    if (rank == 1) {
        int32_t bit0 = (mask >> 0) & 0x01;
        mask = (bit0 << N_NCHW) | (0 << C_NCHW) | (0 << H_NCHW) | (0 << W_NCHW);
    } else if (rank == 2) {
        int32_t bit0 = (mask >> 0) & 0x01;
        int32_t bit1 = (mask >> 1) & 0x01;
        mask = (0 << N_NCHW) | (0 << C_NCHW) | (bit0 << H_NCHW) | (bit1 << W_NCHW);
    } else if (rank == 3) {
        int32_t bit0 = (mask >> 0) & 0x01;
        int32_t bit1 = (mask >> 1) & 0x01;
        int32_t bit2 = (mask >> 2) & 0x01;
        mask = (bit0 << N_NCHW) | (0 << C_NCHW) | (bit1 << H_NCHW) | (bit2 << W_NCHW);
    } else /* if (rank == 4) */ {
        int32_t bit0 = (mask >> 0) & 0x01;
        int32_t bit1 = (mask >> 1) & 0x01;
        int32_t bit2 = (mask >> 2) & 0x01;
        int32_t bit3 = (mask >> 3) & 0x01;
        mask = (bit0 << N_NCHW) | (bit3 << C_NCHW) | (bit1 << H_NCHW) | (bit2 << W_NCHW);
    }

    // Replace previous one to new one
    // Since previous data is in operandValues, no need to free.
    edenOperand->buffer->addr = newDims.get();
    edenOperand->buffer->size = sizeof(int32_t) * 4;
    LOGD(EDEN_DRIVER, "newDims = {%d, %d, %d, %d}\n", newDims[N_NCHW], newDims[C_NCHW], newDims[H_NCHW], newDims[W_NCHW]);
    LOGD(EDEN_DRIVER, "mask = %d\n", mask);

    // Keep unique_ptr into internalBuffers to keep it until model is destructed.
    modelInfo.internalBuffers.push_back(std::move(newDims));
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void adjustOperandDims(const V1_2::Model& model, const V1_2::Operation& androidOperation, ModelInfo& modelInfo, EdenOperation* edenOperation, EdenModel* edenModel) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    int32_t* inputOperandIndexes = edenOperation->inputOperandIndexes;

    LOGD(EDEN_DRIVER, "input operand indexes are...\n");
    for (int32_t idx = 0; idx < edenOperation->numOfInputs; idx++) {
        LOGD(EDEN_DRIVER, "inputOperandIndexes[%d]=%d\n", idx, inputOperandIndexes[idx]);
    }

    int32_t* outputOperandIndexes = edenOperation->outputOperandIndexes;

    LOGD(EDEN_DRIVER, "output operand indexes are...\n");
    for (int32_t idx = 0; idx < edenOperation->numOfOutputs; idx++) {
        LOGD(EDEN_DRIVER, "outputOperandIndexes[%d]=%d\n", idx, outputOperandIndexes[idx]);
    }

    int32_t edenOpType = edenOperation->opType;
    LOGD(EDEN_DRIVER, "edenOpType:%d\n", edenOpType);

    switch (edenOpType) {
    case EDEN_OP_CONV_2D:
    case EDEN_OP_DEPTHWISE_CONV_2D:
    case EDEN_OP_DECONV_2D:
    {
        auto &operands = edenModel->GetMapToOperand();
        EdenOperand *filterOperand = operands.at(inputOperandIndexes[1]);
        if (filterOperand->isNCHW) {
            std::vector<int32_t> inputIndexes;
            inputIndexes.push_back(inputOperandIndexes[1]);  // 2(filter)
            reorderFromNHWCToNCHW(inputIndexes, edenModel);
        }
        break;
    }
    case EDEN_OP_ADD:
    case EDEN_OP_SUB:
    case EDEN_OP_MUL:
    case EDEN_OP_DIV:
    case EDEN_OP_LOGICAL_AND:
    case EDEN_OP_LOGICAL_OR:
    case EDEN_OP_MINIMUM:
    case EDEN_OP_ELEMENTWISE_MAX:
    {
        const V1_2::Operand& androidFirstOperand = model.operands[androidOperation.inputs[0]];
        const V1_2::Operand& androidSecondOperand = model.operands[androidOperation.inputs[1]];

        if (androidFirstOperand.dimensions.size() != androidSecondOperand.dimensions.size()) {
            auto& operands = edenModel->GetMapToOperand();
            EdenOperand* inOperandFirst = operands.at(inputOperandIndexes[0]);
            inOperandFirst->shapeInfo->numOfDims = 4;
            EdenOperand* inOperandSecond = operands.at(inputOperandIndexes[1]);
            inOperandSecond->shapeInfo->numOfDims = 4;

            size_t max_dim_size = std::max(androidFirstOperand.dimensions.size(), androidSecondOperand.dimensions.size());
            int i0=0;
            int i1=0;
            for (size_t j = 0; j < max_dim_size; j++) {
                if (j + androidFirstOperand.dimensions.size() < max_dim_size) {
                    inOperandFirst->shapeInfo->dims[j] = 1;
                } else {
                    inOperandFirst->shapeInfo->dims[j] = androidFirstOperand.dimensions[i0];
                    i0++;
                }
                if (j + androidSecondOperand.dimensions.size() < max_dim_size) {
                    inOperandSecond->shapeInfo->dims[j] = 1;
                } else {
                    inOperandSecond->shapeInfo->dims[j] = androidSecondOperand.dimensions[i1];
                    i1++;
                }
            }
            for (size_t i = max_dim_size; i < 4; i++) {
                inOperandFirst->shapeInfo->dims[i] = 1;
                inOperandSecond->shapeInfo->dims[i] = 1;
            }
            std::vector<int32_t> inputIndexes;
            inputIndexes.push_back(inputOperandIndexes[0]);
            inputIndexes.push_back(inputOperandIndexes[1]);
            if (max_dim_size == 2) {
                reorderFromHWToNC(inputIndexes, edenModel);
            } else {
                reorderFromNHWCToNCHW(inputIndexes, edenModel);
            }
        }

        break;
    }
    case EDEN_OP_FULLY_CONNECTED:  // 9
    {
        auto& operands = edenModel->GetMapToOperand();
        EdenOperand* inOperand = operands.at(inputOperandIndexes[0]);
        EdenOperand* outOperand = operands.at(outputOperandIndexes[0]);

        // Unknown dimension, unknown rank
        if ((inOperand->shapeInfo->numOfDims == 0) ||
            (outOperand->shapeInfo->numOfDims == 0)) {
            LOGD(EDEN_DRIVER, "Oops, unknown dimension is detected, skip it!");
            break;
        }

        uint32_t inputSize = inOperand->shapeInfo->dims[N_NCHW] * inOperand->shapeInfo->dims[C_NCHW] *
                             inOperand->shapeInfo->dims[H_NCHW] * inOperand->shapeInfo->dims[W_NCHW];
        uint32_t batchSize = inputSize / outOperand->shapeInfo->dims[H_NCHW];

        inOperand->shapeInfo->dims[H_NCHW] = outOperand->shapeInfo->dims[H_NCHW];
        inOperand->shapeInfo->dims[W_NCHW] = batchSize;

        std::vector<int32_t> inputIndexes;
        inputIndexes.push_back(inputOperandIndexes[0]);  // input, if rank >= 2, reshaped to 2-D
        inputIndexes.push_back(inputOperandIndexes[1]);  // weights, 2-D
        reorderFromHWToNC(inputIndexes, edenModel);

        std::vector<int32_t> outputIndexes;
        outputIndexes.push_back(outputOperandIndexes[0]);  // output, 2-D
        reorderFromHWToNC(outputIndexes, edenModel);

        break;
    }
    case EDEN_OP_HASHTABLE_LOOKUP:   // 10
    {
        auto& operands = edenModel->GetMapToOperand();
        EdenOperand* inOperand = operands.at(inputOperandIndexes[0]);
        EdenOperand* outOperand = operands.at(outputOperandIndexes[0]);

        int input_size = inOperand->shapeInfo->dims[N_NCHW] * inOperand->shapeInfo->dims[C_NCHW] *
                             inOperand->shapeInfo->dims[H_NCHW] * inOperand->shapeInfo->dims[W_NCHW];

        inOperand = operands.at(inputOperandIndexes[1]);
        int key_size = inOperand->shapeInfo->dims[N_NCHW] * inOperand->shapeInfo->dims[C_NCHW] *
                             inOperand->shapeInfo->dims[H_NCHW] * inOperand->shapeInfo->dims[W_NCHW];

        inOperand = operands.at(inputOperandIndexes[2]);
        int value_size = inOperand->shapeInfo->dims[N_NCHW] * inOperand->shapeInfo->dims[C_NCHW] *
                             inOperand->shapeInfo->dims[H_NCHW] * inOperand->shapeInfo->dims[W_NCHW];

        inOperand->shapeInfo->dims[N_NCHW] = key_size;
        inOperand->shapeInfo->dims[C_NCHW] = value_size / key_size;
        inOperand->shapeInfo->dims[H_NCHW] = 1;
        inOperand->shapeInfo->dims[W_NCHW] = 1;

        int out_one = outOperand->shapeInfo->dims[N_NCHW] * outOperand->shapeInfo->dims[C_NCHW] *
                             outOperand->shapeInfo->dims[H_NCHW] * outOperand->shapeInfo->dims[W_NCHW];
        outOperand->shapeInfo->dims[N_NCHW] = input_size;
        outOperand->shapeInfo->dims[C_NCHW] = out_one / input_size;
        outOperand->shapeInfo->dims[H_NCHW] = 1;
        outOperand->shapeInfo->dims[W_NCHW] = 1;

        break;
    }
    case EDEN_OP_EMBEDDING_LOOKUP:
    {
        size_t inputSize = model.operands[androidOperation.inputs[1]].dimensions.size();
        if (inputSize==2) {
          std::vector<int32_t> inputIndexes;
          inputIndexes.push_back(inputOperandIndexes[1]);
          reorderFromHWToNC(inputIndexes, edenModel);
        }
        break;
     }
    case EDEN_OP_LSH_PROJECTION:  // 15
    {
        std::vector<int32_t> inputIndexes;
        if (model.operands[androidOperation.inputs[0]].dimensions.size() == 2)
          inputIndexes.push_back(inputOperandIndexes[0]);  // 0(hash)
        if (model.operands[androidOperation.inputs[1]].dimensions.size() == 2)
          inputIndexes.push_back(inputOperandIndexes[1]);  // 1(input)
        if (!inputIndexes.empty())
          reorderFromHWToNC(inputIndexes, edenModel);
        break;
    }
    case EDEN_OP_LSTM:  // 16
    {
        std::vector<int32_t> inputIndexes;
        inputIndexes.push_back(inputOperandIndexes[0]);    // input, 2-D
        inputIndexes.push_back(inputOperandIndexes[1]);    // input-to-input weights, 2-D
        inputIndexes.push_back(inputOperandIndexes[2]);    // input-to-forget weights, 2-D
        inputIndexes.push_back(inputOperandIndexes[3]);    // input-to-cell weights, 2-D
        inputIndexes.push_back(inputOperandIndexes[4]);    // input-to-output weights, 2-D
        inputIndexes.push_back(inputOperandIndexes[5]);    // recurrent-to-input weights, 2-D
        inputIndexes.push_back(inputOperandIndexes[6]);    // recurrent-to-forget weights, 2-D
        inputIndexes.push_back(inputOperandIndexes[7]);    // recurrent-to-cell weights, 2-D
        inputIndexes.push_back(inputOperandIndexes[8]);    // recurrent-to-output weights, 2-D
        inputIndexes.push_back(inputOperandIndexes[16]);   // projection weights, 2-D
        inputIndexes.push_back(inputOperandIndexes[18]);   // output state, 2-D
        inputIndexes.push_back(inputOperandIndexes[19]);   // cell state, 2-D
        reorderFromHWNCToNCHW(inputIndexes, edenModel);

        std::vector<int32_t> outputIndexes;
        outputIndexes.push_back(outputOperandIndexes[0]);  // scratch buffer, 2-D
        outputIndexes.push_back(outputOperandIndexes[1]);  // output state, 2-D
        outputIndexes.push_back(outputOperandIndexes[2]);  // cell state, 2-D
        outputIndexes.push_back(outputOperandIndexes[3]);  // output, 2-D
        reorderFromHWToNC(outputIndexes, edenModel);

        break;
    }
    case EDEN_OP_RNN:                // 24
    {
        std::vector<int32_t> inputIndexes;
        inputIndexes.push_back(inputOperandIndexes[0]);  // 0(input)
        inputIndexes.push_back(inputOperandIndexes[1]);  // 1(weights)
        inputIndexes.push_back(inputOperandIndexes[2]);  // 2(recurrent_weights)
        inputIndexes.push_back(inputOperandIndexes[4]);  // 4(hidden_state)
        reorderFromHWToNC(inputIndexes, edenModel);

        std::vector<int32_t> outputIndexes;
        outputIndexes.push_back(outputOperandIndexes[0]);  // 0(hidden_state)
        outputIndexes.push_back(outputOperandIndexes[1]);  // 1(output)
        reorderFromHWToNC(outputIndexes, edenModel);

        break;
    }
    case EDEN_OP_SOFTMAX:            // 25
    case EDEN_OP_LOG_SOFTMAX:
    {
        size_t inputSize = model.operands[androidOperation.inputs[0]].dimensions.size();
        if (inputSize == 2) {
            std::vector<int32_t> inputIndexes;
            inputIndexes.push_back(inputOperandIndexes[0]);  // 0(IFM)
            reorderFromHWToNC(inputIndexes, edenModel);
        }
        break;
    }
    case EDEN_OP_SVDF:               // 27
    {
        std::vector<int32_t> inputIndexes;
        inputIndexes.push_back(inputOperandIndexes[0]);  // 0(input)
        inputIndexes.push_back(inputOperandIndexes[1]);  // 1(weights_feature)
        inputIndexes.push_back(inputOperandIndexes[2]);  // 2(weights_time)
        inputIndexes.push_back(inputOperandIndexes[4]);  // 4(state)
        reorderFromHWToNC(inputIndexes, edenModel);

        std::vector<int32_t> outputIndexes;
        outputIndexes.push_back(outputOperandIndexes[0]);  // 0(state)
        outputIndexes.push_back(outputOperandIndexes[1]);  // 1(output)
        reorderFromHWToNC(outputIndexes, edenModel);

        break;
    }
    case EDEN_OP_MEAN:  // 31
    {
        auto& operands = edenModel->GetMapToOperand();
        EdenOperand* inputOperand = operands.at(inputOperandIndexes[0]);  // input
        EdenOperand* axisOperand = operands.at(inputOperandIndexes[1]);  // axis

        // Unknown dimension, unknown rank
        if (axisOperand->shapeInfo->numOfDims == 0) {
            LOGD(EDEN_DRIVER, "Oops, unknown dimension is detected, skip it!");
            break;
        }

        int32_t inputAndroidDims = (int)model.operands[androidOperation.inputs[0]].dimensions.size();
        inputOperand->shapeInfo->numOfDims = inputAndroidDims;
        std::vector<int32_t> inputIndexes;
        if (inputAndroidDims == 2) {
            inputIndexes.push_back(inputOperandIndexes[0]);
            reorderFromHWToNC(inputIndexes, edenModel);
        }
        break;
    }
    case EDEN_OP_STRIDED_SLICE:      // 35
    {
        auto& operands = edenModel->GetMapToOperand();
        EdenOperand* operandForBegin = operands.at(inputOperandIndexes[1]);
        EdenOperand* operandForEnd = operands.at(inputOperandIndexes[2]);
        EdenOperand* operandForStrides = operands.at(inputOperandIndexes[3]);
        EdenOperand* operandForOptions = operands.at(inputOperandIndexes[4]);

        // Unknown dimension, unknown rank
        if ((operandForBegin->shapeInfo->numOfDims == 0) ||
            (operandForEnd->shapeInfo->numOfDims == 0)   ||
            (operandForStrides->shapeInfo->numOfDims == 0)) {
            LOGD(EDEN_DRIVER, "Oops, unknown dimension is detected, skip it!");
            break;
        }

        StridedSliceOptions* stridedSliceOptions = reinterpret_cast<StridedSliceOptions*>(operandForOptions->buffer->addr);

        int32_t rankForBegin = operandForBegin->shapeInfo->dims[0];
        LOGD(EDEN_DRIVER, "Extending rank for Begin...\n");
        LOGD(EDEN_DRIVER, "name=%s\n", operandForBegin->name.name);

        extendRankToFour(modelInfo, operandForBegin, rankForBegin, 0, stridedSliceOptions->beginMask);
        LOGD(EDEN_DRIVER, "before operandForBegin->shapeInfo->dims[0]=%d\n", operandForBegin->shapeInfo->dims[0]);
        operandForBegin->shapeInfo->dims[0] = 4;
        LOGD(EDEN_DRIVER, "after operandForBegin->shapeInfo->dims[0]=%d\n", operandForBegin->shapeInfo->dims[0]);

        int32_t rankForEnd = operandForEnd->shapeInfo->dims[0];
        LOGD(EDEN_DRIVER, "Extending rank for End...\n");
        LOGD(EDEN_DRIVER, "name=%s\n", operandForEnd->name.name);

        extendRankToFour(modelInfo, operandForEnd, rankForEnd, 1, stridedSliceOptions->endMask);
        LOGD(EDEN_DRIVER, "before operandForEnd->shapeInfo->dims[0]=%d\n", operandForEnd->shapeInfo->dims[0]);
        operandForEnd->shapeInfo->dims[0] = 4;
        LOGD(EDEN_DRIVER, "after operandForEnd->shapeInfo->dims[0]=%d\n", operandForEnd->shapeInfo->dims[0]);

        int32_t rankForStrides = operandForStrides->shapeInfo->dims[0];
        LOGD(EDEN_DRIVER, "Extending rank for Strides...\n");
        LOGD(EDEN_DRIVER, "name=%s\n", operandForStrides->name.name);

        extendRankToFour(modelInfo, operandForStrides, rankForStrides, 1, stridedSliceOptions->shrinkAxisMask);
        LOGD(EDEN_DRIVER, "before operandForStrides->shapeInfo->dims[0]=%d\n", operandForStrides->shapeInfo->dims[0]);
        operandForStrides->shapeInfo->dims[0] = 4;
        LOGD(EDEN_DRIVER, "after operandForStrides->shapeInfo->dims[0]=%d\n", operandForStrides->shapeInfo->dims[0]);

        break;
    }
    case EDEN_OP_PAD_V2:  // 77
    case EDEN_OP_PAD:  // 48
    {
        auto& operands = edenModel->GetMapToOperand();

        if (edenOperation->numOfInputs > 2) {
            EdenOperand* padvalueOperand = operands.at(inputOperandIndexes[2]);
            padvalueOperand->shapeInfo->numOfDims = 1;
            padvalueOperand->shapeInfo->dims[0] = 1;
            padvalueOperand->shapeInfo->dims[1] = 1;
            padvalueOperand->shapeInfo->dims[2] = 1;
            padvalueOperand->shapeInfo->dims[3] = 1;
        }

        // Padding[i,0] indicates ith padding for front.
        // But ith index is meaningful on NHWC.
        // Since data layout is changed to NCHW, this index meaning should be transfered properly.
        EdenOperand* inputOperand = operands.at(inputOperandIndexes[0]);  // input
        EdenOperand* paddingsOperand = operands.at(inputOperandIndexes[1]);  // Paddings
        int32_t* bufferAddr = reinterpret_cast<int32_t*>(paddingsOperand->buffer->addr);

        EdenOperand* outputOperand = operands.at(outputOperandIndexes[0]);

        // max dim is 4
        int32_t dim = model.operands[androidOperation.inputs[0]].dimensions.size();

        // paddingValue is always 4x2 in EDEN
        int32_t paddingSize = 8 * sizeof(int32_t);
        int32_t *paddingValue = nullptr;
        paddingValue = new int32_t[8];
        memset(paddingValue, 0, paddingSize);

        switch (dim) {
            case 1:
                inputOperand->shapeInfo->dims[3] = inputOperand->shapeInfo->dims[0];
                inputOperand->shapeInfo->dims[0] = 1;
                outputOperand->shapeInfo->dims[3] = outputOperand->shapeInfo->dims[0];
                outputOperand->shapeInfo->dims[0] = 1;

                paddingValue[(2 * W_NCHW) + 0] = bufferAddr[(2 * N_NHWC) + 0];
                paddingValue[(2 * W_NCHW) + 1] = bufferAddr[(2 * N_NHWC) + 1];
                break;
            case 2:
                paddingValue[(2 * H_NCHW) + 0] = bufferAddr[(2 * N_NHWC) + 0];
                paddingValue[(2 * H_NCHW) + 1] = bufferAddr[(2 * N_NHWC) + 1];
                paddingValue[(2 * W_NCHW) + 0] = bufferAddr[(2 * H_NHWC) + 0];
                paddingValue[(2 * W_NCHW) + 1] = bufferAddr[(2 * H_NHWC) + 1];
                break;
            case 3:
                paddingValue[(2 * N_NCHW) + 0] = bufferAddr[(2 * N_NHWC) + 0];
                paddingValue[(2 * N_NCHW) + 1] = bufferAddr[(2 * N_NHWC) + 1];
                paddingValue[(2 * H_NCHW) + 0] = bufferAddr[(2 * H_NHWC) + 0];
                paddingValue[(2 * H_NCHW) + 1] = bufferAddr[(2 * H_NHWC) + 1];
                paddingValue[(2 * W_NCHW) + 0] = bufferAddr[(2 * W_NHWC) + 0];
                paddingValue[(2 * W_NCHW) + 1] = bufferAddr[(2 * W_NHWC) + 1];
                break;
            case 4:
                paddingValue[(2 * N_NCHW) + 0] = bufferAddr[(2 * N_NHWC) + 0];
                paddingValue[(2 * N_NCHW) + 1] = bufferAddr[(2 * N_NHWC) + 1];
                paddingValue[(2 * C_NCHW) + 0] = bufferAddr[(2 * C_NHWC) + 0];
                paddingValue[(2 * C_NCHW) + 1] = bufferAddr[(2 * C_NHWC) + 1];
                paddingValue[(2 * H_NCHW) + 0] = bufferAddr[(2 * H_NHWC) + 0];
                paddingValue[(2 * H_NCHW) + 1] = bufferAddr[(2 * H_NHWC) + 1];
                paddingValue[(2 * W_NCHW) + 0] = bufferAddr[(2 * W_NHWC) + 0];
                paddingValue[(2 * W_NCHW) + 1] = bufferAddr[(2 * W_NHWC) + 1];
                break;
            default:
                LOGE(EDEN_DRIVER, "input dim: %d. Max input dim supportted is 4!", dim);
                break;
        }

        paddingsOperand->buffer->size = paddingSize;
        paddingsOperand->buffer->addr = reinterpret_cast<void*>(paddingValue);

        paddingsOperand->shapeInfo->numOfDims = 4;
        paddingsOperand->shapeInfo->dims[2] = 4;
        paddingsOperand->shapeInfo->dims[3] = 2;
        inputOperand->shapeInfo->numOfDims = 4;
        outputOperand->shapeInfo->numOfDims = 4;

        break;
    }
    case EDEN_OP_EXPAND_DIMS:
    {
        auto& operands = edenModel->GetMapToOperand();
        EdenOperand* inputOperand = operands.at(inputOperandIndexes[0]);
        inputOperand->shapeInfo->dims[0] = 1;
        inputOperand->shapeInfo->dims[1] = 1;
        inputOperand->shapeInfo->dims[2] = 1;
        inputOperand->shapeInfo->dims[3] = 1;
        for (size_t i = 0; i < model.operands[androidOperation.inputs[0]].dimensions.size(); ++i) {
            inputOperand->shapeInfo->dims[i] = model.operands[androidOperation.inputs[0]].dimensions[i];
        }

        break;
    }
    case EDEN_OP_BIDIRECTIONAL_SEQUENCE_LSTM:  // 52
    {
        std::vector<int32_t> inputIndexes3D, inputIndexes2D;
        inputIndexes3D.push_back(inputOperandIndexes[0]);    // input, 3-D
        inputIndexes3D.push_back(inputOperandIndexes[39]);    // input, 3-D
        reorderFromNXHWToNHWX(inputIndexes3D, edenModel);

        inputIndexes2D.push_back(inputOperandIndexes[4]);    // fw_input_to_output_weights, 2-D
        inputIndexes2D.push_back(inputOperandIndexes[8]);    // fw_recurrent_to_output_weights, 2-D
        inputIndexes2D.push_back(inputOperandIndexes[21]);   // bw_input_to_output_weights, 2-D
        inputIndexes2D.push_back(inputOperandIndexes[25]);    // bw_recurrent_to_output_weights, 2-D
        reorderFromHWNCToNCHW(inputIndexes2D, edenModel);

        std::vector<int32_t> outputIndexes3D;
        outputIndexes3D.push_back(outputOperandIndexes[0]);  // fw, 2-D
        if (edenOperation->numOfOutputs == 2) {
            outputIndexes3D.push_back(outputOperandIndexes[1]);  // bw, 2-D
        }
        reorderFromNXHWToNHWX(outputIndexes3D, edenModel);

        break;
    }
    case EDEN_OP_BIDIRECTIONAl_RNN:  // 54
    {
        std::vector<int32_t> inputIndexes3D, inputIndexes2D;
        inputIndexes3D.push_back(inputOperandIndexes[0]); // input
        inputIndexes3D.push_back(inputOperandIndexes[9]); // aux_input
        reorderFromNXHWToNHWX(inputIndexes3D, edenModel);

        inputIndexes2D.push_back(inputOperandIndexes[1]); // fw_weights
        inputIndexes2D.push_back(inputOperandIndexes[2]); // fw_recurrent_weights
        inputIndexes2D.push_back(inputOperandIndexes[4]); // fw_hidden_state
        inputIndexes2D.push_back(inputOperandIndexes[5]); // bw_weights
        inputIndexes2D.push_back(inputOperandIndexes[6]); // bw_recurrent_weights
        inputIndexes2D.push_back(inputOperandIndexes[8]); // bw_hidden_state
        reorderFromHWNCToNCHW(inputIndexes2D, edenModel);
        break;
    }
    case EDEN_OP_GATHER:  // 70
    {
        auto& operands = edenModel->GetMapToOperand();
        EdenOperand* indexOperand = operands.at(edenOperation->inputOperandIndexes[1]);
        if (indexOperand->shapeInfo->numOfDims == 1) {
            indexOperand->shapeInfo->numOfDims = 4;
            indexOperand->shapeInfo->dims[3] = indexOperand->shapeInfo->dims[0];
            indexOperand->shapeInfo->dims[0] = 1;
            indexOperand->shapeInfo->dims[1] = 1;
            indexOperand->shapeInfo->dims[2] = 1;
        }
        break;
    }
    case EDEN_OP_PRELU:  // 38
    {
        auto& operands = edenModel->GetMapToOperand();
        size_t inputSize = model.operands[androidOperation.inputs[0]].dimensions.size();
        size_t alphaSize = model.operands[androidOperation.inputs[1]].dimensions.size();
        if (inputSize == 4 && alphaSize == 3) {
            EdenOperand* indexOperand = operands.at(inputOperandIndexes[1]);
            indexOperand->shapeInfo->dims[1] = indexOperand->shapeInfo->dims[3];
            indexOperand->shapeInfo->dims[0] = 1;
            indexOperand->shapeInfo->dims[2] = 1;
            indexOperand->shapeInfo->dims[3] = 1;
        }
        break;
    }
    case EDEN_OP_QUANTIZED_16BIT_LSTM:  // 86
    {
        std::vector<int32_t> inputIndexes;
        inputIndexes.push_back(inputOperandIndexes[0]);
        inputIndexes.push_back(inputOperandIndexes[1]);
        inputIndexes.push_back(inputOperandIndexes[2]);
        inputIndexes.push_back(inputOperandIndexes[3]);
        inputIndexes.push_back(inputOperandIndexes[4]);
        inputIndexes.push_back(inputOperandIndexes[5]);
        inputIndexes.push_back(inputOperandIndexes[6]);
        inputIndexes.push_back(inputOperandIndexes[7]);
        inputIndexes.push_back(inputOperandIndexes[8]);
        inputIndexes.push_back(inputOperandIndexes[13]);
        inputIndexes.push_back(inputOperandIndexes[14]);
        reorderFromHWNCToNCHW(inputIndexes, edenModel);
        std::vector<int32_t> outputIndexes;
        outputIndexes.push_back(outputOperandIndexes[0]);
        outputIndexes.push_back(outputOperandIndexes[1]);
        reorderFromHWToNC(outputIndexes, edenModel);
        break;
    }

    case EDEN_OP_REDUCE_SUM:  // 97
    case EDEN_OP_REDUCE_MIN:  // 98
    case EDEN_OP_REDUCE_MAX:  // 99
    case EDEN_OP_REDUCE_PROD:  // 100
    case EDEN_OP_REDUCE_ANY:  // 101
    case EDEN_OP_REDUCE_ALL:  // 102
    {
        auto& operands = edenModel->GetMapToOperand();
        EdenOperand* axisOperand = operands.at(inputOperandIndexes[1]);  // axis
        auto inputSize = model.operands[androidOperation.inputs[0]].dimensions.size();
        // Unknown dimension, unknown rank
        if (axisOperand->shapeInfo->numOfDims == 0) {
            LOGD(EDEN_DRIVER, "Oops, unknown dimension is detected, skip it!");
            break;
        }

        for (int32_t idx = 0; idx < axisOperand->shapeInfo->dims[0] ; idx++) {
            int32_t* bufferAddr = reinterpret_cast<int32_t*>(axisOperand->buffer->addr);
            if (bufferAddr[idx] < 0) {
                bufferAddr[idx] += inputSize;
            }
        }
        break;
    }
    case EDEN_OP_DETECTION_POSTPROCESSING:
    {
        std::vector<int32_t> inputIndexes;
        inputIndexes.push_back(inputOperandIndexes[2]);  // 2(ANCHORS)
        reorderFromHWToNC(inputIndexes, edenModel);
        int32_t tmp_idx = edenOperation->inputOperandIndexes[1];
        // for the inconsistent loading order of input operand between Android and EDEN
        edenOperation->inputOperandIndexes[1] = edenOperation->inputOperandIndexes[0];
        edenOperation->inputOperandIndexes[0] = tmp_idx;
        break;
    }
    case EDEN_OP_RANDOM_MULTINOMIAL:
    {
        std::vector<int32_t> inputIndexes;
        inputIndexes.push_back(inputOperandIndexes[0]);
        reorderFromHWToNC(inputIndexes, edenModel);
        break;
    }
    case EDEN_OP_TOPK_V2: // 78
    {
        auto& operands = edenModel->GetMapToOperand();
        EdenOperand* inputOperand = operands.at(inputOperandIndexes[0]);
        EdenOperand* outOperand = operands.at(edenOperation->outputOperandIndexes[0]);
        EdenOperand* outIndicesOperand = operands.at(edenOperation->outputOperandIndexes[1]);
        inputOperand->shapeInfo->dims[0] = 1;
        inputOperand->shapeInfo->dims[1] = 1;
        inputOperand->shapeInfo->dims[2] = 1;
        inputOperand->shapeInfo->dims[3] = 1;
        switch (model.operands[androidOperation.inputs[0]].dimensions.size()) {
            case 1:
                inputOperand->shapeInfo->dims[3] = model.operands[androidOperation.inputs[0]].dimensions[0];
                break;
            case 2:
                inputOperand->shapeInfo->dims[2] = model.operands[androidOperation.inputs[0]].dimensions[0];
                inputOperand->shapeInfo->dims[3] = model.operands[androidOperation.inputs[0]].dimensions[1];
                break;
            case 3:
                inputOperand->shapeInfo->dims[1] = model.operands[androidOperation.inputs[0]].dimensions[0];
                inputOperand->shapeInfo->dims[2] = model.operands[androidOperation.inputs[0]].dimensions[1];
                inputOperand->shapeInfo->dims[3] = model.operands[androidOperation.inputs[0]].dimensions[2];
                break;
            case 4:
                inputOperand->shapeInfo->dims[0] = model.operands[androidOperation.inputs[0]].dimensions[0];
                inputOperand->shapeInfo->dims[1] = model.operands[androidOperation.inputs[0]].dimensions[1];
                inputOperand->shapeInfo->dims[2] = model.operands[androidOperation.inputs[0]].dimensions[2];
                inputOperand->shapeInfo->dims[3] = model.operands[androidOperation.inputs[0]].dimensions[3];
                break;
            default:
                break;

        }

        inputOperand->shapeInfo->numOfDims = 4;
        outOperand->shapeInfo->numOfDims = 4;
        outIndicesOperand->shapeInfo->numOfDims = 4;
        break;
    }
        default:
            break;
    }
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

bool loadDefaultValuesForOptionalInputs(const V1_2::Model& /*model*/, int32_t indexNumber, const V1_2::Operation& androidOperation, char* dstAddr, int32_t length) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    bool loaded = false;
    int32_t opType = static_cast<int32_t>(androidOperation.type);

    switch (opType) {
        case ANEURALNETWORKS_LSH_PROJECTION:  // 15
            loadDefaultValuesForOptionalInputsForLshProjection(indexNumber, androidOperation, dstAddr, length);
            loaded = true;
            break;
        default:
            LOGD(EDEN_DRIVER, "Nothing to load default values for opType=%d\n", opType);
            break;
    }

    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
    return loaded;
}

void loadDefaultValuesForOptionalInputsForLshProjection(int32_t indexNumber, const V1_2::Operation& androidOperation, void* dstAddr, int32_t length) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    int32_t numOfInputs = androidOperation.inputs.size();
    if ((numOfInputs <= indexNumber) ||
        (indexNumber != 2)) {
        LOGE(EDEN_DRIVER, "Oops, indexNumber=[%d] is not Optional! Please check it!\n", indexNumber);
        return;
    }

    LOGD(EDEN_DRIVER, "Start to load default values for optional input at indexNumber=%d\n", indexNumber);

    // Load default value
    if (indexNumber == 2) {  // 2(Weight)
        const float defaultValue = 1.0;
        float* floatPtr = static_cast<float*>(dstAddr);
        int32_t numOfData = (length / sizeof(float));

        for (int32_t idx = 0; idx < numOfData; idx++) {
            LOGD(EDEN_DRIVER, "Load floatPtr[%d]=%f, defaultValue=%f\n", idx, floatPtr[idx], defaultValue);
            floatPtr[idx] = defaultValue;
        }
    }
    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

void loadDefaultValuesForOptionalInputsForBidirectionalSequenceLstm(int32_t indexNumber, const V1_2::Operation& androidOperation, void* dstAddr, int32_t length) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    int32_t numOfInputs = androidOperation.inputs.size();

    int32_t optionalIndex[33] = {1, 5, 9, 10, 11, 12, 16, 17, 18, 22, 26, 27, 28, 29, 33, 34, 39, 40,
            41, 42, 43, 44, 45, 46, 47, 53, 54, 55, 56, 57, 58, 59, 60};

    if ((numOfInputs <= indexNumber)) {
        LOGE(EDEN_DRIVER, "Oops, indexNumber=[%d] is not Optional! Please check it!\n", indexNumber);
        return;
    }

    for (uint32_t i = 0; i < 33; i++) {
        if (indexNumber == optionalIndex[i]) {
            float* floatPtr = static_cast<float*>(dstAddr);
            floatPtr = nullptr;

            LOGE(EDEN_DRIVER, "Addr will be nullptr, length is %d!\n", length);
            return;
        }
    }

    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
}

/**
 * @brief Verify request in terms of dimension
 * @details This function verifies a given request in terms of dimension.
 * @param[in] request Request
 * @param[in] model Android NN Model
 * @returns return code
 */
bool verifyDimension(const V1_0::Request& request, const V1_2::Model& model) {
    LOGD(EDEN_DRIVER, "%s(+)\n", __func__);

    const auto& inputs = request.inputs;
    const auto& outputs = request.outputs;

    LOGD(EDEN_DRIVER, "inputs.size()=%zu, outputs.size()=%zu\n", inputs.size(), outputs.size());

    for (size_t idx = 0; idx < inputs.size(); idx++) {
        const RequestArgument& inputArgs = inputs[idx];
        if (inputArgs.hasNoValue) {
            continue;
        }

        auto androidOperandIdx = model.inputIndexes[idx];
        auto androidOperand = model.operands[androidOperandIdx];

        for (size_t i = 0; i < inputArgs.dimensions.size(); i++) {
            if (inputArgs.dimensions[i] != androidOperand.dimensions[i]) {
                LOGW(EDEN_DRIVER, "Error, different dimension of inputs\n");
                LOGW(EDEN_DRIVER, "inputArgs.dimensions={\n");
                for (int32_t idx : inputArgs.dimensions) {
                    LOGW(EDEN_DRIVER, "%d \n", idx);
                }
                LOGW(EDEN_DRIVER, "}, androidOperand.dimensions={\n");
                for (int32_t idx : androidOperand.dimensions) {
                    LOGW(EDEN_DRIVER, "%d \n", idx);
                }
                LOGW(EDEN_DRIVER, "}\n");
                return false;
            }
        }
    }

    for (size_t idx = 0; idx < outputs.size(); idx++) {
        const RequestArgument& outputArgs = outputs[idx];
        if (outputArgs.hasNoValue) {
            continue;
        }

        auto androidOperandIdx = model.outputIndexes[idx];
        auto androidOperand = model.operands[androidOperandIdx];

        for (size_t i = 0; i < outputArgs.dimensions.size(); i++) {
            if (outputArgs.dimensions[i] != androidOperand.dimensions[i]) {
                LOGW(EDEN_DRIVER, "Error, different dimension of outputs\n");
                LOGW(EDEN_DRIVER, "outputArgs.dimensions={\n");
                for (int32_t idx : outputArgs.dimensions) {
                    LOGW(EDEN_DRIVER, "%d \n", idx);
                }
                LOGW(EDEN_DRIVER, "}, androidOperand.dimensions={\n");
                for (int32_t idx : androidOperand.dimensions) {
                    LOGW(EDEN_DRIVER, "%d \n", idx);
                }
                LOGW(EDEN_DRIVER, "}\n");
                return false;
            }
        }
    }

    LOGD(EDEN_DRIVER, "%s(-)\n", __func__);
    return true;
}

/**
 * @brief show Request
 * @details This function shows a Request.
 * @returns void
 */
void showRequest(const V1_0::Request& request) {
    const std::vector<RequestArgument>& inputs = request.inputs;
    for (size_t idx = 0; idx < inputs.size(); idx++) {
        const RequestArgument& inputArgs = inputs[idx];

        LOGD(EDEN_DRIVER, "request.inputs[%zu].hasNoValue:%d\n", idx, inputArgs.hasNoValue);
        LOGD(EDEN_DRIVER, "request.inputs[%zu].location.poolIndex:%d\n", idx, inputArgs.location.poolIndex);
        LOGD(EDEN_DRIVER, "request.inputs[%zu].location.offset:%d\n", idx, inputArgs.location.offset);
        LOGD(EDEN_DRIVER, "request.inputs[%zu].location.length:%d\n", idx, inputArgs.location.length);
    }

    const std::vector<RequestArgument>& outputs = request.outputs;
    for (size_t idx = 0; idx < outputs.size(); idx++) {
        const RequestArgument& outputArgs = outputs[idx];

        LOGD(EDEN_DRIVER, "request.outputs[%zu].hasNoValue:%d\n", idx, outputArgs.hasNoValue);
        LOGD(EDEN_DRIVER, "request.outputs[%zu].location.poolIndex:%d\n", idx, outputArgs.location.poolIndex);
        LOGD(EDEN_DRIVER, "request.outputs[%zu].location.offset:%d\n", idx, outputArgs.location.offset);
        LOGD(EDEN_DRIVER, "request.outputs[%zu].location.length:%d\n", idx, outputArgs.location.length);
    }

    const hidl_vec<hidl_memory>& pools = request.pools;
    for (size_t idx = 0; idx < pools.size(); idx++) {
        sp<IMemory> mappedMemory = mapMemory(request.pools[idx]);
        char* virtAddr = reinterpret_cast<char*>(static_cast<void*>(mappedMemory->getPointer()));
        int32_t size = mappedMemory->getSize();
        LOGD(EDEN_DRIVER, "virtAddr=[%p], size=[%d]\n", reinterpret_cast<void*>(virtAddr), size);
    }
}

}  // namespace eden_driver
}  // namespace nn
}  // namespace android
