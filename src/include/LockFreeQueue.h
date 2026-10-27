#ifndef LOCK_FREE_QUEUE_H
#define LOCK_FREE_QUEUE_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

template <typename T>
class MPMCQueue{
public:
    explicit MPMCQueue(size_t capacity)
        :capacity_(capacity)
        ,mask_(capacity-1)
        ,buffer_(capacity){
        if(capacity_ == 0 || (capacity_ & mask_)!=0){//容量是 2 的幂时，这个位与的结果是 0。
            throw std::invalid_argument("capacity must be a power of 2");
        }
        for(size_t i = 0; i<capacity_;++i){
            buffer_[i].sequence.store(i,std::memory_order_relaxed);//将每一格初始化，memory_order_relaxed 表示只保证这一个数被写成 i，不额外约束别的内存操作的顺序。
        }
        //入队和出队初始化
        enqueue_pos_.store(0,std::memory_order_relaxed);
        dequeue_pos_.store(0,std::memory_order_relaxed);
    }

    bool push(T const& data){
        Cell* cell;
        size_t pos = enqueue_pos_.load(std::memory_order_relaxed);//读出入队期望值
        for(;;){
            cell = &buffer_[pos&mask_];//获取位置的cell地址
            size_t seq = cell->sequence.load(std::memory_order_acquire);
            intptr_t diff = (intptr_t)seq - (intptr_t)pos;

            if(diff==0){
                if(enqueue_pos_.compare_exchange_weak(
                    pos,pos+1,std::memory_order_relaxed)){
                    break;    
                }
            }else if(diff<0){//这个格子还在上一圈，数据没有被取走，队列已满
                return false;
            }else{//已经过时，需要更新pos
                pos = enqueue_pos_.load(std::memory_order_relaxed);
            }
        }
        cell->data = data;
        cell->sequence.store(pos+1,std::memory_order_release);//发布可取信号
        return true;
    }

    bool pop(T& data){
        Cell* cell;
        size_t pos = dequeue_pos_.load(std::memory_order_relaxed);//读出“下一个可以出队的位置”
        //relaxed 表示这次只是拿一个大概的号码
        for(;;){//无线循环，抢到格子就break;
            cell = &buffer_[pos &  mask_];//用位置确定格子下标
            //load是std::atomic 自带的函数
            size_t seq = cell->sequence.load(std::memory_order_acquire);//读预出队的位置
            intptr_t diff = (intptr_t)seq - (intptr_t)(pos+1);

            if(diff==0){//位置已经发布，可以竞争位置
                if(dequeue_pos_.compare_exchange_weak(
                    pos,pos+1,std::memory_order_relaxed)){
                        break;
                }
            }else if(diff<0){//队列为空，没数据出栈
                return false;
            }else{//pos已经过期
                pos=dequeue_pos_.load(std::memory_order_relaxed);
            }
        }
        data = cell->data;
        cell->sequence.store(pos+mask_+1,std::memory_order_release);//写入函数
        return true;
    }

private:
    struct Cell{//槽
        std::atomic<size_t> sequence;//，用来表示这个槽当前对应初始位置计数，
        //这个变量可以被多个线程同时读写

        T data;//数据
    };

    const size_t capacity_;//槽容量

    const size_t mask_;//使数组可以转圈使用。
    
    std::vector<Cell> buffer_;//实际存放capacity个槽的数组


    //CPU 一般按 64 字节为一整行缓存来搬运数据。
    alignas(64) std::atomic<size_t> enqueue_pos_;//下一个可放入的位置
    //alignas(64) 则要求变量的起始地址是 64 的倍数

    alignas(64) std::atomic<size_t> dequeue_pos_;//下一个可以取出的位置
};
#endif