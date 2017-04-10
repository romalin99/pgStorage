/*
 * =====================================================================================
 *
 *       Filename:  ServiceRegistry.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  03/15/2017 02:22:15 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Normandie (sams), romalin99@gmail.com
 *   Organization:  |ORGANIZATION |
 *
 * =====================================================================================
 */
#pragma once

#include <string>
#include <vector>
#include <list>
#include <unordered_map>
#include <mutex>
#include <pqxx/pqxx> 
#include <pthread.h>
#include <fstream>
#include <atomic>
#include <thread>
#include <random>
#include <memory>
#include <glog/logging.h>
#include "include/customdatatypes.h"
#include "include/AsyncObjectPool.h"
#include "common/nlohmann/json.hpp"
#include "include/ethicallutils.h"
#include "concurrentqueue.h"
#include "nlohmann/json.hpp"
#include "concurrentqueue.h"
using namespace pqxx;
#define RESET   "\033[0m\n"
#define BLACK   "\033[30m "      /* Black */
#define RED     "\033[31m "      /* Red */
#define GREEN   "\033[32m "      /* Green */
#define YELLOW  "\033[33m "      /* Yellow */
#define BLUE    "\033[34m "      /* Blue */
#define MAGENTA "\033[35m "      /* Magenta */
#define CYAN    "\033[36m "      /* Cyan */
#define WHITE   "\033[37m "      /* White */
#define BOLDBLACK   "\033[1m\033[30m "      /* Bold Black */
#define BOLDRED     "\033[1m\033[31m "      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m "      /* Bold Green */
#define BOLDYELLOW  "\033[1m\033[33m "      /* Bold Yellow */
#define BOLDBLUE    "\033[1m\033[34m "      /* Bold Blue */
#define BOLDMAGENTA "\033[1m\033[35m "      /* Bold Magenta */
#define BOLDCYAN    "\033[1m\033[36m "      /* Bold Cyan */
#define BOLDWHITE   "\033[1m\033[37m "      /* Bold White */
//std::cout << RED << "Red Color" << RESET << std::endl;
//std::cout << GREEN << "Green Color" << RESET << std::endl;
//std::cout< <RED <<"Hello, World! in RED\n";
//std::cout<<GREEN <<"Hello, World! in GREEN\n";

class  PostgresCluster;
using instance_t = struct _tag_Instance 
{
	_tag_Instance():used(false),queues(new moodycamel::ConcurrentQueue<size_t>())
	{}
    std::string ip;
    uint16_t    port={20004};
	uint16_t    conn={60};
	std::string user;
	std::string passwd;
	std::string dbname;
    volatile std::atomic_llong  metrics;
	volatile std::atomic_bool used;

	std::vector<pqxx::connection*> pclients;
	moodycamel::ConcurrentQueue<size_t> *queues={nullptr};
};

class ServiceRegistry 
{
	friend class  PostgresCluster;
private:
	volatile std::atomic<int> v_ause_[100];
	//service registry table
    std::unordered_map<std::string, instance_t*> srtable_; //{ip-port,item} 
	std::list<instance_t*> addrtable_;
    mutable std::mutex data_mtx_;
	
	std::string ipport_;
	instance_t      item_;
	std::atomic<std::string*> avacurip_; // available current ip:port with used seldom.

	void CreateConnPool(instance_t*&);
	// self register
	std::unique_ptr<std::thread> heartbeattid_;
	volatile std::atomic_bool heartactive_;
	void HeartBeat();
	void Register(std::string, std::string, uint16_t ,std::string, std::string, uint16_t);
	std::string GetSelfPath();
	std::string LoadFile(const char *, bool);

	volatile std::atomic_bool lfirst_={false};
	static instance_t* insptr_; // nullptr to null
	uint32_t loop_load_ = {120};
	std::default_random_engine generator_;
	std::hash<std::string> hash_;
protected:
	size_t 	GenerateRandom(size_t n);
	int32_t JumpConsistentHash(std::string& rKey, int32_t num_buckets);
	bool 	Reconnect(instance_t*& , pqxx::connection*&);
	//pqxx::connection* GetNewConn(instance_t*&);
public:
	//explicit ServiceRegistry() = default;
	ServiceRegistry();
	ServiceRegistry(const ServiceRegistry& ) = delete;
	ServiceRegistry& operator=(const ServiceRegistry& ) = delete;
	ServiceRegistry(ServiceRegistry&& ) = delete;
	ServiceRegistry& operator=(const ServiceRegistry&& ) = delete;
    virtual ~ServiceRegistry();

	instance_t*&  RouteSelector();
    bool empty() const; 
    size_t size() const; 
public:
};
