// Simplest FFmpeg Audio Decoder.cpp : 定义控制台应用程序的入口点。

/**
* 最简单的基于 FFmpeg 的音频解码器
* Simplest FFmpeg Audio Decoder
*
* 刘文晨 Liu Wenchen
* 812288728@qq.com
* 电子科技大学/电子信息
* University of Electronic Science and Technology of China / Electronic and Information Science
* https://blog.csdn.net/ProgramNovice
*
* 本程序可以将音频码流（MP3，AAC等）解码为 PCM 采样数据。
*
* This software decode audio streams (MP3, ACC...) to PCM data.
*
*/

#include "stdafx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define __STDC_CONSTANT_MACROS
#ifdef _WIN32
// Windows
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"
};
#else
// Linux
#endif

// 1 second of 48kHz 32bit audio，单位是字节
#define MAX_AUDIO_FRAME_SIZE 192000 

int main(int argc, char* argv[])
{
	// 结构体及变量定义
	AVFormatContext* pFormatCtx;
	int i, audioStream;
	AVCodecContext* pCodecCtx;
	AVCodec* pCodec;
	AVPacket* packet;
	uint8_t* out_buffer;
	AVFrame* pFrame;
	int ret;
	uint32_t len = 0;
	int got_picture;
	int index = 0;
	int64_t in_channel_layout;
	struct SwrContext *au_convert_ctx;

	// 输出文件路径
	FILE *pFile = fopen("output.pcm", "wb");
	// 输入文件路径
	char url[] = "skycity.mp3";

	// 注册支持的所有的文件格式（容器）及其对应的 CODEC，只需要调用一次
	av_register_all();
	// 对网络库进行全局初始化（加载 socket 库以及网络加密协议相关的库，为后续使用网络相关提供支持）
	// 注意：此函数仅用于解决旧 GnuTLS 或 OpenSSL 库的线程安全问题。
	// 如果 libavformat 链接到这些库的较新版本，或者不使用它们，则无需调用此函数。
	// 否则，需要在使用它们的任何其他线程启动之前调用此函数。
	avformat_network_init();
	// 使用默认参数分配并初始化一个 AVFormatContext 对象
	pFormatCtx = avformat_alloc_context();
	// 打开输入媒体流，分配编解码器上下文、解复用上下文、I/O 上下文
	if (avformat_open_input(&pFormatCtx, url, NULL, NULL) != 0)
	{
		printf("Can't open input stream.\n");
		return -1;
	}
	// 读取媒体文件的数据包以获取媒体流信息
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
	{
		printf("Can't find stream information.\n");
		return -1;
	}

	printf("---------------- File Information ---------------\n");
	// 将 AVFormatContext 结构体中媒体文件的信息进行格式化输出
	av_dump_format(pFormatCtx, 0, url, false);
	printf("-------------------------------------------------\n");

	audioStream = -1;
	for (i = 0; i < pFormatCtx->nb_streams; i++)
	{
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			audioStream = i;
			break;
		}
	}
	if (audioStream == -1)
	{
		printf("Can't find a audio stream.\n");
		return -1;
	}
	// pCodecCtx 是指向音频流的编解码器上下文的指针
	pCodecCtx = pFormatCtx->streams[audioStream]->codec;
	// 查找 ID 为 pCodecCtx->codec_id 的已注册的音频流解码器
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL)
	{
		printf("Codec not found.\n");
		return -1;
	}
	// 初始化指定的编解码器
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
	{
		printf("Can't open codec.\n");
		return -1;
	}

	// 为 packet 分配空间
	packet = (AVPacket *)av_malloc(sizeof(AVPacket));
	// 将 packet 中的可选字段初始化为默认值
	av_init_packet(packet);

	// 输出音频参数
	// 通道类型：双声道
	uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO;
	int out_nb_samples = pCodecCtx->frame_size;
	// 采样格式：pcm_s16le（整型 16bit）
	AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
	// 采样率：44100
	int out_sample_rate = 44100;
	int out_channels = av_get_channel_layout_nb_channels(out_channel_layout);
	int out_buffer_size = av_samples_get_buffer_size(NULL, out_channels, out_nb_samples, out_sample_fmt, 1);

	out_buffer = (uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE * 2);
	pFrame = av_frame_alloc();

	// 根据声道数目获取声道布局
	in_channel_layout = av_get_default_channel_layout(pCodecCtx->channels);
	// 创建重采样结构体 SwrContext 对象
	au_convert_ctx = swr_alloc();
	// 设置重采样的转换参数
	au_convert_ctx = swr_alloc_set_opts(au_convert_ctx, out_channel_layout, out_sample_fmt, out_sample_rate,
		in_channel_layout, pCodecCtx->sample_fmt, pCodecCtx->sample_rate, 0, NULL);
	// 初始化 SwrContext 对象
	swr_init(au_convert_ctx);

	// 读取码流中的音频若干帧或者视频一帧
	while (av_read_frame(pFormatCtx, packet) >= 0)
	{
		if (packet->stream_index == audioStream)
		{
			// 解码 packet 中的音频数据，pFrame 存储解码数据
			ret = avcodec_decode_audio4(pCodecCtx, pFrame, &got_picture, packet);
			if (ret < 0)
			{
				printf("Error in decoding audio frame.\n");
				return -1;
			}
			if (got_picture > 0)
			{
				// 进行格式转换，返回值为实际转换的采样数
				swr_convert(au_convert_ctx, &out_buffer, MAX_AUDIO_FRAME_SIZE, (const uint8_t **)pFrame->data, pFrame->nb_samples);
				// 打印每一帧的信息
				printf("index: %5d\t pts: %lld\t packet size: %d\n", index, packet->pts, packet->size);
				// 向输出文件中写入 PCM 数据
				fwrite(out_buffer, 1, out_buffer_size, pFile);
				index++;
			}
		}
		// 清空 packet 里面的数据
		av_free_packet(packet);
	}

	// 释放 SwrContext 对象
	swr_free(&au_convert_ctx);
	// 关闭文件指针
	fclose(pFile);
	// 释放内存
	av_free(out_buffer);
	// 关闭解码器
	avcodec_close(pCodecCtx);
	// 关闭输入音频文件
	avformat_close_input(&pFormatCtx);

	return 0;
}