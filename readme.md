FLV MF MediaSource and Progress Http downloading
===============================================

What you should know or what you care about
-----------

#### Async Programming

Media Foundation(MF)实现非常依赖异步编程。而且MF提供了一个异步编程模型，这个模型应该和C#的异步编程模型很相似，因为不懂C#异步模型，具体是否如此也不能确定。想要学习MF，首先应该熟悉一下MF的异步编程模型，对此很糊涂，后面的工作就会很迷糊。

程序没有使用C++11提供的异步模型(promise/future)，而是直接使用了微软提供的ppl。这是基于task的异步编程模型，相对于promise/future更易使用，而且如果你了解c#的async/await模式，ppl就很容易理解。ppl中的task/task_completion_event 基本上对应c++11中的promise/future。

##### Windows Overlapped IO to task event

有了ppl，我们很容易就能将Windows Overlapped IO转换到c++/ppl的task。微软为异步IO提供了新的线程池IO函数（TPIO）[refer]，使用线程池IO,使用系统的线程池管理，程序代码量会减少很多。代码写的越少，bugs数量就越少。

CPPRESTSDK提供async-fstream。这个东西好复杂，跟std::iostream一样复杂， 抽象的程度太高，学习成本很高，没有什么学习的欲望。直接使用WIN32API就几十行代码的事儿，简单明了。这也是我们需要实现自己的io task的原因。

#### Media Foundation or FLV/MP4 Format [flv.spec] [f4v.spec] [mp4.spec]

MF有自己的MP4 MediaSource。播放MP4根本不是问题。在线播放MP4不需要什么编码, MF原生支持MP4/H.264的回放。能够播放FLV是整个代码最原始的需求。我们也不是要解决所有的flv播放问题；除了使用AAC/H264编码的FLV，其他编码格式的FLV统统不支持；

FLV是容器格式，spec里面列出了支持的音视频编码格式，这些格式里面MF原生支持有限，但考虑到目前在线的FLV恐怕也不会自找麻烦的使用那些已经不再流行的编码格式。FLV相对于MP4 file format更简单一些，它没有MP4各种各样的盒子，编程过程会简单很多。F4V和FLV不同，F4V和MP4格式是一样的，基本编解码也相通。实现F4V的播放和MP4一样，不是问题。

##### Unsupported

- Encodings
- WebServer does not accept ranges requests

#### MediaSource and MediaStream State Transfer and MEEvents

''' c++
enum {
    OPENNING,
    OPENED,
    PLAYYING,
    PAUSE,
    STOPPED
    SHUTDOWN,
};
'''
> **OPENNING**
> : 还不知道具体的编解码格式和参数；FLV文件在读取到avcC[^avcC]和之后，状态转移到Opened

> **OPENED**
> : 成功构造pd，创建音视频流，完成EndOpen回调。等待Player的播放通知。

> **PLAYING** **PAUSE**, **STOPPED**
> : 播放器的基本状态，其中最需要注意状态转移过程中MediaSourceEvent和MediaStreamEvent发送的时机和条件。我是没心情弄清这个，基本比对MPEG-1 MediaSource和MSDN做的。

> **SHUTDOWN**
> : 同步调用，一旦出现不可恢复的错误，就能直接调用SHUTDOWN。而异步调用，出现错误后也需要调用SHUTDOWN。需要注意的是，什么样的错误需要发出MEError。糊涂，照MPEG-1办。

##### H.264 SEQUENCE HEADER

avcC数据结构直接作为media-type的sequence-header是没有效果的，直接将avcC交给H.264解码器也没有效果。实际上只有把START-CODE分割的SPS和PPS在第一帧之前送到解码器，视频流才能正确解码。这问题困扰了我很久，阅读代码的时候需要注意这样一段代码。


#### How to implement COM easily

#### Windows Runtime Libary C++ Template

#### How to use this

#### C++11


[^webm] : "webm"
[^home]: "at github"
[^MPEG-1]: "MPEG-1 Source"
[^WavSource]: "Wav Source"
[^WRL] : "WRL C++ Template"
[^RGS]: "rgs script and IRegistar"
[^Casablanca]: "CPP REST SDK"
[^FlvSpec]: "Flv format specification"
[^MP4Spec]: "MP4 format specification"

MediaSource
-----------

ByteStreamHandler
------------

Registry Location
------------

Http Client
----------

Progressive Download Problems
----------

Scheme Handler and Registry Location
----------
