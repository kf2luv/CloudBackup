#include <iostream>
#include "cpp-httplib/httplib.h"

void handler1(const httplib::Request& req, httplib::Response& resp)
{
    //处理请求，生成相应
    resp.set_content("Hello World!", "text/plain");
}

void handler2(const httplib::Request& req, httplib::Response& resp)
{
    std::string num1 = req.matches[1];
    // std::string num2 = req.matches[2];
    // int ret = std::stoi(num1) + std::stoi(num2);
    // resp.set_content(std::to_string(ret), "text/plain");
    resp.set_content(num1, "text/plain");
}

void handler3(const httplib::Request& req, httplib::Response& resp)
{
    if(req.has_file("file1")) //获取文件内容
    {
        auto file = req.get_file_value("file1");
        std::cout << "文件名" << file.filename << std::endl;
        std::cout << "文件类型" << file.content_type << std::endl;
        std::cout << "文件内容" << file.content << std::endl;
    }
    else
    {
        std::cout << "文件不存在" << std::endl;
    }
}

int main()
{
    httplib::Server svr;

    svr.Get("/hi", handler1);

    svr.Get(R"(/numbers/(\d+))", handler2);

    svr.Post("/upload", handler3);


    svr.listen("0.0.0.0", 9090);
    
    return 0;
}