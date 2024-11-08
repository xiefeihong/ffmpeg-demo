#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

// BMP 文件头定义
#pragma pack(push, 1)
typedef struct {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} BITMAPFILEHEADER;

typedef struct {
    uint32_t biSize;
    int32_t biWidth;
    int32_t biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t biXPelsPerMeter;
    int32_t biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} BITMAPINFOHEADER;
#pragma pack(pop)

// 写入 BMP 文件
void write_bmp(const char *filename, uint8_t *data, int width, int height) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    BITMAPFILEHEADER file_header;
    BITMAPINFOHEADER info_header;

    file_header.bfType = 0x4D42; // 'BM'
    file_header.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + width * height * 3;
    file_header.bfReserved1 = 0;
    file_header.bfReserved2 = 0;
    file_header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    info_header.biSize = sizeof(BITMAPINFOHEADER);
    info_header.biWidth = width;
    info_header.biHeight = height;
    info_header.biPlanes = 1;
    info_header.biBitCount = 24;
    info_header.biCompression = 0; // BI_RGB
    info_header.biSizeImage = width * height * 3;
    info_header.biXPelsPerMeter = 0;
    info_header.biYPelsPerMeter = 0;
    info_header.biClrUsed = 0;
    info_header.biClrImportant = 0;

    fwrite(&file_header, sizeof(BITMAPFILEHEADER), 1, file);
    fwrite(&info_header, sizeof(BITMAPINFOHEADER), 1, file);
    fwrite(data, width * height * 3, 1, file);

    fclose(file);
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

        write_bmp(buf, rgb_data, ctx->width, ctx->height);


        if (pkt) {
            av_packet_unref(pkt);
        }
    }
end:
    return 0;
}

// output.mp4 %03d.bmp
int main(int argc, char **argv)
{
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
        exit(-1);
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
        av_log(NULL, AV_LOG_ERROR, "NO MEMORY!\n");
        goto err;
    }

    // 创建AVPacket
    AVPacket *pkt = av_packet_alloc();
    if (!pkt) {
        av_log(NULL, AV_LOG_ERROR, "NO MEMORY!\n");
        goto err;
    }

    // 从源多媒体文件中读到的视频数据到目的文件中
    while (av_read_frame(fmt_ctx, pkt) >= 0) {
        if (pkt->stream_index == idx){
            decode(ctx, frame, pkt, dst);
        }
    }
    decode(ctx, frame, NULL, dst);

err:
    if (fmt_ctx) {
        avformat_close_input(&fmt_ctx);
        fmt_ctx =NULL;
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
