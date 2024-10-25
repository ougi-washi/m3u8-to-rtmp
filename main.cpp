// MTR https://github.com/ougi-washi/m3u8-to-rtmp
// extern "C" {
// #include <libavcodec/avcodec.h>
// #include <libavformat/avformat.h>
// #include <libavutil/avutil.h>
// #include <libavutil/opt.h>
// #include <libavutil/hwcontext.h>
// #include <libswscale/swscale.h>
// }

// #include <iostream>

#include "handles.h"
#include <iostream>

i32 main(i32 argc, char const *argv[])
{
    mtr::info info = {};
    info.input_url = "";
    info.output_url = "";
    info.resolution_scale = 0.5f;
    info.video_bitrate = 4096;

    mtr::handle* handle = new mtr::handle();

    if (!mtr::init(handle, info))
    {
        std::cerr << "Error: Could not initialize handle" << std::endl;
        return -1;
    }

    mtr::process(handle);
    mtr::cleanup(handle);
    return 0;
}

// AVBufferRef* hw_device_ctx = nullptr;
// int init_hw_device(AVCodecContext* decoder_ctx) {
//     int err = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_CUDA, nullptr, nullptr, 0);
//     if (err < 0) {
//         char err_buf[AV_ERROR_MAX_STRING_SIZE];
//         std::cerr << "Error: Failed to create CUDA device context: " << av_make_error_string(err_buf, AV_ERROR_MAX_STRING_SIZE, err) << std::endl;
//         return err;
//     }
//     decoder_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
//     return 0;
// }

// int main(int argc, char* argv[]) {
//     const char* input_url = "";
//     const char* output_url = "";

//     avformat_network_init();

//     // Input format context
//     AVFormatContext* input_format_context = nullptr;
//     if (avformat_open_input(&input_format_context, input_url, nullptr, nullptr) < 0) {
//         std::cerr << "Error: Could not open input stream" << std::endl;
//         return -1;
//     }

//     if (avformat_find_stream_info(input_format_context, nullptr) < 0) {
//         std::cerr << "Error: Could not find stream information" << std::endl;
//         return -1;
//     }

//     // Find video stream
//     AVCodec* decoder = nullptr;
//     AVCodecContext* decoder_ctx = nullptr;
//     int video_stream_index = -1;

//     for (int i = 0; i < input_format_context->nb_streams; i++) {
//         if (input_format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
//             video_stream_index = i;
//             decoder = avcodec_find_decoder(input_format_context->streams[i]->codecpar->codec_id);
//             if (!decoder) {
//                 std::cerr << "Error: Could not find decoder" << std::endl;
//                 return -1;
//             }

//             decoder_ctx = avcodec_alloc_context3(decoder);
//             avcodec_parameters_to_context(decoder_ctx, input_format_context->streams[i]->codecpar);

//             if (init_hw_device(decoder_ctx) < 0) {
//                 std::cerr << "Error: Failed to initialize hardware device" << std::endl;
//                 return -1;
//             }

//             if (avcodec_open2(decoder_ctx, decoder, nullptr) < 0) {
//                 std::cerr << "Error: Could not open decoder" << std::endl;
//                 return -1;
//             }
//             break;
//         }
//     }

//     if (video_stream_index == -1) {
//         std::cerr << "Error: Could not find video stream" << std::endl;
//         return -1;
//     }

//     // Output format context
//     AVFormatContext* output_format_context = nullptr;
//     avformat_alloc_output_context2(&output_format_context, nullptr, "flv", output_url);

//     if (!output_format_context) {
//         std::cerr << "Error: Could not create output context" << std::endl;
//         return -1;
//     }

//     // Find the NVENC encoder
//     AVCodec* encoder = avcodec_find_encoder_by_name("h264_nvenc");
//     if (!encoder) {
//         std::cerr << "Error: Could not find NVENC encoder" << std::endl;
//         return -1;
//     }

//     AVStream* out_stream = avformat_new_stream(output_format_context, encoder);
//     if (!out_stream) {
//         std::cerr << "Error: Could not create output stream" << std::endl;
//         return -1;
//     }

//     AVCodecContext* encoder_ctx = avcodec_alloc_context3(encoder);

//     encoder_ctx->bit_rate = 4096; // Set desired bitrate
//     encoder_ctx->width = decoder_ctx->width;
//     encoder_ctx->height = decoder_ctx->height;
//     encoder_ctx->time_base = input_format_context->streams[video_stream_index]->time_base;
//     encoder_ctx->pix_fmt = AV_PIX_FMT_CUDA; // Use CUDA pixel format
//     encoder_ctx->max_b_frames = 0;
//     encoder_ctx->gop_size = 12; // Adjust GOP size as needed

//     // Set the hardware frames context for encoding
//     AVBufferRef* hw_frames_ctx = av_hwframe_ctx_alloc(hw_device_ctx);
//     if (!hw_frames_ctx) {
//         std::cerr << "Error: Could not allocate hardware frame context" << std::endl;
//         return -1;
//     }
//     // Configure the frames context
//     AVHWFramesContext* frames_ctx = (AVHWFramesContext*)hw_frames_ctx->data;
//     frames_ctx->format = AV_PIX_FMT_CUDA;  // Set the hardware pixel format
//     frames_ctx->sw_format = decoder_ctx->pix_fmt;  // Set the software pixel format, e.g., AV_PIX_FMT_YUV420P
//     frames_ctx->width = decoder_ctx->width;
//     frames_ctx->height = decoder_ctx->height;
//     frames_ctx->initial_pool_size = 20;  // Adjust as needed

