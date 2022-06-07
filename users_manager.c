#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <regex.h>
#include <time.h>
#include <ulfius.h>
#include <jansson.h>
#include <yder.h>

#define PORT 1234
#define MAX_BUFF_SIZE 100
#define MAX_STR_LEN 5

int exit_flag;

regex_t username_validation_regex;

int callback_print_users(const struct _u_request *request, struct _u_response *response, void *user_data)
{
    (void)request;
    (void)response;
    (void)user_data;

    json_t *json_response = json_object();
    json_t *json_users_array = json_array();

    if (!json_response || !json_users_array)
    {
        fprintf(stderr, "Failed creating JSON response\n");
        y_log_message(Y_LOG_LEVEL_ERROR, "Failed creating JSON response.");
        exit(1);
    }

    int total_users = 0;

    struct passwd *entry;

    while ((entry = getpwent()))
    {
        json_t *json_aux = json_object();

        if ((!json_aux) ||
            (json_object_set(json_aux, "user_id", json_integer(entry->pw_uid)) == -1) ||
            (json_object_set(json_aux, "username", json_string(entry->pw_name)) == -1) ||
            (json_array_append(json_users_array, json_aux) == -1))
        {
            fprintf(stderr, "Failed creating JSON user register\n");
            y_log_message(Y_LOG_LEVEL_ERROR, "Failed creating JSON user register.");
            exit(1);
        }

        total_users++;
    }

    if (json_object_set(json_response, "data", json_users_array) == -1)
    {
        fprintf(stderr, "json_object_set failed\n");
        y_log_message(Y_LOG_LEVEL_ERROR, "Failed creating JSON response.");
        exit(1);
    }

    if (ulfius_set_json_body_response(response, 200, json_response) != U_OK)
    {
        fprintf(stderr, "ulfius_set_string_body_response failed\n");
        y_log_message(Y_LOG_LEVEL_ERROR, "Could not set JSON body response.");
        exit(1);
    }

    // Escritura en archivo de log
    char buffer[MAX_BUFF_SIZE];

    if (snprintf(buffer, MAX_BUFF_SIZE, "Total users created: %d.", total_users) < 0)
    {
        fprintf(stderr, "snprintf failed\n");
        y_log_message(Y_LOG_LEVEL_ERROR, "Could not make response for log.");
        exit(1);
    }

    y_log_message(Y_LOG_LEVEL_INFO, buffer);

    return U_CALLBACK_CONTINUE;
}

// 0 = inválida, 1 = válida
int validate_string(char *s)
{
    if (strlen(s) > MAX_STR_LEN)
        return 0;

    return regexec(&username_validation_regex, s, 0, NULL, 0);
}

char *get_current_time(void)
{
    time_t current_time;

    char *current_time_str;

    if ((current_time = time(NULL)) == ((time_t)-1))
    {
        fprintf(stderr, "time failed\n");
        exit(1);
    }

    if ((current_time_str = ctime(&current_time)) == NULL)
    {
        fprintf(stderr, "ctime failed\n");
        exit(1);
    }

    return current_time_str;
}

