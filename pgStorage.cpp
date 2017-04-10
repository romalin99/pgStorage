#include <unistd.h>
#include <fstream>
#include <iostream>

#include "postgres/PostgresCluster.h"
#include "include/AsyncObjectPool.h"
#include "include/customdatatypes.h"
#include "include/ethicallutils.h"
#include "nlohmann/json.hpp"
using namespace std;

PostgresCluster gPgCluser; 
AsyncObjectPool<ComplexPostgresRequest> gPgAPool; 

void SignalHandle(const char* data, int size) {
	//google::LogMessage::Flush();
    std::string str = std::string(data,size);
	{
		std::ofstream fs("./logs/glog_dump.log", std::ios::app);
		fs<<str;
		fs.close();
	}
	LOG(ERROR)<<str;
}

void load_configuration()
{
    using json = nlohmann::json;
    Configuration_t  kConfiguration;
    try
    {
        std::string selfpath = get_selfpath(); selfpath.append("conf/pgStorage.conf");
        std::string jsonconf = loadfile(selfpath.c_str(), false);
        json jroot = json::parse(jsonconf);
        kConfiguration.redis_conf.conn_timeout  = jroot["redis"]["conn_timeout"].get<int>();
        kConfiguration.redis_conf.poolsize      = jroot["redis"]["conn_pool_size"].get<int>();
        kConfiguration.redis_conf.clustersize   = jroot["redis"]["cluster_size"].get<int>();
        kConfiguration.redis_conf.rw_timeout    = jroot["redis"]["rw_timeout"].get<int>();
        kConfiguration.redis_conf.max_conns     = jroot["redis"]["max_conns"].get<int>();
        json  arrRedisIps                       = jroot["redis"]["ips"];
        for(const auto& ele : arrRedisIps)
        {
            json obj =  ele ;
            uint32_t port = obj["port"].get<uint32_t>();
            ip_t ipobj{ obj["ip"].get<std::string>(), port};
            kConfiguration.redis_conf.ips.emplace_back(ipobj);
        }

        kConfiguration.hbase_conf.poolsize       = jroot["hbase"]["conn_pool_size"].get<int>();
        kConfiguration.hbase_conf.conn_timeout   = jroot["hbase"]["conn_timeout"].get<int>() * 1000;
        kConfiguration.hbase_conf.rw_timeout     = jroot["hbase"]["rw_timeout"].get<int>() *1000;
        json  arrHbaseIps                        = jroot["hbase"]["ips"];
        for(size_t i=0; i< arrHbaseIps.size(); ++i)
        {
            std::string ip = arrHbaseIps[i]["ip"].get<std::string>();
            uint32_t port  = arrHbaseIps[i]["port"].get<uint32_t>();
            kConfiguration.hbase_conf.ips.emplace_back(ip_t(ip, port));
        }
    }
    catch(...)
    {
        LOG(FATAL)<<"json configuration context is error";
    }
}

void TestTransaction() {
	req_t req;  res_t res;
	req.table = "compa";
	req.method = "select";
	req.type=2;
	req.statement = R"(select * from compa limit 10)";
	gPgCluser.TExec(res, req);
	cout<<res.result<<endl;
}
void TestNonTransaction() {
	req_t req;  res_t res;
	req.table = "compa";
	req.method = "select";
	req.type=1;
	req.statement = R"(select * from compa limit 10)";
	req.uuid=get_uuid();
	gPgCluser.NTExec(res, req);
	cout<<res.result<<endl;
}

void TestInsert() {
	req_t req;  res_t res;
	req.table = "compa2";
	req.method = "insert";
	req.type=2;
	req.statement = R"(INSERT INTO compa2(NAME,AGE,ADDRESS,SALARY) VALUES('Mark', 25, 'Rich-Mond ', 65000.00 );)";
	req.uuid=get_uuid();
	gPgAPool.enqueue(std::unique_ptr<ComplexPostgresRequest>(new ComplexPostgresRequest(req)));
}
int main(int argc, char* argv[]) 
{
	TestNonTransaction();
	TestTransaction();
	TestInsert();
	return 0;
}
