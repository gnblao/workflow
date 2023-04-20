/*************************************************************************
    > File Name: stream_parser.h
    > Author: gnblao
    > Mail: gnbalo
    > Created Time: 2023年04月18日 星期二 21时23分20秒
 ************************************************************************/

#ifndef _PROTOCOL_STREAM_PARSER_H_
#define _PROTOCOL_STREAM_PARSER_H_

#include <stddef.h>

typedef struct __stream_parser
{
	int size;
    void *data;
} stream_parser_t;


#ifdef __cplusplus
extern "C"
{
#endif

void stream_parser_init(stream_parser_t *parser);
void stream_parser_deinit(stream_parser_t *parser);
int stream_parser_append_message(const void *buf, size_t *n,
									stream_parser_t *parser);

#ifdef __cplusplus
}
#endif



#endif  // _PROTOCOL_STREAM_PARSER_H_
