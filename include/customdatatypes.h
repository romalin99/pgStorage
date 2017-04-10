/*
 * =====================================================================================
 *
 *       Filename:  customdatatypes.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  05/04/2016 11:04:17 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Normandie (sams), romalin99@gmail.com
 *   Organization:  |ORGANIZATION |
 *
 * =====================================================================================
 */
#pragma once

//typedef ptrdiff_t Size;
//template< class Type, Size n >
//Size CountOf( Type (&)[n] ) { return n; }

#define TIME_INIT   std::chrono::steady_clock::time_point start, end;
#define TIME_START start = std::chrono::steady_clock::now();
/*
#define TIME_END   end = std::chrono::steady_clock::now(); \
											std::cout << std::chrono::duration_cast<std::chrono::microseconds>(end-start).count() << " us line:" <<__LINE__<<"   in "<<__func__<< std::endl;
*/
#define TIME_END   end = std::chrono::steady_clock::now(); CLOG(INFO,"performance")<< std::chrono::duration_cast<std::chrono::microseconds>(end-start).count() << " us,"<< __func__;
typedef struct ip_group{
    std::string ip;
    uint32_t port;
    ip_group(std::string ip,  uint32_t port){ this->ip = ip;  this->port=port;}
}ip_t;

typedef struct region_group{
    uint32_t begin;
    uint32_t end;
    region_group(uint32_t beg,  uint32_t end){  this->begin = beg;    this->end= end; }
}region_t;

using Configuration_t = struct __tag_configuration
{
    struct  redis_t {
        int  conn_timeout;
        int  poolsize;
        int  clustersize;
        int  rw_timeout;
        int  max_conns;
        std::vector<ip_t> ips;
    } redis_conf;

    struct  hbase_t {
        int conn_timeout;
        int poolsize;
        int rw_timeout;
        std::vector<ip_t> ips;
    } hbase_conf;
};



