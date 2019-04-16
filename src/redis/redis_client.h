#ifndef __ECR_REDIS_CLIENT__
#define __ECR_REDIS_CLIENT__

#include <stdbool.h>
#include "../ecr/job/job.h"
#include "../ecr/status/status.h"

status_info* initRedis(char *hostname, int port, bool is_unix);
status_info* deinitRedis();
ecr_job* createJob(char *id, char *description, char *source_code, char *command, bool has_source_code);
status_info* storeJob(char *key, ecr_job *job);
ecr_job* retrieveJob(char *key);
status_info* removeJob(char *key);

#endif //__REDIS_CLIENT__
