/*********************************************************************/
/*  文件名   :  看谁排得快 —— 多排序算法性能比较与分析系统            */
/*  题  目   :  题目6 看谁排得快                                      */
/*  说  明   :  本程序用 8 种排序算法对同一批数据进行排序, 统计并比较  */
/*              各算法的运行时间、比较次数、移动次数和排序趟数,       */
/*              自动分析数据特征, 给出"怎样的序列用怎样的方法最快"    */
/*              的结论建议, 并支持用任意数据验证结论。                */
/*  开发环境 :  Dev-C++ / VS2010 及以上 / g++ (标准 C++, 无平台依赖)  */
/*  运行说明 :  编译后直接运行, 按菜单提示操作。                      */
/*              程序会在当前目录生成/使用以下文件:                    */
/*                data.txt            —— 原始数据文件(可手工编辑)     */
/*                sort_report.txt     —— 本次实验报表(覆盖写)         */
/*                sort_report.csv     —— 历史实验报表(追加写)         */
/*                sort_conclusion.txt —— 分析结论存档(选做功能)       */
/*********************************************************************/

#include <iostream>   // 标准输入输出(cout/cin)
#include <fstream>    // 文件输入输出(ifstream/ofstream)
#include <sstream>    // 字符串流(istringstream/ostringstream, 用于整行解析与拼接)
#include <iomanip>    // 输出格式控制(setw/setprecision/fixed)
#include <string>     // string 类
#include <vector>     // vector 动态数组容器
#include <cstdlib>    // rand/srand/exit
#include <ctime>      // time/clock/CLOCKS_PER_SEC
#include <cmath>      // log2(计算快排递归深度上限)
#include <algorithm>  // std::sort(仅用于统计"不同取值个数"这一数据特征, 不参与计时实验)

using namespace std;

/*===================================================================*/
/*                          一、全局常量                              */
/*===================================================================*/

const int    ALGO_NUM     = 8;        // 参加比较的排序算法个数
const int    SIMPLE_LIMIT = 20000;    // O(n^2)类简单算法的规模上限: 超过则自动跳过, 避免等待过久
const int    N_MAX        = 500000;   // 数据集允许的最大规模(内存与时间兼顾)
const int    TRACE_MAX    = 32;       // 逐趟演示模式允许的最大元素个数(演示只讲过程, 不讲规模)
const int    SHOW_MAX     = 60;       // 屏幕显示数据时"全部显示"的阈值, 超过则只显示前后若干个
const char  *DATA_FILE      = "data.txt";             // 默认数据文件名
const char  *REPORT_TXT     = "sort_report.txt";      // 本次报表文件名(文本表格)
const char  *REPORT_CSV     = "sort_report.csv";      // 历史报表文件名(CSV, 可用Excel打开)
const char  *CONCLUSION_FILE = "sort_conclusion.txt"; // 结论存档文件名

// 算法名称表(下标与函数表一一对应)
const char *ALGO_NAME[ALGO_NUM] = {
    "直接插入排序", "折半插入排序", "希尔排序",   "冒泡排序",
    "快速排序",     "简单选择排序", "堆排序",     "二路归并排序"
};

// O(n^2) 算法标记表: true 表示该算法是简单算法, 数据过大时自动跳过
const bool ALGO_SIMPLE[ALGO_NUM] = {
    true,           // 直接插入排序 O(n^2)
    true,           // 折半插入排序 O(n^2)(比较次数降为O(nlogn), 移动仍O(n^2))
    false,          // 希尔排序     约 O(n^1.3)
    true,           // 冒泡排序     O(n^2)
    false,          // 快速排序     平均 O(nlogn)
    true,           // 简单选择排序 O(n^2)
    false,          // 堆排序       O(nlogn)
    false           // 二路归并排序 O(nlogn)
};

/*===================================================================*/
/*                     二、统计结构与全局变量                          */
/*===================================================================*/

/**
 * SortStats —— 一次排序实验的统计结果
 * 每运行一个算法, 就填写一个这样的结构, 用于屏幕表格和报表文件。
 */
struct SortStats
{
    const char *name;    // 算法名称(指向 ALGO_NAME 表中的常量字符串)
    double      timeMs;  // 排序用时(毫秒), 用 clock() 前后相减换算得到
    long long   cmp;     // 关键字"比较"次数(每调用一次 needSwap 计 1 次)
    long long   mov;     // 元素"移动"次数(每赋值一个元素计 1 次, 一次交换计 3 次)
    long long   passes;  // "排序趟数"(各算法口径见各自函数头注释)
    bool        skipped; // 是否因数据规模过大而被跳过
    bool        ok;      // 排序结束后序列是否确实有序(用于自检, 理论上恒为 true)

    // 构造函数: 刚创建时先复位, 防止读到随机垃圾值
    SortStats() { reset(""); }

    // 复位函数: 参数 n 为算法名称, 其余统计量清零
    void reset(const char *n)
    {
        name    = n;      // 记录算法名
        timeMs  = 0.0;    // 用时清零
        cmp     = 0;      // 比较次数清零
        mov     = 0;      // 移动次数清零
        passes  = 0;      // 趟数清零
        skipped = false;  // 未跳过
        ok      = false;  // 尚未验证正确
    }
};

vector<int> g_data;          // 当前数据集(整个程序围绕它工作)
bool        g_hasData  = false; // 是否已经准备好数据(菜单1/2/3完成后为 true)
bool        g_desc     = false; // 排序方向: false=由小到大(升序), true=由大到小(降序)
string      g_dataDesc = "";    // 当前数据集的文字描述(写入报表, 如"随机数据 n=10000")
SortStats   g_results[ALGO_NUM]; // 最近一次"运行全部算法"的统计结果
bool        g_hasResults = false; // g_results 中是否保存着与当前数据对应的结果
string      g_resultDesc = "";   // g_results 对应的数据描述(防止数据已变而结果未变)
int         g_resultN    = 0;    // g_results 对应的数据规模
bool        g_trace    = false; // 逐趟演示开关: true 时排序函数每完成一趟就打印数组

// ---- 函数前置声明(解决定义顺序上的相互调用) ----
void heapSortCore(int b[], int n, SortStats *st);   // 堆排序主体(被快速排序失衡收尾复用)
int  partition(int a[], int lo, int hi, SortStats *st); // 快速排序的一次划分(被递归主体调用)

/*===================================================================*/
/*                        三、基础工具函数                             */
/*===================================================================*/

/**
 * needSwap —— 判断"x 是否需要排到 y 后面"(带比较计数)
 * 参数: x,y     参与比较的两个关键字
 *       st      统计结构指针, 每次比较计入 st->cmp
 * 返回: true  —— x 应排在 y 之后, 需要交换/移动
 *       false —— 顺序正确
 * 说明: 全部 8 种算法统一通过本函数比较关键字, 升降序方向由全局
 *       变量 g_desc 控制, 因此每种算法无需为升降序写两份代码。
 *       相等的关键字返回 false(不交换), 这保证了插入/冒泡/归并的稳定性。
 */
inline bool needSwap(int x, int y, SortStats *st)
{
    st->cmp++;                          // 每次真正的关键字比较都计数
    if (g_desc) return x < y;           // 降序: x 比 y 小说明 x 应排后面
    else        return x > y;           // 升序: x 比 y 大说明 x 应排后面
}

/**
 * inOrder —— 判断相邻两元素是否已经有序(不计数, 用于特征分析与自检)
 * 参数: x,y 相邻两元素
 * 返回: 按当前方向判断 x 是否应排在 y 前面(相等也算有序)
 */
inline bool inOrder(int x, int y)
{
    if (g_desc) return x >= y;          // 降序时前一个不小于后一个即有序
    else        return x <= y;          // 升序时前一个不大于后一个即有序
}

/**
 * swapVals —— 交换两个元素并累计移动次数(一次交换计 3 次移动)
 * 参数: x,y  要交换的两个元素(引用);  st 统计结构指针
 */
inline void swapVals(int &x, int &y, SortStats *st)
{
    int t = x;                          // t —— 交换用的临时变量
    x = y;                              // 第 1 次移动
    y = t;                              // 第 2 次移动
    st->mov += 3;                       // 交换共 3 次赋值, 计 3 次移动
}

/**
 * isSorted —— 检查数组是否已按当前方向排好序(算法自检, 不计入统计)
 * 参数: a 数组首地址;  n 元素个数;  descDir 是否按降序检查
 * 返回: true=有序, false=存在逆序对
 */
