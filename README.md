## 原理介绍
[[memory_tracker.cpp]]文件和[[memory_tracker.h]]文件
编译依赖动态库：-ldl -rdynamic  编译.o和最终产物时都需要
原理：主要是重写了new、delete函数 和 宏替换malloc、free函数来实现内存监测
## 接口函数介绍
// 初始化内存跟踪
void init_memory_tracking();
开始调试，修改标识位，开始记录内存情况
  
// 启用/禁用内存跟踪
void enable_memory_tracking(bool enable);

// 关闭内存跟踪并清理资源
void shutdown_memory_tracking();

// 设置输出次数阈值  比如计数未释放内存块低于K的不进行打印
void set_output_min_count(size_t count);

//设置输出排序方式（默认值为1）   0：不排序   1：按未释放次数排序   2：按未释放内存总量排序
void set_output_sort_type(size_t type);

// 打印当前未释放的内存统计信息
void print_memory_stats();

输出格式如下：
序号索引，申请内存方式，调用栈的地址，调用者的名称，调用者的函数+偏移量，调用者申请内存计数差值（与上次打印的差值），调用者申请内存字节差值
![[file-接口函数介绍-20251110090704.jpg]]
申请内存方式：new和malloc
调用栈的地址：谁调用的new或malloc，通常标准库调的比较多
调用者的名称：动态库或mfp.afx
调用者函数+偏移量：
- new ：调用者的函数打印不出来用unknown补齐，偏移量是汇编指令的偏移
- malloc：一定会打印调用者的函数，和函数的具体行号
调用者申请内存计数差值：第一次打印是 当前计数-0； 第二次打印是：当前打印计数-第一次打印计数；以此类推...
调用者申请内存字节差值：同上，显示的是申请内存的总字节数差值
				
## 使用方法
1. 把两只文件拿到工程里，所有文件引入头文件memory_tracker.h
2. 修改makefile，注意memory_tracker.cpp要用g++编译，注意动态库依赖参数
3. 替换所有c语言malloc函数为MT_MALLOC, free函数为MT_FREE
4. 注册命令绑定函数，以下是我的写的函数，用cmd_register注册一下，方便调试
```
static int32_t pi_memory_tracker_debug(int32_t argc, char* argv[])
{
    int vaule = 0;
    int cmd = atoi(argv[0]);
    if (argc >= 2)
        vaule = atoi(argv[1]);
    switch(cmd)
    {
        case 1:
            enable_memory_tracking(vaule==1?true:false);
            break;
        case 2:
            set_output_min_count(vaule);
            break;
        case 3:
            set_output_sort_type(vaule);
        case 4:
            print_memory_stats();
            break;
        case 9:
            shutdown_memory_tracking();
        default:
            break;
    }
    return 0;
}
cmd_register("memory", "debug", pi_memory_tracker_debug, NULL);
```

## 调用栈信息代码追溯方法
我们以两个实例来展示一下debug方法
## 实例一
mfp.afx定位
![[b6bdcc839e03d324d3de.png]]
我们使用命令打印：
addr2line -e mfp.afx 0x345dea
注意这个mfp.afx一定要是带符号的，路径：buildroot/output/build/controller-custom/appsrc
如果你非常的幸运，那么他直接打出来的就是对应的代码和行号，直接去看源代码了

如果你不幸运跟我一样，输出如下：
![[file-实例一-20251107175719.jpg]]

**无法直接定位到具体代码行号**
那么接下来跟着我操作
1. 如果后面有function和偏移行号，那我们可以快速定位出来
_ ZN15PollingTreeLeaf7InquireEv+1422
调用栈解析：
	（1）_ ZN15PollingTreeLeaf7InquireEv+1422 是函数+偏移地址
	（2）_ ZN15PollingTreeLeaf7Inquire 表示 PollingTreeLeaf::Inquire,是c++对函数名修饰的最终结果
	（3）如果有C1，C2 表示 构造函数, 区别是是否包含基类，没有就是普通函数
	（4）Ev 表示 参数为空
1. 这是已经缩小范围到函数，如果还想继续缩小范围就需要借助带符号的mfp.afx和调用栈指针0x345dea
2.  先反汇编mfp.afx （也可以不用反汇编，一直用addr2line往前查看地址，直到跳出标准库看到我们自己写的代码，当然效率太低）
/opt/csky_860_compilers/bin/csky-abiv2-linux-objdump -d -S -l mfp.afx  > disam.txt
4. 文件特别大，用vim打开，搜索 345dea
![[file-实例一-20251110095314.jpg]]

