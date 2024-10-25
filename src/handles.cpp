#include "handles.h"
#include <iostream>


b8 mtr::init(handle *handle, const info &info){
    const char* input_url = info.input_url.c_str();
    const char* output_url = info.output_url.c_str();

    avformat_network_init();

    // Input format context
    AVFormatContext* input_format_context = nullptr;
    if (avformat_open_input(&input_format_context, input_url, nullptr, nullptr) < 0) {
        std::cerr << "Error: Could not open input stream" << std::endl;
        return false;
    }

    if (avformat_find_stream_info(input_format_context, nullptr) < 0) {
        std::cerr << "Error: Could not find stream information" << std::endl;
        return false;
    }

    // Find video stream
    AVCodec* decoder = nullptr;
    AVCodecContext* decoder_ctx = nullptr;
    i32 video_stream_index = -1;

    for (i32 i = 0; i < input_format_context->nb_streams; i++) {
        if (input_format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            decoder = avcodec_find_decoder(input_format_context->streams[i]->codecpar->codec_id);
            if (!decoder) {
                std::cerr << "Error: Could not find decoder" << std::endl;
                return false;
            }

            decoder_ctx = avcodec_alloc_context3(decoder);
            avcodec_parameters_to_context(decoder_ctx, input_format_context->streams[i]->codecpar);

            i32 err = av_hwdevice_ctx_create(&handle->hw_device_ctx, AV_HWDEVICE_TYPE_CUDA, nullptr, nullptr, 0);
            if (err < 0) {
                char err_buf[AV_ERROR_MAX_STRING_SIZE];
                std::cerr << "Error: Failed to create CUDA device context: " << av_make_error_string(err_buf, AV_ERROR_MAX_STRING_SIZE, err) << std::endl;
                return false;
            }
            decoder_ctx->hw_device_ctx = av_buffer_ref(handle->hw_device_ctx);

            if (avcodec_open2(decoder_ctx, decoder, nullptr) < 0) {
                std::cerr << "Error: Could not open decoder" << std::endl;
                return false;
            }
            break;
        }
    }

    if (video_stream_index == -1) {
        std::cerr << "Error: Could not find video stream" << std::endl;
        return false;
    }

    // Output format context
    AVFormatContext* output_format_context = nullptr;
    avformat_alloc_output_context2(&output_format_context, nullptr, "flv", output_url);

    if (!output_format_context) {
        std::cerr << "Error: Could not create output context" << std::endl;
        return false;
    }

    // Find the NVENC encoder
    AVCodec* encoder = avcodec_find_encoder_by_name("h264_nvenc");
    if (!encoder) {
        std::cerr << "Error: Could not find NVENC encoder" << std::endl;
        return false;
    }

    AVStream* out_stream = avformat_new_stream(output_format_context, encoder);
    if (!out_stream) {
        std::cerr << "Error: Could not create output stream" << std::endl;
        return false;
    }

    AVCodecContext* encoder_ctx = avcodec_alloc_context3(encoder);

    encoder_ctx->bit_rate = 4096; // Set desired bitrate
    encoder_ctx->width = decoder_ctx->width;
    encoder_ctx->height = decoder_ctx->height;
    encoder_ctx->time_base = input_format_context->streams[video_stream_index]->time_base;
    encoder_ctx->pix_fmt = AV_PIX_FMT_CUDA; // Use CUDA pixel format
    encoder_ctx->max_b_frames = 0;
    encoder_ctx->gop_size = 12; // Adjust GOP size as needed

    // Set the hardware frames context for encoding
    AVBufferRef* hw_frames_ctx = av_hwframe_ctx_alloc(handle->hw_device_ctx);
    if (!hw_frames_ctx) {
        std::cerr << "Error: Could not allocate hardware frame context" << std::endl;
        return false;
    }
    // Configure the frames context
    AVHWFramesContext* frames_ctx = (AVHWFramesContext*)hw_frames_ctx->data;
    frames_ctx->format = AV_PIX_FMT_CUDA;  // Set the hardware pixel format
    frames_ctx->sw_format = decoder_ctx->pix_fmt;  // Set the software pixel format, e.g., AV_PIX_FMT_YUV420P
    frames_ctx->width = decoder_ctx->width;
    frames_ctx->height = decoder_ctx->height;
    frames_ctx->initial_pool_size = 20;  // Adjust as needed

    // Initialize the hardware frames context
    i32 err = av_hwframe_ctx_init(hw_frames_ctx);
    if (err < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        std::cerr << "Error: Failed to initialize hardware frames context: " << av_make_error_string(err_buf, AV_ERROR_MAX_STRING_SIZE, err) << std::endl;
        av_buffer_unref(&hw_frames_ctx);
        return false;
    }

    // Step 2: Set the encoder's hardware frames context and pixel format
    encoder_ctx->hw_frames_ctx = av_buffer_ref(hw_frames_ctx);
    encoder_ctx->pix_fmt = AV_PIX_FMT_CUDA;  // Match the hardware frames context format

    if (output_format_context->oformat->flags & AVFMT_GLOBALHEADER) {
        encoder_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if (avcodec_open2(encoder_ctx, encoder, nullptr) < 0) {
        std::cerr << "Error: Could not open encoder" << std::endl;
        return false;
    }

    avcodec_parameters_from_context(out_stream->codecpar, encoder_ctx);

    // Open output URL
    if (!(output_format_context->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&output_format_context->pb, output_url, AVIO_FLAG_WRITE) < 0) {
            std::cerr << "Error: Could not open output URL" << std::endl;
            return false;
        }
    }

    if (avformat_write_header(output_format_context, nullptr) < 0) {
        std::cerr << "Error: Could not write header to output" << std::endl;
        return -1;
    }

    AVPacket packet;
    AVFrame* frame = av_frame_alloc();
    AVPacket* enc_pkt = av_packet_alloc();
    
    return true;
}
