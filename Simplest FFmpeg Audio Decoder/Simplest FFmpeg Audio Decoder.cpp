// Simplest FFmpeg Audio Decoder.cpp : �������̨Ӧ�ó������ڵ㡣

/**
* ��򵥵Ļ��� FFmpeg ����Ƶ������
* Simplest FFmpeg Audio Decoder
*
* ���ĳ� Liu Wenchen
* 812288728@qq.com
* ���ӿƼ���ѧ/������Ϣ
* University of Electronic Science and Technology of China / Electronic and Information Science
* https://blog.csdn.net/ProgramNovice
*
* ��������Խ���Ƶ������MP3��AAC�ȣ�����Ϊ PCM �������ݡ�
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

// 1 second of 48kHz 32bit audio����λ���ֽ�
#define MAX_AUDIO_FRAME_SIZE 192000 

int main(int argc, char* argv[])
{
	// �ṹ�弰��������
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

	// ����ļ�·��
	FILE *pFile = fopen("output.pcm", "wb");
	// �����ļ�·��
	char url[] = "skycity.mp3";

	// ע��֧�ֵ����е��ļ���ʽ�������������Ӧ�� CODEC��ֻ��Ҫ����һ��
	av_register_all();
	// ����������ȫ�ֳ�ʼ�������� socket ���Լ��������Э����صĿ⣬Ϊ����ʹ����������ṩ֧�֣�
	// ע�⣺�˺��������ڽ���� GnuTLS �� OpenSSL ����̰߳�ȫ���⡣
	// ��� libavformat ���ӵ���Щ��Ľ��°汾�����߲�ʹ�����ǣ���������ô˺�����
	// ������Ҫ��ʹ�����ǵ��κ������߳�����֮ǰ���ô˺�����
	avformat_network_init();
	// ʹ��Ĭ�ϲ������䲢��ʼ��һ�� AVFormatContext ����
	pFormatCtx = avformat_alloc_context();
	// ������ý���������������������ġ��⸴�������ġ�I/O ������
	if (avformat_open_input(&pFormatCtx, url, NULL, NULL) != 0)
	{
		printf("Can't open input stream.\n");
		return -1;
	}
	// ��ȡý���ļ������ݰ��Ի�ȡý������Ϣ
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
	{
		printf("Can't find stream information.\n");
		return -1;
	}

	printf("---------------- File Information ---------------\n");
	// �� AVFormatContext �ṹ����ý���ļ�����Ϣ���и�ʽ�����
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
	// pCodecCtx ��ָ����Ƶ���ı�����������ĵ�ָ��
	pCodecCtx = pFormatCtx->streams[audioStream]->codec;
	// ���� ID Ϊ pCodecCtx->codec_id ����ע�����Ƶ��������
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL)
	{
		printf("Codec not found.\n");
		return -1;
	}
	// ��ʼ��ָ���ı������
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
	{
		printf("Can't open codec.\n");
		return -1;
	}

	// Ϊ packet ����ռ�
	packet = (AVPacket *)av_malloc(sizeof(AVPacket));
	// �� packet �еĿ�ѡ�ֶγ�ʼ��ΪĬ��ֵ
	av_init_packet(packet);

	// �����Ƶ����
	// ͨ�����ͣ�˫����
	uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO;
	int out_nb_samples = pCodecCtx->frame_size;
	// ������ʽ��pcm_s16le������ 16bit��
	AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
	// �����ʣ�44100
	int out_sample_rate = 44100;
	int out_channels = av_get_channel_layout_nb_channels(out_channel_layout);
	int out_buffer_size = av_samples_get_buffer_size(NULL, out_channels, out_nb_samples, out_sample_fmt, 1);

	out_buffer = (uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE * 2);
	pFrame = av_frame_alloc();

	// ����������Ŀ��ȡ��������
	in_channel_layout = av_get_default_channel_layout(pCodecCtx->channels);
	// �����ز����ṹ�� SwrContext ����
	au_convert_ctx = swr_alloc();
	// �����ز�����ת������
	au_convert_ctx = swr_alloc_set_opts(au_convert_ctx, out_channel_layout, out_sample_fmt, out_sample_rate,
		in_channel_layout, pCodecCtx->sample_fmt, pCodecCtx->sample_rate, 0, NULL);
	// ��ʼ�� SwrContext ����
	swr_init(au_convert_ctx);

	// ��ȡ�����е���Ƶ����֡������Ƶһ֡
	while (av_read_frame(pFormatCtx, packet) >= 0)
	{
		if (packet->stream_index == audioStream)
		{
			// ���� packet �е���Ƶ���ݣ�pFrame �洢��������
			ret = avcodec_decode_audio4(pCodecCtx, pFrame, &got_picture, packet);
			if (ret < 0)
			{
				printf("Error in decoding audio frame.\n");
				return -1;
			}
			if (got_picture > 0)
			{
				// ���и�ʽת��������ֵΪʵ��ת���Ĳ�����
				swr_convert(au_convert_ctx, &out_buffer, MAX_AUDIO_FRAME_SIZE, (const uint8_t **)pFrame->data, pFrame->nb_samples);
				// ��ӡÿһ֡����Ϣ
				printf("index: %5d\t pts: %lld\t packet size: %d\n", index, packet->pts, packet->size);
				// ������ļ���д�� PCM ����
				fwrite(out_buffer, 1, out_buffer_size, pFile);
				index++;
			}
		}
		// ��� packet ���������
		av_free_packet(packet);
	}

	// �ͷ� SwrContext ����
	swr_free(&au_convert_ctx);
	// �ر��ļ�ָ��
	fclose(pFile);
	// �ͷ��ڴ�
	av_free(out_buffer);
	// �رս�����
	avcodec_close(pCodecCtx);
	// �ر�������Ƶ�ļ�
	avformat_close_input(&pFormatCtx);

	return 0;
}