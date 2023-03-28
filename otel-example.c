#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <uuid/uuid.h>
#include <cjson/cJSON.h>
#include <curl/curl.h>

void gen_uuid_16(char* out){
    char buff[37];
    uuid_t uu;
    uuid_generate_random(uu);
    uuid_unparse(uu, buff);
    size_t j = 0;
    for (size_t i=0; i<36 && j<16; i++){
        if (buff[i] != '-'){
            out[j] = buff[i];
            j++;
        }
    }
    out[16] = '\0';
}

void gen_uuid_32(char* out){
    char buff[37];
    uuid_t uu;
    uuid_generate_random(uu);
    uuid_unparse(uu, buff);
    size_t j = 0;
    for (size_t i=0; i<36; i++){
        if (buff[i] != '-'){
            out[j] = buff[i];
            j++;
        }
    }
    out[32] = '\0';
}

typedef struct otel_json_generator{
    cJSON* span_stack[20];
    size_t span_stack_size;
    char trace_id[33];
    char* traces_url;
} otel_json_generator;


otel_json_generator get_otel_json_generator(char* traces_url){
    otel_json_generator ojg;
    gen_uuid_32(ojg.trace_id);
    ojg.traces_url = (char*) malloc(strlen(traces_url) + 1);
    strcpy(ojg.traces_url, traces_url);
    ojg.span_stack_size = 0;
    curl_global_init(CURL_GLOBAL_ALL);
    return ojg;
}

void close_otel_json_generator(otel_json_generator* ojg){
    curl_global_cleanup();
}

void start_otel_span(otel_json_generator* ojg, char* span_name){
    cJSON* span_json = cJSON_CreateObject();

    cJSON_AddStringToObject(span_json, "traceId", ojg->trace_id);
    char* span_id = (char*)malloc(17);
    gen_uuid_16(span_id);

    cJSON_AddStringToObject(span_json, "spanId", span_id);
    free(span_id);
    cJSON_AddStringToObject(span_json, "kind", "SPAN_KIND_INTERNAL");
    cJSON_AddStringToObject(span_json, "name", span_name);
    cJSON_AddArrayToObject(span_json, "attributes");
    cJSON_AddNumberToObject(span_json, "startTimeUnixNano", (uint64_t)(time(NULL)*1000000 + 1));
    if (ojg->span_stack_size > 0){
        cJSON_AddStringToObject(
            span_json,
            "parent_span_id",
            cJSON_GetObjectItemCaseSensitive(ojg->span_stack[ojg->span_stack_size - 1], "spanId")->valuestring
        );
    }
    else{
        cJSON_AddStringToObject(
            span_json,
            "parent_span_id",
            ""
        );
    }
    ojg->span_stack[ojg->span_stack_size] = span_json;
    ojg->span_stack_size += 1;

}

void add_attribute_to_curr_span(otel_json_generator* ojg, char* key, char* value){
    if (ojg->span_stack_size > 0){
        cJSON* current_span_attributes = cJSON_GetObjectItemCaseSensitive(
            ojg->span_stack[ojg->span_stack_size - 1],
            "attributes"
        );
        cJSON* attr_json = cJSON_CreateObject();
        cJSON_AddStringToObject(attr_json, "key", key);
        cJSON* val_json = cJSON_CreateObject();
        cJSON_AddStringToObject(val_json, "stringValue", value);
        cJSON_AddItemToObject(attr_json, "value", val_json);
        cJSON_AddItemToArray(current_span_attributes, attr_json);

    }
}

void end_current_span(otel_json_generator* ojg){
    if (ojg->span_stack_size > 0){
        cJSON* current_span = ojg->span_stack[ojg->span_stack_size - 1];
        cJSON_AddNumberToObject(current_span, "endTimeUnixNano", (uint64_t)(time(NULL)*1000000 + 1));
        ojg->span_stack[ojg->span_stack_size - 1] = NULL;
        ojg->span_stack_size -= 1;


        cJSON* trace_json = cJSON_CreateObject();
        cJSON* sspans_json = cJSON_CreateObject();
        cJSON* spans_json = cJSON_CreateObject();

        cJSON* spans_arr = cJSON_AddArrayToObject(spans_json, "spans");
        cJSON_AddItemToArray(spans_arr, current_span);

        cJSON* sspans_arr = cJSON_AddArrayToObject(sspans_json, "scopeSpans");
        cJSON_AddItemToArray(sspans_arr, spans_json);

        cJSON* rspans_arr = cJSON_AddArrayToObject(trace_json, "resourceSpans");
        cJSON_AddItemToArray(rspans_arr, sspans_json);


        printf("%s\n", cJSON_Print(trace_json));
        CURL *curl;
        CURLcode res;

        curl = curl_easy_init();
        if(curl) {
            curl_easy_setopt(curl, CURLOPT_URL, ojg->traces_url);
            struct curl_slist *hs=NULL;
            hs = curl_slist_append(hs, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hs);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, cJSON_Print(trace_json));

            res = curl_easy_perform(curl);
            curl_easy_cleanup(curl);
        }

        cJSON_Delete(trace_json);
    }
}


otel_json_generator ojg;

void fn1(){
    start_otel_span(&ojg, "fn1 span1");
    add_attribute_to_curr_span(&ojg, "traceval", "from fn1 span1");
    printf("in fn1 span1");
    end_current_span(&ojg);

    start_otel_span(&ojg, "fn1 span2");
    add_attribute_to_curr_span(&ojg, "traceval", "from fn1 span2");
    printf("in fn2 span2");
    end_current_span(&ojg);
}

void fn2(){
    start_otel_span(&ojg, "fn2 span");
    add_attribute_to_curr_span(&ojg, "traceval", "from fn2 span");
    printf("in fn2 span. should be child of childspan");
    end_current_span(&ojg);
}

int main(){
    ojg = get_otel_json_generator("http://0.0.0.0:4318/v1/traces");
    start_otel_span(&ojg, "nspan");
    add_attribute_to_curr_span(&ojg, "parentkey", "parentlol");
    fn1();
    start_otel_span(&ojg, "childspan");
    add_attribute_to_curr_span(&ojg, "childkey", "childlol");
    end_current_span(&ojg);
    end_current_span(&ojg);
    close_otel_json_generator(&ojg);
    return 0;
}
