/*
  Copyright (c) 2021 Sogou, Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  Author: Li Yingxin (liyingxin@sogou-inc.com)
*/

#include <endian.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include "mysql_byteorder.h"
#include "websocket_parser.h"

static inline unsigned long long htonll(unsigned long long val)
{
    if(__BYTE_ORDER == __LITTLE_ENDIAN)  
    {
         return (((unsigned long long )htonl((int)((val << 32) >> 32))) << 32) | (unsigned int)htonl((int)(val >> 32));  
    }  
    else if (__BYTE_ORDER == __BIG_ENDIAN)  
    {  
         return val;  
    }  
}  

//网络序转主机序
static inline unsigned long long ntohll(unsigned long long val)  
{  
    if (__BYTE_ORDER == __LITTLE_ENDIAN)
    {
        return (((unsigned long long )ntohl((int)((val << 32) >> 32))) << 32) | (unsigned int)ntohl((int)(val >> 32));  
    }  
    else if (__BYTE_ORDER == __BIG_ENDIAN)  
    {  
        return val;  
    }
 }

void websocket_parser_init(websocket_parser_t *parser)
{
	parser->fin = 0;
	parser->mask = 0;
	parser->opcode = -1;
	parser->nleft = 0;
	parser->payload_length = 0;
	parser->payload_data = NULL;
	parser->nreceived = 0;
	parser->is_server = 0;
	parser->status_code = WSStatusCodeUndefined;
	parser->masking_key_offset = 0;
	memset(parser->masking_key, 0, WS_MASKING_KEY_LENGTH);
	memset(parser->header_buf, 0, WS_MAX_HEADER_LENGTH);
    parser->status = WS_HEADER_DOING;
}

void websocket_parser_deinit(websocket_parser_t *parser)
{
	if (parser->payload_length != 0)
		free(parser->payload_data);
}

int websocket_parser_append_message(const void *buf, size_t *n,
									websocket_parser_t *parser)
{
	const unsigned char *b = (const unsigned char *)buf;
	const unsigned char *buf_end = (const unsigned char *)buf + *n;
    
    unsigned char *p;

    if (parser->status == WS_HEADER_DOING) {
		memcpy(parser->header_buf + parser->nreceived, b,
			   WS_MAX_HEADER_LENGTH - parser->nreceived);

        if (parser->nreceived + *n < WS_BASIC_HEADER_LENGTH + parser->nleft)
		{
			parser->nreceived += *n;
			return 0;
		} 
        else 
        {
            int tmp = WS_BASIC_HEADER_LENGTH + parser->nleft - parser->nreceived;
            tmp = *n > tmp ? tmp : *n;
            b += tmp;
            parser->nreceived += tmp;

            p = parser->header_buf;
            parser->fin = *p >> 7;
            parser->opcode = *p & 0xF;
            p++;
            parser->mask = *p >> 7;
            parser->payload_length = *p & 0x7F;
            p++;
            
            if (parser->payload_length == 126)
                parser->nleft = 2;
            else if (parser->payload_length == 127) 
                parser->nleft = 8;
            else
                parser->nleft = 0;
            
            if (parser->mask)
                parser->nleft += 4;
            
            if (buf_end - b < WS_BASIC_HEADER_LENGTH + parser->nleft - parser->nreceived) {
                parser->nreceived += buf_end - b; 
                return 0;
            }
            
            b += WS_BASIC_HEADER_LENGTH + parser->nleft - parser->nreceived;
            parser->nreceived = WS_BASIC_HEADER_LENGTH + parser->nleft;

            if (parser->nleft == 2 || parser->nleft == 6) {
                uint16_t *len_ptr = (uint16_t *)p;
                parser->payload_length = ntohs(*len_ptr);
                p += 2;
            }
            
            if (parser->nleft == 8 || parser->nleft == 12) {
                uint64_t *len_ptr = (uint64_t *)p;
                parser->payload_length = ntohll(*len_ptr);
                p += 8;
            }

            if (parser->nleft == 4 || parser->nleft == 6 || parser->nleft == 12) {
                memcpy(parser->masking_key, p, WS_MASKING_KEY_LENGTH);
            }

            parser->status = WS_FRAME_DOING;
            parser->nleft = parser->payload_length;
            
            //printf("parser->payload_length:%lld\n", parser->payload_length);	
            
            if (parser->nleft == 0) {
                parser->status = WS_FRAME_DONE;
                return 1;
            }
            parser->payload_data = malloc(parser->payload_length);
        }
    }

    if (parser->status == WS_FRAME_DOING) {
        if (!parser->payload_data)
            return -1;

        if (buf_end - b < parser->nleft)
        {
            memcpy(parser->payload_data + parser->payload_length - parser->nleft,
                    b, buf_end - b);
            
            parser->nleft -= (buf_end - b);
            parser->nreceived += buf_end - b;
            return 0;
        }
        else
        {
            memcpy(parser->payload_data + parser->payload_length - parser->nleft,
                    b, parser->nleft);
            b += parser->nleft;
            *n -= (buf_end - b);
            
            parser->nreceived += parser->nleft;
            parser->nleft = 0;
            return 1;
        }
    }

    return 0;
}

