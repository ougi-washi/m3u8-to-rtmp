#include "handles.h"
#include <iostream>
#include <fstream>
#include "utility.h"
#include "json/json.h"

using json = nlohmann::json;

template<typename T>
void set(T& target, const json& j, const std::string& key){
    if(j.contains(key)){
        target = j[key].get<T>();
    }
}

const mtr::info mtr::get_info(const std::string &file_path){
    if (!std::filesystem::exists(mtr::root_path + file_path)){
        std::cerr << "Error: Could not find file " << file_path << std::endl;
        return {};
    }
    std::ifstream parsed_file(mtr::root_path + file_path);
    json json;
    parsed_file >> json;
    mtr::info info = {};
    set(info.input_url,         json, "input_url");
    set(info.output_url,        json, "output_url");
    set(info.video_bitrate,     json, "video_bitrate");
    set(info.gop_size,          json, "gop_size");
    set(info.audio_bitrate,     json, "audio_bitrate");
    return info;
}

b8 mtr::init(handle *handle, const info &info)
{
    const char* input_url = info.input_url.c_str();
    const char* output_url = info.output_url.c_str();

    avformat_network_init();
    av_log_set_level(AV_LOG_VERBOSE);

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

    handle->encoder_ctx->bit_rate = info.video_bitrate * 1000; // Set desired bitrate
    handle->encoder_ctx->width = handle->decoder_ctx->width;
    handle->encoder_ctx->height = handle->decoder_ctx->height;
    handle->encoder_ctx->time_base = handle->input_format_context->streams[handle->video_stream_index]->time_base;
    handle->encoder_ctx->pix_fmt = AV_PIX_FMT_CUDA; // Use CUDA pixel format
    handle->encoder_ctx->max_b_frames = 0;
    handle->encoder_ctx->gop_size = info.gop_size;

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

    // Audio
    for (i32 i = 0; i < handle->input_format_context->nb_streams; i++) {
        if (handle->input_format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            handle->audio_stream_index = i;
            handle->audio_decoder = avcodec_find_decoder(handle->input_format_context->streams[i]->codecpar->codec_id);
            if (!handle->audio_decoder) {
                std::cerr << "Error: Could not find audio decoder" << std::endl;
                return false;
            }

            handle->audio_decoder_ctx = avcodec_alloc_context3(handle->audio_decoder);
            avcodec_parameters_to_context(handle->audio_decoder_ctx, handle->input_format_context->streams[i]->codecpar);

            if (avcodec_open2(handle->audio_decoder_ctx, handle->audio_decoder, nullptr) < 0) {
                std::cerr << "Error: Could not open audio decoder" << std::endl;
                return false;
            }
            break;
        }
    }

    handle->audio_encoder = avcodec_find_encoder(AV_CODEC_ID_AAC);  // Change based on your desired format
    if (!handle->audio_encoder) {
        std::cerr << "Error: Could not find audio encoder" << std::endl;
        return false;
    }

    handle->audio_out_stream = avformat_new_stream(handle->output_format_context, handle->audio_encoder);
    handle->audio_encoder_ctx = avcodec_alloc_context3(handle->audio_encoder);

    handle->audio_encoder_ctx->sample_rate = handle->audio_decoder_ctx->sample_rate;
    handle->audio_encoder_ctx->channel_layout = handle->audio_decoder_ctx->channel_layout;
    handle->audio_encoder_ctx->channels = av_get_channel_layout_nb_channels(handle->audio_encoder_ctx->channel_layout);
    handle->audio_encoder_ctx->sample_fmt = handle->audio_encoder->sample_fmts[0];  // Set compatible format
    handle->audio_encoder_ctx->bit_rate = info.audio_bitrate * 1000;  // Adjust as needed

    if (avcodec_open2(handle->audio_encoder_ctx, handle->audio_encoder, nullptr) < 0) {
        std::cerr << "Error: Could not open audio encoder" << std::endl;
        return false;
    }

    avcodec_parameters_from_context(handle->audio_out_stream->codecpar, handle->audio_encoder_ctx);
    
    handle->audio_frame = av_frame_alloc();
    handle->audio_enc_pkt = av_packet_alloc();

    return true;
}

