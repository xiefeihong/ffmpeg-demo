#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

static void save_pic(unsigned char *buf, int linesize, int width, int height, char *name) {
    FILE *f;
    f = fopen(name, "wb");
    fprintf(f, "P5\n%d %d\n%d\n", width, height, 255);
    for (int i = 0; i < height; ++i) {
        fwrite(buf + i * linesize, 1, width, f);
    }
    fclose(f);
}

static int decode(AVCodecContext *ctx, AVFrame *frame, AVPacket *pkt, const char *filename) {
    int ret = -1;
    char buf[1024];
    ret = avcodec_send_packet(ctx, pkt);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "failed to send frame to decode\n");
        goto end;
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return 0;
        } else if (ret < 0) {
            return -1;
        }
        snprintf(buf, sizeof(buf), filename, ctx->frame_num);
        save_pic(frame->data[0], frame->linesize[0], frame->width, frame->height, buf);
        if (pkt) {
            av_packet_unref(pkt);
        }
    }
end:
    return 0;
}

// output.mp4 %03d
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
