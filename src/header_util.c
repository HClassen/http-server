#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "header_util.h"
#include "http_server.h"

const char *http_code_to_str(int code){
	switch(code){
		case 200: return "HTTP/1.1 200 OK";
		case 201: return "HTTP/1.1 201 Created";
		case 202: return "HTTP/1.1 202 Accepted";
		case 203: return "HTTP/1.1 203 Non-Authoritative Information";
		case 204: return "HTTP/1.1 204 No Content";
		case 205: return "HTTP/1.1 205 Reset Content";
		case 206: return "HTTP/1.1 206 Partial Content";
		case 300: return "HTTP/1.1 300 Multiple Choices";
		case 301: return "HTTP/1.1 301 Moved Permanently";
		case 303: return "HTTP/1.1 303 See Other";
		case 304: return "HTTP/1.1 304 Not Modified";
		case 307: return "HTTP/1.1 307 Temporary Redirect";
		case 308: return "HTTP/1.1 308 Permanent Redirect";
		case 400: return "HTTP/1.1 400 Bad Request";
		case 401: return "HTTP/1.1 401 Unauthorized	";
		case 402: return "HTTP/1.1 402 Payment Required";
		case 403: return "HTTP/1.1 403 Forbidden";
		case 404: return "HTTP/1.1 404 Not Found";
		case 405: return "HTTP/1.1 405 Method Not Allowed";
		case 406: return "HTTP/1.1 406 Not Acceptable";
		case 408: return "HTTP/1.1 408 Request Timeout";
		case 410: return "HTTP/1.1 410 Gone";
		case 411: return "HTTP/1.1 411 Length Required";
		case 412: return "HTTP/1.1 412 Precondition Failed";
		case 413: return "HTTP/1.1 413 Request Entity Too Large";
		case 414: return "HTTP/1.1 414 URI Too Long";
		case 415: return "HTTP/1.1 415 Unsupported Media Type";
		case 416: return "HTTP/1.1 416 Requested range not satisfiable";
		case 417: return "HTTP/1.1 417 conpectation Failed";
		case 422: return "HTTP/1.1 422 Unprocessable Entity";
		case 423: return "HTTP/1.1 423 Locked";
		case 424: return "HTTP/1.1 424 Failed Dependency";
		case 426: return "HTTP/1.1 426 Upgrade Required";
		case 428: return "HTTP/1.1 428 Precondition Required";
		case 429: return "HTTP/1.1 429 Too Many Requests";
		case 432: return "HTTP/1.1 431 Request Header Fields Too Large";
		case 451: return "HTTP/1.1 451 Unavailable For Legal Reasons";
		case 418: return "HTTP/1.1 418 I’m a teapot";
		case 500: return "HTTP/1.1 500 Internal Server Error";
		case 501: return "HTTP/1.1 501 Not Implemented";
		case 503: return "HTTP/1.1 503 Service Unavailable";
		case 505: return "HTTP/1.1 505 HTTP Version not supported";
		case 507: return "HTTP/1.1 507 Insufficient Storage";
		default: return "HTTP/1.1 500 Internal Server Error";
	}
}

void header_cleanup(hash_table *ht){
    hash_cleanup(ht);
}

int header_lookup(hash_table *ht, char *key, const char **result){
    if(ht == NULL || key == NULL || result == NULL){
        return HTTPSERVER_EINPUT;
    }

    hash_entry *ptr = hash_lookup(ht, key);
    if(ptr == NULL){
        (*result) = NULL;
        return HTTPSERVER_OK;
    }
    (*result) = hash_cast(char, ptr->value);
    return HTTPSERVER_OK;
}

int header_is_set(hash_table *ht, char *key, char *val){
    const char *ret = NULL;
    int err = header_lookup(ht, key, &ret);
    if(err != HTTPSERVER_OK){
        return err;
    }

    if(ret == NULL){
        return 0;
    }
    return !strncmp(val, ret, strlen(val));
}

int header_insert(hash_table *ht, char *key, char *val){
    int err = hash_insert(ht, strdup(key), (void *) strdup(val), HASH_FREE_VAL | HASH_FREE_KEY);
    switch(err){
        case HASH_OK: return HTTPSERVER_OK;
        case HASH_EINPUT: return HTTPSERVER_EINPUT;
        case HASH_ENOMEM: return HTTPSERVER_ENOMEM;
        default: return HTTPSERVER_ERROR;
    }
}

int header_insert_mult(hash_table *ht, char *array[][2], size_t size){
    for(size_t i = 0; i<size; i++){
        char *key = array[i][0];
        char *value = array[i][1];
        int err = header_insert(ht, key, value);
        if(err != HTTPSERVER_OK){
            return err;
        }
    }
    return HTTPSERVER_OK;
}

hash_table header_create(int size){
    hash_table head;
    int err = hash_init(&head, size, NULL);
    if(err != HASH_OK){
        printf("error header hashtable\n");
        exit(1);
    }
    return head;
}