bool isSorted(const int a[], int n, bool descDir)
{
    for (int i = 0; i + 1 < n; i++)     // 依次检查相邻元素
    {
        if (descDir) { if (a[i] < a[i + 1]) return false; } // 降序出现逆序
        else         { if (a[i] > a[i + 1]) return false; } // 升序出现逆序
    }
    return true;                        // 全部相邻对都有序
}

/**
 * showArr —— 在一行内打印数组(超过 20 个只打印前 20 个, 避免刷屏)
 * 参数: a 数组首地址;  n 元素个数
 */
void showArr(const int a[], int n)
{
    int m = (n < 20 ? n : 20);          // m —— 实际打印的元素个数
    cout << "[";
    for (int i = 0; i < m; i++)         // 逐个打印
    {
        if (i > 0) cout << " ";         // 元素之间用空格分隔
        cout << a[i];
    }
    if (n > m) cout << " ... (共" << n << "个)"; // 太长时给出省略提示
    cout << "]" << endl;
}

/**
 * randRange —— 产生 [lo, hi] 范围内的随机整数
 * 参数: lo 下界, hi 上界(含)
 * 返回: [lo, hi] 内的整数
 * 说明: rand() 本身上限只有 RAND_MAX(至少 32767), 把两个 rand()
 *       拼接起来可把取值范围扩大到约 10^9, 满足大数据值域需要。
 */
int randRange(int lo, int hi)
{
    int span = hi - lo + 1;                            // span —— 值域宽度
    int r = (int)(((long long)rand() * 32768 + rand()) % span); // 拼接两个随机数再取模
    return lo + r;
}

/**
 * readInt —— 从键盘读取一个 [lo, hi] 范围内的整数(带容错)
 * 参数: prompt 提示字符串;  lo/hi 允许的范围
 * 返回: 合法的整数
 * 说明: 采用"整行读取 + 字符串流解析"的方式, 若用户输入字母、
 *       符号或超范围数字, 会提示重输而不会让程序崩溃或死循环;
 *       若输入流结束(EOF, 如 Ctrl+Z), 则安全退出程序。
 */
int readInt(const char *prompt, int lo, int hi)
{
    while (true)                        // 循环直到读到合法输入
    {
        cout << prompt;                 // 显示提示
        string line;                    // line —— 存放用户输入的一整行
        if (!getline(cin, line))        // 读入失败(输入流结束)
        {
            cout << "\n输入流已结束, 程序退出。" << endl;
            exit(0);                    // 安全退出
        }
        istringstream ss(line);         // ss —— 用于从整行中解析整数
        int v;                          // v —— 解析出来的数值
        char c;                         // c —— 用于检测数字后面是否跟了杂字符
        if ((ss >> v) && !(ss >> c) && v >= lo && v <= hi)
            return v;                   // 是纯整数且在范围内: 返回
        cout << "  输入无效, 请输入 " << lo << " ~ " << hi << " 之间的整数!" << endl;
    }
}

/**
 * readLine —— 读取一行字符串(允许为空), EOF 时安全退出
 * 参数: prompt 提示字符串
 * 返回: 读到的整行(已去掉首尾空白, 见 trimStr)
 */
string readLine(const char *prompt)
{
    cout << prompt;                     // 显示提示
    string line;                        // line —— 用户输入的一整行
    if (!getline(cin, line))            // 输入流结束
    {
        cout << "\n输入流已结束, 程序退出。" << endl;
        exit(0);
    }
    return line;
}

/**
 * trimStr —— 去掉字符串首尾的空白字符(空格/制表符/回车换行)
 * 参数: s 原字符串
 * 返回: 去掉首尾空白后的新字符串
 */
string trimStr(const string &s)
{
    size_t b = s.find_first_not_of(" \t\r\n"); // b —— 第一个非空白字符位置
    if (b == string::npos) return "";          // 整行都是空白: 返回空串
    size_t e = s.find_last_not_of(" \t\r\n");  // e —— 最后一个非空白字符位置
    return s.substr(b, e - b + 1);             // 截取中间的有效部分
}

/**
 * pauseEnter —— 暂停, 按回车继续(让用户看清输出后再回到菜单)
 */
void pauseEnter()
{
    cout << "\n按回车键返回菜单...";
    string s;                           // s —— 仅用于等待回车
    getline(cin, s);
}

/*===================================================================*/
/*                     四、数据准备模块(菜单1/2/3)                     */
/*        对应功能要求(1): 文件数据 / 随机数据 / 规律数据              */
/*===================================================================*/

/**
 * loadFromFile —— 从文本文件读入原始数据(用户可提前用记事本编辑)
 * 参数: filename 数据文件名, 文件中用空白(空格/换行/Tab)分隔整数
 * 返回: 无; 成功时更新 g_data / g_dataDesc / g_hasData, 失败时给出提示
 */
void loadFromFile(const string &filename)
{
    ifstream fin(filename.c_str());     // fin —— 输入文件流
    if (!fin)                           // 文件打不开(不存在/被占用)
    {
        cout << "  错误: 无法打开文件 " << filename << " !" << endl;
        return;
    }
    vector<int> v;                      // v —— 先读到临时数组, 成功后才替换当前数据
    int x;                              // x —— 从文件中读到的整数
    while (fin >> x) v.push_back(x);    // 逐个读入, 遇到非数字自动停止
    fin.close();
    if (v.empty())                      // 一个数都没读到
    {
        cout << "  错误: 文件中没有读到有效整数!" << endl;
        return;
    }
    if ((int)v.size() > N_MAX)          // 超过规模上限则截断, 防止内存占用过大
    {
        v.resize(N_MAX);
        cout << "  提示: 数据过多, 已截取前 " << N_MAX << " 个。" << endl;
    }
    g_data     = v;                     // 替换当前数据集
    g_hasData  = true;
    g_dataDesc = "文件读入(" + filename + ")";
    g_hasResults = false;               // 数据变了, 旧实验结果作废
    cout << "  成功读入 " << g_data.size() << " 个数据: ";
    showArr(&g_data[0], (int)g_data.size());
}

/**
 * genRandom —— 产生 n 个随机整数(值域 0~999999)
 * 参数: n 数据个数
 * 返回: 无; 更新 g_data / g_dataDesc / g_hasData
 */
void genRandom(int n)
{
    g_data.clear();                     // 清空旧数据
    g_data.reserve(n);                  // 预留空间, 避免反复扩容
    for (int i = 0; i < n; i++)         // 逐个产生随机数
        g_data.push_back(randRange(0, 999999));
    g_hasData  = true;
    g_dataDesc = "随机数据";
    g_hasResults = false;               // 旧结果作废
    cout << "  已产生 " << n << " 个随机数据(值域 0~999999): " << endl;
    showArr(&g_data[0], n);
}

/**
 * genPattern —— 产生有规律的改造数据集
 * 参数: kind 1=升序有序 2=逆序 3=基本有序(约5%乱序) 4=少量重复值
 *       n    数据个数
 * 返回: 无; 更新 g_data / g_dataDesc / g_hasData
 * 说明: 这三种"极端"数据正是本课题研究的重点——排序时间与初始
 *       序列的关系, 主要就体现在有序/逆序/大量重复这些形态上。
 */
void genPattern(int kind, int n)
{
    g_data.clear();
    g_data.reserve(n);
    if (kind == 1 || kind == 2 || kind == 3)
    {
        for (int i = 0; i < n; i++)     // 先生成 1..n 的升序序列
            g_data.push_back(i + 1);
        if (kind == 2)                  // 逆序: 首尾对称交换
        {
            for (int i = 0, j = n - 1; i < j; i++, j--)
            {
                int t = g_data[i]; g_data[i] = g_data[j]; g_data[j] = t;
            }
        }
        if (kind == 3)                  // 基本有序: 随机交换约 n/20 对元素
        {
            int swaps = n / 20;         // swaps —— 打乱次数(约5%)
            for (int k = 0; k < swaps; k++)
            {
                int i = randRange(0, n - 1);        // 随机位置 i
                int j = randRange(0, n - 1);        // 随机位置 j
                int t = g_data[i]; g_data[i] = g_data[j]; g_data[j] = t;
            }
        }
    }
    else // kind == 4: 少量重复值 —— 值域只有 n/10, 平均每个值出现约10次
    {
        int range = n / 10;             // range —— 取值范围宽度
        if (range < 5) range = 5;       // 保证小规模数据也有重复的可能
        for (int i = 0; i < n; i++)
            g_data.push_back(randRange(0, range));
    }
    g_hasData = true;
    g_hasResults = false;               // 旧结果作废
    switch (kind)                       // 记录数据描述, 供报表使用
    {
        case 1: g_dataDesc = "规律数据-升序有序";   break;
        case 2: g_dataDesc = "规律数据-逆序";       break;
        case 3: g_dataDesc = "规律数据-基本有序";   break;
        default:g_dataDesc = "规律数据-少量重复值"; break;
    }
    cout << "  已产生" << g_dataDesc << " " << n << " 个: " << endl;
    showArr(&g_data[0], n);
}

