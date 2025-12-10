#include "memory_tracker.h"
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <map>
#include <dlfcn.h>    // 用于dladdr
#include <execinfo.h> // 用于backtrace
#include <iostream>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <unordered_map>

#define FUNCTION_LINE_MAX_LEN 88
#define FILE_MAX_LEN 32
#define LINE_MAX_LEN 7


// 简单的分配信息结构
struct AllocationInfo {
    void* ptr;
    size_t size;
    size_t type; // 0:new  1:malloc
    char function_line[FUNCTION_LINE_MAX_LEN];
    char file[FILE_MAX_LEN];
    uintptr_t address;
    size_t allocation_id;
    AllocationInfo* next;
};

// 新增：统计相同来源的泄漏计数
struct SourceLeakCount {
    size_t type;
    size_t count;
    size_t total_size;
    std::string file;
    std::string function_line;
};

// 全局变量
static AllocationInfo* allocation_list = nullptr;
static size_t total_allocated = 0;
static size_t total_freed = 0;
static size_t allocation_counter = 0;
static std::mutex allocation_mutex;
static bool tracking_enabled = false;
static size_t output_min_count = 5;
static size_t output_sort_type = 1; // 0:不排序 1:按照次数排序  2:按照total_size排序





void init_memory_tracking() {
    tracking_enabled = true;
}

void shutdown_memory_tracking() {
    std::lock_guard<std::mutex> lock(allocation_mutex);
    AllocationInfo* current = allocation_list;
    while (current) {
        AllocationInfo* next = current->next;
        free(current);
        current = next;
    }
    allocation_list = nullptr;
    total_allocated = 0;
    total_freed = 0;
    allocation_counter = 0;
}

void enable_memory_tracking(bool enable) {
    std::lock_guard<std::mutex> lock(allocation_mutex);
    tracking_enabled = enable;
}

void set_output_min_count(size_t count)
{
    output_min_count = count;
}

void set_output_sort_type(size_t type)
{
    output_sort_type = type;
}

char* GetFunctionLine(const char* function, int line)
{
	char strLine[LINE_MAX_LEN] = {0};
    short file_len = 0;
	if(line > 99999)
		line = 99999;
	sprintf(&strLine[0], "+%d", line);
	char* tmp = (char*)malloc(FUNCTION_LINE_MAX_LEN);
	memset(tmp, 0, FUNCTION_LINE_MAX_LEN);
    if(function != NULL){
        file_len = strlen(function);
        if((file_len + LINE_MAX_LEN) > (FUNCTION_LINE_MAX_LEN-1))
        {
            strncpy((tmp+FUNCTION_LINE_MAX_LEN-1-LINE_MAX_LEN), strLine, LINE_MAX_LEN);
            strncpy(tmp, (function+file_len-(FUNCTION_LINE_MAX_LEN-1-LINE_MAX_LEN)), FUNCTION_LINE_MAX_LEN-1-LINE_MAX_LEN);
        }
        else
        {
            strncpy(tmp, function, file_len);
            strncpy(tmp + file_len, strLine, LINE_MAX_LEN);
        }
    }
	return tmp;
}

// 获取函数名和偏移量（Linux特定）
bool get_function_info(void* address, char* function_name, size_t func_name_size, char* file_name, size_t file_name_size, int* line) {
    Dl_info info;
    if (dladdr(address, &info)) {
        // 获取文件名
        if (info.dli_fname) {
            int dli_name_len = strlen(info.dli_fname);
            if(dli_name_len > file_name_size-1){
                strncpy(file_name, info.dli_fname+(dli_name_len-file_name_size+1), file_name_size - 1);
            }
            else {
                strncpy(file_name, info.dli_fname, dli_name_len);
            }
            file_name[file_name_size - 1] = '\0';
        } else {
            strcpy(file_name, "unknown");
        }

        // 获取函数名
        if (info.dli_sname) {
            strncpy(function_name, info.dli_sname, func_name_size - 1);
            function_name[func_name_size - 1] = '\0';
        } else {
            strcpy(function_name, "unknown");
        }
        
        // 计算行号偏移（近似）
        if (info.dli_saddr && address > info.dli_saddr) {
            *line = (int)((char*)address - (char*)info.dli_saddr);
        } else {
            *line = 0;
        }
        return true;
    }
    return false;
}