int header_set_date(hash_table *ht){
    time_t t = time(NULL); 
    if(t == -1){
        return HTTPSERVER_ERROR;
    }

    struct tm *utc = gmtime(&t);

    size_t len = strftime(NULL, 64, "%a, %d %b %Y %H:%M:%S GMT", utc);
    char *date = malloc(sizeof(char) * len + 1);
    if(date == NULL){
        return HTTPSERVER_ENOMEM;
    }
    strftime(date, len + 1, "%a, %d %b %Y %H:%M:%S GMT", utc);
    date[len] = 0;
    header_insert(ht, "Date", date);
    free(date);
    return HTTPSERVER_OK;
}

void header_set_keep_alive(hash_table *ht, int timeout, int max){
    int len = 20;
    if(max != 0){
        len += 18;
    }
    char str[len];
    if(max == 0){
        snprintf(str, len, "%s%d", "timeout=", timeout);
    }else{
        snprintf(str, len, "%s%d%s%d", "timeout=", timeout, ", max=", max);
    }

    header_insert(ht, "Keep-Alive", str);
}

void header_set_content_type(hash_table *ht, mime_type t){
    char *mime[] = { "image/x-icon", "image/jpeg", "image/png", "image/gif", \
    "text/plain", "text/html", "text/css", "text/javascript", "video/mp4"};

    header_insert(ht, "Content-Type", mime[t]);
}

int header_set_content_length(hash_table *ht, size_t length){
    int len = snprintf(NULL, 0, "%lu", length);
    char *cl = malloc(sizeof(char) * len + 1);
    if(cl == NULL){
        return HTTPSERVER_ENOMEM;
    } 
    snprintf(cl, len + 1, "%lu", length);
    header_insert(ht, "Content-Length", cl);
    free(cl);
    return HTTPSERVER_OK;
}

void header_set_general(hash_table *ht, char *sname, int keep_alive, int max_nr_req){
    header_set_date(ht);
    if(keep_alive){
        header_insert(ht, "Connection", "keep-alive");
        header_set_keep_alive(ht, keep_alive, max_nr_req);
    }else{
        header_insert(ht, "Connection", "close");
    }
    header_insert(ht, "Cache-Control", "no-cache");
    header_insert(ht, "Server", sname);
}

int header_len(hash_table *ht, const char* res_code){
    int col_br_len = strlen("\r\n"), sep_len = strlen(": ");
    int size = strlen(res_code) + col_br_len;
    for(int i = 0; i<ht->hashsize; i++){
        hash_entry *e = ht->table[i];
        if(e != NULL){
            size += strlen(e->key) + sep_len + strlen((char *) e->value) + col_br_len;
        }
    }
    size += col_br_len;
    return size;
}

int header_build(hash_table *ht, const char* res_code, char *buf, int size){
    if(buf == NULL || size <= 0){
        return -1;
    }
    char *sep = ": ", *col_br = "\r\n";
    int  sep_len = strlen(sep), col_br_len = strlen(col_br);
    int ind = strlen(res_code) + col_br_len;

    strncpy(buf, res_code, strlen(res_code));
    strncpy(&buf[strlen(res_code)], col_br, col_br_len);
    for(int i = 0; i<ht->hashsize; i++){
        hash_entry *e = ht->table[i];
        if(e != NULL){
            if(ind >= size){
                return -1;
            }
            strncpy(&buf[ind], e->key, strlen(e->key));
            ind += strlen(e->key);
            strncpy(&buf[ind], sep, sep_len);
            ind += sep_len;
            strncpy(&buf[ind], e->value, strlen(e->value));
            ind += strlen(e->value);
            strncpy(&buf[ind], col_br, col_br_len);
            ind += col_br_len;
        }
    }
    buf[ind] = '\r';
    buf[ind + 1] = '\n';
    //strncpy(&buf[ind], col_br, col_br_len);
    return 0;
}

mime_type res_to_mime(char *s){
    char *t = strchr(s, '.');
    t = &t[1];
    char *end[] = { "ico", "jpg", "png", "gif", "htm", "css", "js", "txt", "mp4"};
    if(strncmp(t, end[0], 3) == 0){
        return ico;
    }else if(strncmp(t, end[1], 3) == 0){
        return jpeg;
    }else if(strncmp(t, end[2], 3) == 0){
        return png;
    }else if(strncmp(t, end[3], 3) == 0){
        return gif;
    }else if(strncmp(t, end[4], 3) == 0){
        return html;
    }else if(strncmp(t, end[5], 3) == 0){
        return css;
    }else if(strncmp(t, end[6], 3) == 0){
        return js;
    }else if(strncmp(t, end[7], 3) == 0){
        return plain;
    }else if(strncmp(t, end[8], 3) == 0){
        return mp4;
    }else{
        return plain;
    }
}