5. 他会对应一条汇编指令，和我们对应的代码，然后开始往前倒，直到找到我们的源码
![[file-实例一-20251110095349.jpg]]
6. 如上所示：48行是一个“}”，所以内存应该是44行申请的，我们找到了具体的行号，这一行这里就去做的new的动作，然后我们观察这里的内存有没有释放，
## 实例二
libcommon.so定位
![[c27f8e98ecd2995b58e4.png]]
地址：0x20036242
1. 这个地址很明显跟我们mfp.afx是不一样的，这里是一个虚拟内存的地址，所以第一步我们要找到这个动态库加载的基地址
2. 使用命令 ：cat /proc/208/maps     //208是mfp进程的pid
3. ![[fe03de676dbb22a21038.png]]
4. 我们只需要关注libcommon.so的第一行第一列的地址即可，lib库是分段加载的
- r-xp是代码段，只读
- r--p是隔离段，用于内存对齐
- rw--p是读写段，可读可写
5. 然后我们拿到了libcommon.so的基地址是0x2001f000
6. 开始计算偏移地址: 0x20036242-0x2001f000 = 0x17242, 有了这个地址我们就可以拿去debug我们的common库了
如果addr2line可以出来最好，不行就只能反汇编去找了
具体方法还是跟上面一样，我就不赘述了



## 特殊注意
1. 动态库之间不要有依赖
 如果memory_tracker.cpp放在了lib1库里面编译，千万要注意，不要用lib2去引用lib1，否则lib2传输__FILE__  时，lib1接收到const char* p无法访问，访问就段错误；**new函数是可以定位的，malloc不行**
 段错误原因解释：lib2的传的文件和函数信息是在数据段， 如果lib2被卸载后，那这个地址会是野指针，因此会段错误（AI给出的解释），个人理解lib2调用lib1申请内存时，lib2不会立即卸载，具体原因不详，猜测可能是数据段不同
 2. 输入完命令 echo memory debug 4 > /tmp/cmd之后会需要等会才能打印
 这是在开始整理内存块计数，会有进度显示：5%的步长更新，需要耐心等待一会
 3. 优化源文件代码之前建议先看一下我踩的坑，否则动不动就会死机，各种原因调到你怀疑人生
**剩下最后一个点可以优化，就是内存管理我用的链表，free时查找的效率太差，可以优化成哈希或树结构提升效率，不影响最终结果**

## 结语
稍微吐槽一下我遇到的坑
（1）就是动态库依赖一跑就是死机，我把memory_tracker.cpp放在了libcommon库中，在libframework中宏替换了MT_MALLOC，先是动态库依赖，makefile解决，在是strlen死机，第一次没判空，后面判了还是死，最后才发现是野指针段错误
（2）无限递归，因为我们是重写的new，所以我本想着用map管理，但是因为map本身是c++容器，底层实现还是依赖new函数，所以他会一直进入我重写的new函数，直到栈溢出死机
（3）malloc和new底层都是使用的 malloc函数实现，所以重写malloc是个极其不现实的问题， 因为我们要解决内存管理问题，就势必会开辟堆的空间，所以如果重写malloc那就会一直无限递归回调malloc函数，所以最后选择的就是重写new函数，malloc宏替换后在调用new，复用手动实现的mt_new函数做内存记录
（4）内存申请的来源获取，malloc是可以宏替换的，因此可以直接使用__FILE__直接拿到想要的信息，但是new函数不一样，我们是重写他，不是重载（这也是我们不用手动替换new的原因），而且还有很多都是标准库去调用的new函数，我们不可能去替换标准库里的new。然后new函数的来源就成了难点，最后采用调用栈和带符号的程序库去溯源，有了上面的方法，可以精准定位是哪一行
（5）死锁问题，在我们工程里，malloc、free 、new、delete一定是多线程处理，我们通过一个链表管理一定需要借助锁来实现，应该用锁保护全局变量链表。在输出的时候亦是如此，但是print_memory_stats函数里加上锁就死锁，我起初以为是链接处理效率太低而造成卡顿（这是我写进度条的根本原因），加日志发现不是，根本就没继续往下执行（加日志调试是一个痛苦的过程，耗材升级工具烧录），最后发现卡在string那里，最后才想到，我在进入print_memory_stats函数已经加了锁，在函数内部有调用了string类型取new空间，死锁在了new那里。最后不得已去掉了print_memory_stats函数的锁，反正我要一个临时状态就行了，而且我也不操作链表，只是把内容读出来放在我的map中做排序等处理
（6）日志输出问题；第一个就是格式化输出，本来function_line长度写的128，但是输出的时候应该是缓存区不够了，回车换行都没有，巨痛苦，最后优化到了88；第二个就是dleta方面，一开始写的是输出内存的当前状态，后来对比的时候发现内存块太多了，很难发现谁是增加了谁是减少，后来采用缓存一个pre状态，每次输出的时候减去上一次的状态，这样就能快速的看出来到底是谁在悄咪咪一直泄露
 


