#include "handles.h"
#include <iostream>

b8 mtr::init(handle *handle, const info &info){
    const char* input_url = info.input_url.c_str();
    const char* output_url = info.output_url.c_str();

    avformat_network_init();

    // Input format context
    if (avformat_open_input(&handle->input_format_context, input_url, nullptr, nullptr) < 0) {
        std::cerr << "Error: Could not open input stream" << std::endl;
        return false;
    }

    if (avformat_find_stream_info(handle->input_format_context, nullptr) < 0) {
        std::cerr << "Error: Could not find stream information" << std::endl;
        return false;
    }

    for (i32 i = 0; i < handle->input_format_context->nb_streams; i++) {
        if (handle->input_format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            handle->video_stream_index = i;
            handle->decoder = avcodec_find_decoder(handle->input_format_context->streams[i]->codecpar->codec_id);
            if (!handle->decoder) {
                std::cerr << "Error: Could not find decoder" << std::endl;
                return false;
            }

            handle->decoder_ctx = avcodec_alloc_context3(handle->decoder);
            avcodec_parameters_to_context(handle->decoder_ctx, handle->input_format_context->streams[i]->codecpar);

            i32 err = av_hwdevice_ctx_create(&handle->hw_device_ctx, AV_HWDEVICE_TYPE_CUDA, nullptr, nullptr, 0);
            if (err < 0) {
                char err_buf[AV_ERROR_MAX_STRING_SIZE];
                std::cerr << "Error: Failed to create CUDA device context: " << av_make_error_string(err_buf, AV_ERROR_MAX_STRING_SIZE, err) << std::endl;
                return false;
            }
            handle->decoder_ctx->hw_device_ctx = av_buffer_ref(handle->hw_device_ctx);

            if (avcodec_open2(handle->decoder_ctx, handle->decoder, nullptr) < 0) {
                std::cerr << "Error: Could not open handle->decoder" << std::endl;
                return false;
            }
            break;
        }
    }

    if (handle->video_stream_index == -1) {
        std::cerr << "Error: Could not find video stream" << std::endl;
        return false;
    }

    // Output format context
    avformat_alloc_output_context2(&handle->output_format_context, nullptr, "flv", output_url);

    if (!handle->output_format_context) {
        std::cerr << "Error: Could not create output context" << std::endl;
        return false;
    }

    // Find the NVENC handle->encoder
    handle->encoder = avcodec_find_encoder_by_name("h264_nvenc");
    if (!handle->encoder) {
        std::cerr << "Error: Could not find NVENC encoder" << std::endl;
        return false;
    }

    handle->out_stream = avformat_new_stream(handle->output_format_context, handle->encoder);
    if (!handle->out_stream) {
        std::cerr << "Error: Could not create output stream" << std::endl;
        return false;
    }

    handle->encoder_ctx = avcodec_alloc_context3(handle->encoder);

    handle->encoder_ctx->bit_rate = 4096; // Set desired bitrate
    handle->encoder_ctx->width = handle->decoder_ctx->width;
    handle->encoder_ctx->height = handle->decoder_ctx->height;
    handle->encoder_ctx->time_base = handle->input_format_context->streams[handle->video_stream_index]->time_base;
    handle->encoder_ctx->pix_fmt = AV_PIX_FMT_CUDA; // Use CUDA pixel format
    handle->encoder_ctx->max_b_frames = 0;
    handle->encoder_ctx->gop_size = 12; // Adjust GOP size as needed

    // Set the hardware frames context for encoding
    handle->hw_frames_ctx = av_hwframe_ctx_alloc(handle->hw_device_ctx);
    if (!handle->hw_frames_ctx) {
        std::cerr << "Error: Could not allocate hardware frame context" << std::endl;
        return false;
    }
    // Configure the frames context
    AVHWFramesContext* frames_ctx = (AVHWFramesContext*)handle->hw_frames_ctx->data;
    frames_ctx->format = AV_PIX_FMT_CUDA;  // Set the hardware pixel format
    frames_ctx->sw_format = handle->decoder_ctx->pix_fmt;  // Set the software pixel format, e.g., AV_PIX_FMT_YUV420P
    frames_ctx->width = handle->decoder_ctx->width;
    frames_ctx->height = handle->decoder_ctx->height;
    frames_ctx->initial_pool_size = 20;  // Adjust as needed

    // Initialize the hardware frames context
    i32 err = av_hwframe_ctx_init(handle->hw_frames_ctx);
    if (err < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        std::cerr << "Error: Failed to initialize hardware frames context: " << av_make_error_string(err_buf, AV_ERROR_MAX_STRING_SIZE, err) << std::endl;
        av_buffer_unref(&handle->hw_frames_ctx);
        return false;
    }

    // Step 2: Set the handle->encoder's hardware frames context and pixel format
    handle->encoder_ctx->hw_frames_ctx = av_buffer_ref(handle->hw_frames_ctx);
    handle->encoder_ctx->pix_fmt = AV_PIX_FMT_CUDA;  // Match the hardware frames context format

    if (handle->output_format_context->oformat->flags & AVFMT_GLOBALHEADER) {
        handle->encoder_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if (avcodec_open2(handle->encoder_ctx, handle->encoder, nullptr) < 0) {
        std::cerr << "Error: Could not open handle->encoder" << std::endl;
        return false;
    }

    avcodec_parameters_from_context(handle->out_stream->codecpar, handle->encoder_ctx);

    // Open output URL
    if (!(handle->output_format_context->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&handle->output_format_context->pb, output_url, AVIO_FLAG_WRITE) < 0) {
            std::cerr << "Error: Could not open output URL" << std::endl;
            return false;
        }
    }

    if (avformat_write_header(handle->output_format_context, nullptr) < 0) {
        std::cerr << "Error: Could not write header to output" << std::endl;
        return -1;
    }

    handle->frame = av_frame_alloc();
    handle->enc_pkt = av_packet_alloc();    
    return true;
}

