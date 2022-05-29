/*************************************************************************
    > File Name: WebSocketChannelImpl.h
    > Author: gnblao 
    > Mail: gnblao
    > Created Time: 2022年04月10日 星期日 17时16分19秒
 ************************************************************************/

#include <cstddef>
#include <functional>
#include <iostream>
#include "HttpMessage.h"
#include "ProtocolMessage.h"
#include "SubTask.h"
#include "WFChannelMsg.h"
#include "WFTask.h"
#include "WFGlobal.h"
#include "WFChannel.h"
#include "WebSocketMessage.h"
#include "Workflow.h"
#include "WFTaskFactory.h"
#include "WebSocketChannelImpl.h"
#include "websocket_parser.h"

#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <string>


static int __base64decode(std::string& base64, std::string &src) {
	size_t ret_len;
    char *p;

	if (base64.length() % 4 != 0 || base64.length() > INT_MAX)
		return -1;

	ret_len = ((base64.length() / 4) * 3);
	p = (char *)malloc(ret_len + 1);
	if (!p)
		return -1;

	if (EVP_DecodeBlock((uint8_t*)p, (uint8_t*)base64.c_str(),
						(int)base64.length()) == -1)
	{
		free(p);
		return -1;
	}

	if (base64.length() > 1 && ((char *)(base64.c_str()))[base64.length() - 1] == '=')
	{
		if (base64.length() > 2 && ((char *)(base64.c_str()))[base64.length() - 2] == '=')
			ret_len -= 2;
		else
			ret_len -= 1;
	}

    src.append(p, ret_len);
    free(p);
	
    return 0;
}

static int __base64encode(std::string& src, std::string &base64)
{
	char *ret;
	size_t ret_len, max_len;

	if (src.length() > INT_MAX)
		return -1;

	max_len = (((src.length() + 2) / 3) * 4) + 1;
	ret = (char *)malloc(max_len);
	if (!ret)
		return -1;

	ret_len = EVP_EncodeBlock((uint8_t *)ret, (uint8_t *)src.c_str(),
							  (int)src.length());
	if (ret_len >= max_len)
	{
		free(ret);
		return -1;
	}
	ret[ret_len] = 0;
    
    base64.append(ret, ret_len);

    free(ret);
	return 0;
}

static std::string __base64encode(std::string& src)
{
    std::string base64;
    __base64encode(src, base64);
    return base64;
}

static inline std::string __sha1_bin(const std::string& str)
{
	unsigned char md[20];

    EVP_Digest(str.c_str(), str.size(), md, NULL, EVP_sha1(), NULL);
	return std::string((const char *)md, 20);
}

int WebSocketTools::send_ping()
{
    auto *task = new WSFrame(this->channel, nullptr);

    auto msg = task->get_msg();
    msg->set_opcode(WebSocketFramePing);
    
    if (this->channel->is_server()) {
        msg->set_server();
        int ping = this->gen_masking_key();
        msg->append_data((char *)&ping, 4);
    } else {
        msg->set_masking_key(this->gen_masking_key());  
        msg->set_client();
    }

    task->start();
    return 0;
}

int WebSocketTools::send_pong()
{
    return 0;
}

int WebSocketTools::send_close(int status_code, std::string str)
{
    auto *task = new WSFrame(this->channel, nullptr);

    auto msg = task->get_msg();
    msg->set_opcode(WebSocketFrameConnectionClose);
    msg->set_masking_key(this->gen_masking_key());  
    msg->set_status_code_data(status_code, str.c_str(), str.length());

    task->start();
    
    this->handshake_status = WS_HANDSHAKE_CLOSING;

    return 0;
}

int WebSocketTools::send_text(const char *data, size_t size)
{
    auto *task = new WSFrame(this->channel, nullptr);

    auto msg = task->get_msg();
    msg->set_opcode(WebSocketFrameText);
    msg->set_masking_key(this->gen_masking_key());  

    msg->set_text_data(data, size, true);

    task->start();
    return 0;
}

int WebSocketTools::send_binary(const char *data, size_t size) 
{
    auto *task = new WSFrame(this->channel, nullptr);

    auto msg = task->get_msg();
    msg->set_opcode(WebSocketFrameText);
    msg->set_masking_key(this->gen_masking_key());  
    msg->set_text_data(data, size, true);

    task->start();
    return 0;
}

int WebSocketTools::process_ping(protocol::WebSocketFrame *in = nullptr) 
{
    auto *task = new WSFrame(this->channel, nullptr);

    auto msg = task->get_msg();
    msg->set_opcode(WebSocketFramePong);
    if (this->channel->is_server()) {
        msg->set_server();
    } else {
        msg->set_masking_key(this->gen_masking_key());  
        msg->set_client();
    }

    if (in){
        msg->set_data(in->get_parser());
    }
    
    series_of(dynamic_cast<WSFrame *>(in->session))->push_back(task);
    return 0;
}

