// -------------------------------------------------------------------------------------------------
// nvenc_caps_dump.cpp
// Displays NVENC capabilities per codec in a readable way, decoding bitmasks.
// Dependencies: CUDA Runtime/Driver + Video Codec SDK (NvEncoder/NvEncoder.h + nvEncodeAPI.h).
// -------------------------------------------------------------------------------------------------

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <stdexcept>
#include <cstring>

#include <cuda.h>
#include "NvEncoder/NvEncoder.h"
#include "nvEncodeAPI.h"

static bool GuidEqual(const GUID& a, const GUID& b)
{
    return a.Data1 == b.Data1 &&
           a.Data2 == b.Data2 &&
           a.Data3 == b.Data3 &&
           std::memcmp(a.Data4, b.Data4, sizeof(a.Data4)) == 0;
}

static const char* CodecNameFromGuid(const GUID& g)
{
    if (GuidEqual(g, NV_ENC_CODEC_H264_GUID)) return "H.264/AVC";
    if (GuidEqual(g, NV_ENC_CODEC_HEVC_GUID)) return "HEVC/H.265";
    if (GuidEqual(g, NV_ENC_CODEC_AV1_GUID))  return "AV1";
    return "Unknown";
}

// Small class to open an NVENC session without allocating input buffers
class NvEncoderQuery : public NvEncoder
{
public:
    explicit NvEncoderQuery(void* device)
        : NvEncoder(NV_ENC_DEVICE_TYPE_CUDA, device, 1920, 1080, NV_ENC_BUFFER_FORMAT_NV12, 1, false) {}
protected:
    void AllocateInputBuffers(int32_t) override {}
    void ReleaseInputBuffers() override {}
};

