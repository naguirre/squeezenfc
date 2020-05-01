#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#include <cjson/cJSON.h>

#include "squeeze.h"

static char *_mac_addr = NULL;
static char _jsonrpc_url[4096];

static Playlist _playlists[4096];
static unsigned int _nb_playlists = 0;

typedef struct _http_data {
    char *ptr;
    size_t len;
} http_data;


static size_t read_callback(void *dest, size_t size, size_t nmemb, void *userp)
{
    http_data *s = userp;
    size_t new_len = s->len + size*nmemb;
    s->ptr = realloc(s->ptr, new_len+1);
    if (s->ptr == NULL)
    {
        printf("realloc() failed\n");
        return 0;
    }
    memcpy(s->ptr+s->len, dest, size*nmemb);
    s->ptr[new_len] = '\0';
    s->len = new_len;

    return size*nmemb;
}

static cJSON *_list_playlists(void)
{
    cJSON *root = cJSON_CreateObject();
    if(!root)
    {
        fprintf(stderr, "Error: cJSON_CreateObject failed.\n");
        return NULL;
    }

    cJSON_AddNumberToObject(root, "id", 1);
    cJSON_AddStringToObject(root, "method", "slim.request");
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString("-"));
    cJSON *cmd = cJSON_CreateArray();
    cJSON_AddItemToArray(cmd, cJSON_CreateString("playlists"));
    cJSON_AddItemToArray(cmd, cJSON_CreateNumber(0));
    cJSON_AddItemToArray(cmd, cJSON_CreateNumber(999));
    cJSON_AddItemToArray(params, cmd);
    cJSON_AddItemToObject(root, "params", params);

    return root;
}


static cJSON * _basic_action(const char *action)
{
    cJSON *root = cJSON_CreateObject();
    if(!root)
    {
        fprintf(stderr, "Error: cJSON_CreateObject failed.\n");
        return NULL;
    }

    cJSON_AddNumberToObject(root, "id", 1);
    cJSON_AddStringToObject(root, "method", "slim.request");
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString(_mac_addr));
    cJSON *cmd = cJSON_CreateArray();
    cJSON_AddItemToArray(cmd, cJSON_CreateString(action));
    cJSON_AddItemToArray(params, cmd);
    cJSON_AddItemToObject(root, "params", params);

    return root;
}

static char * _http_post(const char *url, const char *post_data)
{
    char *retdata = NULL;
    CURL *curl = NULL;
    CURLcode res = CURLE_FAILED_INIT;
    char errbuf[CURL_ERROR_SIZE] = { 0, };
    struct curl_slist *headers = NULL;
    char agent[1024] = { 0, };

    curl = curl_easy_init();
    if(!curl)
    {
        fprintf(stderr, "Error: curl_easy_init failed.\n");
        goto cleanup;
    }

    http_data http_data;

    http_data.ptr = malloc(1);
    http_data.ptr[0] = '\0';
    http_data.len = 0;

    snprintf(agent, sizeof agent, "libcurl/%s",
             curl_version_info(CURLVERSION_NOW)->version);
    agent[sizeof agent - 1] = 0;
    curl_easy_setopt(curl, CURLOPT_USERAGENT, agent);

    headers = curl_slist_append(headers, "Expect:");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, -1L);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, read_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &http_data);

    res = curl_easy_perform(curl);
    if(res != CURLE_OK)
    {
        size_t len = strlen(errbuf);
        fprintf(stderr, "\nlibcurl: (%d) ", res);
        if(len)
            fprintf(stderr, "%s%s", errbuf, ((errbuf[len - 1] != '\n') ? "\n" : ""));
        fprintf(stderr, "%s\n\n", curl_easy_strerror(res));
        goto cleanup;
    }

    retdata = strdup(http_data.ptr);;

cleanup:
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return retdata;

}