int websocket_parser_parse(websocket_parser_t *parser)
{
	if (parser->opcode < WebSocketFrameContinuation || 
		(parser->opcode < WebSocketFrameConnectionClose &&
		 parser->opcode > WebSocketFrameBinary) ||
		parser->opcode > WebSocketFramePong)
	{
		parser->status_code = WSStatusCodeProtocolError;
		return -1;
	}

	unsigned char *p = (unsigned char *)parser->payload_data;

	if (parser->opcode == WebSocketFrameConnectionClose)
	{	
		uint16_t *ptr = (uint16_t *)p;
		parser->status_code = ntohs(*ptr);
	}

	websocket_parser_mask_data(parser);

	if (parser->opcode == WebSocketFrameText &&
		parser->payload_length && !utf8_check(p, parser->payload_length))
	{
		parser->status_code = WSStatusCodeUnsupportedData;
		return -1;
	}

	return 0;
}

void websocket_parser_mask_data(websocket_parser_t *parser)
{
	if (!parser->mask)
		return;

	unsigned long long i;
	unsigned char *p = (unsigned char *)parser->payload_data;

	for (i = 0; i < parser->payload_length; i++)
		*p++ ^= parser->masking_key[i % 4];

	return;
}

//https://www.cl.cam.ac.uk/~mgk25/ucs/utf8_check.c
unsigned char *utf8_check(unsigned char *s, size_t len)
{
  unsigned char *end = s + len;
  while (*s && s != end) {
    if (*s < 0x80)
      /* 0xxxxxxx */
      s++;
    else if ((s[0] & 0xe0) == 0xc0) {
      /* 110XXXXx 10xxxxxx */
      if ((s[1] & 0xc0) != 0x80 ||
	  (s[0] & 0xfe) == 0xc0)                        /* overlong? */
	return s;
      else
	s += 2;
    } else if ((s[0] & 0xf0) == 0xe0) {
      /* 1110XXXX 10Xxxxxx 10xxxxxx */
      if ((s[1] & 0xc0) != 0x80 ||
	  (s[2] & 0xc0) != 0x80 ||
	  (s[0] == 0xe0 && (s[1] & 0xe0) == 0x80) ||    /* overlong? */
	  (s[0] == 0xed && (s[1] & 0xe0) == 0xa0) ||    /* surrogate? */
	  (s[0] == 0xef && s[1] == 0xbf &&
	   (s[2] & 0xfe) == 0xbe))                      /* U+FFFE or U+FFFF? */
	return s;
      else
	s += 3;
    } else if ((s[0] & 0xf8) == 0xf0) {
      /* 11110XXX 10XXxxxx 10xxxxxx 10xxxxxx */
      if ((s[1] & 0xc0) != 0x80 ||
	  (s[2] & 0xc0) != 0x80 ||
	  (s[3] & 0xc0) != 0x80 ||
	  (s[0] == 0xf0 && (s[1] & 0xf0) == 0x80) ||    /* overlong? */
	  (s[0] == 0xf4 && s[1] > 0x8f) || s[0] > 0xf4) /* > U+10FFFF? */
	return s;
      else
	s += 4;
    } else
      return s;
  }

  if (s == end)
    return s;

  return NULL;
}

