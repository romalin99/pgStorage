/*
 * =====================================================================================
 *
 *       Filename:  ServiceRegistry.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  03/15/2017 02:24:52 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Normandie (sams), romalin99@gmail.com
 *   Organization:  |ORGANIZATION DESCRITIONS|
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include "ServiceRegistry.h"

//std::vector<TInstance> kIPs{{"192.168.2.43",20004}, {"192.168.2.44",20004}}; 
std::vector<std::string> kIPs{"192.168.2.43 20004", "192.168.2.43 20004"};

struct ComplexPostgresRequest;
extern AsyncObjectPool<ComplexPostgresRequest> gPgAPool;
instance_t* ServiceRegistry::insptr_ = nullptr;
ServiceRegistry::ServiceRegistry() : heartactive_(true) 
{
	heartbeattid_ = std::make_unique<std::thread>(std::bind(&ServiceRegistry::HeartBeat, this));
}

ServiceRegistry::~ServiceRegistry()
{
	heartactive_.store(false);
	//std::unique_lock<std::mutex> lock{data_mtx_};

	for(auto& ele :  srtable_)
	{
		if(ele.second  && (ele.second)->used )
		{
			for( auto k : (ele.second)->pclients)
			{
				if(k) { k->disconnect(); delete k; }
			}
		}
		if((ele.second)->queues) delete (ele.second)->queues;
	}

	//pthread_kill(heartbeattid_->native_handle(), 9);// 无线程信号函数，作用整个进程退出
	if(heartbeattid_->joinable())
	{
		heartbeattid_->join();
		heartbeattid_.reset();;
	}
}

size_t ServiceRegistry::GenerateRandom(size_t n)
{
	std::uniform_int_distribution<int> distribution(0, n-1);
    return distribution(generator_);
}

std::string ServiceRegistry::GetSelfPath()
{
#define MAX_SIZE 256
    char buff[MAX_SIZE]{0};
    ssize_t len = ::readlink("/proc/self/exe", buff, sizeof(buff)-1);
    if(len < 0 || len >= MAX_SIZE)
    {
       std::cout<<"readlink /proc/self/exe failed!"<<std::endl;
        exit(-1);
    }
    else
    {
        size_t  idx = len;
        for( ;buff[idx] != '/'; idx--) buff[idx]='\0';
        return std::string(buff);
    }
}

std::string ServiceRegistry::LoadFile(const char *name, bool binary)
{
    std::ifstream ifs(name, binary ? std::ifstream::binary : std::ifstream::in);
    if (!ifs.is_open()) return std::string("");
    std::stringstream strSS;
    strSS << ifs.rdbuf();
    if( ifs.bad() ) return std::string("");
    std::string tmp;
    tmp = strSS.str();
    ifs.close();
    return  tmp;
}

bool ServiceRegistry::empty() const  {
	std::unique_lock<std::mutex> lock{data_mtx_};
	return srtable_.empty();
}

size_t ServiceRegistry::size() const {
	std::unique_lock<std::mutex> lock{data_mtx_};
	return srtable_.size();
}

void ServiceRegistry::CreateConnPool(instance_t*& refIns)
{
	char bufdbi[256]{0};
	snprintf(bufdbi, sizeof(bufdbi),"dbname=%s hostaddr=%s port=%d user=%s password=%s", refIns->dbname .c_str(), refIns->ip.c_str(), refIns->port, refIns->user.c_str(), refIns->passwd.c_str());
	std::cout<<bufdbi<<std::endl;

	for(uint16_t i=0; i < refIns->conn; ++i)
	{
		//refIns->queues = new moodycamel::ConcurrentQueue<size_t>;
		try
		{
			pqxx::connection *pconn = new ::pqxx::connection(bufdbi);
			if(pconn && pconn->is_open())
			{
				(refIns->queues)->enqueue(i);
				refIns->pclients.emplace_back(pconn); 
			}
			else
				std::cerr<<"new ::pqxx::connection(dbinfos) failed! and boolean="<<pconn->is_open()<<std::endl;
		}
		catch(const pqxx::sql_error& e)
		{
			std::cerr<<"SQL error: "<<e.what() <<"Query was: "<<e.query()<<std::endl;
			sleep(100);
			continue;
		}
		catch(const std::exception& e)
		{
			std::cerr<<e.what()<<std::endl;
		}
		std::cerr <<BLUE<<__FILE__<<":"<<__LINE__<<" "<<"Pre CreateConnPool"<< RESET;
	}

	std::cerr<<__FILE__<<":"<<__LINE__<<GREEN<<refIns->ip<<":"<<refIns->port<<" ConnectionPool size: "<<(refIns->queues)->size_approx()<<",pclientsize="<<(refIns->pclients).size()<<RESET;
}

instance_t*& ServiceRegistry::RouteSelector() 
{
	std::unique_lock<std::mutex> lock{data_mtx_};
	if(nullptr == avacurip_.load(std::memory_order_acquire))
	{
		lock.unlock();
		do {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			if(lfirst_) break; 
		}while(1);
		lock.lock();
	}
	auto search = srtable_.find((*avacurip_.load(std::memory_order_acquire)));
	if(srtable_.end() != search)
		return search->second;
	else {
		for(auto& ele : srtable_) {
			if((ele.second)->used) return ele.second;
		}
	}

	return insptr_;
}

void ServiceRegistry::Register(std::string db, std::string ip,  uint16_t port, std::string user, std::string passwd,uint16_t nconn) 
{
	 //iret1 = system("nc  -v -w 1 -z 192.168.2.43 20004");
	char bufCmd[256]{0};
	snprintf(bufCmd,sizeof(bufCmd),"nc  -v -w 1 -z  %s %u", ip.c_str(), port);
	std::cout<<"ExCMD: "<<bufCmd<<std::endl;
	std::string ipport(ip); 
	{ ipport.append(":");ipport.append(std::to_string(port)); }
	if(0 == system(bufCmd)) 	// indicate server instance is availabel, if find it do nothing , or new many connections and register it to service registry center.
	{
		auto itr = srtable_.find(ipport);
		if(srtable_.end() ==  itr)
		{
			instance_t*  itm = new instance_t();
			{ itm->ip = ip; itm->port = port; itm->user = user; itm->passwd = passwd; itm->conn = nconn; itm->dbname = db; }
			std::cerr << RED<<__FILE__<<":"<<__LINE__<<" "<<"Pre CreateConnPool"<< RESET;
			CreateConnPool(itm);

			{
				std::unique_lock<std::mutex> lock{data_mtx_};
				//add address of the new instance to follow tables.
				srtable_.insert(std::make_pair(ipport, itm));			
				addrtable_.push_back(itm);
			}
		} //else do nothing
	}
	else
	{
		//server instance unavailable , then search ServiceRegistry table for deleting, if find it, and delete it for memory leak. or do nothing.
		auto itr = srtable_.find(ipport);
		if(srtable_.end() !=  itr) {
			std::unique_lock<std::mutex> lock{data_mtx_}; 
			//firstly disable the used bit
			(itr->second)->used = false;

			//secondly  remove the item from address table
			addrtable_.remove_if([=](instance_t*& kp){
				if((kp->ip == ip) && (kp->port == port) ) return true; else return false;
			});

			//thirdly transfer ownensformr of invalid instance item
			instance_t* refAddr = std::move(itr->second);

			//fourthly  move it to objec pool for handling asynchronously
			std::thread([=](std::string ips){
				std::cout<<ips<<std::endl;
				std::this_thread::sleep_for(std::chrono::seconds(60*3));
				//clear all connections resource of the invalid instance for avoiding memory leak.
				for(auto& ele : (refAddr)->pclients){
					std::cout<<__FILE__<<":"<<__LINE__<<" delete connection addres: "<<ele<<std::endl;
					if(ele) { ele->disconnect(); delete ele; }
				}
				if(refAddr->queues) delete refAddr->queues;
			}, ipport).detach();

			//finally delete it from ServiceRegistry table
			srtable_.erase(itr);

		} //else do nothing
	}
}

void  ServiceRegistry::HeartBeat() 
{
	std::cout<<"心跳线程启动......."<<std::endl;
	using json = nlohmann::json;
	while(heartactive_.load())
	{
		std::cerr << RED<<__FILE__<<":"<<__LINE__<<" "<<"object pool size: "<<gPgAPool.size()<< RESET;

		uint16_t connpoolsize{0};
		try
		{
			std::string selfpath = GetSelfPath(); selfpath.append("conf/pgStorage.conf");
			std::string jsonconf = LoadFile(selfpath.c_str(), false);
			json jroot = json::parse(jsonconf);

			connpoolsize 	= jroot["postgres"]["conn_pool_size"].get<int>();
			loop_load_ 		= jroot["postgres"]["loop_load"].get<uint32_t>();
			std::string _db = jroot["postgres"]["dbname"].get<std::string>();
			json  arrIps 	= jroot["postgres"]["ips"];
			for(size_t i=0; i< arrIps.size(); ++i)
			{
				std::string _ip     = arrIps[i]["ip"].get<std::string>();
				uint16_t _port      = arrIps[i]["port"].get<uint16_t>();
				std::string _user   = arrIps[i]["username"].get<std::string>();
				std::string _passwd = arrIps[i]["password"].get<std::string>();
				Register(_db, _ip, _port, _user, _passwd, connpoolsize);
			}
		}
		catch(...)
		{
			std::cerr<<"json configuration context is error"<<std::endl;
		}

		{
			std::string *psvip = avacurip_.load(std::memory_order_acquire);
			if(psvip) std::cerr << "\033[32m "<<__LINE__<<", "<<__func__ <<" "<<*(avacurip_.load())<<", table:"<<addrtable_.size()<<"\033[0m\n";
			std::unique_lock<std::mutex> lock{data_mtx_};
			addrtable_.sort([](instance_t*& fir, instance_t*& sec){	return (fir->metrics < sec->metrics); }); //待较复杂的route instance 算法
			if(addrtable_.size()>0)
			{
				//avacurip_ = addrtable_.front()->ip + std::string(":")+ std::to_string(addrtable_.front()->port);
				std::string *psvip = avacurip_.load(std::memory_order_acquire);
				std::cerr<<"psvip="<<psvip<<std::endl;
				std::string *svip = new std::string(addrtable_.front()->ip + std::string(":")+ std::to_string(addrtable_.front()->port));
				avacurip_.store(svip, std::memory_order_release);
				lfirst_=true;
				if(psvip) delete psvip;
			}
		}

		std::this_thread::sleep_for(std::chrono::seconds(loop_load_));
	}
}

bool ServiceRegistry::Reconnect(instance_t*& refins, pqxx::connection*& refcon)
{
    int idc{0};
    char bufdbi[256]{0};
    snprintf(bufdbi, sizeof(bufdbi),"dbname=%s hostaddr=%s port=%d user=%s password=%s", 	
												 	(refins->dbname).c_str(), 	
													(refins->ip).c_str(), 
													refins->port, 
													(refins->user).c_str(), 
													(refins->passwd).c_str()
													);
LOOP1:
    try{
        pqxx::connection *pconn = new ::pqxx::connection(bufdbi);
        //for instance: pclients_[idx][inx_client]->func-name();
        if(refcon) {
            delete refcon;
            refcon = pconn;
        }
    }
    catch(const pqxx::sql_error& e) 
	{
        LOG(ERROR)<<"reconnecting failed! SQL error: "<< e.what()<<" Query was: "<<e.query();
        if(++idc >= 3)  {
            LOG(ERROR)<<"reconnect "<<idc<<" times failed!";
            return false;
        }
        sleep(3);
        goto LOOP1;
    }
    catch(const std::exception& e) 
	{
        LOG(FATAL)<<e.what();
    }

    return  true;
}

int32_t ServiceRegistry::JumpConsistentHash(std::string& rKey, int32_t num_buckets)
{
	uint64_t key = (uint64_t)hash_(rKey);
    int64_t b = -1, j = 0;
    while (j < num_buckets) {
        b = j;
        key = key * 2862933555777941757ULL + 1;
        j = (b + 1) * (double(1LL << 31) / double((key >> 33) + 1));
    }
    //判断是否有效
    return b;
}

/*
pqxx::connection* ServiceRegistry::GetNewConn(instance_t*&) { }
*/