// Readable names for caps
static const std::unordered_map<int, std::string> kCapNames = {
    {NV_ENC_CAPS_NUM_MAX_BFRAMES, "Max B-Frames"},
    {NV_ENC_CAPS_SUPPORTED_RATECONTROL_MODES, "Supported Rate Control Modes"},
    {NV_ENC_CAPS_SUPPORT_FIELD_ENCODING, "Field Encoding"},
    {NV_ENC_CAPS_SUPPORT_MONOCHROME, "Monochrome Encode"},
    {NV_ENC_CAPS_SUPPORT_FMO, "FMO"},
    {NV_ENC_CAPS_SUPPORT_QPELMV, "Quarter-Pel ME"},
    {NV_ENC_CAPS_SUPPORT_BDIRECT_MODE, "BDirect Mode (H.264)"},
    {NV_ENC_CAPS_SUPPORT_CABAC, "CABAC (H.264)"},
    {NV_ENC_CAPS_SUPPORT_ADAPTIVE_TRANSFORM, "Adaptive Transform"},
    {NV_ENC_CAPS_SUPPORT_STEREO_MVC, "Stereo MVC"},
    {NV_ENC_CAPS_NUM_MAX_TEMPORAL_LAYERS, "Max Temporal Layers (or support flag)"},
    {NV_ENC_CAPS_SUPPORT_HIERARCHICAL_PFRAMES, "Hierarchical P-Frames"},
    {NV_ENC_CAPS_SUPPORT_HIERARCHICAL_BFRAMES, "Hierarchical B-Frames"},
    {NV_ENC_CAPS_LEVEL_MAX, "Max Level"},
    {NV_ENC_CAPS_LEVEL_MIN, "Min Level"},
    {NV_ENC_CAPS_SEPARATE_COLOUR_PLANE, "Separate Color Plane"},
    {NV_ENC_CAPS_WIDTH_MAX, "Max Width"},
    {NV_ENC_CAPS_HEIGHT_MAX, "Max Height"},
    {NV_ENC_CAPS_SUPPORT_TEMPORAL_SVC, "Temporal SVC"},
    {NV_ENC_CAPS_SUPPORT_DYN_RES_CHANGE, "Dynamic Resolution Change"},
    {NV_ENC_CAPS_SUPPORT_DYN_BITRATE_CHANGE, "Dynamic Bitrate Change"},
    {NV_ENC_CAPS_SUPPORT_DYN_FORCE_CONSTQP, "Dynamic Force CONSTQP"},
    {NV_ENC_CAPS_SUPPORT_DYN_RCMODE_CHANGE, "Dynamic RC Mode Change"},
    {NV_ENC_CAPS_SUPPORT_SUBFRAME_READBACK, "Subframe Readback"},
    {NV_ENC_CAPS_SUPPORT_CONSTRAINED_ENCODING, "Constrained Encoding"},
    {NV_ENC_CAPS_SUPPORT_INTRA_REFRESH, "Intra Refresh"},
    {NV_ENC_CAPS_SUPPORT_CUSTOM_VBV_BUF_SIZE, "Custom VBV Buffer Size"},
    {NV_ENC_CAPS_SUPPORT_DYNAMIC_SLICE_MODE, "Dynamic Slice Mode"},
    {NV_ENC_CAPS_SUPPORT_REF_PIC_INVALIDATION, "Reference Picture Invalidation"},
    {NV_ENC_CAPS_PREPROC_SUPPORT, "Pre-Processing (bitmask)"},
    {NV_ENC_CAPS_ASYNC_ENCODE_SUPPORT, "Async Encode"},
    {NV_ENC_CAPS_MB_NUM_MAX, "Max Macroblocks per Frame"},
    {NV_ENC_CAPS_MB_PER_SEC_MAX, "Max Macroblocks per Second"},
    {NV_ENC_CAPS_SUPPORT_YUV444_ENCODE, "YUV444 Encode"},
    {NV_ENC_CAPS_SUPPORT_LOSSLESS_ENCODE, "Lossless Encode"},
    {NV_ENC_CAPS_SUPPORT_SAO, "Sample Adaptive Offset (HEVC)"},
    {NV_ENC_CAPS_SUPPORT_MEONLY_MODE, "ME Only Mode"},
    {NV_ENC_CAPS_SUPPORT_LOOKAHEAD, "Lookahead"},
    {NV_ENC_CAPS_SUPPORT_TEMPORAL_AQ, "Temporal AQ"},
    {NV_ENC_CAPS_SUPPORT_10BIT_ENCODE, "10-bit Encode"},
    {NV_ENC_CAPS_NUM_MAX_LTR_FRAMES, "Max LTR Frames"},
    {NV_ENC_CAPS_SUPPORT_WEIGHTED_PREDICTION, "Weighted Prediction"},
    {NV_ENC_CAPS_DYNAMIC_QUERY_ENCODER_CAPACITY, "Dynamic Encoder Capacity (%)"},
    {NV_ENC_CAPS_SUPPORT_BFRAME_REF_MODE, "B-frame as Reference"},
    {NV_ENC_CAPS_SUPPORT_EMPHASIS_LEVEL_MAP, "Emphasis Level Map"},
    {NV_ENC_CAPS_WIDTH_MIN, "Min Width"},
    {NV_ENC_CAPS_HEIGHT_MIN, "Min Height"},
    {NV_ENC_CAPS_SUPPORT_MULTIPLE_REF_FRAMES, "Multiple Reference Frames"},
    {NV_ENC_CAPS_SUPPORT_ALPHA_LAYER_ENCODING, "HEVC Alpha Layer Encode"},
    {NV_ENC_CAPS_NUM_ENCODER_ENGINES, "Number of Encoder Engines"},
    {NV_ENC_CAPS_SINGLE_SLICE_INTRA_REFRESH, "Single Slice Intra Refresh"},
    {NV_ENC_CAPS_DISABLE_ENC_STATE_ADVANCE, "Disable Encoder State Advance"},
    {NV_ENC_CAPS_OUTPUT_RECON_SURFACE, "Reconstructed Frame Output"},
    {NV_ENC_CAPS_OUTPUT_BLOCK_STATS, "Output Block Stats"},
    {NV_ENC_CAPS_OUTPUT_ROW_STATS, "Output Row Stats"}
};

// Decode bitmask of RC modes (NV_ENC_CAPS_SUPPORTED_RATECONTROL_MODES)
// NVENC convention: bit (1 << NV_ENC_PARAMS_RC_MODE) if supported.
static std::string DecodeRateControlModes(int mask)
{
    struct Item { int bit; const char* name; };
    const Item items[] = {
        { 1 << NV_ENC_PARAMS_RC_CONSTQP, "CONSTQP" },
        { 1 << NV_ENC_PARAMS_RC_VBR,     "VBR"     },
        { 1 << NV_ENC_PARAMS_RC_CBR,     "CBR"     }
    };
    std::ostringstream oss;
    bool first = true;
    for (auto& it : items) {
        if (mask & it.bit) {
            if (!first) oss << ", ";
            oss << it.name;
            first = false;
        }
    }
    if (first) return "None";
    return oss.str();
}

// Decode field encoding (0/1/2)
static const char* DecodeFieldEncoding(int v)
{
    switch (v) {
        case 0: return "Interlaced: not supported";
        case 1: return "Interlaced field: supported";
        case 2: return "Interlaced frame + field: supported";
    }
    return "Unknown";
}

// B as reference mode (0/1/2)
static const char* DecodeBRefMode(int v)
{
    switch (v) {
        case 0: return "Not supported";
        case 1: return "Each B-frame can be reference";
        case 2: return "Only middle B-frame as reference";
    }
    return "Unknown";
}

