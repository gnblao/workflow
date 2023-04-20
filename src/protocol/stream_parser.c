/*************************************************************************
    > File Name: stream_parser.c
    > Author: gnblao
    > Mail: gnbalo
    > Created Time: 2023年04月18日 星期二 21时26分59秒
 ************************************************************************/


#include <stdlib.h>
#include <string.h>
#include "stream_parser.h"

#define STREAM_MAX_SIZE (1UL<<20) // 1M

void stream_parser_init(stream_parser_t *parser) {
    parser->size = 0;
    parser->data = NULL;
}

void stream_parser_deinit(stream_parser_t *parser)
{
	if (parser->size != 0) {
		free(parser->data);
        parser->data = NULL;
    }
}

int stream_parser_append_message(const void *buf, size_t *n,
									stream_parser_t *parser)
{
    int len;
    if (*n > STREAM_MAX_SIZE)
        len = STREAM_MAX_SIZE;
    else
        len = *n;
    
    parser->data = malloc(len);
    if (!parser->data) 
        return -1;
    
    memcpy(parser->data, buf, len);
    *n = len;
    parser->size = len;

    return 1;
}	

