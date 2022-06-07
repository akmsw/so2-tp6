#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <ulfius.h>
#include <jansson.h>
#include <yder.h>

#define PORT 1235
#define MAX_BUFF_SIZE 100

int exit_flag;

long int counter;

int callback_print_counter(const struct _u_request *request, struct _u_response *response, void *user_data)
{
    (void)request;
    (void)user_data;

    json_t *json_response = json_object();

    if ((!json_response) ||
        (json_object_set(json_response, "code", json_integer(200)) == -1) ||
        (json_object_set(json_response, "description", json_integer(counter)) == -1))
    {
        fprintf(stderr, "Failed creating JSON response\n");
        y_log_message(Y_LOG_LEVEL_ERROR, "Failed creating JSON response.");
        exit(1);
    }

    if (ulfius_set_json_body_response(response, 200, json_response) != U_OK)
    {
        fprintf(stderr, "ulfius_set_json_body_response (get@print) failed\n");
        y_log_message(Y_LOG_LEVEL_ERROR, "Could not set JSON body response.");
        exit(1);
    }

    return U_CALLBACK_CONTINUE;
}

int callback_increment_counter(const struct _u_request *request, struct _u_response *response, void *user_data)
{
    (void)response;
    (void)user_data;

    counter++;

    json_t *json_response = json_object();

    if ((!json_response) ||
        (json_object_set(json_response, "code", json_integer(200)) == -1) ||
        (json_object_set(json_response, "description", json_integer(counter)) == -1))
    {
        fprintf(stderr, "Failed creating JSON response\n");
        y_log_message(Y_LOG_LEVEL_ERROR, "Failed creating JSON response.");
        exit(1);
    }

    if (ulfius_set_json_body_response(response, 200, json_response) != U_OK)
    {
        fprintf(stderr, "ulfius_set_json_body_response (get@increment) failed\n");
        y_log_message(Y_LOG_LEVEL_ERROR, "Could not set JSON body response.");
        exit(1);
    }

    char buffer[MAX_BUFF_SIZE];
    const char *ip = u_map_get(request->map_header, "X-Forwarded-For");

    if (!ip)
    {
        fprintf(stderr, "snprintf failed\n");
        y_log_message(Y_LOG_LEVEL_ERROR, "Could not get client IP for log.");
        exit(1);
    }

    if (snprintf(buffer, MAX_BUFF_SIZE, "Counter incremented from device %s.", ip) < 0)
    {
        fprintf(stderr, "snprintf failed\n");
        y_log_message(Y_LOG_LEVEL_ERROR, "Could not make response for log.");
        exit(1);
    }

    // Escritura en archivo de log
    y_log_message(Y_LOG_LEVEL_INFO, buffer);

    return U_CALLBACK_CONTINUE;
}

void handler(int s)
{
    (void)s;
    exit_flag = 1;
}

int main(void)
{
    exit_flag = 0;

    struct _u_instance instance;

    if (ulfius_init_instance(&instance, PORT, NULL, NULL) != U_OK)
    {
        fprintf(stderr, "ulfius_init_instance failed\n");
        return 1;
    }

    if (ulfius_add_endpoint_by_val(&instance, "GET", "/print_counter", NULL, 0, &callback_print_counter, NULL) != U_OK)
    {
        fprintf(stderr, "ulfius_add_endpoint_by_val (get) failed\n");
        return 1;
    }

    if (ulfius_add_endpoint_by_val(&instance, "POST", "/increment_counter", NULL, 0, &callback_increment_counter, NULL) != U_OK)
    {
        fprintf(stderr, "ulfius_add_endpoint_by_val (post) failed\n");
        return 1;
    }

    if (signal(SIGINT, handler) == SIG_ERR ||
        (signal(SIGTSTP, handler) == SIG_ERR) ||
        (signal(SIGTERM, handler) == SIG_ERR))
    {
        fprintf(stderr, "signal failed\n");
        exit(1);
    }

    if (y_init_logs("COUNTER_MANAGER", Y_LOG_MODE_FILE, Y_LOG_LEVEL_INFO, "log_counter_manager.log", "<COUNTER MANAGER LOG FILE>") == 0)
    {
        fprintf(stderr, "y_init_logs failed\n");
        return 1;
    }

    if (ulfius_start_framework(&instance) != U_OK)
    {
        fprintf(stderr, "ulfius_start_framework failed\n");
        y_log_message(Y_LOG_LEVEL_ERROR, "Could not initialize ulfius framework.");
        exit(1);
    }

    counter = 0;

    while (!exit_flag);

    if (ulfius_stop_framework(&instance) != U_OK)
    {
        fprintf(stderr, "ulfius_stop_framework failed\n");
        y_log_message(Y_LOG_LEVEL_ERROR, "Could not stop ulfius framework.");
        exit(1);
    }

    ulfius_clean_instance(&instance);

    if (y_close_logs() == 0)
    {
        fprintf(stderr, "y_close_logs failed\n");
        return 1;
    }

    return 0;
}