// ME-only mode (0/1/2)
static const char* DecodeMeOnlyMode(int v)
{
    switch (v) {
        case 0: return "Not supported";
        case 1: return "Supported for I and P";
        case 2: return "Supported for I, P and B";
    }
    return "Unknown";
}

// Preproc flags – the full SDK exposes NV_ENC_PREPROC_FLAGS. If unavailable here,
// we generically decode the most common bits for readability.
static std::string DecodePreprocFlags(int mask)
{
    struct Bit { int b; const char* name; };
    // Name the typical NVENC API bits (if constants are unavailable in this snippet)
    const Bit bits[] = {
        { 0, "Denoise" },        // bit 0
        { 1, "Resize" },         // bit 1
        { 2, "Deinterlace" },    // bit 2
        { 3, "Adaptive Transform/EdgeEnhance" }, // depends on SDK version
        { 4, "Color Conversion" }
    };
    if (mask == 0) return "None";
    std::ostringstream oss;
    bool first = true;
    for (auto& b : bits) {
        if (mask & (1 << b.b)) {
            if (!first) oss << ", ";
            oss << b.name;
            first = false;
        }
    }
    // If other bits are set
    int knownMask = 0;
    for (auto& b : bits) knownMask |= (1 << b.b);
    int unknown = mask & ~knownMask;
    if (unknown) {
        if (!first) oss << ", ";
        oss << "Other(0x" << std::hex << unknown << std::dec << ")";
    }
    return oss.str();
}

// Levels (MAX/MIN) -> string per codec
static std::string DecodeLevel(const GUID& codec, int val)
{
    if (GuidEqual(codec, NV_ENC_CODEC_H264_GUID)) {
        static const std::unordered_map<int, std::string> h264 = {
            {  9, "1b" }, { 10, "1.0" }, { 11, "1.1" }, { 12, "1.2" }, { 13, "1.3" },
            { 20, "2.0" }, { 21, "2.1" }, { 22, "2.2" },
            { 30, "3.0" }, { 31, "3.1" }, { 32, "3.2" },
            { 40, "4.0" }, { 41, "4.1" }, { 42, "4.2" },
            { 50, "5.0" }, { 51, "5.1" }, { 52, "5.2" },
            { 60, "6.0" }, { 61, "6.1" }, { 62, "6.2" }
        };
        auto it = h264.find(val);
        return (it != h264.end()) ? it->second : ("Unknown(" + std::to_string(val) + ")");
    }
    if (GuidEqual(codec, NV_ENC_CODEC_HEVC_GUID)) {
        static const std::unordered_map<int, std::string> hevc = {
            {  30, "1.0" }, {  60, "2.0" }, {  63, "2.1" },
            {  90, "3.0" }, {  93, "3.1" },
            { 120, "4.0" }, { 123, "4.1" },
            { 150, "5.0" }, { 153, "5.1" }, { 156, "5.2" },
            { 180, "6.0" }, { 183, "6.1" }, { 186, "6.2" }
        };
        auto it = hevc.find(val);
        return (it != hevc.end()) ? it->second : ("Unknown(" + std::to_string(val) + ")");
    }
    if (GuidEqual(codec, NV_ENC_CODEC_AV1_GUID)) {
        // Mapping based on header enum: 0..23 => 2,2.1,...,7.3
        static const std::vector<std::string> av1 = {
            "2.0","2.1","2.2","2.3",
            "3.0","3.1","3.2","3.3",
            "4.0","4.1","4.2","4.3",
            "5.0","5.1","5.2","5.3",
            "6.0","6.1","6.2","6.3",
            "7.0","7.1","7.2","7.3"
        };
        if (val >= 0 && val < static_cast<int>(av1.size())) return av1[val];
    }
    return "Unknown(" + std::to_string(val) + ")";
}

static std::string ToSupportedNotSupported(int value)
{
    return value ? "Supported" : "Not supported";
}