/**
 * showCurrentData —— 在屏幕上显示当前数据集(太长时只显示前后部分)
 */
void showCurrentData()
{
    if (!g_hasData)                     // 还没有数据
    {
        cout << "  当前没有数据, 请先用菜单 1/2/3 准备数据!" << endl;
        return;
    }
    int n = (int)g_data.size();         // n —— 数据个数
    cout << "  当前数据 " << n << " 个 [" << g_dataDesc
         << ", 排序方向: " << (g_desc ? "由大到小" : "由小到大") << "]" << endl;
    if (n <= SHOW_MAX)                  // 规模不大: 全部显示
    {
        cout << "  ";
        showArr(&g_data[0], n);
    }
    else                                // 规模大: 显示前30个和后10个
    {
        cout << "  前30个: ";
        showArr(&g_data[0], 30);
        cout << "  后10个: ";
        showArr(&g_data[n - 10], 10);
    }
}

/**
 * saveDataToFile —— 把当前数据保存到文本文件(便于下次用菜单1读回)
 * 参数: filename 目标文件名
 */
void saveDataToFile(const string &filename)
{
    if (!g_hasData)                     // 没有数据可存
    {
        cout << "  当前没有数据, 无法保存!" << endl;
        return;
    }
    ofstream fout(filename.c_str());    // fout —— 输出文件流
    if (!fout)
    {
        cout << "  错误: 无法创建文件 " << filename << " !" << endl;
        return;
    }
    int n = (int)g_data.size();
    for (int i = 0; i < n; i++)         // 每行存 10 个, 用空格分隔
    {
        fout << g_data[i];
        if ((i + 1) % 10 == 0 || i == n - 1) fout << endl;
        else fout << " ";
    }
    fout.close();
    cout << "  已把 " << n << " 个数据保存到 " << filename << endl;
}

/*===================================================================*/
/*                 五、排序算法模块(8种, 均支持升降序)                 */
/*    趟数(passes)的统计口径在各函数注释中分别说明, 全篇保持统一:      */
/*      插入类: 每插入一个元素算一趟, 共 n-1 趟;                      */
/*      希尔  : 每个增量算一趟, 趟数=增量序列长度;                    */
/*      冒泡  : 每次完整扫描算一趟, 有序时提前结束, 趟数<=n-1;        */
/*      选择  : 每选出一个最值算一趟, 共 n-1 趟;                      */
/*      快排  : 每调用一次 partition 划分算一趟;                      */
/*      堆排  : 建堆记 1 趟, 之后每次出堆记 1 趟, 共 n 趟;            */
/*      归并  : 每一轮整体归并算一趟, 趟数=ceil(log2(n))。            */
/*===================================================================*/

/**
 * insertSort —— 1.直接插入排序
 * 原理: 把第 i 个元素插入到前面已有序的 a[0..i-1] 中, 从后向前
 *       边比较边后移, 找到位置后放入。基本有序时接近 O(n)。
 * 参数: a 数组首地址;  n 元素个数;  st 统计结构指针
 * 趟数: 每插入一个元素为一趟, 共 n-1 趟
 */
void insertSort(int a[], int n, SortStats *st)
{
    for (int i = 1; i < n; i++)         // i —— 当前待插入元素的下标, 前 i 个已有序
    {
        st->passes++;                   // 第 i 趟
        int key = a[i];                 // key —— 待插入的元素(先取出, 计1次移动)
        st->mov++;
        int j = i - 1;                  // j —— 从有序区末尾向前扫描的下标
        // 边比较边把比 key 大(方向相关)的元素后移一格
        while (j >= 0 && needSwap(a[j], key, st))
        {
            a[j + 1] = a[j];            // 后移一格(计1次移动)
            st->mov++;
            j--;
        }
        a[j + 1] = key;                 // key 放入空出的位置(计1次移动)
        st->mov++;
        if (g_trace && n <= TRACE_MAX)  // 演示模式: 打印本趟结果
        {
            cout << "  第" << i << "趟(插入" << key << "): ";
            showArr(a, n);
        }
    }
}

/**
 * binaryInsertSort —— 2.折半插入排序
 * 原理: 与直接插入相同, 但插入位置用"折半查找"确定, 比较次数
 *       降为 O(nlogn); 因找到位置后仍要整块后移, 移动次数不变。
 * 参数: a 数组首地址;  n 元素个数;  st 统计结构指针
 * 趟数: 每插入一个元素为一趟, 共 n-1 趟
 * 稳定性: 相等元素插入其后方(low 指向第一个大于 key 的位置),
 *         保持了原有相对次序, 是稳定排序。
 */
void binaryInsertSort(int a[], int n, SortStats *st)
{
    for (int i = 1; i < n; i++)         // i —— 当前待插入元素下标
    {
        st->passes++;                   // 第 i 趟
        int key = a[i];                 // key —— 待插入元素
        st->mov++;
        int low = 0;                    // low —— 折半查找区间左端
        int high = i - 1;               // high —— 折半查找区间右端
        // 折半查找 key 的插入位置: 最终 low 即插入点
        while (low <= high)
        {
            int mid = (low + high) / 2;     // mid —— 区间中点
            if (needSwap(a[mid], key, st))  // a[mid] 应排在 key 后面: 插入点在左半
                high = mid - 1;
            else                            // a[mid] <= key: 插入点在右半(相等也在右, 保证稳定)
                low = mid + 1;
        }
        // a[low..i-1] 整体后移一格
        for (int j = i - 1; j >= low; j--)
        {
            a[j + 1] = a[j];
            st->mov++;
        }
        a[low] = key;                   // 插入(计1次移动)
        st->mov++;
        if (g_trace && n <= TRACE_MAX)
        {
            cout << "  第" << i << "趟(折半插入" << key << "): ";
            showArr(a, n);
        }
    }
}

/**
 * shellSort —— 3.希尔排序(缩小增量排序)
 * 原理: 按增量 gap 分组做插入排序, 增量逐趟减半到 1。大增量的
 *       "粗排"使序列基本有序, 让最后一趟(gap=1)的插入排序近乎 O(n)。
 * 参数: a 数组首地址;  n 元素个数;  st 统计结构指针
 * 趟数: 每个增量算一趟, 趟数 = 增量序列长度(约 log2(n))
 * 说明: 本程序采用经典增量序列 n/2, n/4, ..., 1。
 */
void shellSort(int a[], int n, SortStats *st)
{
    for (int gap = n / 2; gap > 0; gap /= 2)    // gap —— 本趟的增量
    {
        st->passes++;                   // 每个增量为一趟
        for (int i = gap; i < n; i++)   // 对每个分组做插入排序(各分组交错进行)
        {
            int key = a[i];             // key —— 本组内待插入元素
            st->mov++;
            int j;                      // j —— 组内向前扫描的下标
            for (j = i - gap; j >= 0 && needSwap(a[j], key, st); j -= gap)
            {
                a[j + gap] = a[j];      // 组内后移(计1次移动)
                st->mov++;
            }
            a[j + gap] = key;           // 插入
            st->mov++;
        }
        if (g_trace && n <= TRACE_MAX)
        {
            cout << "  增量gap=" << gap << "一趟后: ";
            showArr(a, n);
        }
    }
}

/**
 * bubbleSort —— 4.冒泡排序(带提前结束标志)
 * 原理: 每一趟从头到尾两两比较相邻元素, 逆序则交换, 一趟过后
 *       最大(小)元素"沉"到末尾; 若某趟没有发生交换说明已有序, 提前结束。
 * 参数: a 数组首地址;  n 元素个数;  st 统计结构指针
 * 趟数: 每次完整扫描为一趟; 完全有序时仅 1 趟(这正是它对有序
 *       序列特别快的原因), 一般不超过 n-1 趟。
 */
