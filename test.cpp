#include "memory_tracker.h"
#include <iostream>
#include <vector>

// 测试函数
void test_malloc_free() {
    std::cout << "测试 malloc/free..." << std::endl;
    
    // 分配内存
    char* ptr1 = (char*)MT_MALLOC(100);
    char* ptr2 = (char*)MT_MALLOC(200);
    
    // 使用内存
    for(int i = 0; i < 100; i++) {
        ptr1[i] = 'a';
    }
    
    // 释放部分内存
    MT_FREE(ptr1);
    
    // 不释放ptr2，制造内存泄漏
    // MT_FREE(ptr2);  // 注释掉这行以测试内存泄漏
}

void test_new_delete() {
    std::cout << "测试 new/delete..." << std::endl;
    
    // 分配内存
    int* ptr1 = new int[10];
    int* ptr2 = new int[20];
    
    // 使用内存
    for(int i = 0; i < 10; i++) {
        ptr1[i] = i;
    }
    
    // 释放部分内存
    delete[] ptr1;
    
    // 不释放ptr2，制造内存泄漏
    // delete[] ptr2;  // 注释掉这行以测试内存泄漏
}

void test_nested_calls() {
    std::cout << "测试嵌套调用..." << std::endl;
    
    char* ptr = (char*)MT_MALLOC(50);
    for(int i = 0; i < 50; i++) {
        ptr[i] = 'b';
    }
    
    // 不释放内存，制造泄漏
    // MT_FREE(ptr);
}

int main() {
    std::cout << "=== 内存泄漏检测测试 ===" << std::endl;
    
    // 初始化内存跟踪
    init_memory_tracking();
    
    // 设置输出最小次数为1
    set_output_min_count(1);
    
    // 运行各种测试
    test_malloc_free();
    test_new_delete();
    test_nested_calls();
    
    // 显示内存统计信息
    std::cout << "\n--- 内存使用情况 ---" << std::endl;
    print_memory_stats();
    // 清理资源
    shutdown_memory_tracking();
    
    std::cout << "测试完成!" << std::endl;
    return 0;
}