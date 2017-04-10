/*
 * =====================================================================================
 *
 *       Filename:  PostgresCluster.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  03/20/2017 10:23:59 AM
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
#include "PostgresCluster.h"



PostgresCluster::~PostgresCluster() { }

void PostgresCluster::TExec(res_t& _return, req_t& req)
{
	instance_t*& refIns = sr_.RouteSelector(); //路由选择较优一个实例
	moodycamel::ConcurrentQueue<size_t>*& refqueue = refIns->queues;
	pqxx::result res;

	size_t inx_pool{0}; // 连接池索引
	if((refIns) && (1 == refqueue->try_dequeue(inx_pool)))
    {
		refIns->metrics++;
		//LOG(INFO)<<"ID="<<req.uuid<<",table="<<req.table<<",method="<<req.method<<",type="<<req.type<<",Sql:{"<<req.statement<<"}";
		pqxx::connection*&  refconn = refIns->pclients[inx_pool];
		try{
			pqxx::work w(*refconn); //conn.set_client_encoding("GBK"); pqxx::result res=w.exec("select * from disttab limit 80;");
			res = w.exec(req.statement);
            w.commit();
			_return.state = Ok;
		}
		catch(const pqxx::sql_error& e)
		{
			_return.state = Err;
            LOG(ERROR)<<"SQL error: "<< e.what()<<" Query was: "<<e.query();
            if(-1 == refconn->sock())
            {
                if(sr_.Reconnect(refIns, refconn))
                {
                    try {
                            pqxx::work w(*refconn);
							res = w.exec(req.statement);
                            w.commit();
							_return.state=Ok;
                    }
                    catch(const pqxx::sql_error& e) {
                        LOG(ERROR)<<"SQL error: "<< e.what()<<" Query was: "<<e.query();
                    }
                    catch(const std::exception& e) {
                        LOG(ERROR)<<e.what();
                    }
                }
                else {  LOG(ERROR)<<"Reconnect failed!";  }
            }
            else {  refconn->activate(); }
		}
		catch(const std::exception& e) {
			_return.state = Err;
            LOG(ERROR)<<e.what();
        }

		refqueue->enqueue(inx_pool);

		// todo something
        if(("select"==req.method) &&  (res.size()>0)) {
			std::string context{R"("result":[)"};
			
            for(auto row=res.begin(); row != res.end(); ++row)
            {
				std::string rowcon{R"({)"};
                for(auto field = row->begin();field != row->end(); ++field)
				{	
					rowcon.append(R"(")");rowcon.append(field->name());rowcon.append(R"(":)"); // key
					rowcon.append(R"(")");rowcon.append(field.c_str());rowcon.append(R"(",)");  //value
				}
				rowcon[rowcon.length()-1] = '}';
				rowcon[rowcon.length()] = ',';
				rowcon.append(R"(,)");
				context.append(rowcon);
            }
			context[context.length()-1]=']';
			_return.result = std::move(context);
        }
	}
	else
	{
		// if return null, new connection for coming request 
		_return.state = Err;
        LOG(ERROR)<<"client proxy is busying when calling, {refIns="<<refIns<<"}";
	}
}

void PostgresCluster::NTExec(res_t& _return, req_t& req)
{
	instance_t*& refIns = sr_.RouteSelector(); //路由选择较优一个实例
	moodycamel::ConcurrentQueue<size_t>*& refqueue = refIns->queues;
	pqxx::result res;

	size_t inx_pool{0}; // 连接池索引
	if((refIns) && (true == refqueue->try_dequeue(inx_pool)))
    {
		refIns->metrics++;
		//LOG(INFO)<<"ID="<<req.uuid<<",table="<<req.table<<",method"<<req.method<<",type="<<req.type<<",Sql:{"<<req.statement<<"}";
		pqxx::connection*&  refconn = refIns->pclients[inx_pool];
		try{
			pqxx::nontransaction nw(*(refconn));
			res = nw.exec(req.statement);
			_return.state = Ok;
		}
		catch(const pqxx::sql_error& e)
		{
			//_return.state = Err;
            LOG(ERROR)<<"SQL error: "<< e.what()<<" Query was: "<<e.query();
			_return.state = e.what();
            if(-1 == refconn->sock())
            {
                if(sr_.Reconnect(refIns, refconn))
                {
                    try {
							pqxx::nontransaction nw(*(refconn));
							res = nw.exec(req.statement);
							_return.state = Ok;
                    }
                    catch(const pqxx::sql_error& e) {
                        LOG(ERROR)<<"SQL error: "<< e.what()<<" Query was: "<<e.query();
                    }
                    catch(const std::exception& e) {
                        LOG(ERROR)<<e.what();
                    }
                }
                else {  LOG(ERROR)<<"Reconnect failed!";  }
            }
            else {  refconn->activate(); }
		}
		catch(const std::exception& e) {
			_return.state = Err;
            LOG(ERROR)<<e.what();
        }

		refqueue->enqueue(inx_pool);
		// todo something
		if(("select"==req.method) &&  (res.size()>0)) {
            std::string context{R"("result":[)"};
            
            for(auto row=res.begin(); row != res.end(); ++row)
            {
                std::string rowcon{R"({)"};
                for(auto field = row->begin();field != row->end(); ++field)
                {    
                    rowcon.append(R"(")");rowcon.append(field->name());rowcon.append(R"(":)"); // key
                    rowcon.append(R"(")");rowcon.append(field.c_str());rowcon.append(R"(",)");  //value
                }
                rowcon[rowcon.length()-1] = '}';
                rowcon[rowcon.length()] = ',';
                rowcon.append(R"(,)");
                context.append(rowcon);
            }
            context[context.length()-1]=']';
            _return.result = std::move(context);
        }
	}
	else
	{
		// new connection for coming request 
		_return.state = Err;
        //LOG(ERROR)<<"client proxy is busying when calling, {refIns="<<refIns<<"}";
        LOG(ERROR)<<"client proxy is busying when calling, {refIns="<<refIns<<"}, size="<<refqueue->size_approx();
	}
}

void PostgresCluster::BatchTExec(res_t& _return, req_t& req) {  }
void PostgresCluster::BatchNTExec(res_t& _return, req_t& req) {  }