void mtr::process(handle *handle){
    AVPacket packet = {};
    while (av_read_frame(handle->input_format_context, &packet) >= 0) {
        std::cout << "Read frame: " << packet.stream_index << " of size " << packet.size << std::endl;
        if (packet.stream_index == handle->video_stream_index) {
            i32 ret = avcodec_send_packet(handle->decoder_ctx, &packet);
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
        else if (packet.stream_index == handle->audio_stream_index) {
            i32 ret = avcodec_send_packet(handle->audio_decoder_ctx, &packet);
            if (ret < 0) {
                std::cerr << "Error sending audio packet for decoding" << std::endl;
                break;
            }

            // ret = avcodec_receive_packet(handle->audio_encoder_ctx, handle->audio_enc_pkt);
            // if (ret < 0) {
            //     if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            //         return; // no packet to write yet
            //     } else {
            //         std::cerr << "Error receiving audio packet" << std::endl;
            //         return;
            //     }
            // }

            while (ret >= 0) {
                ret = avcodec_receive_frame(handle->audio_decoder_ctx, handle->audio_frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    std::cerr << "Error during audio decoding" << std::endl;
                    return;
                }

                ret = avcodec_send_frame(handle->audio_encoder_ctx, handle->audio_frame);
                if (ret < 0) {
                    std::cerr << "Error sending audio frame to encoder" << std::endl;
                    return;
                }

                while (ret >= 0) {
                    ret = avcodec_receive_packet(handle->audio_encoder_ctx, handle->audio_enc_pkt);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                        break;
                    } else if (ret < 0) {
                        std::cerr << "Error during audio encoding" << std::endl;
                        return;
                    }

                    handle->audio_enc_pkt->stream_index = handle->audio_out_stream->index;
                    ret = avcodec_receive_frame(handle->audio_decoder_ctx, handle->audio_frame);

                    // if (handle->audio_enc_pkt && handle->audio_enc_pkt->data && handle->audio_enc_pkt->size > 0) {
                    //     i32 ret = av_interleaved_write_frame(handle->output_format_context, handle->audio_enc_pkt);
                    //     if (ret < 0) {
                    //         char err_buf[AV_ERROR_MAX_STRING_SIZE];
                    //         std::cerr << "Error: writing audio packet: " << av_make_error_string(err_buf, AV_ERROR_MAX_STRING_SIZE, ret);
                    //     }
                    //     av_packet_unref(handle->audio_enc_pkt);  // Free packet for reuse
                    // } else {
                    //     fprintf(stderr, "Invalid packet: No data or size\n");
                    // }
                }
            }
        }
        av_packet_unref(&packet);
    }
    std::cout << "Finished processing" << std::endl;
}

void mtr::cleanup(handle *handle){
    // Flush the encoder
    avcodec_send_frame(handle->encoder_ctx, nullptr);
    while (avcodec_receive_packet(handle->encoder_ctx, handle->enc_pkt) == 0) {
        av_interleaved_write_frame(handle->output_format_context, handle->enc_pkt);
        av_packet_unref(handle->enc_pkt);
    }

    av_write_trailer(handle->output_format_context);

    // Video
    av_frame_free(&handle->frame);
    avcodec_free_context(&handle->encoder_ctx);
    avcodec_free_context(&handle->decoder_ctx);
    av_buffer_unref(&handle->hw_frames_ctx);
    av_buffer_unref(&handle->hw_device_ctx);
    avformat_close_input(&handle->input_format_context);
    // Audio
    avcodec_free_context(&handle->audio_encoder_ctx);
    avcodec_free_context(&handle->audio_decoder_ctx);
    av_frame_free(&handle->audio_frame);
    av_packet_free(&handle->audio_enc_pkt);

    // Final
    if (!(handle->output_format_context->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&handle->output_format_context->pb);
    }
    avformat_free_context(handle->output_format_context);
}
