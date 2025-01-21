#ifndef FIFOBUFFER_H
#define FIFOBUFFER_H

#include <list>
#include <mutex>
#include <string.h>

//数据节点
struct DataNode
{
    DataNode()
    {
        begin = 0;
        end = 0;
    }

    //记录当前数据块的始、末位置
    int begin;
    int end;
};

//缓冲区类 实现队列存储数据块 只拷贝一次，提供给外部使用
class FiFoBuffer
{
public:
    FiFoBuffer();
    ~FiFoBuffer();

    //初始化缓冲区
    bool initFiFoBuffer(int bufferLength);
    //释放缓冲区
    void freeFiFoBuffer();
    //重置缓冲区（不需要重新申请内存）
    void resetFiFoBuffer();

    //获取当前的数据块节点数量
    int getDataNodeSize();
    //获取缓冲区总长度
    int getFiFoBufferLength();
    //获取缓冲区剩余空间大小
    int getRemainSapce();

    //数据写入缓冲区尾部 注：缓存区满则写入失败
    bool pushData(char *data, int length);
    //返回缓冲区头部数据及长度供外部使用 需要popDelete来释放此数据块空间
    char *popData(int *length);
    //调用此函数删除头部数据块
    bool popDelete();

private:
    //线程安全锁
    std::mutex fifoMutex;

    //缓冲区相关变量
    char *fifoBuffer;
    int fifoBufferLength;
    int fifoBegin;
    int fifoEnd;

    //数据块节点
    std::list<DataNode> dataNodeList;
};


//判断x是否是2的次方
#define is_power_of_two(x) ((x) != 0 && (((x) & ((x) - 1)) == 0))

//取a和b中最小值
#define Min(a, b) (((a) < (b)) ? (a) : (b))

//环形缓存
class RingBuffer
{
public:
    RingBuffer();
    ~RingBuffer();

    //根据传入size 初始化缓冲区
    bool initBuffer(uint32_t bufferSize);
    //释放缓冲区
    void freeBuffer();
    //重置缓冲区（不需要重新申请内存）
    void resetBuffer();

    //缓存区是否为空
    bool isEmpty();
    //缓存区是否已满
    bool isFull();
    //返回可读数据长度
    uint32_t getReadableLen();
    //返回剩余数据长度
    uint32_t getRemainLen();
    //返回缓冲区总长度
    uint32_t getBufferSize();

    //缓存区写入数据, 返回实际写入大小
    uint32_t writeBuffer(char *inBuf, uint32_t inSize);
    //缓存区读出数据 返回实际读出大小
    uint32_t readBuffer(char *outBuf, uint32_t outSize);

private:
    uint8_t *buffer;        //缓冲区
    uint32_t bufferSize;    //缓冲区总大小
    uint32_t write;         //写入位置
    uint32_t read;          //读出位置
    std::mutex mutex;       //互斥锁
};

#endif // FIFOBUFFER_H

