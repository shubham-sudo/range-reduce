#ifndef RUN_WORKLOAD_H_
#define RUN_WORKLOAD_H_

#include <memory>

#include "db_env.h"

extern std::string kDBPath;
extern std::string buffer_file;
extern std::string rqstats_file;

int runWorkload(std::unique_ptr<DBEnv> &env);

void runSanityCheck(std::unique_ptr<DBEnv> &env, size_t total_operations);

#endif // RUN_WORKLOAD_H_
