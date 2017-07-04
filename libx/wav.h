//*************************** 文 * 件 * 描 * 述 ***************************/
// 文件名称	:	wav
// 功能介绍	:	读取和保存音频文件
// 作者		:	许云飞
// 联系方式	:	kindapple@163.com
// 注意		：	目前支持的格式有 8k 16bit pcm, 8k 8bit alaw/ulaw; 单双/通道
//************************************************************************/
// 修改日期				修改者			修改内容
// 2013.4.27			许云飞			创建文件
// 2013.6.19			许云飞			增加掐头去尾、最小/最大长度检查
//************************************************************************/
#pragma once
#include <limits.h>


// 音频编码格式
enum WavType
{
	pcm, alaw, ulaw
};


//***********************************************************************/
// 功能		：	读取音频文件到缓冲区，格式为 8k 8bit alaw 无头
//				如果为单通道，则存在左通道上。缓冲区大小由该函数分配。
// 返回值	：	成功返回 true
//				打开文件失返回 false
//				nchl为“1”和“2”之外的值返回 false
//				内存不够返回 false
//				headcut, tailcut, minlen, maxlen 为负数返回 false
//				minlen > maxlen 返回 false
//				有效长度小于 minlen 则返回 false
// 注意		：	失败则 wavbuf 置为 NULL，wavlen 置为 -1
/************************************************************************/
bool CreateWavALaw(
	const char *const wavfile,			// 输入，音频文件路径
	const int nchl,						// 输入，通道数为“1”或“2”
	short *&wavbuf1,					// 输出，左通道音频数据，nchl为“1”时只有该通道有效
	int &wavlen1,						// 输出，左通道音频长度，单位“点数”，nchl为“1”时只有该通道有效
	short *&wavbuf2,					// 输出，右通道音频数据，nchl为“1”时该通道无效
	int &wavlen2,						// 输出，右通道音频长度，单位“点数”，nchl为“1”时该通道无效
	const int headcut = 0,				// 输入，跳过开头一部分长度，单位“秒”
	const int tailcut = 0,				// 输入，跳过末端一部分长度，单位“秒”
	const int minlen = 0,				// 输入，最短有效长度，单位“秒”，小于该长度时，就不读取了
	const int maxlen = INT_MAX			// 输入，最长有效长度，单位“秒”，大于该长度时，从后往前读取
	);


//***********************************************************************/
// 功能		：	读取音频文件到缓冲区，格式为 8k 8bit ulaw 无头
//				如果为单通道，则存在左通道上。缓冲区大小由该函数分配。
// 返回值	：	成功返回 true
//				打开文件失返回 false
//				nchl为“1”和“2”之外的值返回false
//				内存不够返回 false
//				headcut, tailcut, minlen, maxlen 为负数返回 false
//				minlen > maxlen 返回 false
//				有效长度小于 minlen 则返回 false
// 注意		：	失败则 wavbuf 置为 NULL，wavlen 置为 -1
//***********************************************************************/
bool CreateWavULaw(
	const char *const wavfile,			// 输入，音频文件路径
	const int nchl,						// 输入，通道数为“1”或“2”
	short *&wavbuf1,					// 输出，左通道音频数据，nchl为“1”时只有该通道有效
	int &wavlen1,						// 输出，左通道音频长度，单位“点数”，nchl为“1”时只有该通道有效
	short *&wavbuf2,					// 输出，右通道音频数据，nchl为“1”时该通道无效
	int &wavlen2,						// 输出，右通道音频长度，单位“点数”，nchl为“1”时该通道无效
	const int headcut = 0,				// 输入，跳过开头一部分长度，单位“秒”
	const int tailcut = 0,				// 输入，跳过末端一部分长度，单位“秒”
	const int minlen = 0,				// 输入，最短有效长度，单位“秒”，小于该长度时，就不读取了
	const int maxlen = INT_MAX			// 输入，最长有效长度，单位“秒”，大于该长度时，从后往前读取
	);


