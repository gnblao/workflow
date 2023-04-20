/*************************************************************************
    > File Name: StreamMessage.h
    > Author: gnblao
    > Mail: gnbalo
    > Created Time: 2023年04月18日 星期二 21时52分20秒
 ************************************************************************/

#ifndef _PROTOCOL_STREAMMESSAGE_H_
#define _PROTOCOL_STREAMMESSAGE_H_

#include "ProtocolMessage.h"
#include "stream_parser.h"
#include <cstddef>

namespace protocol
{

class StreamMessage : public ProtocolMessage
{
public:
	const stream_parser_t *get_parser() { return this->parser; }

    virtual int append_fill(const void *data, size_t size) {
        size_t n = size;
        return this->append(data, &n);
    };

protected:
	virtual int encode(struct iovec vectors[], int max);
	virtual int append(const void *buf, size_t *size);

public:
	StreamMessage() : parser(new stream_parser_t)
	{
		stream_parser_init(this->parser);
	}

	virtual ~StreamMessage()
	{
		stream_parser_deinit(this->parser);
		delete this->parser;
	}

	StreamMessage(const StreamMessage& msg) = delete;
	StreamMessage& operator = (const StreamMessage& msg) = delete;
	
    StreamMessage(StreamMessage&& msg);
	StreamMessage& operator = (StreamMessage&& msg);

private:
	stream_parser_t *parser;
};


}
#endif  // _PROTOCOL_STREAMMESSAGE_H_