int squeeze_find_playlist_by_name(const char *serial)
{
    for (int i  = 0; i < _nb_playlists; i++)
    {
        if (strstr(_playlists[i].name, serial))
        {
            return i;
        }
    }
    return -1;
}

void _playlist_operation(const char *action)
{
    char *tmp;
    cJSON *root = cJSON_CreateObject();
    if(!root)
    {
        fprintf(stderr, "Error: cJSON_CreateObject failed.\n");
    }

    cJSON_AddNumberToObject(root, "id", 1);
    cJSON_AddStringToObject(root, "method", "slim.request");
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString(_mac_addr));
    cJSON *cmd = cJSON_CreateArray();
    cJSON_AddItemToArray(cmd, cJSON_CreateString("playlist"));
    cJSON_AddItemToArray(cmd, cJSON_CreateString("index"));
    cJSON_AddItemToArray(cmd, cJSON_CreateString(action));
    cJSON_AddItemToArray(params, cmd);
    cJSON_AddItemToObject(root, "params", params);

    tmp = cJSON_PrintUnformatted(root);
    _http_post(_jsonrpc_url, tmp);
    cJSON_Delete(root);
    free(tmp);
}

void squeeze_next_song(void)
{
    _playlist_operation("+1");
}

void squeeze_prev_song(void)
{
    _playlist_operation("-1");
}

void squeeze_basic_action(const char *action)
{
    cJSON *json;
    char *tmp;
    json = _basic_action(action);
    tmp = cJSON_PrintUnformatted(json);
    _http_post(_jsonrpc_url, tmp);
    cJSON_Delete(json);
    free(tmp);
}

void squeeze_volume_set(int volume)
{
    char *tmp;
    cJSON *root = cJSON_CreateObject();
    if(!root)
    {
        fprintf(stderr, "Error: cJSON_CreateObject failed.\n");
    }

    cJSON_AddNumberToObject(root, "id", 1);
    cJSON_AddStringToObject(root, "method", "slim.request");
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString(_mac_addr));
    cJSON *cmd = cJSON_CreateArray();
    cJSON_AddItemToArray(cmd, cJSON_CreateString("mixer"));
    cJSON_AddItemToArray(cmd, cJSON_CreateString("volume"));
    char buf[6];
    snprintf(buf, sizeof(buf), "%d", volume);
    cJSON_AddItemToArray(cmd, cJSON_CreateString(buf));
    cJSON_AddItemToArray(params, cmd);
    cJSON_AddItemToObject(root, "params", params);

    tmp = cJSON_PrintUnformatted(root);
    _http_post(_jsonrpc_url, tmp);
    cJSON_Delete(root);
    free(tmp);
}

int squeeze_volume_get(void)
{
    int volume = 0;
    cJSON *root = cJSON_CreateObject();
    if(!root)
    {
        fprintf(stderr, "Error: cJSON_CreateObject failed.\n");
        return -1;
    }

    cJSON_AddNumberToObject(root, "id", 1);
    cJSON_AddStringToObject(root, "method", "slim.request");
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString(_mac_addr));//b8:27:eb:23:9c:90"));
    cJSON *cmd = cJSON_CreateArray();
    cJSON_AddItemToArray(cmd, cJSON_CreateString("status"));
    cJSON_AddItemToArray(cmd, cJSON_CreateString("-"));
    cJSON_AddItemToArray(cmd, cJSON_CreateString("1"));
    cJSON_AddItemToArray(cmd, cJSON_CreateString("tags:uB"));
    cJSON_AddItemToArray(params, cmd);
    cJSON_AddItemToObject(root, "params", params);
    char *req = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    char *resp = _http_post(_jsonrpc_url, req);
    free(req);
    root = cJSON_Parse(resp);
    cJSON * result = cJSON_GetObjectItemCaseSensitive(root,"result");
    cJSON *mixer_volume = cJSON_GetObjectItemCaseSensitive(result, "mixer volume");
    if (cJSON_IsNumber(mixer_volume))
    {
        volume = mixer_volume->valueint;
    }
    else
    {
        volume = atoi(mixer_volume->valuestring);
    }
    cJSON_Delete(root);
    free(resp);

    return volume;
}