void bubbleSort(int a[], int n, SortStats *st)
{
    bool swapped = true;                // swapped —— 上一趟是否发生过交换(先设 true 以进入循环)
    for (int i = 0; i < n - 1 && swapped; i++)  // i —— 已完成的趟数, 也是本趟的扫描终点偏移
    {
        swapped = false;                // 先假设本趟无交换
        st->passes++;                   // 趟数加一
        for (int j = 0; j < n - 1 - i; j++)     // j —— 相邻比较的左元素下标
        {
            if (needSwap(a[j], a[j + 1], st))   // 逆序: 交换
            {
                swapVals(a[j], a[j + 1], st);   // 交换计3次移动
                swapped = true;                 // 标记发生过交换
            }
        }
        if (g_trace && n <= TRACE_MAX)
        {
            cout << "  第" << st->passes << "趟后: ";
            showArr(a, n);
        }
    }
}

/**
 * quickSortCore —— 5.快速排序的递归主体(含"小文件优先"优化)
 * 原理: 每次选一个枢轴把区间划分成"枢轴前都不大于它、枢轴后都不
 *       小于它"两部分, 再对两部分递归。递归时先处理较短的一侧,
 *       较长的一侧用循环继续, 使递归深度保持在 O(logn)。
 * 参数: a 数组首地址;  lo/hi 当前区间;  depthLeft 剩余递归深度限额;
 *       st 统计结构指针
 * 趟数: 每调用一次 partition 记一趟
 * 说明: depthLeft 用尽(划分长期严重失衡, 典型于大量重复关键字时)
 *       时切换为堆排序收尾, 这就是"内省排序( introsort )"思想,
 *       保证最坏情况也接近 O(nlogn)。
 */
void quickSortCore(int a[], int lo, int hi, int depthLeft, SortStats *st)
{
    while (lo < hi)                     // 较长的一侧用循环代替递归
    {
        if (depthLeft <= 0)             // 递归深度超限: 当前区间改用堆排序
        {
            heapSortCore(a + lo, hi - lo + 1, st);  // 只对本区间堆排, 不再新增趟数
            return;
        }
        depthLeft--;                    // 每深入一层消耗一点深度限额
        int p = partition(a, lo, hi, st);   // p —— 枢轴最终位置(一次划分记一趟)
        if (p - lo < hi - p)            // 左侧更短: 先递归左侧
        {
            quickSortCore(a, lo, p - 1, depthLeft, st);
            lo = p + 1;                 // 右侧留到循环里继续
        }
        else                            // 右侧更短: 先递归右侧
        {
            quickSortCore(a, p + 1, hi, depthLeft, st);
            hi = p - 1;                 // 左侧留到循环里继续
        }
    }
}

/**
 * partition —— 快速排序的一次划分(三者取中 + Lomuto 方案)
 * 参数: a 数组首地址;  lo/hi 区间;  st 统计结构指针
 * 返回: 枢轴最终所在下标
 * 说明: 三者取中 = 取 a[lo]、a[mid]、a[hi] 的中位数作枢轴, 可
 *       避免序列本身有序时"首元素作枢轴"导致的 O(n^2) 退化。
 */
int partition(int a[], int lo, int hi, SortStats *st)
{
    if (hi - lo == 1)                   // 区间只有2个元素: 直接比较交换
    {
        if (needSwap(a[lo], a[hi], st))
            swapVals(a[lo], a[hi], st);
        st->passes++;                   // 一次划分记一趟
        return lo;
    }
    // ---- 三者取中: 通过三次比较使 a[lo]<=a[mid]<=a[hi](按当前方向) ----
    int mid = lo + (hi - lo) / 2;       // mid —— 区间中点
    if (needSwap(a[lo], a[mid], st)) swapVals(a[lo], a[mid], st);  // 使 lo 端最小
    if (needSwap(a[lo], a[hi],  st)) swapVals(a[lo], a[hi],  st);
    if (needSwap(a[mid], a[hi], st)) swapVals(a[mid], a[hi], st);  // mid 处即中位数
    swapVals(a[mid], a[hi], st);        // 把中位数换到 hi 位置充当枢轴
    int pivot = a[hi];                  // pivot —— 枢轴值(取出计1次移动)
    st->mov++;
    int i = lo - 1;                     // i —— "较小元素区"的右边界
    for (int j = lo; j < hi; j++)       // j —— 扫描下标
    {
        if (needSwap(pivot, a[j], st))  // a[j] 应排在枢轴前面: 扩充较小元素区
        {
            i++;
            swapVals(a[i], a[j], st);
        }
    }
    swapVals(a[i + 1], a[hi], st);      // 枢轴归位到较小区与较大区的交界处
    st->passes++;                       // 一次划分记一趟
    if (g_trace && hi - lo + 1 <= TRACE_MAX)    // 演示模式: 打印本次划分后的区间
    {
        cout << "  枢轴" << pivot << "归位于[" << lo << "," << hi << "]内下标"
             << (i + 1) << "后: ";
        showArr(a + lo, hi - lo + 1);   // 只显示本区间, 便于看清划分效果
    }
    return i + 1;                       // 返回枢轴位置
}

/**
 * quickSort —— 5.快速排序(对外入口)
 * 参数: a 数组首地址;  n 元素个数;  st 统计结构指针
 * 说明: 递归深度限额取 2*log2(n)+10 —— 正常划分用不完, 只有
 *       划分长期严重失衡时才会触发堆排序收尾。
 */
void quickSort(int a[], int n, SortStats *st)
{
    if (n < 2) return;                  // 0/1 个元素天然有序
    int depthLimit = 2 * (int)(log((double)n) / log(2.0)) + 10; // 深度限额
    quickSortCore(a, 0, n - 1, depthLimit, st);
}

/**
 * selectSort —— 6.简单选择排序
 * 原理: 第 i 趟从 a[i..n-1] 中选出最值与 a[i] 交换。比较次数固定
 *       为 n(n-1)/2, 但每趟最多交换一次, 全程移动仅 O(n) 次 ——
 *       这一特性使它在"逆序序列"上移动次数最少。
 * 参数: a 数组首地址;  n 元素个数;  st 统计结构指针
 * 趟数: 每选出一个最值为一趟, 共 n-1 趟
 */
void selectSort(int a[], int n, SortStats *st)
{
    for (int i = 0; i < n - 1; i++)     // i —— 本趟待安放的位置
    {
        st->passes++;                   // 趟数加一
        int best = i;                   // best —— 当前最值元素下标
        for (int j = i + 1; j < n; j++) // j —— 扫描下标
            if (needSwap(a[best], a[j], st))    // a[j] 比当前最值更优
                best = j;                       // 记录更优下标
        if (best != i)                  // 最值不在本趟起点才需要交换
            swapVals(a[i], a[best], st);
        if (g_trace && n <= TRACE_MAX)
        {
            cout << "  第" << st->passes << "趟后: ";
            showArr(a, n);
        }
    }
}

/**
 * siftDown —— 堆排序的下沉筛选(在 b[0..end] 上把 b[root] 下沉到位)
 * 参数: b 数组首地址(可指向某区间起点);  root 待下沉的堆顶下标;
 *       end 本轮堆的末尾下标;  st 统计结构指针
 * 说明: 升序用"大顶堆"(堆顶最大, 取出放到末尾), 降序用"小顶堆",
 *       方向差异同样由 needSwap 统一处理。
 */
void siftDown(int b[], int root, int end, SortStats *st)
{
    while (root * 2 + 1 <= end)         // 只要左孩子存在就需要比较
    {
        int child = root * 2 + 1;       // child —— 左孩子下标
        if (child + 1 <= end && needSwap(b[child + 1], b[child], st))
            child++;                    // 右孩子比左孩子更"优"(升序时更大)则选右孩子
        if (needSwap(b[child], b[root], st))    // 孩子比父亲更"优": 交换下沉
        {
            swapVals(b[root], b[child], st);
            root = child;               // 继续从孩子位置向下筛
        }
        else break;                     // 父亲已不劣于孩子: 堆性质满足, 结束
    }
}

/**
 * heapSortCore —— 堆排序主体(建堆 + 逐个出堆)
 * 参数: b 数组首地址;  n 元素个数;  st 统计结构指针
 * 说明: 供 heapSort(整体)与快速排序的失衡收尾(区间)共用,
 *       因此本函数不统计趟数, 由调用方按自己的口径记录。
 */
void heapSortCore(int b[], int n, SortStats *st)
{
    for (int i = n / 2 - 1; i >= 0; i--)    // i —— 最后一个非叶子结点起向前建堆
        siftDown(b, i, n - 1, st);          // 从后往前逐个下沉, 完成建堆
    if (g_trace && n <= TRACE_MAX)
    {
        cout << "  建堆完成后: ";
        showArr(b, n);
    }
    for (int end = n - 1; end >= 1; end--)  // end —— 当前堆的末尾下标
    {
        swapVals(b[0], b[end], st);     // 堆顶(当前最值)与堆尾交换, 最值就位
        siftDown(b, 0, end - 1, st);    // 剩余部分重新调整成堆
        if (g_trace && n <= TRACE_MAX)
        {
            cout << "  出堆1次后(末尾" << (n - end) << "个已就位): ";
            showArr(b, end);            // 只显示还未就位的前缀
        }
    }
}

