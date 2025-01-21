#include "fifobuffer.h"

FiFoBuffer::FiFoBuffer()
{
    fifoBuffer = nullptr;
    fifoBufferLength = 0;
    fifoBegin = 0;
    fifoEnd = 0;
}

FiFoBuffer::~FiFoBuffer()
{
    freeFiFoBuffer();
}

bool FiFoBuffer::initFiFoBuffer(int bufferLength)
{
    if(bufferLength < 0)
        return false;

    std::lock_guard<std::mutex> lock(fifoMutex);
    if (fifoBuffer != nullptr)
        return false;

    fifoBuffer = new char[bufferLength];
    if(fifoBuffer == nullptr)
        return false;

    fifoBufferLength = bufferLength;
    memset(fifoBuffer, 0, fifoBufferLength);

    fifoBegin = 0;
    fifoEnd = 0;
    dataNodeList.clear();
    return true;
}

void FiFoBuffer::freeFiFoBuffer()
{
    std::lock_guard<std::mutex> lock(fifoMutex);
    fifoBegin = 0;
    fifoEnd = 0;
    fifoBufferLength = 0;
    dataNodeList.clear();

    if (fifoBuffer != nullptr)
    {
        delete[] fifoBuffer;
        fifoBuffer = NULL;
    }
}

void FiFoBuffer::resetFiFoBuffer()
{
    std::lock_guard<std::mutex> lock(fifoMutex);
    fifoBegin = 0;
    fifoEnd = 0;
    dataNodeList.clear();
    memset(fifoBuffer, 0, fifoBufferLength);
}

int FiFoBuffer::getDataNodeSize()
{
    std::lock_guard<std::mutex> lock(fifoMutex);
    return dataNodeList.size();
}

int FiFoBuffer::getFiFoBufferLength()
{
    std::lock_guard<std::mutex> lock(fifoMutex);
    return fifoBufferLength;
}

int FiFoBuffer::getRemainSapce()
{
    std::lock_guard<std::mutex> lock(fifoMutex);
    if (fifoBufferLength <= 0)
        return fifoBufferLength;

    if (dataNodeList.size() <= 0)
        return fifoBufferLength;

    int listLengthCount = 0;
    std::list<DataNode>::iterator it;
    for (it = dataNodeList.begin(); it != dataNodeList.end(); ++it)
    {
        DataNode node = *it;
        listLengthCount += node.end - node.begin;
    }

    return  fifoBufferLength - listLengthCount;
}

bool FiFoBuffer::pushData(char *data, int length)
{
    std::lock_guard<std::mutex> lock(fifoMutex);

    //条件判断
    if (fifoBufferLength <= 0 || fifoBuffer == NULL || data == NULL || length <= 0 || length > fifoBufferLength)
        return false;

    //检测剩余的空间是否够存储 不够则重头开始存储
    if (fifoBufferLength - fifoEnd < length)
        fifoEnd = 0;

    if (dataNodeList.size() > 0)
    {
        int nStart = 0;
        DataNode nodeFront = dataNodeList.front();
        nStart = nodeFront.begin;

        if (fifoEnd == 0)
        {
            //是否可以重头开始
            if (nStart < length)
                return false;
        }
        else
        {
            if (nStart >= fifoEnd)
            {
                //剩余空间不够
                if ((nStart - fifoEnd) < length)
                    return false;
            }
            else
            {
                //剩余空间不够
                if (fifoBufferLength - fifoEnd < length)
                    return false;
            }
        }
    }

    //记录新的数据块
    DataNode node;
    node.begin = fifoEnd;
    node.end = node.begin + length;
    dataNodeList.push_back(node);

    memcpy(fifoBuffer + fifoEnd, data, length);
    fifoEnd += length;
    return true;
}

char *FiFoBuffer::popData(int *length)
{
    std::lock_guard<std::mutex> lock(fifoMutex);

    char *buffer = nullptr;
    if (dataNodeList.size() <= 0)
    {
        *length = 0;
        return buffer;
    }

    DataNode node;
    node = dataNodeList.front();

    //为了稳定性再次判断返回的节点是否有效
    int offset = node.end - node.begin;
    if (node.begin >= 0 &&
        node.end > 0 &&
        node.begin < fifoBufferLength &&
        node.end <= fifoBufferLength &&
        offset > 0 &&
        offset <= fifoBufferLength)
    {
        buffer = fifoBuffer + node.begin;
        *length = offset;
        return buffer;
    }
    else
    {
        buffer = NULL;
        *length = 0;
        return buffer;
    }
}

