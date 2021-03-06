// #include "redis_client_private.c"
#include "redis_client.h"

#ifdef _WIN32
        #include <windows.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hiredis/hiredis.h>
#include "redis_client.h"

static struct timeval timeout = { 1, 500000 }; // 1.5 seconds
static redisContext *ctx; // global & private

static status_info* prv_connect(char *hostname, int port, bool is_unix);
static status_info* prv_disconnect();
static status_info* prv_store_job(ecr_job *job);
static ecr_job* prv_retrieve_job(char *key);
static status_info* prv_remove_job(char *key);
static ecr_job* prv_create_job(char *id, char *description, ecr_job_data *data);
static ecr_job_data* prv_create_job_data(char *content, bool is_command, language lang);

/**
 * @brief  Opens a connection to a redis instance
 * @note   
 * @param  *hostname: Server where redis is running
 * @param  port: Server port
 * @param  is_unix: Is client OS a Unix system
 * @retval status_info reference containing return code
 */
status_info* prv_connect(char *hostname, int port, bool is_unix) {
  assert(!ctx);
  status_info *status = status_info_new();
  assert(status);

  if (is_unix) {
        ctx = redisConnectUnixWithTimeout(hostname, timeout);
    } else {
        ctx = redisConnectWithTimeout(hostname, port, timeout);
    }
    if (ctx == NULL || ctx->err) {
        if (ctx) {
            redisFree(ctx);
            status->code = REDIS_STATUS_SUCCESS;
            snprintf(status->message, strlen(ctx->errstr), "Connection error: %s\n", ctx->errstr);
        } else {
            status->code = REDIS_STATUS_SUCCESS;
            snprintf(status->message, strlen(ctx->errstr), "Connection error: can't allocate redis context\n");
        }
        return status;
    }
    status->code = REDIS_STATUS_SUCCESS,
    status->message = "SUCCESS";
    return status;
}
/**
 * @brief  Disconnects from redis server
 * @note   
 * @retval status_info reference containing return code
 */
status_info* prv_disconnect() {
  assert(ctx);
  status_info *info = status_info_new();
  redisFree(ctx);
  info->code = REDIS_STATUS_SUCCESS;
  info->message = "Connection to redis closed";
  return info;
}
/**
 * @brief  Stores a job in redis db
 * @note   
 * @param  *job: reference to job
 * @retval status_info reference containing return code
 */
status_info* prv_store_job(ecr_job *job) {
    assert(ctx);
    assert(job);
    char *job_string = ecr_job_tostring(job);
    status_info *info = status_info_new();

    redisReply *reply = redisCommand(ctx, "SET %s:%b %b", ECR_REDIS_JOB_PREFIX, job->id, strlen(job->id), job_string, strlen(job_string));
    assert(reply);

    info->code = REDIS_STATUS_SUCCESS;
    if (reply->len) {
      info->message = ecr_strdup(reply->str);
    }
    freeReplyObject(reply);
    return info;
}
/**
 * @brief  Retrieves a job from redis db
 * @note   
 * @param  *key: job id
 * @retval reference to job instance 
 */
ecr_job* prv_retrieve_job(char *key) {
  assert(ctx);
  assert(key);

  redisReply *reply = redisCommand(ctx, "GET %s:%s", ECR_REDIS_JOB_PREFIX, key);
  assert(reply);

  cJSON *job_json = cJSON_Parse(reply->str);
  assert(job_json);

  cJSON *job_data_json = cJSON_GetObjectItem(job_json, "data");

  ecr_job_data *job_data = ecr_job_data_new(ecr_strdup(cJSON_GetObjectItemCaseSensitive(job_data_json, "content")->valuestring),
                   (bool)cJSON_GetObjectItemCaseSensitive(job_data_json, "is_command")->valueint,
                   (language)cJSON_GetObjectItemCaseSensitive(job_data_json, "lang")->valueint
  );

  ecr_job *job = ecr_job_new(ecr_strdup(cJSON_GetObjectItemCaseSensitive(job_json, "id")->valuestring), 
                            ecr_strdup(cJSON_GetObjectItemCaseSensitive(job_json, "description")->valuestring),
                            job_data
  );


  freeReplyObject(reply);
  cJSON_Delete(job_json);
  return job;
}
/**
 * @brief  Removes a job from redis db
 * @note   
 * @param  *key: job id
 * @retval status_info reference containing return code
 */
status_info* prv_remove_job(char *key) {
  assert(ctx);
  assert(key);
  status_info *info = status_info_new();
  assert(info);

  redisReply *reply = redisCommand(ctx, "DEL %s:%s", ECR_REDIS_JOB_PREFIX, key);
  assert(reply);

  info->code = REDIS_STATUS_SUCCESS;
  if (reply->len) {
    info->message = ecr_strdup(reply->str);
  }

  return info;
}
/**
 * @brief  Creates a new job
 * @note   
 * @param  *id: job id
 * @param  *description: job description 
 * @param  *data: Pointer to job_data instance
 * @retval reference to job instance
 */
ecr_job* prv_create_job(char *id, char *description, ecr_job_data *data) {
  assert(id);
  assert(description);
  assert(data);
  ecr_job *job = ecr_job_new(ecr_strdup(id),
                            ecr_strdup(description),
                            data
  );
  return job;
}
/**
 * @brief  Create job_data instance
 * @note   
 * @param  *content: String content of command or script
 * @param  is_command: Is the content a command
 * @param  lang: Language used to parse script or command
 * @retval 
 */
ecr_job_data* prv_create_job_data(char *content, bool is_command, language lang) {
  assert(content);
  ecr_job_data *job_data = ecr_job_data_new(ecr_strdup(content), is_command, lang);
  return job_data;
}

/**
 * @brief  Initializes a redis client
 * @note   
 * @retval reference to redis_client struct
 */
redis_client* redis_client_new() {
  redis_client *client = (redis_client*)malloc(sizeof(redis_client));
  assert(client);
  client->connect = prv_connect;
  client->disconnect = prv_disconnect;
  client->create_job = prv_create_job;
  client->create_job_data = prv_create_job_data;
  client->remove_job = prv_remove_job;
  client->retrieve_job = prv_retrieve_job;
  client->store_job = prv_store_job;
  return client;
}

/**
 * @brief  Destroys a redis client
 * @note   
 * @param  **client: 
 * @retval None
 */
void redis_client_destroy(redis_client **client) {
  assert(client);
  if (*client) {
    redis_client *self = *client;
    free (self);
    *client = NULL;
  }
}