/**
 * heapSort —— 6+.堆排序(对外入口)
 * 参数: a 数组首地址;  n 元素个数;  st 统计结构指针
 * 趟数: 建堆记 1 趟 + 每次出堆记 1 趟, 共 n 趟
 */
void heapSort(int a[], int n, SortStats *st)
{
    if (n < 2) return;
    heapSortCore(a, n, st);             // 完整排序
    st->passes = n;                     // 趟数口径: 1(建堆) + n-1(出堆)
}

/**
 * heapSortRange —— 对区间 [lo, hi] 做堆排序(供快速排序失衡时收尾)
 * 参数: a 数组首地址;  lo/hi 区间端点;  st 统计结构指针
 */
void heapSortRange(int a[], int lo, int hi, SortStats *st)
{
    heapSortCore(a + lo, hi - lo + 1, st);  // 把区间起点当作数组首地址复用主体
}

/**
 * mergeRange —— 把两个各自有序的相邻区间 a[lo..mid] 与 a[mid+1..hi]
 *                归并成一个有序区间(借助辅助数组 tmp)
 * 参数: a 原数组;  lo/mid/hi 区间端点;  tmp 辅助数组;  st 统计结构指针
 * 说明: 相等时优先取左区间元素, 保证了归并排序的稳定性。
 */
void mergeRange(int a[], int lo, int mid, int hi, int tmp[], SortStats *st)
{
    int i = lo;                         // i —— 左区间扫描下标
    int j = mid + 1;                    // j —— 右区间扫描下标
    int k = lo;                         // k —— 辅助数组的写入下标
    while (i <= mid && j <= hi)         // 两区间都还有剩余时, 取较小者
    {
        if (needSwap(a[i], a[j], st))   // 左边更大(方向相关): 取右边
            tmp[k++] = a[j++];
        else                            // 左边不大于右边: 取左边(相等取左, 保证稳定)
            tmp[k++] = a[i++];
        st->mov++;                      // 每写入一个元素计1次移动
    }
    while (i <= mid)  { tmp[k++] = a[i++]; st->mov++; } // 左区间剩余直接搬入
    while (j <= hi)   { tmp[k++] = a[j++]; st->mov++; } // 右区间剩余直接搬入
    for (k = lo; k <= hi; k++)          // 把归并结果写回原数组
    {
        a[k] = tmp[k];
        st->mov++;
    }
}

/**
 * mergeSort —— 7+.二路归并排序(自底向上非递归实现)
 * 原理: 初始把每个元素看作长度 1 的有序段, 每一轮把相邻有序段
 *       两两归并使段长翻倍, 直到整个数组有序。
 * 参数: a 数组首地址;  n 元素个数;  st 统计结构指针
 * 趟数: 每一轮整体归并为一趟, 共 ceil(log2(n)) 趟
 * 说明: 采用非递归写法的好处是趟数概念与教材"第 len 趟归并"完全
 *       一致, 且不存在递归深度问题; 需要额外 O(n) 辅助空间。
 */
void mergeSort(int a[], int n, SortStats *st)
{
    if (n < 2) return;                  // 0/1 个元素无需归并
    vector<int> tmpv(n);                // tmpv —— 辅助数组(一次申请反复使用)
    int *tmp = &tmpv[0];                // tmp —— 首地址
    for (int len = 1; len < n; len *= 2)    // len —— 当前有序段长度, 逐轮翻倍
    {
        st->passes++;                   // 每一轮整体归并为一趟
        for (int lo = 0; lo < n - len; lo += 2 * len)   // lo —— 本段归并的起点
        {
            int mid = lo + len - 1;                         // 左段末尾
            int hi = lo + 2 * len - 1;                      // 右段末尾(可能越界)
            if (hi > n - 1) hi = n - 1;                     // 越界则截到数组末尾
            mergeRange(a, lo, mid, hi, tmp, st);
        }
        if (g_trace && n <= TRACE_MAX)
        {
            cout << "  段长" << len << "的归并轮完成后: ";
            showArr(a, n);
        }
    }
}

// 排序函数指针表: 下标与 ALGO_NAME / ALGO_SIMPLE 一一对应
typedef void (*SortFunc)(int a[], int n, SortStats *st);
const SortFunc ALGO_FUNC[ALGO_NUM] = {
    insertSort, binaryInsertSort, shellSort, bubbleSort,
    quickSort,  selectSort,       heapSort,  mergeSort
};

/*===================================================================*/
/*               六、实验与报表模块(菜单7/8/9/10)                      */
/*        对应功能要求(2)(3)(4): 排序/统计/生成报表存盘                */
/*===================================================================*/

/**
 * runExperiment —— 对同一批数据运行全部 8 种算法并统计
 * 参数: data    参加实验的数据
 *       results 输出的统计数组(下标与算法表一致)
 * 返回: 无
 * 说明: 每个算法都作用在数据的一份"拷贝"上, 保证大家拿到的是
 *       完全相同的初始序列, 统计才公平。计时用 clock(), 精度约
 *       毫秒级, 极小数据测得 0ms 属正常现象。
 */
void runExperiment(const vector<int> &data, SortStats results[])
{
    int n = (int)data.size();           // n —— 数据规模
    for (int idx = 0; idx < ALGO_NUM; idx++)    // idx —— 当前算法编号
    {
        results[idx].reset(ALGO_NAME[idx]);     // 复位该算法的统计
        if (n == 0)                     // 空数据: 全部标记跳过
        {
            results[idx].skipped = true;
            continue;
        }
        if (ALGO_SIMPLE[idx] && n > SIMPLE_LIMIT)   // O(n^2)算法遇超大数据
        {
            results[idx].skipped = true;            // 标记跳过, 报表中注明
            continue;
        }
        vector<int> buf(data);          // buf —— 数据拷贝, 各算法互不干扰
        int *a = &buf[0];               // a —— 拷贝的首地址
        clock_t t1 = clock();           // 开始计时
        ALGO_FUNC[idx](a, n, &results[idx]);    // 执行排序
        clock_t t2 = clock();           // 结束计时
        results[idx].timeMs = (double)(t2 - t1) * 1000.0 / CLOCKS_PER_SEC; // 换算毫秒
        results[idx].ok = isSorted(a, n, g_desc);   // 自检结果是否有序
    }
}

/**
 * printResultTable —— 把一组统计结果打印成屏幕表格
 * 参数: rs 统计数组;  n 数据规模;  desc 数据描述
 */
void printResultTable(const SortStats rs[], int n, const string &desc)
{
    cout << "\n===================== 排序结果统计表 =====================" << endl;
    cout << " 数据: " << desc << "   规模: " << n << " 个   方向: "
         << (g_desc ? "由大到小(降序)" : "由小到大(升序)") << endl;
    cout << "-----------------------------------------------------------" << endl;
    cout << " 序号 算法\t\t时间(ms)\t比较次数\t移动次数\t趟数\t状态" << endl;
    for (int i = 0; i < ALGO_NUM; i++)  // 逐行打印各算法
    {
        cout << " " << i + 1 << "    " << rs[i].name << "\t";
        if (rs[i].skipped)              // 被跳过的算法
        {
            cout << " -\t-\t-\t-\t跳过(规模过大)" << endl;
            continue;
        }
        cout << fixed << setprecision(3) << rs[i].timeMs << "\t"  // 时间保留3位小数
             << rs[i].cmp << "\t" << rs[i].mov << "\t" << rs[i].passes << "\t"
             << (rs[i].ok ? "正确" : "错误!") << endl;
    }
    cout << "===========================================================" << endl;
}

/**
 * fastestIndex —— 找出统计表中用时最短且结果正确的算法下标
 * 参数: rs 统计数组
 * 返回: 最快算法下标; 全部无效时返回 -1
 */
int fastestIndex(const SortStats rs[])
{
    int best = -1;                      // best —— 当前最快算法下标
    for (int i = 0; i < ALGO_NUM; i++)
    {
        if (rs[i].skipped || !rs[i].ok) continue;   // 跳过/错误的算法不参与
        if (best < 0 || rs[i].timeMs < rs[best].timeMs)
            best = i;                   // 更快则刷新记录
    }
    return best;
}