bool FiFoBuffer::popDelete()
{
    std::lock_guard<std::mutex> lock(fifoMutex);

    if (dataNodeList.size() <= 0)
        return false;

    //删除数据块记录即可，表示该数据块区域可被再次使用
    dataNodeList.pop_front();
    if (dataNodeList.size() <= 0)
    {
        fifoBegin = 0;
        fifoEnd = 0;
    }

    return true;
}


RingBuffer::RingBuffer()
{
    buffer = nullptr;
    bufferSize = 0;
    write = 0;
    read = 0;
}

RingBuffer::~RingBuffer()
{
    freeBuffer();
}

bool RingBuffer::initBuffer(uint32_t size)
{
    //需要保证为2的次幂 取余运算转换为与运算 提升效率，即write%bufferSize == write &(bufferSize-1)
    if (!is_power_of_two(size))
    {
        if (size < 2)
            size = 2;

        //向上取2的次幂
        int i = 0;
        for (; size != 0; i++)
            size >>= 1;

        size = 1U << i;
    }

    std::lock_guard<std::mutex> lock(mutex);
    buffer = new uint8_t[size];
    if(buffer == nullptr)
        return false;

    memset(buffer, 0, size);
    bufferSize = size;
    write = 0;
    read = 0;
    return true;
}

void RingBuffer::freeBuffer()
{
    std::lock_guard<std::mutex> lock(mutex);
    bufferSize = 0;
    write = 0;
    read = 0;

    if (buffer != nullptr)
    {
        delete[] buffer;
        buffer = nullptr;
    }
}

void RingBuffer::resetBuffer()
{
    std::lock_guard<std::mutex> lock(mutex);
    write = 0;
    read = 0;
    memset(buffer, 0, bufferSize);
}

bool RingBuffer::isEmpty()
{
    std::lock_guard<std::mutex> lock(mutex);
    return write == read;
}

bool RingBuffer::isFull()
{
    std::lock_guard<std::mutex> lock(mutex);
    return bufferSize == (write - read);
}

uint32_t RingBuffer::getReadableLen()
{
    std::lock_guard<std::mutex> lock(mutex);
    return write - read;
}

uint32_t RingBuffer::getRemainLen()
{
    std::lock_guard<std::mutex> lock(mutex);
    return bufferSize - (write - read);
}

uint32_t RingBuffer::getBufferSize()
{
    std::lock_guard<std::mutex> lock(mutex);
    return bufferSize;
}

uint32_t RingBuffer::writeBuffer(char *inBuf, uint32_t inSize)
{
    std::lock_guard<std::mutex> lock(mutex);

    if(buffer == nullptr || inBuf == nullptr || inSize == 0)
        return -1;

    //写入数据大小和缓冲区剩余空间大小 取最小值为最终写入大小
    inSize = Min(inSize, bufferSize - (write - read));

    //写数据如果写到末尾仍未写完的情况，那么回到头部继续写
    uint32_t len = Min(inSize, bufferSize - (write & (bufferSize - 1)));
    //区间为写指针位置到缓冲区末端
    memcpy(buffer + (write & (bufferSize - 1)), inBuf, len);
    //回到缓冲区头部继续写剩余数据
    memcpy(buffer, inBuf + len, inSize - len);

    //无符号溢出则为 0
    write += inSize;
    return inSize;
}

uint32_t RingBuffer::readBuffer(char *outBuf, uint32_t outSize)
{
    std::lock_guard<std::mutex> lock(mutex);

    if(buffer == nullptr || outBuf == nullptr || outSize == 0)
        return -1;

    //读出数据大小和缓冲区可读数据大小 取最小值为最终读出大小
    outSize = Min(outSize, write - read);

    //读数据如果读到末尾仍未读完的情况, 那么回到头部继续读
    uint32_t len = Min(outSize, bufferSize - (read & (bufferSize - 1)));
    //区间为读指针位置到缓冲区末端
    memcpy(outBuf, buffer + (read & (bufferSize - 1)), len);
    //回到缓冲区头部继续读剩余数据
    memcpy(outBuf + len, buffer, outSize - len);

    //无符号溢出则为 0
    read += outSize;
    return outSize;
}

