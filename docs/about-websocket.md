# 这是基于channel实现的websocket协议  
（ws的特性实现不完善，有想法的小伙伴，可以玩玩～～）


git：
https://github.com/gnblao/workflow/tree/channel

## 关于channel的一些想法
  （https://github.com/sogou/workflow/issues/873）


## 现有websocket的两个dome  
server：/tutorial/tutorial-22-ws_echo_server.cc  
client：/tutorial/tutorial-14-websocket_cli.cc  

[编译和安装]（https://github.com/sogou/workflow#readme）   


## ws
srv：
./ws_echo_server 5679

cli：
./websocket_cli ws://127.0.0.1:5679

## wss
srv：
./ws_echo_server 5679 server.crt server.key
（注：openssl req -new -x509 -keyout server.key -out server.crt -config openssl.cnf）
cli：
./websocket_cli wss://127.0.0.1:5679


欢迎提一些issues和🧱，和有趣的想法～～～
