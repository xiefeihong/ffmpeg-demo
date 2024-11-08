#include <stdio.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>

// 保存帧为PPM文件
void save_frame(AVFrame *frame, int width, int height, char *filename) {
    FILE *f;
    int y;

    f = fopen(filename, "wb");
    if (f == NULL) {
        return;
    }

    // 写入PPM文件头
    fprintf(f, "P6\n%d %d\n255\n", width, height);

    // 逐行写入像素数据
    for (y = 0; y < height; y++) {
        fwrite(frame->data[0] + y * frame->linesize[0], 1, width * 3, f);
    }

    fclose(f);
}

// output.mp4 %03d.ppm
int main(int argc, char *argv[]) {

    char *src, *dst;
    AVFormatContext *fmt_ctx = NULL;
    int i, stream, num_bytes, ret;
    AVCodecContext *ctx = NULL;
    AVCodec *codec = NULL;
    AVFrame *src_frame, *dst_frame;
    AVPacket *pkt;
    uint8_t *buffer = NULL;
    struct SwsContext *sws_ctx = NULL;
    char filename[20];

    av_log_set_level(AV_LOG_DEBUG);

    // 输入参数
    if (argc < 3) {
        av_log(NULL, AV_LOG_ERROR, "arguments must be more than 2\n");
        goto err;
    }

    src = argv[1];
    dst = argv[2];

    // 打开视频文件
    ret = avformat_open_input(&fmt_ctx, src, NULL, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "%s\n", av_err2str(ret));
        goto err;
    }

    // 检索流信息
    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not find stream: %s\n", av_err2str(ret));
        goto err;
    }

    // 查找视频流
    stream = -1;
    for (i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            stream = i;
            break;
        }
    }
    if (stream == -1) {
        av_log(NULL, AV_LOG_ERROR, "Could not find video stream\n");
        goto err;
    }

    // 获取视频编解码器上下文
    ctx = avcodec_alloc_context3(NULL);
    avcodec_parameters_to_context(ctx, fmt_ctx->streams[stream]->codecpar);

    // 查找解码器
    codec = avcodec_find_decoder(ctx->codec_id);
    if (codec == NULL) {
        av_log(NULL, AV_LOG_ERROR, "don't find Codec\n");
        goto err;;
    }

    // 打开解码器
    ret = avcodec_open2(ctx, codec, NULL);
    if (ret < 0) {
        av_log(ctx, AV_LOG_ERROR, "Don't open codec: %s \n", av_err2str(ret));
        goto err;
    }

    // 分配视频帧内存
    src_frame = av_frame_alloc();
    dst_frame = av_frame_alloc();

    // 确定所需缓冲区大小并分配缓冲区
    num_bytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, ctx->width, ctx->height, 1);
    buffer = av_malloc(num_bytes * sizeof(uint8_t));
    av_image_fill_arrays(dst_frame->data, dst_frame->linesize, buffer, AV_PIX_FMT_RGB24, ctx->width, ctx->height, 1);

    // 初始化SWS上下文用于格式转换
    sws_ctx = sws_getContext(ctx->width, ctx->height, ctx->pix_fmt,
                             ctx->width, ctx->height, AV_PIX_FMT_RGB24,
                             SWS_BILINEAR, NULL, NULL, NULL);

    // 创建AVPacket
    pkt = av_packet_alloc();
    if (!pkt) {
        av_log(NULL, AV_LOG_ERROR, "NO MEMORY!\n");
        goto err;
    }

    i = 0;
    // 读取视频帧
    while (av_read_frame(fmt_ctx, pkt) >= 0) {
        if (pkt->stream_index == stream) {
            // 解码视频帧
            avcodec_send_packet(ctx, pkt);
            avcodec_receive_frame(ctx, src_frame);

            // 转换格式并保存帧
            sws_scale(sws_ctx, (uint8_t const * const *)src_frame->data,
                      src_frame->linesize, 0, ctx->height,
                      dst_frame->data, dst_frame->linesize);
            sprintf(filename, dst, i + 1);
            save_frame(dst_frame, ctx->width, ctx->height, filename);
            i++;
        }
    }

err:
    if (buffer) {
        av_free(buffer);
    }
    if (src_frame) {
        av_frame_free(&src_frame);
    }
    if (dst_frame) {
        av_frame_free(&dst_frame);
    }
    if (pkt) {
        av_packet_unref(pkt);
    }
    if (ctx) {
        avcodec_close(ctx);
    }
    if (fmt_ctx) {
        avformat_close_input(&fmt_ctx);
    }
    return 0;
}