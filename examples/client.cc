#include <iostream>
#include "cpp-httplib/httplib.h"

const std::string HOST = "123.249.9.114"; // 服务器ip地址
const int PORT = 9090;                    // 服务器端口号

int main(int argc, char* argv[])
{
    httplib::Client cli(HOST, PORT);

    // std::string path(argv[1]);

    // auto ret = cli.Get(path);
    // if (ret)
    // {
    //     std::cout << ret->status << std::endl;
    //     std::cout << ret->body << std::endl;
    // }
    // else
    // {
    //     auto err = ret.error();
    //     std::cout << "HTTP error: " << httplib::to_string(err) << std::endl;
    // }

    httplib::MultipartFormDataItems items = {
        {"file1", "this is file content", "hello.txt", "text/plain"}
    };

    auto ret = cli.Post("/upload", items);
    if (ret)
    {
        std::cout << ret->status << std::endl;
        std::cout << ret->body << std::endl;
    }
    else
    {
        auto err = ret.error();
        std::cout << "HTTP error: " << httplib::to_string(err) << std::endl;
    }


    return 0;
}