#ifndef MEMORY_TRACKER_H
#define MEMORY_TRACKER_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void* mt_malloc(size_t size, const char* function, int line);
void mt_free(void* ptr);

#define MT_MALLOC(size) mt_malloc(size, __func__, __LINE__) 
#define MT_FREE(ptr) mt_free((void*)ptr)

// 初始化内存跟踪
void init_memory_tracking();

// 启用/禁用内存跟踪
void enable_memory_tracking(bool enable);

// 关闭内存跟踪并清理资源
void shutdown_memory_tracking();

// 设置输出次数阈值
void set_output_min_count(size_t count);

//设置输出排序方式
void set_output_sort_type(size_t type);

// 打印内存统计信息
void print_memory_stats();

#ifdef __cplusplus
}
#endif





#endif // MEMORY_TRACKER_API_H