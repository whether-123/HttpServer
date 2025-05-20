#include<iostream>
#include<string>
#include<memory>
#include "HttpServer.hpp"
#include "Log.hpp"
#include<ctime>

int main(int argc,char* argv[])
{
    if(argc!=2)
    {
        std::cout<<"CorrectInput:\n\r"<< argv[0]<<" port"<<std::endl;
        exit(4);
    }
    
    std::shared_ptr<HttpServer> http_server(new HttpServer(atoi(argv[1])));
    http_server->InitServer();
    http_server->Loop();

    return 0;
}