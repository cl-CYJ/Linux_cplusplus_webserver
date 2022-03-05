#ifndef _HTTPCONNECTION_H__
#define _HTTPCONNECTION_H__

#include <sys/epoll.h>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/mman.h>
#include <cstring>
#include <stdarg.h>
#include <sys/uio.h>

class http_conn{
public:

    //所有socket上的事件都被注册到同一个epoll上
    static int m_epollfd;
    //统计用户的数量
    static int m_user_count;
    //定义读缓冲与写缓冲区的大小
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;
     // 文件名的最大长度
    static const int FILENAME_LEN = 200;       

    // HTTP请求方法，这里只支持GET
    enum METHOD {GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT};
    
    /*
        解析客户端请求时，主状态机的状态
        CHECK_STATE_REQUESTLINE:当前正在分析请求行
        CHECK_STATE_HEADER:当前正在分析头部字段
        CHECK_STATE_CONTENT:当前正在解析请求体
    */
    enum CHECK_STATE { CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT };
    
    /*
        服务器处理HTTP请求的可能结果，报文解析的结果
        NO_REQUEST          :   请求不完整，需要继续读取客户数据
        GET_REQUEST         :   表示获得了一个完成的客户请求
        BAD_REQUEST         :   表示客户请求语法错误
        NO_RESOURCE         :   表示服务器没有资源
        FORBIDDEN_REQUEST   :   表示客户对资源没有足够的访问权限
        FILE_REQUEST        :   文件请求,获取文件成功
        INTERNAL_ERROR      :   表示服务器内部错误
        CLOSED_CONNECTION   :   表示客户端已经关闭连接了
    */
    enum HTTP_CODE { NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION };
    
    // 从状态机的三种可能状态，即行的读取状态，分别表示
    // 1.读取到一个完整的行 2.行出错 3.行数据尚且不完整
    enum LINE_STATUS { LINE_OK = 0, LINE_BAD, LINE_OPEN };

    http_conn() {};
    ~http_conn() {}; 

    //处理客户端请求
    void process(); 
    //初始化新接收的连接
    void init(int sockfd, const sockaddr_in & addr); 
    //关闭连接
    void close_conn();
    //非阻塞的读
    bool read();
    //非阻塞的写
    bool write();

private:
    int m_sockfd; //该HTTp连接的socket
    sockaddr_in m_addr; //通信的socket地址
    char m_read_buf[READ_BUFFER_SIZE]; //读缓冲区
    char m_write_buf[WRITE_BUFFER_SIZE]; //写缓冲区
    int m_write_idx; //表示写缓冲区下一个可用位置
    int m_read_idx; //标识读缓冲区下一个可用位置

    int m_checked_idx; //当前正在分析的字符在读缓冲区的位置
    int m_start_line; //当前正在解析的行的起始位置

    char m_real_file[FILENAME_LEN]; // 客户请求的目标文件的完整路径，其内容等于 doc_root + m_url, doc_root是网站根目录
    char * m_url; //请求目标文件的文件名
    char * m_version; //协议版本
    METHOD m_method; //请求方法
    char * m_host; //请求的服务器的域名
    bool m_linger; //是否保持连接
    int m_content_length; // HTTP请求的消息总长度
    char* m_file_address; // 客户请求的目标文件被mmap到内存中的起始位置
    //目标文件的状态。通过它我们可以判断文件是否存在、是否为目录、是否可读，并获取文件大小等信息
    struct stat m_file_stat; 
    //采用writev来执行写操作。
    struct iovec m_iv[2]; 
    //表示被写内存块的数量
    int m_iv_count;

    int bytes_to_send;              // 将要发送的数据的字节数
    int bytes_have_send;            // 已经发送的字节数
    
    CHECK_STATE m_check_state; //主状态机当前所处的状态
    //初始化连接其余的信息
    void init();
    //解析HTTP请求
    HTTP_CODE process_read();
    //填写HTTP响应
    bool process_write(HTTP_CODE ret);    
    //解析请求行
    HTTP_CODE parse_request_line(char * text);
    //解析请求头
    HTTP_CODE parse_request_header(char * text);
    //解析请求体
    HTTP_CODE parse_request_content(char * text);
    //解析一行
    LINE_STATUS parse_line();
    //内联函数
    inline char * get_line() {return m_read_buf + m_start_line;}
    
    HTTP_CODE do_request(); // 
    void unmap(); //对内存映射区执行munmap操作

    // 这一组函数被process_write调用以填充HTTP应答。
    bool add_response( const char* format, ... );
    bool add_content( const char* content );
    bool add_content_type();
    bool add_status_line( int status, const char* title );
    bool add_headers( int content_length );
    bool add_content_length( int content_length );
    bool add_linger();
    bool add_blank_line();
    
};

#endif //_HTTPCONNECTION_H__