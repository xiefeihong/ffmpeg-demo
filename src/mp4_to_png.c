#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
// 引入libpng库
#include <png.h>

// 写入PNG文件
void write_png(const char *filename, uint8_t *data, int width, int height) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        fclose(fp);
        fprintf(stderr, "png_create_write_struct failed\n");
        exit(EXIT_FAILURE);
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_write_struct(&png_ptr, NULL);
        fclose(fp);
        fprintf(stderr, "png_create_info_struct failed\n");
        exit(EXIT_FAILURE);
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        fprintf(stderr, "Error during png writing\n");
        exit(EXIT_FAILURE);
    }

    png_init_io(png_ptr, fp);

    // 设置PNG图像的相关信息
    png_set_IHDR(png_ptr, info_ptr, width, height,
                 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    png_write_info(png_ptr, info_ptr);

    // 写入图像数据
    for (int y = 0; y < height; y++) {
        png_write_row(png_ptr, data + y * width * 3);
    }

    png_write_end(png_ptr, NULL);

    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
}

static int decode(AVCodecContext *ctx, AVFrame *frame, AVPacket *pkt, const char *filename) {
    int ret = -1;
    char buf[1024];
    ret = avcodec_send_packet(ctx, pkt);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "failed to send frame to decode\n");
        goto end;
    }

    frame->format = ctx->pix_fmt;
    frame->width = ctx->width;
    frame->height = ctx->height;
    uint8_t *rgb_data = av_malloc(av_image_get_buffer_size(AV_PIX_FMT_RGB24, ctx->width, ctx->height, 1));
    if (!rgb_data) {
        fprintf(stderr, "Could not allocate RGB data buffer\n");
        return 1;
    }

    struct SwsContext *sws_ctx = sws_getContext(ctx->width, ctx->height, ctx->pix_fmt,
                                                ctx->width, ctx->height, AV_PIX_FMT_RGB24,
                                                SWS_BILINEAR, NULL, NULL, NULL);
    if (!sws_ctx) {
        fprintf(stderr, "Could not initialize the conversion context\n");
        return 1;
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return 0;
        } else if (ret < 0) {
            return -1;
        }

        int rgb_linesize = ctx->width * 3;
        uint8_t *rgb_data_ptr[1] = { rgb_data }; // 对于单平面格式，如 RGB24
        const int dstStride[1] = { rgb_linesize };
        sws_scale(sws_ctx, (const uint8_t *const *)frame->data, frame->linesize, 0,
                  ctx->height, rgb_data_ptr, dstStride);

        snprintf(buf, sizeof(buf), filename, ctx->frame_num);

        write_png(buf, rgb_data, ctx->width, ctx->height);

        if (pkt) {
            av_packet_unref(pkt);
        }
    }
end:
    return 0;
}

// output.mp4 %03d.png
int main(int argc, char **argv) {
    const char *src, *dst;
    int ret = 0;
    AVFormatContext *fmt_ctx;
    AVCodecContext *ctx;
    av_log_set_level(AV_LOG_DEBUG);
    if (argc <= 2) {
        fprintf(stderr, "Usage: %s <input file> <output file>\n", argv[0]);
        exit(0);
    }
    src = argv[1];
    dst = argv[2];

    // 打开多媒体文件
    ret = avformat_open_input(&fmt_ctx, src, NULL, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "%s\n", av_err2str(ret));
        goto err;
    }

    // 从多媒体文件中找到视频流
    int idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (idx < 0) {
        av_log(fmt_ctx, AV_LOG_ERROR, "Does not include video stream!\n");
        goto err;
    }
    AVStream *in_stream = fmt_ctx->streams[idx];

    // 查找解码器
    const AVCodec *codec = avcodec_find_decoder(in_stream->codecpar->codec_id);
    if (!codec) {
        av_log(NULL, AV_LOG_ERROR, "Could not find Codec\n");
        goto err;
    }

    // 创建解码器上下文
    ctx = avcodec_alloc_context3(codec);
    if (!ctx) {
        av_log(NULL, AV_LOG_ERROR, "NO MEMEORY\n");
        goto err;
    }
    avcodec_parameters_to_context(ctx, in_stream->codecpar);

    // 解码器与解码器上下文绑定到一起
    ret = avcodec_open2(ctx, codec, NULL);
    if (ret < 0) {
        av_log(ctx, AV_LOG_ERROR, "Don't open codec: %s \n", av_err2str(ret));
        goto err;
    }

    // 创建AVFrame
    AVFrame *frame = av_frame_alloc();
    if (!frame) {
        av_log(NULL, AV_LOG_ERROR, "NO MEMEORY!\n");
        goto err;
    }

    // 创建AVPacket
    AVPacket *pkt = av_packet_alloc();
    if (!pkt) {
        av_log(NULL, AV_LOG_ERROR, "NO MEMEORY!\n");
        goto err;
    }

    // 从源多媒体文件中读到的视频数据到目的文件中
    while (av_read_frame(fmt_ctx, pkt) >= 0) {
        if (pkt->stream_index == idx) {
            // 调用write_png函数来保存为PNG图片
            decode(ctx, frame, pkt, dst);
        }
    }
    decode(ctx, frame, NULL, dst);

    err:
    if (fmt_ctx) {
        avformat_close_input(&fmt_ctx);
        fmt_ctx = NULL;
    }
    if (ctx) {
        avcodec_free_context(&ctx);
        ctx = NULL;
    }
    if (frame) {
        av_frame_free(&frame);
        frame = NULL;
    }
    if (pkt) {
        av_packet_free(&pkt);
        pkt = NULL;
    }
    return 0;
}