void mtr::process(handle *handle){
    AVPacket packet = {};
    while (av_read_frame(handle->input_format_context, &packet) >= 0) {
        std::cout << "Read frame: " << packet.stream_index << " of size " << packet.size << std::endl;
        if (packet.stream_index == handle->video_stream_index) {
            int ret = avcodec_send_packet(handle->decoder_ctx, &packet);
            if (ret < 0) {
                std::cerr << "Error sending packet for decoding" << std::endl;
                break;
            }

            while (ret >= 0) {
                ret = avcodec_receive_frame(handle->decoder_ctx, handle->frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    std::cerr << "Error during decoding" << std::endl;
                    return;
                }

                handle->frame->pts = av_rescale_q(handle->frame->pts, handle->input_format_context->streams[handle->video_stream_index]->time_base, handle->encoder_ctx->time_base);

                // Send frame to NVENC encoder
                ret = avcodec_send_frame(handle->encoder_ctx, handle->frame);
                if (ret < 0) {
                    std::cerr << "Error sending frame to encoder" << std::endl;
                    return;
                }

                while (ret >= 0) {
                    ret = avcodec_receive_packet(handle->encoder_ctx, handle->enc_pkt);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                        break;
                    } else if (ret < 0) {
                        std::cerr << "Error during encoding" << std::endl;
                        return;
                    }

                    handle->enc_pkt->pts = av_rescale_q(handle->enc_pkt->pts, handle->encoder_ctx->time_base, handle->out_stream->time_base);
                    handle->enc_pkt->dts = av_rescale_q(handle->enc_pkt->dts, handle->encoder_ctx->time_base, handle->out_stream->time_base);
                    handle->enc_pkt->duration = av_rescale_q(handle->enc_pkt->duration, handle->encoder_ctx->time_base, handle->out_stream->time_base);
                    handle->enc_pkt->stream_index = handle->out_stream->index;

                    if (av_interleaved_write_frame(handle->output_format_context, handle->enc_pkt) < 0) {
                        std::cerr << "Error writing frame" << std::endl;
                        return;
                    }

                    av_packet_unref(handle->enc_pkt);
                }
            }
        }
        av_packet_unref(&packet);
    }
}

void mtr::cleanup(handle *handle){
    // Flush the encoder
    avcodec_send_frame(handle->encoder_ctx, nullptr);
    while (avcodec_receive_packet(handle->encoder_ctx, handle->enc_pkt) == 0) {
        av_interleaved_write_frame(handle->output_format_context, handle->enc_pkt);
        av_packet_unref(handle->enc_pkt);
    }

    av_write_trailer(handle->output_format_context);

    // Cleanup
    av_frame_free(&handle->frame);
    avcodec_free_context(&handle->encoder_ctx);
    avcodec_free_context(&handle->decoder_ctx);
    av_buffer_unref(&handle->hw_frames_ctx);
    av_buffer_unref(&handle->hw_device_ctx);
    avformat_close_input(&handle->input_format_context);
    if (!(handle->output_format_context->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&handle->output_format_context->pb);
    }
    avformat_free_context(handle->output_format_context);
}