int callback_create_user(const struct _u_request *request, struct _u_response *response, void *user_data)
{
    (void)response;
    (void)user_data;

    // Obtención de información para crear usuario
    json_error_t json_error;
    json_t *json_request = ulfius_get_json_body_request(request, &json_error);

    if (!json_request)
    {
        fprintf(stderr, "ulfius_get_json_body_request failed\n");
        y_log_message(Y_LOG_LEVEL_ERROR, "Could not retrieve JSON request.");
        exit(1);
    }

    char *user = (char *)json_string_value(json_object_get(json_request, "username"));
    char *password = (char *)json_string_value(json_object_get(json_request, "password"));

    if (!user || !password)
    {
        fprintf(stderr, "json_string_value failed\n");
        y_log_message(Y_LOG_LEVEL_ERROR, "Could not retrieve JSON data from request.");
        exit(1);
    }

    // Validación y creación del usuario
    if (!(validate_string(user) && validate_string(password)))
    {
        fprintf(stderr, "password and/or username should not have more than 5 characters from a to z\n");
        return U_CALLBACK_ERROR;
    }

    if (getpwnam(user))
    {
        fprintf(stderr, "username already exists\n");
        return U_CALLBACK_ERROR;
    }

    char cmd[MAX_BUFF_SIZE];

    if (snprintf(cmd, MAX_BUFF_SIZE,
                 "sudo useradd %s && (echo %s; echo %s) | sudo passwd %s",
                 user, password, password, user) < 0)
    {
        fprintf(stderr, "snprintf (cmd_create) failed\n");
        y_log_message(Y_LOG_LEVEL_ERROR, "Could not make response for log.");
        exit(1);
    }

    system(cmd);

    // Envío de request de incremento de contador
    struct _u_request req;
    struct _u_response res;

    if (ulfius_init_request(&req) != U_OK)
    {
        fprintf(stderr, "ulfius_init_request failed\n");
        y_log_message(Y_LOG_LEVEL_ERROR, "Could not initialize request for counter increment.");
        exit(1);
    }

    req.http_verb = "POST";
    req.http_url = "http://userscounter.com/api/increment_counter";
    req.timeout = 20;

    if (!req.http_verb || !req.http_url)
    {
        fprintf(stderr, "strdup failed\n");
        y_log_message(Y_LOG_LEVEL_ERROR, "Could not set attributes in counter increment request.");
        exit(1);
    }

    if (ulfius_init_response(&res) != U_OK)
    {
        fprintf(stderr, "ulfius_init_response failed\n");
        y_log_message(Y_LOG_LEVEL_ERROR, "Could not initialize counter increment request response.");
        exit(1);
    }

    if (ulfius_send_http_request(&req, &res) != U_OK)
    {
        fprintf(stderr, "ulfius_send_http_request failed\n");
        y_log_message(Y_LOG_LEVEL_ERROR, "Could not send counter increment request.");
        return U_CALLBACK_ERROR;
    }

    // Armado y seteo de response en formato JSON
    struct passwd *new_user = getpwnam(user);

    if (!new_user)
    {
        fprintf(stderr, "getpwnam failed\n");
        y_log_message(Y_LOG_LEVEL_ERROR, "Something went wrong creating the user.");
        exit(1);
    }

    char *current_time_str = get_current_time();

    json_t *json_response_create = json_object();

    if ((!json_response_create) ||
        (json_object_set(json_response_create, "user_id", json_integer(new_user->pw_uid)) == -1) ||
        (json_object_set(json_response_create, "username", json_string(new_user->pw_name)) == -1) ||
        (json_object_set(json_response_create, "created_at", json_string(current_time_str)) == -1))
    {
        fprintf(stderr, "Failed creating JSON response\n");
        y_log_message(Y_LOG_LEVEL_ERROR, "Failed creating JSON response.");
        exit(1);
    }

    if (ulfius_set_json_body_response(response, 200, json_response_create) != U_OK)
    {
        fprintf(stderr, "ulfius_set_string_body_response (get) failed\n");
        y_log_message(Y_LOG_LEVEL_ERROR, "Could not set JSON body response.");
        exit(1);
    }

    char buffer[MAX_BUFF_SIZE];

    if (snprintf(buffer, MAX_BUFF_SIZE, "User %d created.", new_user->pw_uid) < 0)
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
        exit(1);
    }

    if (ulfius_add_endpoint_by_val(&instance, "GET", "/print_users", NULL, 0, &callback_print_users, NULL) != U_OK)
    {
        fprintf(stderr, "ulfius_add_endpoint_by_val (get) failed\n");
        exit(1);
    }

    if (ulfius_add_endpoint_by_val(&instance, "POST", "/create_user", NULL, 0, &callback_create_user, NULL) != U_OK)
    {
        fprintf(stderr, "ulfius_add_endpoint_by_val (post) failed\n");
        exit(1);
    }

    if (signal(SIGINT, handler) == SIG_ERR ||
        (signal(SIGTSTP, handler) == SIG_ERR) ||
        (signal(SIGTERM, handler) == SIG_ERR))
    {
        fprintf(stderr, "signal failed\n");
        exit(1);
    }

    if (regcomp(&username_validation_regex, "[^a-zA-Z]", 0) != 0)
    {
        fprintf(stderr, "regcomp failed\n");
        exit(1);
    }

    if (y_init_logs("USERS_MANAGER", Y_LOG_MODE_FILE, Y_LOG_LEVEL_INFO, "log_users_manager.log", "<USERS MANAGER LOG FILE>") == 0)
    {
        fprintf(stderr, "y_init_logs failed\n");
        return 1;
    }

    if (ulfius_start_framework(&instance) != U_OK)
    {
        fprintf(stderr, "ulfius_start_framework failed\n");
        y_log_message(Y_LOG_LEVEL_ERROR, "Could not initialize ulfius framework.");
        return 1;
    }

    while (!exit_flag);

    if (ulfius_stop_framework(&instance) != U_OK)
    {
        fprintf(stderr, "ulfius_stop_framework failed\n");
        y_log_message(Y_LOG_LEVEL_ERROR, "Could not stop ulfius framework.");
        return 1;
    }

    ulfius_clean_instance(&instance);

    if (y_close_logs() == 0)
    {
        fprintf(stderr, "y_close_logs failed\n");
        return 1;
    }

    return 0;
}