void squeeze_load_playlist(unsigned int id)
{
    char tmp[32];

    if (id < 0 || id >= _nb_playlists)
    {
        printf("Error %d < 0 or >= %d", id, _nb_playlists);
        return;
    }

    cJSON *root = cJSON_CreateObject();
    if(!root)
    {
        fprintf(stderr, "Error: cJSON_CreateObject failed.\n");
        return;
    }

    cJSON_AddNumberToObject(root, "id", 1);
    cJSON_AddStringToObject(root, "method", "slim.request");
    cJSON *params = cJSON_CreateArray();
    cJSON_AddItemToArray(params, cJSON_CreateString(_mac_addr));//b8:27:eb:23:9c:90"));
    cJSON *cmd = cJSON_CreateArray();
    cJSON_AddItemToArray(cmd, cJSON_CreateString("playlistcontrol"));
    cJSON_AddItemToArray(cmd, cJSON_CreateString("play_index:0"));
    cJSON_AddItemToArray(cmd, cJSON_CreateString("cmd:load"));
    cJSON_AddItemToArray(cmd, cJSON_CreateString("menu:1"));
    snprintf(tmp, sizeof(tmp), "playlist_id:%d", _playlists[id].id);
    cJSON_AddItemToArray(cmd, cJSON_CreateString(tmp));
    cJSON_AddItemToArray(cmd, cJSON_CreateString("useContextMenu:1"));
    cJSON_AddItemToArray(params, cmd);
    cJSON_AddItemToObject(root, "params", params);
    char *body = cJSON_PrintUnformatted(root);
    _http_post(_jsonrpc_url, body);
    free(body);
    cJSON_Delete(root);
}

void squeeze_reload_playlists(void)
{
    cJSON *json;
    json = _list_playlists();
    char *req = cJSON_PrintUnformatted(json);
    char *resp = _http_post(_jsonrpc_url, req);
    cJSON * root = cJSON_Parse(resp);
    cJSON * result = cJSON_GetObjectItemCaseSensitive(root,"result");

    cJSON *array = cJSON_GetObjectItemCaseSensitive(result, "playlists_loop");
    _nb_playlists = cJSON_GetArraySize(array);
    for (int i = 0; i < _nb_playlists; i++)
    {
        cJSON * playlist = cJSON_GetArrayItem(array, i);
        cJSON * name = cJSON_GetObjectItemCaseSensitive(playlist, "playlist");
        cJSON *id = cJSON_GetObjectItemCaseSensitive(playlist, "id");

        if (cJSON_IsNumber(id) && cJSON_IsString(name))
        {
            _playlists[i].id = id->valueint;
            _playlists[i].name = strdup(name->valuestring);
        }
        else if (cJSON_IsString(id) && cJSON_IsString(name))
        {
            _playlists[i].id = atoi(id->valuestring);
            _playlists[i].name = strdup(name->valuestring);
        }
        else
        {
            printf("Json error\n");
        }

        printf("Add %d %s to playlists\n", _playlists[i].id, _playlists[i].name);
    }
    cJSON_Delete(root);
    cJSON_Delete(json);
    free(resp);
    free(req);

}


int squeeze_init(const char *host, int16_t port, const char *mac_addr)
{

    if (!host || !mac_addr)
        return 0;

    if(curl_global_init(CURL_GLOBAL_ALL))
    {
        fprintf(stderr, "Fatal: The initialization of libcurl has failed.\n");
        return 1;
    }

    _mac_addr = strdup(mac_addr);
    snprintf(_jsonrpc_url, sizeof _jsonrpc_url, "http://%s:%d/jsonrpc.js", host, port);
    printf("jsontrpc_url : %s\n", _jsonrpc_url);
    return 0;
}