void* mt_new(size_t size, void* address, const char* file, const char* function, int line, size_t type){
    char* function_line = NULL;
	void* ptr = malloc(size);
    if (ptr == NULL) {
        return NULL;
    }
	
    if (tracking_enabled) {
		if(function == NULL){
        function_line = (char*)malloc(FUNCTION_LINE_MAX_LEN);
			strcpy(function_line, "unkown");
		}
		else{
			function_line = GetFunctionLine(function, line);
		}
        std::lock_guard<std::mutex> lock(allocation_mutex);
        total_allocated += size;
        allocation_counter++;
        
        AllocationInfo* info = (AllocationInfo*)malloc(sizeof(AllocationInfo));
        if (info) {
            info->ptr = ptr;
            info->size = size;
            info->type = type;
            info->address = (uintptr_t)address;
            info->allocation_id = allocation_counter;
            strcpy(info->function_line, function_line);
            strcpy(info->file, file);
            info->next = allocation_list;
            allocation_list = info;
            
            #ifdef MEMORY_DEBUG_VERBOSE
            std::cout << "Allocated " << size << " bytes at address " << ptr 
                      << " (ID: " << allocation_counter << ")" << std::endl;
            #endif
        }
		free((void*)function_line);
    }
  
    return ptr;
}

void mt_delete(void* ptr){
	if (!ptr) return;
	
    if (tracking_enabled) {
        std::lock_guard<std::mutex> lock(allocation_mutex);
        
        AllocationInfo** current = &allocation_list;
        while (*current) {
            if ((*current)->ptr == ptr) {
                size_t size = (*current)->size;
                total_freed += size;
                
                #ifdef MEMORY_DEBUG_VERBOSE
                std::cout << "Freed " << size << " bytes at address " << ptr 
                          << " (ID: " << (*current)->allocation_id 
                          << ", Source: " << (*current)->source << ")" << std::endl;
                #endif
                
                AllocationInfo* to_delete = *current;
                *current = (*current)->next;
                free(to_delete);
                break;
            }
            current = &(*current)->next;
        }
    }
    
    free(ptr);
    ptr = NULL;
}

#ifdef __cplusplus
extern "C" {
#endif

void* mt_malloc(size_t size, const char* function, int line)
{
	if (tracking_enabled){
		void* return_address = __builtin_return_address(0);
		// sprintf(&saddr[0], "0x%lx", (uintptr_t)return_address);
		// // 尝试获取调用者信息
		// char function_name[256];
		char file_name[FILE_MAX_LEN] = "mfp.afx";
		// int line_offset;
		// get_function_info(return_address, function_name, sizeof(function_name), file_name, sizeof(file_name), &line_offset);
		return mt_new(size, return_address,file_name, function, line, 1);
	}
	else{
		return malloc(size);
	}
	
}

void mt_free(void* ptr)
{
	if (tracking_enabled){
		mt_delete(ptr);
	}
	else{
		if (!ptr) 
			return;
		free(ptr);
		ptr = NULL;
	}
}

#ifdef __cplusplus
}
#endif
size_t get_leak_count() {
    if (!tracking_enabled) return 0;
    
    // std::lock_guard<std::mutex> lock(allocation_mutex);
    size_t count = 0;
    AllocationInfo* current = allocation_list;
    while (current) {
        count++;
        current = current->next;
    }
    return count;
}

// 新增：按来源统计泄漏信息
void get_leak_stats_by_source(std::unordered_map<uintptr_t, SourceLeakCount>& leak_stats) {
    if (!tracking_enabled) return;
    
    // std::lock_guard<std::mutex> lock(allocation_mutex);
    AllocationInfo* current = allocation_list;
    size_t total_count = get_leak_count();
    size_t count = 0;
    int last_printed_percent = -1;
    while (current) {
        // std::cout << "开始 创建 source_str" << count <<std::endl;
        std::string function_line_str(current->function_line);
        std::string file_str(current->file);

        auto result = leak_stats.insert(
            std::make_pair(current->address, SourceLeakCount{current->type, 1, current->size, file_str, function_line_str})
        );
        // std::cout << "插入结束" << count <<std::endl;
        if (!result.second) {
            // 键已存在，更新计数和大小
            result.first->second.count++;
            result.first->second.total_size += current->size;
        }
        current = current->next;
        count++;
        int current_percent = (int)((count * 100LL) / total_count);
        if (current_percent > last_printed_percent && current_percent % 5 == 0 && current_percent != 0) {
            last_printed_percent = current_percent;
            std::cout << "进度: " << current_percent << "% (" 
                << count << "/" << total_count << ")" << std::endl;
        }
        
    }
}