/**
 * runAllNow —— 菜单7: 用当前数据运行全部算法并显示统计表
 */
void runAllNow()
{
    if (!g_hasData)
    {
        cout << "  当前没有数据, 请先用菜单 1/2/3 准备数据!" << endl;
        return;
    }
    runExperiment(g_data, g_results);   // 正式实验(结果存入全局 g_results)
    g_hasResults = true;                // 标记结果有效
    g_resultDesc = g_dataDesc;          // 记录结果对应的数据描述
    g_resultN    = (int)g_data.size();
    printResultTable(g_results, g_resultN, g_resultDesc);
}

/**
 * runSingleNow —— 菜单8: 运行指定的单个算法并显示其统计
 * 参数: idx 算法编号(0 起)
 */
void runSingleNow(int idx)
{
    int n = (int)g_data.size();
    SortStats st;                       // st —— 本次运行的统计
    st.reset(ALGO_NAME[idx]);
    if (ALGO_SIMPLE[idx] && n > SIMPLE_LIMIT)   // O(n^2)算法遇超大数据
    {
        cout << "  " << ALGO_NAME[idx] << "为 O(n^2) 算法, 当前规模 " << n
             << " 过大已跳过(上限 " << SIMPLE_LIMIT << "), 可换小数据或看 O(nlogn) 算法。" << endl;
        return;
    }
    vector<int> buf(g_data);            // 在拷贝上排序
    int *a = &buf[0];
    clock_t t1 = clock();
    ALGO_FUNC[idx](a, n, &st);
    clock_t t2 = clock();
    st.timeMs = (double)(t2 - t1) * 1000.0 / CLOCKS_PER_SEC;
    st.ok = isSorted(a, n, g_desc);
    cout << "  " << ALGO_NAME[idx] << ": 用时" << fixed << setprecision(3) << st.timeMs
         << "ms, 比较" << st.cmp << "次, 移动" << st.mov << "次, 趟数" << st.passes
         << ", 结果" << (st.ok ? "正确" : "错误!") << endl;
}

/**
 * demoOneAlgo —— 菜单9: 逐趟演示某算法的排序过程(建议小数据)
 * 参数: idx 算法编号(0 起)
 * 说明: 数据规模超过 TRACE_MAX 时自动取前 TRACE_MAX 个演示,
 *       因为过程演示只关心"每一趟长什么样", 与规模无关。
 */
void demoOneAlgo(int idx)
{
    vector<int> demo;                   // demo —— 参加演示的数据
    int n = (int)g_data.size();
    if (n > TRACE_MAX)                  // 数据太大: 取前 TRACE_MAX 个
    {
        demo.assign(g_data.begin(), g_data.begin() + TRACE_MAX);
        cout << "  当前数据较大, 取前 " << TRACE_MAX << " 个用于过程演示。" << endl;
    }
    else
        demo = g_data;
    SortStats st;                       // st —— 演示用的统计(顺带输出)
    st.reset(ALGO_NAME[idx]);
    g_trace = true;                     // 打开逐趟打印开关
    cout << "\n--- " << ALGO_NAME[idx] << " 逐趟演示 (方向: "
         << (g_desc ? "降序" : "升序") << ") ---" << endl;
    cout << "  初始: ";
    showArr(&demo[0], (int)demo.size());
    vector<int> buf(demo);              // 在拷贝上演示
    ALGO_FUNC[idx](&buf[0], (int)buf.size(), &st);
    g_trace = false;                    // 关闭演示开关
    cout << "  最终: ";
    showArr(&buf[0], (int)buf.size());
    cout << "  统计: 比较" << st.cmp << "次, 移动" << st.mov
         << "次, 共" << st.passes << "趟" << endl;
}

/**
 * saveReportFiles —— 菜单10: 生成报表并存盘
 * 文件一 sort_report.txt : 覆盖写, 保存最近一次实验的完整表格;
 * 文件二 sort_report.csv : 追加写, 每次实验追加一行/算法, 累积成
 *                          历史报表, 便于用 Excel 画图对比分析。
 */
void saveReportFiles()
{
    if (!g_hasResults)                  // 还没有实验结果
    {
        cout << "  尚无实验结果, 请先执行菜单 7 运行全部算法!" << endl;
        return;
    }
    // ---------- 文本报表(覆盖写) ----------
    ofstream fout(REPORT_TXT);          // fout —— 文本报表输出流
    if (!fout)
    {
        cout << "  错误: 无法创建 " << REPORT_TXT << " !" << endl;
        return;
    }
    fout << "排序算法性能比较报表" << endl;
    fout << "生成时间: " << __DATE__ << " " << __TIME__ << endl;   // 编译宏记录时间
    fout << "数据描述: " << g_resultDesc << "    规模: " << g_resultN << " 个" << endl;
    fout << "排序方向: " << (g_desc ? "由大到小(降序)" : "由小到大(升序)") << endl;
    fout << "----------------------------------------------------------------" << endl;
    fout << "序号\t算法\t\t时间(ms)\t比较次数\t移动次数\t趟数\t状态" << endl;
    for (int i = 0; i < ALGO_NUM; i++)
    {
        fout << i + 1 << "\t" << g_results[i].name << "\t";
        if (g_results[i].skipped) { fout << "-\t-\t-\t-\t跳过(规模过大)" << endl; continue; }
        fout << fixed << setprecision(3) << g_results[i].timeMs << "\t"
             << g_results[i].cmp << "\t" << g_results[i].mov << "\t"
             << g_results[i].passes << "\t" << (g_results[i].ok ? "正确" : "错误") << endl;
    }
    fout.close();
    // ---------- CSV 历史报表(追加写) ----------
    bool needHeader = true;             // needHeader —— CSV 是否需要先写表头
    ifstream fin(REPORT_CSV);           // 检查文件是否已存在
    if (fin) { needHeader = false; fin.close(); }
    ofstream fc(REPORT_CSV, ios::app);  // fc —— CSV 输出流(追加方式)
    if (!fc)
    {
        cout << "  错误: 无法创建 " << REPORT_CSV << " !" << endl;
        return;
    }
    if (needHeader)                     // 首次创建时写列名
        fc << "实验时间,数据描述,规模,方向,算法,时间ms,比较次数,移动次数,趟数,状态" << endl;
    for (int i = 0; i < ALGO_NUM; i++)
    {
        fc << __DATE__ << " " << __TIME__ << "," << g_resultDesc << "," << g_resultN << ","
           << (g_desc ? "降序" : "升序") << "," << g_results[i].name << ",";
        if (g_results[i].skipped) fc << ",,,,,跳过(规模过大)" << endl;
        else fc << fixed << setprecision(3) << g_results[i].timeMs << ","
                << g_results[i].cmp << "," << g_results[i].mov << ","
                << g_results[i].passes << "," << (g_results[i].ok ? "正确" : "错误") << endl;
    }
    fc.close();
    cout << "  报表已保存: " << REPORT_TXT << "(本次) / " << REPORT_CSV << "(历史累积)" << endl;
}

/*===================================================================*/
/*            七、分析与验证模块(菜单11/12/13)                         */
/*        对应功能要求(5)(6)与选做要求: 结论、验证、存盘               */
/*===================================================================*/

/**
 * dataFeatures —— 提取数据集的两个关键特征
 * 参数: d          待分析的数据
 *       inv        输出: 相邻"逆序对"个数(按当前方向)
 *       distinct   输出: 不同取值的个数
 *       invRatio   输出: 逆序对占全部相邻对的比例(0~1), 反映有序程度
 * 说明: 相邻逆序对比例接近 0 => 基本有序; 等于 n-1 => 完全逆序;
 *       不同取值很少 => 大量重复。这两个特征直接决定算法选择。
 */
void dataFeatures(const vector<int> &d, long long &inv, int &distinct, double &invRatio)
{
    int n = (int)d.size();              // n —— 数据规模
    inv = 0; distinct = 0; invRatio = 0.0;
    if (n == 0) return;
    for (int i = 0; i + 1 < n; i++)     // 统计相邻逆序对
        if (!inOrder(d[i], d[i + 1])) inv++;
    invRatio = (n > 1) ? (double)inv / (n - 1) : 0.0;
    vector<int> s(d);                   // s —— 拷贝一份用于统计不同取值
    sort(s.begin(), s.end());           // 借助标准库排序(仅做特征统计, 不参与计时实验)
    distinct = 1;                       // 至少有 1 个不同值
    for (int i = 1; i < n; i++)         // 排序后相邻不同即新取值
        if (s[i] != s[i - 1]) distinct++;
}

