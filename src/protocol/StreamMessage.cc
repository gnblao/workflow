/*************************************************************************
    > File Name: StreamMessage.cc
    > Author: gnblao
    > Mail: gnbalo
    > Created Time: 2023年04月18日 星期二 22时04分12秒
 ************************************************************************/

#include "stream_parser.h"
#include "StreamMessage.h"

namespace protocol 
{

StreamMessage::StreamMessage(StreamMessage&& msg) :
	ProtocolMessage(std::move(msg))
{
	this->parser = msg.parser;
	msg.parser = NULL;
}

StreamMessage& StreamMessage::operator = (StreamMessage&& msg)
{
	if (&msg != this)
	{
		*(ProtocolMessage *)this = std::move(msg);
		if (this->parser)
		{
			stream_parser_deinit(this->parser);
			delete this->parser;
		}

		this->parser = msg.parser;
		msg.parser = NULL;
	}

	return *this;
}

int StreamMessage::append(const void *buf, size_t *size)
{
	return stream_parser_append_message(buf, size, this->parser);
}


int StreamMessage::encode(struct iovec vectors[], int max)
{
    int cnt = 0;

    if (this->parser->size) {
        vectors[cnt].iov_base = this->parser->data;
        vectors[cnt].iov_len = this->parser->size;
        cnt++;
    }

    return cnt;
}

}

