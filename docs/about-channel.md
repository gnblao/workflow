# Channel


基于Sogou/Workflow扩展的全双工通讯方案

* 半双工和全双工通信.  
	* 半双工：req->rsp->req->rsp->req->rsp  
	* 全双工：req->req->req->rsp->req->rsp->rsp-> 或者是csjcjhbvvjbbdsb->rsp

* 半双工协议：http1.X 。。。。。  
* 全双工协议：http2 websocket quic 。。。。。  

全双工协议有个特点在tcp/udp之上为了复用一个链接，搞一个传输frame层。在frame之上堆stream和req之类的.  

目前对半双工的通用通讯架构比较多，对于全双工的基本上都是独立实现，并且没有很好解决io与计算之间异步融合的问题。
[sogou/workflow](https://github.com/sogou/workflow) 框架在这方面的设计，使用session和subtask来解决融合io和计算的异步问题，让人眼前一亮。
所以想在wf架构基础上支持全双工通信。

## channel逻辑实现
* https://github.com/gnblao/workflow/tree/channel


## channel支持协议
* protocol
    * [websocket](about-websocket.md)
    * [stream (TODO)]
    * [quic (TODO)]

### 杂项
* [channel实现过程中的一些细节](https://github.com/sogou/workflow/issues/873) 
