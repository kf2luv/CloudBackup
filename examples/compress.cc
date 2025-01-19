#include <iostream>
#include <fstream>
#include <memory.h>
#include <ctime>
#include "../include/bundle.h"

/**
 * 函数功能：显示程序使用帮助信息
 * 输入参数：无
 * 输出参数：无
 * 返回值：无
 * 调用关系：此函数在命令行参数不正确时被调用
 */
void usage()
{
    std::cout << "-------- USAGE --------" << std::endl;
    std::cout << "./compress path target" << std::endl;
}

/**
 * 函数功能：主函数，处理文件压缩操作
 * 输入参数：argc - 命令行参数个数，argv - 命令行参数数组
 * 输出参数：无
 * 返回值：整数，程序的退出状态码，0表示成功，非0表示失败
 * 调用关系：调用了usage()函数，当命令行参数不正确时
 */
int main(int argc, char *argv[])
{
    // argv
    // 第一个参数：将要压缩的文件路径
    // 第二个参数：压缩包名称
    if (argc != 3)
    {
        usage();
        return -1;
    }
    std::string path(argv[1]);   // 获取文件路径
    std::string target(argv[2]); // 获取压缩包名称

    std::cout << "将要压缩的文件路径" << path << std::endl;
    std::cout << "压缩包名称" << target << std::endl;

    // 1.打开path, 将文件内容整理为string
    std::ifstream ifs;
    ifs.open(path.c_str(), std::ios::binary); // 以二进制模式打开文件
    if (!ifs)
    { // 检查文件是否成功打开
        std::cerr << "无法打开文件" << path << std::endl;
        return 1;
    }

    // 获取文件大小
    ifs.seekg(0, ifs.end);    // 移动到文件末尾
    int length = ifs.tellg(); // 获取当前读指针位置（即文件大小）
    ifs.seekg(0, ifs.beg);    // 将指针移动回文件开头
    // 组织为string
    std::string input;
    input.resize(length);        // 设置字符串的大小为文件大小
    ifs.read(&input[0], length); // 读取文件内容到字符串中
    ifs.close();                 // 关闭文件

    // 2.使用bundle压缩
    auto start_time = std::chrono::high_resolution_clock::now(); // 记录压缩开始时间

    std::string output = bundle::pack(bundle::LZIP, input); // 调用bundle库进行压缩

    auto end_time = std::chrono::high_resolution_clock::now(); // 记录压缩结束时间

    // 3.将output写入到压缩包中
    std::ofstream ofs(target.c_str(), std::ios::binary); // 以二进制模式打开输出文件
    ofs.write(output.c_str(), output.size());            // 将压缩后的数据写入文件
    if (!ofs)
    { // 检查文件是否成功打开
        std::cerr << "无法生成文件" << target << std::endl;
        return 1;
    }
    ofs.close(); // 关闭输出文件

    std::cout << "################# 压缩成功 #################" << std::endl;
    std::cout << "压缩前的字符串大小" << input.size() << std::endl;
    std::cout << "压缩后的字符串大小" << output.size() << std::endl;
    
    // 计算压缩时间
    std::chrono::duration<double> duration = end_time - start_time;
    std::cout << "压缩时间：" << duration.count() << "秒" << std::endl;

    return 0;
}
