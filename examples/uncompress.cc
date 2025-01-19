#include <iostream>
#include <fstream>
#include <memory.h>
#include "bundle/bundle.h"

void usage()
{
    std::cout << "-------- USAGE --------" << std::endl;
    std::cout << "./uncompress path target" << std::endl;
}

int main(int argc, char* argv[])
{
    //argv
    //第一个参数：将要解开的压缩包
    //第二个参数：解压缩后的文件名
    std::string path(argv[1]);
    std::string target(argv[2]);
    if(argc != 3){
        usage();
        return -1;
    }
    std::cout << "将要解开的压缩包" << path << std::endl;
    std::cout << "解压缩后的文件名" << target << std::endl;

    //1.打开path, 将文件内容整理为string
    std::ifstream ifs;
    ifs.open(path.c_str(), std::ios::binary);
    if (!ifs) { // 检查文件是否成功打开
        std::cerr << "无法打开文件" <<  path << std::endl;
        return 1;
    }

    //获取文件大小
    ifs.seekg(0, ifs.end);
    int length = ifs.tellg();
    ifs.seekg(0, ifs.beg);
    //读取文件内容到字符串缓冲区中
    std::cout << "压缩包大小" << length << std::endl;
    // char buf[length];
    // memset(buf, 0, sizeof(buf));
    // ifs.read(buf, length);
    // ifs.close(); 
    // std::string input(buf);//错误！！这种方式构造
    // 空字符截断问题： 如果你读取的文件中包含任何 空字符（\0），那么 std::string input(buf); 
    // 会在遇到第一个空字符时停止，因此不会把后续的文件内容读取到 input 中。
    //修改：std::string input(buf, length);

    std::string input;
    input.resize(length);
    ifs.read(&input[0], length);
    ifs.close();
    //组织为string
    std::cout << "解压缩前的字符串大小" << input.size() << std::endl;
    //2.使用bundle解压缩
    std::string output = bundle::unpack(input);
    std::cout << "解压缩后的字符串大小" << output.size() << std::endl;


    //3.将output写入到压缩包中
    std::ofstream ofs(target.c_str(), std::ios::binary);
    ofs.write(output.c_str(), output.size());
    if (!ofs) { // 检查文件是否成功打开
        std::cerr << "无法生成文件" << target << std::endl;
        return 1;
    }
    ofs.close();

    std::cout << "解压缩成功！"  << std::endl;

    return 0;
}