/**
 * predictBest —— 规则推理: 根据数据特征预测最适合的算法(理论建议)
 * 参数: d        待分析的数据
 *       names    输出: 推荐算法名单(按优先级, 最多 3 个)
 *       reason   输出: 推荐理由的文字说明
 * 说明: 规则按优先级排列: 小规模 > 完全有序 > 完全逆序 >
 *       基本有序 > 大量重复 > 随机中大规模。这些规则正是本课题
 *       要得出的"结论性建议"。
 */
void predictBest(const vector<int> &d, vector<string> &names, string &reason)
{
    names.clear();                      // 清空输出
    reason.clear();
    int n = (int)d.size();              // n —— 数据规模
    if (n == 0) return;
    long long inv;                      // inv —— 相邻逆序对数
    int distinct;                       // distinct —— 不同取值个数
    double invRatio;                    // invRatio —— 逆序比例
    dataFeatures(d, inv, distinct, invRatio);
    ostringstream os;                   // os —— 用于拼接理由文字
    if (n <= 30)                        // 规则1: 小规模数据
    {
        names.push_back("直接插入排序");
        names.push_back("冒泡排序");
        os << "数据规模很小(n<=30), 各算法的主导因素是常数开销而非渐近复杂度: "
           << "直接插入排序无递归、无辅助空间、内层最紧凑, 通常最快; "
           << "冒泡排序带提前结束标志, 有序时一趟完成。";
    }
    else if (inv == 0)                  // 规则2: 完全有序
    {
        names.push_back("冒泡排序");
        names.push_back("直接插入排序");
        os << "序列完全有序: 冒泡排序比较 n-1 次、移动 0 次后即因无交换而提前结束, 时间 O(n); "
           << "直接插入排序同样只比较 n-1 次、移动 0 次。"
           << "注意: 若采用'首元素作枢轴'的朴素快速排序会退化到 O(n^2), 本程序用三者取中避免了该退化。";
    }
    else if (inv == n - 1 && n > 1)     // 规则3: 完全逆序
    {
        names.push_back("简单选择排序");
        names.push_back("堆排序");
        names.push_back("二路归并排序");
        os << "序列完全逆序: 冒泡需要 n-1 趟、插入的移动次数达 O(n^2), 都很不利; "
           << "简单选择排序的比较次数固定为 n(n-1)/2, 但移动仅 O(n) 次, 移动次数最少; "
           << "堆排序/归并排序/三者取中快速排序保持 O(nlogn), 综合更快。";
    }
    else if (invRatio < 0.05)           // 规则4: 基本有序
    {
        names.push_back("直接插入排序");
        names.push_back("冒泡排序");
        os << "序列基本有序(乱序比例<5%): 每个元素离最终位置都很近, "
           << "直接插入排序的比较与移动都很少, 接近 O(n), 通常最优; 冒泡排序趟数少, 次优。";
    }
    else if ((double)distinct * 5 < n)  // 规则5: 大量重复(不同值不足20%)
    {
        names.push_back("二路归并排序");
        names.push_back("堆排序");
        os << "数据大量重复(不同取值不足20%): 标准二路划分的快速排序在大量相等关键字时"
           << "划分严重失衡, 接近 O(n^2)(扩展方向: 三路快排可解决); "
           << "二路归并排序不受重复影响、性能稳定, 堆排序次之。";
    }
    else                                // 规则6: 随机/明显乱序的中大规模数据
    {
        names.push_back("快速排序");
        names.push_back("堆排序");
        names.push_back("二路归并排序");
        os << "随机或明显乱序的中大规模数据: 快速排序平均常数最小、缓存友好, 平均性能最佳; "
           << "堆排序与归并排序最坏情况仍为 O(nlogn), 更有保障(若要求稳定排序应选归并)。";
    }
    if (g_desc) os << "(降序方向不影响结论: 各算法经方向参数适配后复杂度不变)";
    reason = os.str();
}

/**
 * buildAnalysisText —— 生成完整的分析结论文本(屏幕显示与存盘共用)
 * 参数: rs       最近一次实验的统计(可为 NULL 表示尚无实测数据)
 *       rsDesc   该实验对应的数据描述
 * 返回: 多行结论文本
 */
string buildAnalysisText(const SortStats *rs, const string &rsDesc)
{
    ostringstream os;                   // os —— 逐行拼接结论
    int n = (int)g_data.size();         // n —— 当前数据规模
    os << "================ 数据特征分析报告 ================" << endl;
    os << "数据描述: " << g_dataDesc << "    规模: " << n
       << " 个    排序方向: " << (g_desc ? "由大到小(降序)" : "由小到大(升序)") << endl;
    if (n == 0)
    {
        os << "(当前没有数据, 请先用菜单 1/2/3 准备数据)" << endl;
        return os.str();
    }
    long long inv;                      // inv —— 相邻逆序对数
    int distinct;                       // distinct —— 不同取值个数
    double invRatio;                    // invRatio —— 逆序比例
    dataFeatures(g_data, inv, distinct, invRatio);
    // ---- 第一部分: 数据特征 ----
    os << "相邻逆序对: " << inv << " / " << (n > 1 ? n - 1 : 0) << " ("
       << fixed << setprecision(1) << invRatio * 100 << "%)" << endl;
    string orderType;                   // orderType —— 序列形态的文字归类
    if (inv == 0)              orderType = "完全有序";
    else if (inv == n - 1)     orderType = "完全逆序";
    else if (invRatio < 0.05)  orderType = "基本有序(乱序<5%)";
    else if (invRatio < 0.30)  orderType = "轻度乱序";
    else                       orderType = "明显乱序(接近随机)";
    os << "序列形态: " << orderType << endl;
    os << "不同取值: " << distinct << " 个 (重复率 "
       << fixed << setprecision(1) << (1.0 - (double)distinct / n) * 100 << "%)" << endl;
    // ---- 第二部分: 理论建议 ----
    vector<string> names;               // names —— 推荐算法名单
    string reason;                      // reason —— 推荐理由
    predictBest(g_data, names, reason);
    os << "---------------- 理论分析与建议 ----------------" << endl;
    for (size_t i = 0; i < names.size(); i++)   // 打印推荐名单
        os << "建议" << (i + 1) << ": " << names[i] << endl;
    os << "理由: " << reason << endl;
    os << "稳定性提示: 归并/直接插入/冒泡为稳定排序, 其余为不稳定排序, "
       << "若对相等元素的先后次序有要求, 请在推荐中优先选择稳定算法。" << endl;
    // ---- 第三部分: 实测对照(若已有实验结果) ----
    if (rs != NULL)
    {
        os << "---------------- 实测结果对照 ----------------" << endl;
        os << "实测数据: " << rsDesc << endl;
        for (int i = 0; i < ALGO_NUM; i++)      // 打印每个算法的实测用时
        {
            if (rs[i].skipped) continue;        // 跳过的不参与对照
            os << "  " << rs[i].name << ": " << fixed << setprecision(3)
               << rs[i].timeMs << " ms" << endl;
        }
        int f = fastestIndex(rs);               // f —— 实测最快算法下标
        if (f >= 0)
        {
            os << "实测最快: " << rs[f].name << " (" << fixed << setprecision(3)
               << rs[f].timeMs << " ms)" << endl;
            bool hit = false;                   // hit —— 实测最快是否在理论建议名单中
            for (size_t i = 0; i < names.size(); i++)
                if (names[i] == rs[f].name) hit = true;
            if (hit)
                os << "结论验证: 实测结果与理论建议一致。" << endl;
            else
                os << "结论验证: 实测与理论建议不完全一致 —— "
                   << "可能因为规模处于临界区间、常数因子差异或编译器优化不同, "
                   << "请结合比较/移动次数等统计综合分析。" << endl;
        }
    }
    os << "================================================" << endl;
    return os.str();
}

/**
 * analyzeNow —— 菜单11: 分析当前数据特征并给出结论(若有实测则对照)
 */
void analyzeNow()
{
    if (!g_hasData)
    {
        cout << "  当前没有数据, 请先用菜单 1/2/3 准备数据!" << endl;
        return;
    }
    cout << buildAnalysisText(g_hasResults ? g_results : NULL, g_resultDesc);
}

/**
 * saveConclusion —— 菜单12(选做): 把结论文本存档到磁盘
 */