//     // Initialize the hardware frames context
//     int err = av_hwframe_ctx_init(hw_frames_ctx);
//     if (err < 0) {
//         char err_buf[AV_ERROR_MAX_STRING_SIZE];
//         std::cerr << "Error: Failed to initialize hardware frames context: " << av_make_error_string(err_buf, AV_ERROR_MAX_STRING_SIZE, err) << std::endl;
//         av_buffer_unref(&hw_frames_ctx);
//         return -1;
//     }

//     // Step 2: Set the encoder's hardware frames context and pixel format
//     encoder_ctx->hw_frames_ctx = av_buffer_ref(hw_frames_ctx);
//     encoder_ctx->pix_fmt = AV_PIX_FMT_CUDA;  // Match the hardware frames context format

//     if (output_format_context->oformat->flags & AVFMT_GLOBALHEADER) {
//         encoder_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
//     }

//     if (avcodec_open2(encoder_ctx, encoder, nullptr) < 0) {
//         std::cerr << "Error: Could not open encoder" << std::endl;
//         return -1;
//     }

//     avcodec_parameters_from_context(out_stream->codecpar, encoder_ctx);

//     // Open output URL
//     if (!(output_format_context->oformat->flags & AVFMT_NOFILE)) {
//         if (avio_open(&output_format_context->pb, output_url, AVIO_FLAG_WRITE) < 0) {
//             std::cerr << "Error: Could not open output URL" << std::endl;
//             return -1;
//         }
//     }

//     if (avformat_write_header(output_format_context, nullptr) < 0) {
//         std::cerr << "Error: Could not write header to output" << std::endl;
//         return -1;
//     }

//     AVPacket packet;
//     AVFrame* frame = av_frame_alloc();
//     AVPacket* enc_pkt = av_packet_alloc();

//     while (av_read_frame(input_format_context, &packet) >= 0) {
//         std::cout << "Read frame: " << packet.stream_index << " of size " << packet.size << std::endl;
//         if (packet.stream_index == video_stream_index) {
//             int ret = avcodec_send_packet(decoder_ctx, &packet);
//             if (ret < 0) {
//                 std::cerr << "Error sending packet for decoding" << std::endl;
//                 break;
//             }

//             while (ret >= 0) {
//                 ret = avcodec_receive_frame(decoder_ctx, frame);
//                 if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
//                     break;
//                 } else if (ret < 0) {
//                     std::cerr << "Error during decoding" << std::endl;
//                     return -1;
//                 }

//                 frame->pts = av_rescale_q(frame->pts, input_format_context->streams[video_stream_index]->time_base, encoder_ctx->time_base);

//                 // Send frame to NVENC encoder
//                 ret = avcodec_send_frame(encoder_ctx, frame);
//                 if (ret < 0) {
//                     std::cerr << "Error sending frame to encoder" << std::endl;
//                     return -1;
//                 }

//                 while (ret >= 0) {
//                     ret = avcodec_receive_packet(encoder_ctx, enc_pkt);
//                     if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
//                         break;
//                     } else if (ret < 0) {
//                         std::cerr << "Error during encoding" << std::endl;
//                         return -1;
//                     }

//                     enc_pkt->pts = av_rescale_q(enc_pkt->pts, encoder_ctx->time_base, out_stream->time_base);
//                     enc_pkt->dts = av_rescale_q(enc_pkt->dts, encoder_ctx->time_base, out_stream->time_base);
//                     enc_pkt->duration = av_rescale_q(enc_pkt->duration, encoder_ctx->time_base, out_stream->time_base);
//                     enc_pkt->stream_index = out_stream->index;

//                     if (av_interleaved_write_frame(output_format_context, enc_pkt) < 0) {
//                         std::cerr << "Error writing frame" << std::endl;
//                         return -1;
//                     }

//                     av_packet_unref(enc_pkt);
//                 }
//             }
//         }
//         av_packet_unref(&packet);
//     }

//     // Flush the encoder
//     avcodec_send_frame(encoder_ctx, nullptr);
//     while (avcodec_receive_packet(encoder_ctx, enc_pkt) == 0) {
//         av_interleaved_write_frame(output_format_context, enc_pkt);
//         av_packet_unref(enc_pkt);
//     }

//     av_write_trailer(output_format_context);

//     // Cleanup
//     av_frame_free(&frame);
//     avcodec_free_context(&encoder_ctx);
//     avcodec_free_context(&decoder_ctx);
//     av_buffer_unref(&hw_frames_ctx);
//     av_buffer_unref(&hw_device_ctx);
//     avformat_close_input(&input_format_context);
//     if (!(output_format_context->oformat->flags & AVFMT_NOFILE)) {
//         avio_closep(&output_format_context->pb);
//     }
//     avformat_free_context(output_format_context);

//     return 0;
// }
