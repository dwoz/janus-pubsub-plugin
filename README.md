Janus PubSub Plugin
===================

Janus PubSub Plugin is plugin for Janus WebRTC Gateway. The plugin provides
some similar functionality found in the janus core plugin set, but with a
different api and symantics. A handle for this plugin can be a publisher or
subscriber. Publisher publish thier media to a stream with a name. Subscribers
use the publisher's name to subscribe to a publisher's stream. Multiple
subscribers can subscribe to single publisher.  Subscribers can be Janus WebRTC
Sessions or RTP forwarders.


Publish request
---------------


```
{'message': {'request': 'publish', 'name': 'stream 1'}}
```


Subscribe request
-----------------


```
{'message': {'request': 'subscribe', 'name': 'stream 1'}}
```
