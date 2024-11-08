extern "C" {
#include <libavutil/log.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <unistd.h>
}

#include <Magick++/Image.h>
#include <map>

#include "gm_tool.h"
#include "json_tool.h"
#include "file_tool.h"

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

// output.mp4 mpeg4 %03d.png||%03d.bmp 352 288 overlay.png test.json
int main(int argc, char* argv[]) {

    char *dst, *codecname, *src, *overlay_image, *position_json_file;
    int width, height, ret, i;
    struct SwsContext *sws_ctx;
    AVPacket *pkt;
    AVFrame *src_frame, *dst_frame;
    FILE *f;
    AVCodecContext *ctx;
    const AVCodec* codec;

    std::string json;
    std::map<int, Position> positions;

    av_log_set_level(AV_LOG_DEBUG);
    // 输入参数
    if (argc < 8) {
        av_log(NULL, AV_LOG_ERROR, "arguments must be more than 6\n");
        goto err;
    }

    dst = argv[1];
    codecname = argv[2];
    src = argv[3];
    width = atoi(argv[4]);
    height = atoi(argv[5]);
    overlay_image = argv[6];
    position_json_file = argv[7];

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
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }

    src_frame->format = AV_PIX_FMT_RGB24;
    src_frame->width  = ctx->width;
    src_frame->height = ctx->height;
    ret = av_frame_get_buffer(src_frame, 0);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate the video frame\n");
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

    Magick::InitializeMagick(nullptr);
    json = readStringsFromFile(position_json_file);
    positions = parsePositions(json.c_str());

    // 从%03d.png图片获取视频内容
    char img_filename[20];
    i = 0;
    while (true) {
        sprintf(img_filename, src, i + 1);

        // 文件不存在
        if (access(img_filename, F_OK) != 0) {
            break;
        }

        ret = av_frame_make_writable(src_frame);
        if (ret < 0){
            av_log(NULL, AV_LOG_ERROR, "error: %s\n", av_err2str(ret));
            goto err;
        }

        Magick::Image background;
        background.read(img_filename); // 替换为您的背景图像文件名

        Magick::Image overlay;
        overlay.read(overlay_image); // 替换为您的要旋转的图像文件名
        overlay.backgroundColor(Magick::Color("#ffffffff"));

        if (positions.count(i) == 1) {
            printf("position use by %d\n", i);
            Position &position = positions[i];
            composite_to_frame_plus(&background, &overlay, position.offsetX, position.offsetY, position.degrees);
        }
        image_to_frame(&background, src_frame);

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