int WebSocketTools::process_close(protocol::WebSocketFrame *in) 
{
    if (this->handshake_status == WS_HANDSHAKE_CLOSING)
    {
        this->handshake_status = WS_HANDSHAKE_CLOSED;
        this->channel->shutdown();   
    } else { 
        this->handshake_status = WS_HANDSHAKE_CLOSING;

        auto *task = new WSFrame(this->channel, nullptr);

        auto msg = task->get_msg();
        msg->set_opcode(WebSocketFrameConnectionClose);
        msg->set_masking_key(this->gen_masking_key());  

        if (in){
            msg->set_data(in->get_parser());
        }

        task->set_callback([this] (WFChannelMsg<protocol::WebSocketFrame>*) { this->channel->shutdown();});
        series_of(dynamic_cast<WSFrame *>(in->session))->push_back(task);
    }

    return 0;
}

int WebSocketChannelClient::send_header_req(WFChannel*) 
{
    this->set_prepare(nullptr);
    WSHearderReq *task = new WSHearderReq(this, nullptr);

    std::string request_uri;
    std::string header_host;
    bool is_ssl = false;

    if (uri_.scheme && strcasecmp(uri_.scheme, "wss") == 0)
        is_ssl = true;
    
    if (uri_.path && uri_.path[0])
        request_uri = uri_.path;
    else
        request_uri = "/";

    if (uri_.query && uri_.query[0])
    {
        request_uri += "?";
        request_uri += uri_.query;
    }

    if (uri_.host && uri_.host[0])
    {
        header_host = uri_.host;
    }

    if (uri_.port && uri_.port[0])
    {
        int port = atoi(uri_.port);

        if (is_ssl)
        {
            if (port != 443)
            {
                header_host += ":";
                header_host += uri_.port;
            }
        }
        else
        {
            if (port != 80)
            {
                header_host += ":";
                header_host += uri_.port;
            }
        }
    }

    protocol::HttpRequest *req = task->get_msg();
    req->set_method(HttpMethodGet);
    req->set_http_version("HTTP/1.1");
    req->set_request_uri(request_uri);
    req->add_header_pair("Host", header_host);
    req->add_header_pair("Upgrade", "websocket");
    req->add_header_pair("Connection", "Upgrade");
    req->add_header_pair(WS_HTTP_SEC_KEY_K, WS_HTTP_SEC_KEY_V);

    if (this->get_sec_protocol().length())
        req->add_header_pair(WS_HTTP_SEC_PROTOCOL_K, this->get_sec_protocol());
    if (this->get_sec_version().length())
        req->add_header_pair(WS_HTTP_SEC_VERSION_K, this->get_sec_version());

    task->set_state(WFC_MSG_STATE_OUT_LIST);
    task->start();
    return 0;
}


int WebSocketChannelClient::process_text(protocol::WebSocketFrame *in) 
{
    const char *data;
    size_t len = 0;
    if (in){
        if (in->get_data(&data, &len)) {
            std::cout << std::string(data, len)  << std::endl; 
        }
    }
 
    return 0;
}

int WebSocketChannelServer::process_text(protocol::WebSocketFrame *in) 
{
    auto *task = new WSFrame(this, nullptr);

    auto msg = task->get_msg();
    msg->set_opcode(WebSocketFrameText);

    if (this->is_server()) {
        msg->set_server();
    } else {
        msg->set_masking_key(this->gen_masking_key());  
        msg->set_client();
    }
    
    const char *data;
    size_t len = 0;
    if (in){
        if (in->get_data(&data, &len)) {
            std::cout << "-----data len:" << len << std::endl; 
            msg->set_text_data(data, len, true);
        }
    }
    
    msg->set_fin(true);
    
    series_of(dynamic_cast<WSFrame *>(in->session))->push_back(task);
    return 0;
}

int WebSocketChannelServer::process_header_req(protocol::HttpRequest *req)
{
    WSHearderRsp *task = new WSHearderRsp(this, nullptr);

    protocol::HttpResponse *rsp = task->get_msg();
    rsp->set_http_version("HTTP/1.1");
    rsp->set_status_code("101");
    rsp->set_reason_phrase("Switching Protocols");
    rsp->add_header_pair("Upgrade", "websocket");
    rsp->add_header_pair("Connection", "Upgrade");


    if (this->get_sec_protocol().length())
        rsp->add_header_pair(WS_HTTP_SEC_PROTOCOL_K, this->get_sec_protocol());
    if (this->get_sec_version().length())
        rsp->add_header_pair(WS_HTTP_SEC_VERSION_K, this->get_sec_version());

    std::string name;
    std::string value;

    protocol::HttpHeaderCursor cursor(req);
    while (cursor.next(name, value))
    {
        if (!name.compare(WS_HTTP_SEC_KEY_K)) {
            value = __sha1_bin(value+WS_GUID_RFC4122);
            value = __base64encode(value);
            rsp->add_header_pair("Sec-WebSocket-Accept", value);
            break;
        }
    }

    series_of(dynamic_cast<WSHearderReq *>(req->session))->push_back(task);
    return 0;
}