//***********************************************************************/
// 功能		：	读取音频文件到缓冲区，格式为 8k 16bit pcm 无头
//				如果为单通道，则存在左通道上。缓冲区大小由该函数分配。
// 返回值	：	成功返回 true
//				打开文件失败返回 false
//				nchl为“1”和“2”之外的值返回false
//				内存不够返回 false
//				headcut, tailcut, minlen, maxlen 为负数返回 false
//				minlen > maxlen 返回 false
//				有效长度小于 minlen 则返回 false
// 注意		：	失败则 wavbuf 置为 NULL，wavlen 置为 -1
/************************************************************************/
bool CreateWavPCM(
	const char *const wavfile,			// 输入，音频文件路径
	const int nchl,						// 输入，通道数为“1”或“2”
	short *&wavbuf1,					// 输出，左通道音频数据，nchl为“1”时只有该通道有效
	int &wavlen1,						// 输出，左通道音频长度，单位“点数”，nchl为“1”时只有该通道有效
	short *&wavbuf2,					// 输出，右通道音频数据，nchl为“1”时该通道无效
	int &wavlen2,						// 输出，右通道音频长度，单位“点数”，nchl为“1”时该通道无效
	const int headcut = 0,				// 输入，跳过开头一部分长度，单位“秒”
	const int tailcut = 0,				// 输入，跳过末端一部分长度，单位“秒”
	const int minlen = 0,				// 输入，最短有效长度，单位“秒”，小于该长度时，就不读取了
	const int maxlen = INT_MAX			// 输入，最长有效长度，单位“秒”，大于该长度时，从后往前读取
	);


//***********************************************************************/
// 功能		：	读取音频文件到缓冲区，根据文件头自动识别编码格式
//				如果为单通道，则存在左通道上。缓冲区大小由该函数分配。
// 返回值	：	成功返回 true
//				打开文件失败返回 false
//				nchl为“1”和“2”之外的值返回false
//				内存不够返回 false
//				headcut, tailcut, minlen, maxlen 为负数返回 false
//				minlen > maxlen 返回 false
//				有效长度小于 minlen 则返回 false
//				文件格式不对返回 false
// 注意		：	失败则 wavbuf 置为 NULL，wavlen 和 nchl 置为 -1
//***********************************************************************/
bool CreateWav(
	const char *const wavfile,			// 输入，音频文件路径
	int &nchl,							// 输出，通道数为“1”或“2”
	short *&wavbuf1,					// 输出，左通道音频数据，nchl为“1”时只有该通道有效
	int &wavlen1,						// 输出，左通道音频长度，单位“点数”，nchl为“1”时只有该通道有效
	short *&wavbuf2,					// 输出，右通道音频数据，nchl为“1”时该通道无效
	int &wavlen2,						// 输出，右通道音频长度，单位“点数”，nchl为“1”时该通道无效
	const int headcut = 0,				// 输入，跳过开头一部分长度，单位“秒”
	const int tailcut = 0,				// 输入，跳过末端一部分长度，单位“秒”
	const int minlen = 0,				// 输入，最短有效长度，单位“秒”，小于该长度时，就不读取了
	const int maxlen = INT_MAX			// 输入，最长有效长度，单位“秒”，大于该长度时，从后往前读取
	);


//***********************************************************************/
// 功能		：	释放音频数据缓冲区
// 返回值	：	无
// 注意		：	调用上面的函数会分配内存，需要调用该函数释放
//***********************************************************************/
void DestroyWav(
	short *&wavbuf,				// 输入输出，置为 NULL
	int &wavlen					// 输入输出，置为 -1
	);


//***********************************************************************/
// 功能		：	保存音频至文件
// 返回值	：	成功返回 true
//				无效语音返回 false
//				打开文件失败返回 false
//				写文件失败返回 false
// 注意		：	1. 只支持单通道
//				2. 失败的话删除残留文件
//***********************************************************************/
bool SaveWav(
	const short *const wavbuf,			// 输入，音频数据
	const int wavlen,					// 输入，音频点数
	const char *const wavfile,			// 输入，保存到的文件名
	const WavType type = pcm,			// 输入，编码格式
	const bool head = true				// 输入，是否写文件头
	);