void print_memory_stats() {
    std::cout << "enable:" << tracking_enabled << std::endl;
    if (!tracking_enabled) {
        std::cout << "内存跟踪未启用" << std::endl;
        return;
    }
    static std::unordered_map<uintptr_t, SourceLeakCount> pre_leak_stats;
    // std::lock_guard<std::mutex> lock(allocation_mutex);
    size_t leak_count = get_leak_count();
    
    std::cout << "\n===== 内存统计 =====\n";
    std::cout << "总分配内存: " << total_allocated << " 字节\n";
    std::cout << "总释放内存: " << total_freed << " 字节\n";
    std::cout << "潜在内存泄漏: " << total_allocated - total_freed << " 字节\n";
    std::cout << "未释放的块数: " << leak_count << "\n";
    
    if (leak_count > 0) {
        // 按来源统计泄漏
        std::unordered_map<uintptr_t, SourceLeakCount> leak_stats;
        std::cout << "开始读取map表" << std::endl;
        get_leak_stats_by_source(leak_stats);
        std::cout << "读取map表成功 长度是:" << leak_stats.size() << std::endl;
        std::vector<std::pair<uintptr_t, SourceLeakCount>> result (leak_stats.begin(), leak_stats.end());
        std::cout << "开始排序" << output_sort_type<< std::endl;
        if(output_sort_type == 1)
            std::sort(result.begin(), result.end(), [](const std::pair<uintptr_t, SourceLeakCount>& a, const std::pair<uintptr_t, SourceLeakCount>&b){return a.second.count > b.second.count;});
        else if (output_sort_type == 2)
            std::sort(result.begin(), result.end(), [](const std::pair<uintptr_t, SourceLeakCount>& a, const std::pair<uintptr_t, SourceLeakCount>&b){return a.second.total_size > b.second.total_size;});
        std::cout << "排序完成" << std::endl;
        std::cout << "\n===== 按来源统计的未释放内存 =====\n";
        size_t index = 1;
        std::string type;
        for (const auto& stat : result) {
            if(stat.second.type == 0)
                type = "new";
            else
                type = "malloc";
            if(stat.second.count >= output_min_count)
                std::cout << "[" << std::left << std::setw(3) << index++ << "] 来源: " 
                      << std::left << std::setw(7) << type 
                      << "0x" << std::left << std::setw(20) << std::hex << stat.first << std::dec
                      << std::left << std::setw(FILE_MAX_LEN) << stat.second.file
                      << std::left << std::setw(FUNCTION_LINE_MAX_LEN) << stat.second.function_line
                      << ", 次数变化: " << std::left << std::setw(6) << static_cast<int>(stat.second.count) - static_cast<int>(pre_leak_stats[stat.first].count)
                      << ", 总大小变化: " << static_cast<int>(stat.second.total_size) - static_cast<int>(pre_leak_stats[stat.first].total_size) << " 字节" << std::endl;
        }
        pre_leak_stats = leak_stats;
    }
    std::cout << "==========================\n\n";
}


// 重载的new运算符
void* operator new(size_t size) {
	if (tracking_enabled) {
		void* return_address = __builtin_return_address(0);
		// 尝试获取调用者信息
		char function_name[256]={0};
		char file_name[FILE_MAX_LEN]={0};
		int line_offset=0;
		get_function_info(return_address, function_name, sizeof(function_name), file_name, sizeof(file_name), &line_offset);
		// sprintf(&saddr[0], "0x%lx", (uintptr_t)return_address);
		return mt_new(size, return_address, file_name, function_name, line_offset, 0);
	}
	else{
		return malloc(size);
	}
}
//new[] 特用
void* operator new(size_t size, void* address, const char* file, const char* function, int line) {
    return mt_new(size, address, file, function, line, 0);
}

// 重载的delete运算符
void operator delete(void* ptr) noexcept {
	if (tracking_enabled) {
		mt_delete(ptr);
	}
	else{
		if (!ptr) 
			return;
		free(ptr);
		ptr = NULL;
	}
}

// 重载的new[]和delete[]运算符
void* operator new[](size_t size) {
	if (tracking_enabled) {
		void* return_address = __builtin_return_address(0);
		// 尝试获取调用者信息
		char function_name[256]={0};
		char file_name[FILE_MAX_LEN]={0};
		int line_offset=0;
		get_function_info(return_address, function_name, sizeof(function_name), file_name, sizeof(file_name), &line_offset);
		// sprintf(&saddr[0], "0x%lx", (uintptr_t)return_address);
		return operator new(size, return_address, file_name, function_name, line_offset);
	}
	else{
		return malloc(size);
	}
}

void operator delete[](void* ptr) noexcept {
	if (tracking_enabled) {
		operator delete(ptr);
	}
	else{
		if (!ptr) 
			return;
		free(ptr);
		ptr = NULL;
	}
}