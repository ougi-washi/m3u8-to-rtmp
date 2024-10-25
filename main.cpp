// MTR https://github.com/ougi-washi/m3u8-to-rtmp

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include <iostream>

int main() {
    // Initialize FFmpeg
    avformat_network_init();

    // Input and output URLs
    const char* input_url = ""; // Replace with your m3u8 URL
    const char* output_url = ""; // Replace with your RTMP URL

    // Open the input format context
    AVFormatContext* input_format_context = nullptr;
    if (avformat_open_input(&input_format_context, input_url, nullptr, nullptr) < 0) {
        std::cerr << "Error: Unable to open input URL" << std::endl;
        return -1;
    }

    // Find input stream information
    if (avformat_find_stream_info(input_format_context, nullptr) < 0) {
        std::cerr << "Error: Could not find stream info" << std::endl;
        avformat_close_input(&input_format_context);
        return -1;
    }

    // Output format context
    AVFormatContext* output_format_context = nullptr;
    if (avformat_alloc_output_context2(&output_format_context, nullptr, "flv", output_url) < 0) {
        std::cerr << "Error: Could not allocate output context" << std::endl;
        avformat_close_input(&input_format_context);
        return -1;
    }

    // Copy stream information from input to output
    for (int i = 0; i < input_format_context->nb_streams; i++) {
        AVStream* in_stream = input_format_context->streams[i];
        AVStream* out_stream = avformat_new_stream(output_format_context, nullptr);
        if (!out_stream) {
            std::cerr << "Error: Could not create output stream" << std::endl;
            avformat_close_input(&input_format_context);
            avformat_free_context(output_format_context);
            return -1;
        }

        // Copy codec parameters
        if (avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar) < 0) {
            std::cerr << "Error: Failed to copy codec parameters" << std::endl;
            avformat_close_input(&input_format_context);
            avformat_free_context(output_format_context);
            return -1;
        }
        out_stream->codecpar->codec_tag = 0;
    }

    // Open output URL for writing
    if (!(output_format_context->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&output_format_context->pb, output_url, AVIO_FLAG_WRITE) < 0) {
            std::cerr << "Error: Could not open output URL" << std::endl;
            avformat_close_input(&input_format_context);
            avformat_free_context(output_format_context);
            return -1;
        }
    }

    // Write header to the output
    if (avformat_write_header(output_format_context, nullptr) < 0) {
        std::cerr << "Error: Could not write output header" << std::endl;
        avformat_close_input(&input_format_context);
        avio_closep(&output_format_context->pb);
        avformat_free_context(output_format_context);
        return -1;
    }

    // Start reading packets from input and writing to output
    AVPacket packet;
    while (av_read_frame(input_format_context, &packet) >= 0) {
        // Find the output stream
        AVStream* in_stream = input_format_context->streams[packet.stream_index];
        AVStream* out_stream = output_format_context->streams[packet.stream_index];

        // Adjust the packet for the output stream
        packet.pts = av_rescale_q_rnd(packet.pts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF);
        packet.dts = av_rescale_q_rnd(packet.dts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF);
        packet.duration = av_rescale_q(packet.duration, in_stream->time_base, out_stream->time_base);
        packet.pos = -1;

        // Write the packet
        if (av_interleaved_write_frame(output_format_context, &packet) < 0) {
            std::cerr << "Error: Failed to write frame" << std::endl;
            break;
        }
        // Free packet
        av_packet_unref(&packet);
    }

    // Write trailer and clean up
    av_write_trailer(output_format_context);
    avformat_close_input(&input_format_context);
    if (!(output_format_context->oformat->flags & AVFMT_NOFILE))
        avio_closep(&output_format_context->pb);
    avformat_free_context(output_format_context);

    std::cout << "Streaming completed" << std::endl;
    return 0;
}