static void PrintOneCap(NvEncoder& enc, const GUID& codec, NV_ENC_CAPS cap)
{
    const auto capId = static_cast<int>(cap);
    auto nameIt = kCapNames.find(capId);
    const std::string capName = (nameIt != kCapNames.end()) ? nameIt->second : ("CAP_" + std::to_string(capId));

    int v = enc.GetCapabilityValue(codec, cap);

    std::cout << "  - " << std::left << std::setw(38) << capName << " : ";

    switch (cap) {
        case NV_ENC_CAPS_SUPPORTED_RATECONTROL_MODES:
            std::cout << DecodeRateControlModes(v);
            break;

        case NV_ENC_CAPS_SUPPORT_FIELD_ENCODING:
            std::cout << DecodeFieldEncoding(v);
            break;

        case NV_ENC_CAPS_SUPPORT_BFRAME_REF_MODE:
            std::cout << DecodeBRefMode(v);
            break;

        case NV_ENC_CAPS_SUPPORT_MEONLY_MODE:
            std::cout << DecodeMeOnlyMode(v);
            break;

        case NV_ENC_CAPS_PREPROC_SUPPORT:
            std::cout << DecodePreprocFlags(v) << " (0x" << std::hex << v << std::dec << ")";
            break;

        case NV_ENC_CAPS_LEVEL_MAX:
        case NV_ENC_CAPS_LEVEL_MIN:
            std::cout << DecodeLevel(codec, v) << " (" << v << ")";
            break;

        case NV_ENC_CAPS_DYNAMIC_QUERY_ENCODER_CAPACITY:
            std::cout << v << "%";
            break;

        // Numeric values (bounds / quantities)
        case NV_ENC_CAPS_NUM_MAX_BFRAMES:
        case NV_ENC_CAPS_NUM_MAX_TEMPORAL_LAYERS:
        case NV_ENC_CAPS_WIDTH_MAX:
        case NV_ENC_CAPS_HEIGHT_MAX:
        case NV_ENC_CAPS_WIDTH_MIN:
        case NV_ENC_CAPS_HEIGHT_MIN:
        case NV_ENC_CAPS_MB_NUM_MAX:
        case NV_ENC_CAPS_MB_PER_SEC_MAX:
        case NV_ENC_CAPS_NUM_MAX_LTR_FRAMES:
        case NV_ENC_CAPS_NUM_ENCODER_ENGINES:
            if (cap == NV_ENC_CAPS_MB_PER_SEC_MAX) {
                // Also give approximate Mpix/s
                double mpixPerSecApprox = static_cast<double>(v) * 256.0 / 1e6; // 1 MB=16x16=256 pixels
                std::cout << v << " MB/s (~" << std::fixed << std::setprecision(2) << mpixPerSecApprox << " Mpix/s)";
            } else {
                std::cout << v;
            }
            break;

        default:
            // Default: boolean for 0/1, otherwise raw value
            if (v == 0 || v == 1) std::cout << ToSupportedNotSupported(v);
            else                  std::cout << v;
            break;
    }

    std::cout << "\n";
}

static void DumpCapsForCodec(NvEncoder& enc, const GUID& codec)
{
    // Quickly test if the codec is actually supported on this GPU
    // Try a generic cap (e.g., WIDTH_MAX); if exception -> not supported
    try {
        (void)enc.GetCapabilityValue(codec, NV_ENC_CAPS_WIDTH_MAX);
    } catch (const NVENCException&) {
        return; // Codec not supported
    }

    std::cout << "\n=== Codec: " << CodecNameFromGuid(codec) << " ===\n";
    for (int i = 0; i < NV_ENC_CAPS_EXPOSED_COUNT; ++i) {
        try {
            PrintOneCap(enc, codec, static_cast<NV_ENC_CAPS>(i));
        } catch (const NVENCException& e) {
            // Some caps do not apply to this codec; ignore them safely.
            (void)e;
        }
    }
}

int main()
{
    CUdevice device = 0;
    CUcontext cuContext = nullptr;

    if (cuInit(0) != CUDA_SUCCESS) {
        std::cerr << "cuInit failed\n";
        return 1;
    }
    if (cuDeviceGet(&device, 0) != CUDA_SUCCESS) {
        std::cerr << "cuDeviceGet(0) failed\n";
        return 1;
    }
    char name[256] = {};
    cuDeviceGetName(name, sizeof(name), device);
    int drvVersion = 0;
    cuDriverGetVersion(&drvVersion);

    if (cuCtxCreate(&cuContext, 0, device) != CUDA_SUCCESS) {
        std::cerr << "cuCtxCreate failed\n";
        return 1;
    }

    std::cout << "GPU: " << name << " | CUDA driver: " << drvVersion << "\n";

    try {
        NvEncoderQuery encoder(cuContext);

        // Display per supported codec (test the 3 known ones)
        const GUID codecs[] = { NV_ENC_CODEC_H264_GUID, NV_ENC_CODEC_HEVC_GUID, NV_ENC_CODEC_AV1_GUID };
        bool any = false;
        for (const auto& c : codecs) {
            // DumpCapsForCodec will silently skip if not supported (via internal test)
            try {
                DumpCapsForCodec(encoder, c);
                any = true;
            } catch (...) {
                // ignore
            }
        }
        if (!any) {
            std::cout << "No supported NVENC codec found.\n";
        }
    } catch (const NVENCException& e) {
        std::cerr << "NVENC Error: " << e.getErrorString() << " (" << e.getErrorCode() << ")\n";
        if (e.getErrorCode() == NV_ENC_ERR_NO_ENCODE_DEVICE) {
            std::cerr << "No NVENC engine available on this GPU/driver.\n";
        }
        cuCtxDestroy(cuContext);
        return 2;
    }

    cuCtxDestroy(cuContext);
    return 0;
}
