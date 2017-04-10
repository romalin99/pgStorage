/*
 * =====================================================================================
 *
 *       Filename:  postgrescluster.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  03/20/2017 10:03:28 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Normandie (sams), romalin99@gmail.com
 *   Organization:  |ORGANIZATION |
 *
 * =====================================================================================
 */

#pragma once

#include <unistd.h>
#include <algorithm>
#include <chrono>
#include <stddef.h>
#include <future>
#include <algorithm>
#include <vector>
#include <random>
#include <pqxx/pqxx>

#include "include/AsyncObjectPool.h"
#include "include/customdatatypes.h"
#include "include/ethicallutils.h"
#include "concurrentqueue.h"
#include "nlohmann/json.hpp"
#include "ServiceRegistry.h"

using namespace pqxx;

#define Ok  	"Ok"
#define Nil 	"Nil"
#define Busy 	"Busy"
#define Err		"Err"	

using req_t = struct _tag_req{
	std::string db;
	std::string table;
	std::string method;
	uint32_t    type = 2;
	std::string statement;
	std::string uuid;
	_tag_req()=default;
	_tag_req(const struct _tag_req& other) : db(other.db), table(other.table), method(other.method),type(other.type), statement(other.statement), uuid(other.uuid) { }
	_tag_req(const struct _tag_req&& other){
		this->db = std::move(other.db); this->table = std::move(other.table); this->method = std::move(other.method);
		this->type = other.type; this->statement = std::move(other.statement); this->uuid = std::move(other.uuid); 
	}
};

using res_t = struct _tag_res{
	std::string state;
	std::string result;
};

enum class type7: uint8_t{
};

extern PostgresCluster gPgCluser;

class PostgresCluster
{
private:
	ServiceRegistry sr_; 
protected:
	PostgresCluster(const PostgresCluster& ) = delete;
	PostgresCluster& operator=(const PostgresCluster& ) = delete;
	PostgresCluster(PostgresCluster&& ) = delete;
	PostgresCluster& operator=(const PostgresCluster&& ) = delete;
public:
	PostgresCluster()=default;
    virtual ~PostgresCluster();
public:
	void TExec(res_t&, req_t&);
    void NTExec(res_t&, req_t&);
    void BatchTExec(res_t&, req_t&);
    void BatchNTExec(res_t&, req_t&);
};

struct ComplexPostgresRequest
{
	req_t m_req;
	bool m_freq = false;
	ComplexPostgresRequest() = default;
	ComplexPostgresRequest(req_t& req): m_req(req), m_freq(true) {}
	void operator()()
	{
		res_t res_;
		switch(m_req.type)
		{
			case 1: // 事物执行
				gPgCluser.TExec(res_,  m_req);
				break;
			case 2: // 非事物执行
				gPgCluser.NTExec(res_,  m_req);
				break;
			case 3: // Nosql key-value
				break;
			case 4: // extended field
				break;
			default:
				break;
		}
	}

	void execute() { (*this)(); }
	virtual ~ComplexPostgresRequest(){ }
};

