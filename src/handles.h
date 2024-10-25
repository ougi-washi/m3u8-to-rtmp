#pragma once
#include "types.h"
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/hwcontext.h>
#include <libswscale/swscale.h>
}

namespace mtr{

    struct info{
        std::string input_url = {};
        std::string output_url = {}; 
        const f32 resolution_scale = 0.5f;
        const i32 video_bitrate = 4096;
    };

    struct handle{
        AVBufferRef* hw_device_ctx = nullptr;
        AVFormatContext* input_format_context = nullptr;
        AVCodec* decoder = nullptr;
        AVCodecContext* decoder_ctx = nullptr;
        int video_stream_index = -1;
        AVFormatContext* output_format_context = nullptr;
        AVCodec* encoder = nullptr;
        AVStream* out_stream = nullptr;
        AVCodecContext* encoder_ctx = nullptr;
        AVBufferRef* hw_frames_ctx = nullptr;
        AVFrame* frame = nullptr;
        AVPacket* enc_pkt = nullptr;
    };

    b8 init(handle* handle, const info& info);
}


