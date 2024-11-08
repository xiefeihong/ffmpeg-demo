#include <libavutil/log.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <unistd.h>

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

// output.mp4 mpeg4 %03d.png||%03d.bmp 352 288
int main(int argc, char* argv[]) {

    char *dst, *codecname, *src;
    int width, height, ret, i;
    struct SwsContext *sws_ctx;
    AVPacket *pkt;
    AVFrame *src_frame, *dst_frame;
    FILE *f;
    AVCodecContext *ctx;
    const AVCodec* codec;
    AVFormatContext *fmt_ctx = NULL;

    av_log_set_level(AV_LOG_DEBUG);

    // 输入参数
    if (argc < 6) {
        av_log(NULL, AV_LOG_ERROR, "arguments must be more than 5\n");
        goto err;
    }

    dst = argv[1];
    codecname = argv[2];
    src = argv[3];
    width = atoi(argv[4]);
    height = atoi(argv[5]);

    // 查找编码器
    codec = avcodec_find_encoder_by_name(codecname);
    if (!codec) {
        av_log(NULL, AV_LOG_ERROR, "don't find Codec: %s\n", codecname);
        goto err;
    }
    // 创建编码器上下文
    ctx = avcodec_alloc_context3(codec);
    if (!ctx) {
        av_log(NULL, AV_LOG_ERROR, "NO MEMRORY\n");
        goto err;
    }
    // 设置编码器参数
    ctx->width = width;
    ctx->height = height;
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
    ret = avcodec_open2(ctx, codec, NULL);
    if (ret < 0) {
        av_log(ctx, AV_LOG_ERROR, "Don't open codec: %s \n", av_err2str(ret));
        goto err;
    }

    // 创建输出文件
    f = fopen(dst, "wb");
    if (!f) {
        av_log(NULL, AV_LOG_ERROR, "Don't open file: %s\n", dst);
        goto err;
    }

    // 创建AVFrame
    src_frame = av_frame_alloc();
    dst_frame = av_frame_alloc();
    if (!src_frame || !dst_frame) {
        av_log(NULL, AV_LOG_ERROR, "NO MEMRORY\n");
        goto err;
    }

    dst_frame->width = ctx->width;
    dst_frame->height = ctx->height;
    dst_frame->format = ctx->pix_fmt;
    ret = av_frame_get_buffer(dst_frame, 0);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate the video frame\n");
        goto err;
    }

    // 创建AVPacket
    pkt = av_packet_alloc();
    if (!pkt) {
        av_log(NULL, AV_LOG_ERROR, "NO MEMRORY\n");
        goto err;
    }

    // 用于格式转换
    sws_ctx = sws_getContext(ctx->width, ctx->height, AV_PIX_FMT_RGB24,
                             ctx->width, ctx->height, ctx->pix_fmt,
                             SWS_BICUBIC, NULL, NULL, NULL);
    if (!sws_ctx) {
        av_log(NULL, AV_LOG_ERROR, "Could not initialize the conversion context\n");
        goto err;
    }

    // 从%03d.png图片获取视频内容
    char img_filename[20];
    i = 0;
    while (1) {
        sprintf(img_filename, src, i + 1);

        // 文件不存在
        if (access(img_filename, F_OK) != 0) {
            break;
        }

        // 打开图片
        ret = avformat_open_input(&fmt_ctx, img_filename, NULL, NULL);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not open image file: %s\n", av_err2str(ret));
            goto err;
        }

        // 查找图片流信息
        ret = avformat_find_stream_info(fmt_ctx, NULL);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not find stream information in image file: %s\n", av_err2str(ret));
            goto err;
        }

        // 查找图片解码器
        const AVCodec *img_codec = avcodec_find_decoder(fmt_ctx->streams[0]->codecpar->codec_id);
        if (!img_codec) {
            av_log(NULL, AV_LOG_ERROR, "Could not find image codec\n");
            goto err;
        }

        // 创建图片解码器上下文
        AVCodecContext *img_ctx = avcodec_alloc_context3(img_codec);
        if (!img_ctx) {
            av_log(NULL, AV_LOG_ERROR, "Could not allocate image codec context\n");
            goto err;
        }

        // 复制参数到解码器上下文
        ret = avcodec_parameters_to_context(img_ctx, fmt_ctx->streams[0]->codecpar);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not copy codec parameters to image codec context: %s\n", av_err2str(ret));
            goto err;
        }

        // 打开图片解码器
        ret = avcodec_open2(img_ctx, img_codec, NULL);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not open image codec: %s\n", av_err2str(ret));
            goto err;
        }

        // 读取图片帧
        ret = av_read_frame(fmt_ctx, pkt);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not read image frame: %s\n", av_err2str(ret));
            goto err;
        }

        // 解码图片帧
        ret = avcodec_send_packet(img_ctx, pkt);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not send image packet to decoder: %s\n", av_err2str(ret));
            goto err;
        }
        ret = avcodec_receive_frame(img_ctx, src_frame);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not receive image frame from decoder: %s\n", av_err2str(ret));
            goto err;
        }

        // 格式转换
        sws_scale(sws_ctx, (const uint8_t * const *)src_frame->data, src_frame->linesize, 0, ctx->height,
                  dst_frame->data, dst_frame->linesize);

        // 设置pts
        dst_frame->pts = i;

        // 编码
        ret = encode(ctx, dst_frame, pkt, f);
        if (ret == -1) {
            goto err;
        }

        // 释放资源
        avcodec_free_context(&img_ctx);
        avformat_close_input(&fmt_ctx);
        i++;
    }

    encode(ctx, NULL, pkt, f);

err:
    if (sws_ctx) {
        sws_freeContext(sws_ctx);
    }
    if (ctx) {
        avcodec_free_context(&ctx);
    }
    if (src_frame) {
        av_frame_free(&src_frame);
    }
    if (dst_frame) {
        av_frame_free(&dst_frame);
    }
    if (pkt) {
        av_packet_free(&pkt);
    }
    if (f) {
        fclose(f);
    }
    return 0;
}