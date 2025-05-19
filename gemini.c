// Versão adaptada do gemini.c para usar o modelo DeepSeek via OpenRouter
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "cJSON.h"
#include "gemini.h" // Mantemos o nome por compatibilidade

#define API_KEY "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"
#define MODEL "deepseek/deepseek-prover-v2:free"
#define ENDPOINT "https://openrouter.ai/api/v1/chat/completions"

typedef struct {
    char *buffer;
    size_t size;
} MemoryStruct;

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total_size = size * nmemb;
    MemoryStruct *mem = (MemoryStruct *)userp;

    char *ptr = realloc(mem->buffer, mem->size + total_size + 1);
    if (!ptr) return 0;

    mem->buffer = ptr;
    memcpy(&(mem->buffer[mem->size]), contents, total_size);
    mem->size += total_size;
    mem->buffer[mem->size] = 0;

    return total_size;
}

char* chamarIA(const char* prompt) {
    CURL *curl;
    CURLcode res;
    MemoryStruct chunk;
    chunk.buffer = malloc(1);
    chunk.size = 0;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (!curl) {
        curl_global_cleanup();
        return strdup("Erro: Falha ao iniciar CURL");
    }

    curl_easy_setopt(curl, CURLOPT_URL, ENDPOINT);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", API_KEY);
    headers = curl_slist_append(headers, auth_header);

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Construindo JSON no formato OpenRouter
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", MODEL);

    cJSON *messages = cJSON_CreateArray();
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "role", "user");
    cJSON_AddStringToObject(msg, "content", prompt);
    cJSON_AddItemToArray(messages, msg);
    cJSON_AddItemToObject(root, "messages", messages);

    char *json_data = cJSON_PrintUnformatted(root);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);

    res = curl_easy_perform(curl);
    cJSON_Delete(root);
    free(json_data);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    if (res != CURLE_OK) {
        free(chunk.buffer);
        return strdup("Erro: Falha na requisição da IA");
    }

    if (!chunk.buffer || strlen(chunk.buffer) == 0) {
        return strdup("Erro: Resposta vazia da IA");
    }

    // Parsing do JSON de resposta
    cJSON *json = cJSON_Parse(chunk.buffer);
    printf("\n--- RESPOSTA DA IA (raw) ---\n%s\n-----------------------------\n", chunk.buffer);
    free(chunk.buffer);
    if (!json) return strdup("Erro: JSON inválido da IA");

    cJSON *choices = cJSON_GetObjectItem(json, "choices");
    if (!choices || !cJSON_IsArray(choices)) {
        cJSON_Delete(json);
        return strdup("Erro: Campo 'choices' não encontrado ou inválido");
    }

    cJSON *firstChoice = cJSON_GetArrayItem(choices, 0);
    if (!firstChoice) {
        cJSON_Delete(json);
        return strdup("Erro: 'choices[0]' não encontrado");
    }

    cJSON *message = cJSON_GetObjectItem(firstChoice, "message");
    if (!message) {
        cJSON_Delete(json);
        return strdup("Erro: Campo 'message' não encontrado");
    }

    cJSON *text = cJSON_GetObjectItem(message, "content");
    if (!text || !cJSON_IsString(text)) {
        cJSON_Delete(json);
        return strdup("Erro: Campo 'content' não encontrado");
    }

    char *resultado = strdup(text->valuestring);
    if (strncmp(resultado, "text", 4) == 0 || strncmp(resultado, "plaintext", 9) == 0) {
        char* espaco = strchr(resultado, ' ');
        if (espaco && strlen(espaco + 1) > 0) {
            char* limpo = strdup(espaco + 1);
            free(resultado);
            resultado = limpo;
      }
  }
    cJSON_Delete(json);
    return resultado;

}
