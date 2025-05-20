#include <iostream>
#include <string>
#include <mysql/mysql.h>
#include<cstdlib>
#include<unistd.h>

bool GetQueryString(std::string& query_string)
{
    bool result = false;
    std::string method = getenv("METHOD");
    if(method =="GET")
    {
        query_string = getenv("QUERY_STRING");
        result = true;
    }
    else if(method =="POST"  )
    {
        //CGI如何让知道从标准输入读取多少个字节？
        int content_length = atoi(getenv("CONTENT_LENGTH"));
        char c = 0;
        while(content_length)
        {
            read(0,&c,1);
            query_string.push_back(c);
            content_length--;
        }
        result = true;
        std::cerr<<result<<"\n\n";
    }
    else
    {}
}

void CutString(std::string& in,std::string sep,std::string& out1,std::string& out2)
{
    auto pos = in.find(sep);
    if(std::string::npos != pos)
    {
        out1 = in.substr(0,pos);
        out2 = in.substr(pos+sep.size());
    }
}

bool InsertSql(std::string sql)
{
    MYSQL* conn = mysql_init(nullptr);
    mysql_set_character_set(conn,"utf8");
    if(mysql_real_connect(conn,"127.0.0.1","test","12345678a","test",3306,nullptr,0) == nullptr)
    {
        std::cerr<<"connect error!"<<std::endl;
        fprintf(stderr, "Failed to connect to database: Error: %s\n",  mysql_error(conn));
         
        return false;
    }
    std::cerr<<"connect success!"<<std::endl;

    
    std::cerr<<"query: "<<sql<<std::endl;
    int ret = mysql_query(conn,sql.c_str());

    mysql_close(conn);

    return true;
}

int main()
{
    std::string query_string;
    if(GetQueryString(query_string))
    {
        std::cerr << query_string<<std::endl;
        
        std::string name;
        std::string password;
        
        CutString(query_string,"&",name,password);
        std::string _name;
        std::string sql_name;
        CutString(name,"=",_name,sql_name);

        std::string _password;
        std::string sql_password;
        CutString(password,"=",_password,sql_password);
        
        std::string sql = "insert into test(name,password) values(\'";
        sql += sql_name;
        sql +="\',\'";
        sql += sql_password;
        sql += "\')";

        //插入数据库
        if(InsertSql(sql))
        {
            std::cout<<"<html>";
            std::cout<<"<head><meta charset=\"utf-8\"></head>";
            std::cout<<"<body><h1>注册成功！</h1></body>";
        }
    }
    return 0;
}