void saveConclusion()
{
    if (!g_hasData)
    {
        cout << "  当前没有数据, 请先用菜单 1/2/3 准备数据!" << endl;
        return;
    }
    ofstream fout(CONCLUSION_FILE);     // fout —— 结论文件输出流
    if (!fout)
    {
        cout << "  错误: 无法创建 " << CONCLUSION_FILE << " !" << endl;
        return;
    }
    fout << buildAnalysisText(g_hasResults ? g_results : NULL, g_resultDesc);
    fout.close();
    cout << "  结论已存档到 " << CONCLUSION_FILE << endl;
}

/**
 * readDataFromKeyboard —— 把用户逐行输入的整数收集成数据集
 * 参数: v 输出: 收集到的数据
 * 说明: 每行可含多个整数(空格分隔), 输入空行结束。
 */
void readDataFromKeyboard(vector<int> &v)
{
    cout << "  请逐行输入整数(每行可多个, 空格分隔), 输入空行结束:" << endl;
    string line;                        // line —— 用户输入的一行
    while (getline(cin, line))          // 逐行读取
    {
        if (trimStr(line).empty()) break;   // 空行: 输入结束
        istringstream ss(line);         // ss —— 行内解析
        int x;                          // x —— 解析出的整数
        while (ss >> x) v.push_back(x);
    }
}

/**
 * verifyConclusion —— 菜单13: 用任意测试数据集验证结论
 * 流程: 取一组新数据 -> 立即实测全部算法 -> 找出实测最快 -> 与
 *       规则推理的理论建议对照 -> 输出"一致/不一致"及分析。
 */
void verifyConclusion()
{
    cout << "\n----- 用任意数据验证结论 -----" << endl;
    int src = readInt("  数据来源 (1)键盘输入 (2)文本文件: ", 1, 2);
    vector<int> v;                      // v —— 验证用数据集
    if (src == 1)
    {
        readDataFromKeyboard(v);
    }
    else
    {
        string fn = readLine("  输入文件名(直接回车默认 data.txt): ");
        if (trimStr(fn).empty()) fn = DATA_FILE;    // 空输入用默认文件
        ifstream fin(fn.c_str());       // fin —— 验证数据文件流
        if (!fin)
        {
            cout << "  错误: 无法打开文件 " << fn << " !" << endl;
            return;
        }
        int x;                          // x —— 从文件读到的整数
        while (fin >> x) v.push_back(x);
        fin.close();
    }
    if (v.empty())                      // 没收集到数据
    {
        cout << "  未读到有效数据, 验证取消!" << endl;
        return;
    }
    cout << "  验证数据 " << v.size() << " 个: ";
    showArr(&v[0], (int)v.size());
    SortStats rs[ALGO_NUM];             // rs —— 本次验证的统计(独立于全局结果)
    runExperiment(v, rs);               // 立即实测
    printResultTable(rs, (int)v.size(), "验证数据(键盘/文件)");
    int f = fastestIndex(rs);           // f —— 实测最快算法
    if (f < 0)
    {
        cout << "  无有效实测结果, 验证取消!" << endl;
        return;
    }
    vector<string> names;               // names —— 理论建议名单
    string reason;                      // reason —— 理论理由
    predictBest(v, names, reason);
    cout << "  理论建议: ";
    for (size_t i = 0; i < names.size(); i++)
        cout << (i ? "、" : "") << names[i];
    cout << endl << "  理论理由: " << reason << endl;
    bool hit = false;                   // hit —— 理论建议是否命中实测最快
    for (size_t i = 0; i < names.size(); i++)
        if (names[i] == rs[f].name) hit = true;
    cout << "  实测最快: " << rs[f].name << " (" << fixed << setprecision(3)
         << rs[f].timeMs << " ms)" << endl;
    cout << "  验证结论: " << (hit ? "实测与理论建议【一致】, 结论可信!"
                                        : "实测与理论建议【不一致】, 请结合统计次数分析原因。") << endl;
}

/*===================================================================*/
/*                        八、主控模块(菜单)                           */
/*===================================================================*/

/**
 * showMenu —— 打印主菜单
 */
void showMenu()
{
    cout << "\n+--------------------------------------------------+" << endl;
    cout << "|        看谁排得快 —— 排序算法性能比较系统          |" << endl;
    cout << "+--------------------------------------------------+" << endl;
    cout << " 1. 从文件读入原始数据          8. 运行指定的单个算法" << endl;
    cout << " 2. 产生随机数据                9. 逐趟演示某算法过程" << endl;
    cout << " 3. 产生有规律数据              10. 运行全部算法并生成报表存盘" << endl;
    cout << " 4. 显示当前数据                11. 分析数据特征并给出结论" << endl;
    cout << " 5. 把当前数据保存到文件        12. 把结论存档到文件(选做)" << endl;
    cout << " 6. 选择排序方向(升/降序)       13. 用任意测试数据验证结论" << endl;
    cout << " 7. 运行全部排序算法(只统计)    0. 退出程序" << endl;
}

/**
 * main —— 程序入口: 初始化随机种子, 进入菜单循环
 * 返回: 0 表示正常结束
 */
int main()
{
    srand((unsigned)time(NULL));        // 用当前时间作随机种子(只设一次)
    cout << "====================================================" << endl;
    cout << "   欢迎使用《看谁排得快》排序算法性能比较系统!" << endl;
    cout << "   8 种算法 x 多种数据形态, 统计时间/比较/移动/趟数," << endl;
    cout << "   自动分析并给出\"怎样的序列用怎样的方法最快\"的建议。" << endl;
    cout << "   提示: 请先用菜单 1/2/3 准备数据, 再执行 7~13。" << endl;
    cout << "====================================================" << endl;
    while (true)                        // 主循环: 显示菜单 -> 读选项 -> 分派
    {
        showMenu();
        int op = readInt("请输入菜单编号(0-13): ", 0, 13);  // op —— 菜单选项
        switch (op)
        {
            case 0:                     // 退出
                cout << "感谢使用, 再见!" << endl;
                return 0;
            case 1:                     // 从文件读入
            {
                string fn = readLine("输入数据文件名(直接回车默认 data.txt): ");
                if (trimStr(fn).empty()) fn = DATA_FILE;
                loadFromFile(fn);
                break;
            }
            case 2:                     // 随机数据
            {
                int n = readInt("输入数据个数 n(1-500000): ", 1, N_MAX);
                genRandom(n);
                break;
            }
            case 3:                     // 规律数据
            {
                int kind = readInt("选择形态 (1)升序有序 (2)逆序 (3)基本有序 (4)少量重复值: ", 1, 4);
                int n = readInt("输入数据个数 n(1-500000): ", 1, N_MAX);
                genPattern(kind, n);
                break;
            }
            case 4:                     // 显示数据
                showCurrentData();
                break;
            case 5:                     // 保存数据
            {
                string fn = readLine("输入保存文件名(直接回车默认 data.txt): ");
                if (trimStr(fn).empty()) fn = DATA_FILE;
                saveDataToFile(fn);
                break;
            }
            case 6:                     // 选择方向
            {
                int d = readInt("选择方向 (1)由小到大-升序 (2)由大到小-降序: ", 1, 2);
                g_desc = (d == 2);      // 更新全局方向
                g_hasResults = false;   // 方向变了, 旧结果作废
                cout << "  排序方向已设为: " << (g_desc ? "由大到小(降序)" : "由小到大(升序)") << endl;
                break;
            }
            case 7:                     // 运行全部(只统计)
                runAllNow();
                break;
            case 8:                     // 运行单个算法
            {
                if (!g_hasData) { cout << "  当前没有数据, 请先准备数据!" << endl; break; }
                int idx = readInt("输入算法编号(1-8, 见菜单说明): ", 1, ALGO_NUM) - 1;
                runSingleNow(idx);
                break;
            }
            case 9:                     // 逐趟演示
            {
                if (!g_hasData) { cout << "  当前没有数据, 请先准备数据!" << endl; break; }
                int idx = readInt("输入要演示的算法编号(1-8): ", 1, ALGO_NUM) - 1;
                demoOneAlgo(idx);
                break;
            }
            case 10:                    // 运行+报表存盘
                if (!g_hasData) { cout << "  当前没有数据, 请先准备数据!" << endl; break; }
                if (!g_hasResults) runAllNow();     // 没跑过就先跑
                else printResultTable(g_results, g_resultN, g_resultDesc);
                saveReportFiles();
                break;
            case 11:                    // 分析结论
                analyzeNow();
                break;
            case 12:                    // 结论存档
                saveConclusion();
                break;
            case 13:                    // 任意数据验证
                verifyConclusion();
                break;
            default:                    // 理论上到不了这里(readInt 已限定范围)
                break;
        }
    }
}
