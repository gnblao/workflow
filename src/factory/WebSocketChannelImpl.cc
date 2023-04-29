/*************************************************************************
    > File Name: WebSocketChannelImpl.h
    > Author: gnblao
    > Mail: gnblao
    > Created Time: 2022年04月10日 星期日 17时16分19秒
 ************************************************************************/

#include "WebSocketChannelImpl.h"
#include "HttpMessage.h"
#include "ProtocolMessage.h"
#include "SubTask.h"
#include "WFChannel.h"
#include "WFChannelMsg.h"
#include "WFGlobal.h"
#include "WFTask.h"
#include "WFTaskFactory.h"
#include "WebSocketMessage.h"
#include "Workflow.h"
#include "websocket_parser.h"
#include <cstddef>
#include <functional>
#include <iostream>

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <string>

__attribute__((unused)) static int __base64decode(std::string &base64, std::string &src) {
    size_t ret_len;
    char *p;

    if (base64.length() % 4 != 0 || base64.length() > INT_MAX)
        return -1;

    ret_len = ((base64.length() / 4) * 3);
    p = (char *)malloc(ret_len + 1);
    if (!p)
        return -1;

    if (EVP_DecodeBlock((uint8_t *)p, (uint8_t *)base64.c_str(), (int)base64.length()) == -1) {
        free(p);
        return -1;
    }

    if (base64.length() > 1 && ((char *)(base64.c_str()))[base64.length() - 1] == '=') {
        if (base64.length() > 2 && ((char *)(base64.c_str()))[base64.length() - 2] == '=')
            ret_len -= 2;
        else
            ret_len -= 1;
    }

    src.append(p, ret_len);
    free(p);

    return 0;
}

static int __base64encode(std::string &src, std::string &base64) {
    char *ret;
    size_t ret_len, max_len;

    if (src.length() > INT_MAX)
        return -1;

    max_len = (((src.length() + 2) / 3) * 4) + 1;
    ret = (char *)malloc(max_len);
    if (!ret)
        return -1;

    ret_len = EVP_EncodeBlock((uint8_t *)ret, (uint8_t *)src.c_str(), (int)src.length());
    if (ret_len >= max_len) {
        free(ret);
        return -1;
    }
    ret[ret_len] = 0;

    base64.append(ret, ret_len);

    free(ret);
    return 0;
}

static std::string __base64encode(std::string &src) {
    std::string base64;
    __base64encode(src, base64);
    return base64;
}

static inline std::string __sha1_bin(const std::string &str) {
    unsigned char md[20];

    EVP_Digest(str.c_str(), str.size(), md, NULL, EVP_sha1(), NULL);
    return std::string((const char *)md, 20);
}

int WebSocketChannel::send_ping() {
    int ping = this->gen_masking_key();
    return this->send_frame((char *)&ping, sizeof(int), sizeof(int), WebSocketFramePing);
}


int WebSocketChannel::send_close(short status_code) {
    this->handshake_status = WS_HANDSHAKE_CLOSING;
    return this->send_frame((char *)&status_code, sizeof(short), sizeof(short),
            WebSocketFrameConnectionClose);
}

int WebSocketChannel::send_text(const char *data, size_t size) {
   
    return this->send_frame(data, size, size, WebSocketFrameText);
}

int WebSocketChannel::send_binary(const char *data, size_t size) {
    return this->send_frame(data, size, size, WebSocketFrameBinary);
}

int WebSocketChannel::send_frame(const char* buf, int len, int fragment, enum ws_opcode opcode, std::function<void()> bc) {
    //std::lock_guard<std::mutex> locker(mutex_);
    if (len <= fragment) {
        return this->__send_frame(buf, len, opcode, true);
    }

    // first fragment
    int nsend = this->__send_frame(buf, fragment, opcode, false);
    if (nsend < 0) return nsend;

    const char* p = buf + fragment;
    int remain = len - fragment;
    while (remain > fragment) {
        nsend = this->__send_frame(p, fragment, WebSocketFrameContinuation, false);
        if (nsend < 0) return p - buf;
        p += fragment;
        remain -= fragment;
    }

    // last fragment
    nsend = this->__send_frame(p, remain, WebSocketFrameContinuation, true,
            [bc](WFChannelMsg<protocol::WebSocketFrame>*){bc();});
    if (nsend < 0) return p - buf;

    return len;
}

int WebSocketChannel::__send_frame(const char *data, int size, enum ws_opcode opcode, bool fin,
                               std::function<void(WFChannelMsg<protocol::WebSocketFrame>*)> cb, 
                               protocol::WebSocketFrame *in)  {
    int ret;

    auto *task = this->channel->safe_new_channel_msg<WSFrame>();
    if (!task)
        return -1;
    
    auto msg = task->get_msg();
    
    ret = msg->set_frame(data, size, opcode, fin);
    if (ret < 0) {
        delete task;
        return -1;
    }

    if (!this->channel->is_server()) 
        msg->set_masking_key(this->gen_masking_key());
   
    if (cb)
        task->set_callback(std::move(cb));

    task->start();
    return size;
}

int WebSocketChannel::process_ping(protocol::WebSocketFrame *in = nullptr) {
    this->__send_frame(
            (char *)in->get_parser()->payload_data, in->get_parser()->payload_length,
            WebSocketFramePong, true, nullptr, in);
    return 0;
}

int WebSocketChannel::process_close(protocol::WebSocketFrame *in) {
    if (this->handshake_status == WS_HANDSHAKE_CLOSING) {
        this->handshake_status = WS_HANDSHAKE_CLOSED;
        this->channel->shutdown();
    } else {
        this->handshake_status = WS_HANDSHAKE_CLOSING;

        this->__send_frame(
            (char *)in->get_parser()->payload_data, in->get_parser()->payload_length,
            WebSocketFrameConnectionClose, true, 
            [this](WFChannelMsg<protocol::WebSocketFrame> *) { this->channel->shutdown(); }, in);
    }

    return 0;
}

int WebSocketChannelClient::send_header_req() {
    // WSHearderReq *task = new WSHearderReq(this, nullptr);
    WSHearderReq *task = this->safe_new_channel_msg<WSHearderReq>();
    
    if (!task) {
        return -1;
    }

    std::string request_uri;
    std::string header_host;
    bool is_ssl = false;

    if (uri_.scheme && strcasecmp(uri_.scheme, "wss") == 0)
        is_ssl = true;

    if (uri_.path && uri_.path[0])
        request_uri = uri_.path;
    else
        request_uri = "/";

    if (uri_.query && uri_.query[0]) {
        request_uri += "?";
        request_uri += uri_.query;
    }

    if (uri_.host && uri_.host[0]) {
        header_host = uri_.host;
    }

    if (uri_.port && uri_.port[0]) {
        int port = atoi(uri_.port);

        if (is_ssl) {
            if (port != 443) {
                header_host += ":";
                header_host += uri_.port;
            }
        } else {
            if (port != 80) {
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

int WebSocketChannelServer::process_header_req(protocol::HttpRequest *req) {
    //WSHearderRsp *task = new WSHearderRsp(this);
    WSHearderRsp *task = this->safe_new_channel_msg<WSHearderRsp>();
    if (!task)
        return -1;

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
    while (cursor.next(name, value)) {
        if (!name.compare(WS_HTTP_SEC_KEY_K)) {
            value = __sha1_bin(value + WS_GUID_RFC4122);
            value = __base64encode(value);
            rsp->add_header_pair("Sec-WebSocket-Accept", value);
            break;
        }
    }
    
    task->start();
    return 0;
}

