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
        i32 video_bitrate = 4096;
        i32 gop_size = 12;
        i32 audio_bitrate = 128;
    };
    
    struct handle {
        AVFormatContext* input_format_context = nullptr;
        AVFormatContext* output_format_context = nullptr;
        AVCodecContext* decoder_ctx = nullptr;
        AVCodecContext* encoder_ctx = nullptr;
        AVCodecContext* audio_decoder_ctx = nullptr;
        AVCodecContext* audio_encoder_ctx = nullptr;
        AVStream* out_stream = nullptr;
        AVStream* audio_out_stream = nullptr;
        AVPacket* enc_pkt = nullptr;
        AVPacket* audio_enc_pkt = nullptr;
        AVFrame* frame = nullptr;
        AVFrame* audio_frame = nullptr;
        i32 video_stream_index = -1;
        i32 audio_stream_index = -1;
        AVCodec* decoder = nullptr;
        AVCodec* encoder = nullptr;
        AVCodec* audio_decoder = nullptr;
        AVCodec* audio_encoder = nullptr;
        AVBufferRef* hw_device_ctx = nullptr;
        AVBufferRef* hw_frames_ctx = nullptr;
    };

    const info get_info(const std::string& file_path);
    b8 init(handle* handle, const info& info);
    void process(handle* handle);
    void cleanup(handle* handle);
}
