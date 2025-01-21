#ifndef RTP_H
#define RTP_H
#include <stdint.h>

#define RTP_VESION              2
#define RTP_PAYLOAD_TYPE_H264   96
#define RTP_PAYLOAD_TYPE_AAC    97
#define RTP_HEADER_SIZE         12
#define RTP_MAX_PKT_SIZE        1400

 /*
  *    0                   1                   2                   3
  *    7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0
  *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  *   |V=2|P|X|  CC   |M|     PT      |       sequence number         |
  *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  *   |                           timestamp                           |
  *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  *   |           synchronization source (SSRC) identifier            |
  *   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
  *   |            contributing source (CSRC) identifiers             |
  *   :                             ....                              :
  *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  *
  */
struct RtpHeader
{
    /* byte 0 */
    uint8_t csrcLen : 4;//CSRC��������ռ4λ��ָʾCSRC ��ʶ���ĸ�����
    uint8_t extension : 1;//ռ1λ�����X=1������RTP��ͷ�����һ����չ��ͷ��
    uint8_t padding : 1;//����־��ռ1λ�����P=1�����ڸñ��ĵ�β�����һ����������İ�λ�飬���ǲ�����Ч�غɵ�һ���֡�
    uint8_t version : 2;//RTPЭ��İ汾�ţ�ռ2λ����ǰЭ��汾��Ϊ2��

    /* byte 1 */
    uint8_t payloadType : 7;//��Ч�غ����ͣ�ռ7λ������˵��RTP��������Ч�غɵ����ͣ���GSM��Ƶ��JPEMͼ��ȡ�
    uint8_t marker : 1;//��ǣ�ռ1λ����ͬ����Ч�غ��в�ͬ�ĺ��壬������Ƶ�����һ֡�Ľ�����������Ƶ����ǻỰ�Ŀ�ʼ��

    /* bytes 2,3 */
    uint16_t seq;//ռ16λ�����ڱ�ʶ�����������͵�RTP���ĵ����кţ�ÿ����һ�����ģ����к���1��������ͨ�����к�����ⱨ�Ķ�ʧ��������������ģ��ָ����ݡ�

    /* bytes 4-7 */
    uint32_t timestamp;//ռ32λ��ʱ����ӳ�˸�RTP���ĵĵ�һ����λ��Ĳ���ʱ�̡�������ʹ��ʱ���������ӳٺ��ӳٶ�����������ͬ�����ơ�

    /* bytes 8-11 */
    uint32_t ssrc;//ռ32λ�����ڱ�ʶͬ����Դ���ñ�ʶ�������ѡ��ģ��μ�ͬһ��Ƶ���������ͬ����Դ��������ͬ��SSRC��

   /*��׼��RTP Header �����ܴ��� 0-15����Լ��Դ(CSRC)��ʶ��
   
   ÿ��CSRC��ʶ��ռ32λ��������0��15����ÿ��CSRC��ʶ�˰����ڸ�RTP������Ч�غ��е�������Լ��Դ

   */
};

struct RtpPacket
{
    struct RtpHeader rtpHeader;
    uint8_t payload[0];
};

void rtpHeaderInit(struct RtpPacket* rtpPacket, uint8_t csrcLen, uint8_t extension,
                   uint8_t padding, uint8_t version, uint8_t payloadType, uint8_t marker,
                   uint16_t seq, uint32_t timestamp, uint32_t ssrc);

int rtpSendPacketOverUdp(int serverRtpSockfd, const char* ip, int16_t port, struct RtpPacket* rtpPacket, uint32_t dataSize);

struct AdtsHeader
{
    unsigned int syncword;  //12 bit ͬ���� '1111 1111 1111'��һ��ADTS֡�Ŀ�ʼ
    uint8_t id;        //1 bit 0����MPEG-4, 1����MPEG-2��
    uint8_t layer;     //2 bit ����Ϊ0
    uint8_t protectionAbsent;  //1 bit 1����û��CRC��0������CRC
    uint8_t profile;           //1 bit AAC����MPEG-2 AAC�ж�����3��profile��MPEG-4 AAC�ж�����6��profile��
    uint8_t samplingFreqIndex; //4 bit ������
    uint8_t privateBit;        //1bit ����ʱ����Ϊ0������ʱ����
    uint8_t channelCfg;        //3 bit ��������
    uint8_t originalCopy;      //1bit ����ʱ����Ϊ0������ʱ����
    uint8_t home;               //1 bit ����ʱ����Ϊ0������ʱ����

    uint8_t copyrightIdentificationBit;   //1 bit ����ʱ����Ϊ0������ʱ����
    uint8_t copyrightIdentificationStart; //1 bit ����ʱ����Ϊ0������ʱ����
    unsigned int aacFrameLength;               //13 bit һ��ADTS֡�ĳ��Ȱ���ADTSͷ��AACԭʼ��
    unsigned int adtsBufferFullness;           //11 bit �����������ȣ�0x7FF˵�������ʿɱ������������Ҫ���ֶΡ�CBR������Ҫ���ֶΣ���ͬ������ʹ�������ͬ�������ʹ����Ƶ�����ʱ����Ҫע�⡣

    /* number_of_raw_data_blocks_in_frame
     * ��ʾADTS֡����number_of_raw_data_blocks_in_frame + 1��AACԭʼ֡
     * ����˵number_of_raw_data_blocks_in_frame == 0
     * ��ʾ˵ADTS֡����һ��AAC���ݿ鲢����˵û�С�(һ��AACԭʼ֡����һ��ʱ����1024���������������)
     */
    uint8_t numberOfRawDataBlockInFrame; //2 bit
};

const int sampling_frequencies[] = {
    96000,  // 0x0
    88200,  // 0x1
    64000,  // 0x2
    48000,  // 0x3
    44100,  // 0x4
    32000,  // 0x5
    24000,  // 0x6
    22050,  // 0x7
    16000,  // 0x8
    12000,  // 0x9
    11025,  // 0xa
    8000   // 0xb
    // 0xc d e f�Ǳ�����
};

#define ADTS_HEADER_LEN  7;

int adts_header(char *const p_adts_header, const int data_length,
                const int profile, const int samplerate,
                const int channels);

int parseAdtsHeader(uint8_t* in, struct AdtsHeader* res);

#endif
