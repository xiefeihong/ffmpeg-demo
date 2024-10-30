#include <libavutil/log.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>

static int encode(AVCodecContext *ctx, AVFrame *frame, AVPacket *pkt, FILE *out) {
    int ret = avcodec_send_frame(ctx, frame);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to send frame to encoder!\n");
        goto end;
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return 0;
        } else if (ret < 0) {
            return -1;
        }

        fwrite(pkt->data, 1, pkt->size, out);
        av_packet_unref(pkt);
    }
end:
    return 0;
}

// output.mp4 mpeg4
int main(int argc, char* argv[]){

    av_log_set_level(AV_LOG_DEBUG);

    // 输入参数
    if (argc < 3) {
        av_log(NULL, AV_LOG_ERROR, "arguments must be more than 3\n");
        goto err;
    }
    char *dst = argv[1];
    char *codecname = argv[2];
    
    // 查找编码器
    const AVCodec *codec = avcodec_find_encoder_by_name(codecname);
    if (!codec) {
        av_log(NULL, AV_LOG_ERROR, "don't find Codec: %s\n", codecname);
        goto err;
    }

    // 创建编码器上下文
    AVCodecContext *ctx = avcodec_alloc_context3(codec);
    if (!ctx) {
        av_log(NULL, AV_LOG_ERROR, "NO MEMRORY\n");
        goto err;
    }

    // 设置编码器参数
    ctx->width = 640;
    ctx->height = 480;
    ctx->bit_rate = 500000;
    ctx->time_base = (AVRational){1, 25};
    ctx->framerate = (AVRational){25, 1};
    ctx->gop_size = 10;
    ctx->max_b_frames = 1;
    ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    if (codec->id == AV_CODEC_ID_H264) {
        av_opt_set(ctx->priv_data, "preset", "slow", 0);
    }

    // 编码器与编码器上下文绑定到一起
    int ret = avcodec_open2(ctx, codec, NULL);
    if (ret < 0) {
        av_log(ctx, AV_LOG_ERROR, "Don't open codec: %s \n", av_err2str(ret));
        goto err;
    }

    // 创建输出文件
    FILE *f = fopen(dst, "wb");
    if (!f) {
        av_log(NULL, AV_LOG_ERROR, "Don't open file: %s\n", dst);
        goto err;
    }

    // 创建AVFrame
    AVFrame *frame = av_frame_alloc();
    if (!frame) {
        av_log(NULL, AV_LOG_ERROR, "NO MEMORY\n");
        goto err;
    }

    // 创建AVPacket
    AVPacket *pkt = av_packet_alloc();
    if (!pkt) {
        av_log(NULL, AV_LOG_ERROR, "NO MEMORY\n");
        goto err;
    }

    frame->width = ctx->width;
    frame->height = ctx->height;
    frame->format = ctx->pix_fmt;

    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate the video frame\n");
        goto err;
    }

    // 生成视频内容
    for (int i = 0; i < 25; i++) {
        ret = av_frame_make_writable(frame);
        if (ret < 0) {
            break;
        }

        // Y分量
        for (int y = 0; y < ctx->height; ++y) {
            for (int x = 0; x < ctx->width; ++x) {
                frame->data[0][y * frame->linesize[0] + x] = x + y + i * 3;
            }
        }

        // UV分量
        for (int y = 0; y < ctx->height / 2; ++y) {
            for (int x = 0; x < ctx->width / 2; ++x) {
                frame->data[1][y * frame->linesize[1] + x] = 128 + y + i * 2;
                frame->data[2][y * frame->linesize[2] + x] = 64 + x + i * 5;
            }
        }
        frame->pts = i;

        // 编码
        ret = encode(ctx, frame, pkt, f);
        if (ret == -1) {
            goto err;
        }
    }

    encode(ctx, NULL, pkt, f);

err:
    if (ctx) {
        avcodec_free_context(&ctx);
    }
    if (frame) {
        av_frame_free(&frame);
    }
    if (pkt) {
        av_packet_free(&pkt);
    }
    if (f) {
        fclose(f);
    }
    return 0;
}
