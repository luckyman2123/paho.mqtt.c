MQTTAsyncUtils.c -> WebSocket.c -> socket.c

MQTTAsync_connect -> MQTTAsync_receiveThread -> MQTTAsync_cycle　－>  MQTTAsync_connecting ->  WebSocket_connect



SocketBuffer_queueChar -> 

MQTTAsync_sendThread -> MQTTProtocol_connect-> Socket_new

订阅

MQTTProtocol_subscribe　-> MQTTPacket_send_subscribe -> MQTTPacket_send -> WebSocket_putdatas -> WebSocket_buildFrame 

​				-> Socket_putdatas -> Socket_writev -> writev 



MQTTAsync_processCommand: 处理发送的消息

​	

Socket_outInitialize -> SocketBuffer_initialize

​	