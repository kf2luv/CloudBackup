# CloudBackup
云备份共享网盘项目
![](https://ckfs.oss-cn-beijing.aliyuncs.com/img/202501200226629.png)


## 扩展1：实现用户隔离

**实现原理**

 每个用户在备份包文件夹`backup_dir`和压缩包文件夹`pack_dir`中，各自拥有一个属于自己的子文件夹（用户注册时创建）。用户上传文件和下载文件都是在属于自己的子文件夹中操作。

 简单起见，这里把用户子文件夹的名称设置为用户id。服务器文件数据存储结构如下：

 ![](https://ckfs.oss-cn-beijing.aliyuncs.com/img/202501200200141.png)
 如图，当前只存储了3号用户和4号用户的文件备份，且都已被压缩。

 综上，备份文件元信息的意义也要发生改变
 ![](https://ckfs.oss-cn-beijing.aliyuncs.com/img/202501200222033.png)

 **Session**

为了让服务器能够分辨发送请求的是哪个用户，这里用到了Session机制（会话级别的SessionID，浏览器关闭时失效）。为此，我在服务端添加了用户管理器`userManager`，管理用户Session会话ID，其中用了一个哈希表来管理Session ID和用户ID的映射，这样服务端就能够根据Session ID获取用户id，判断当前处理的是哪个用户。

客户端（浏览器）登录后，视为与服务端建立了一次新会话，服务端向客户端返回一个唯一的Session ID（Set-Cookie实现），用于标识此次对话。由于浏览器的Cookie缓存机制，本次会话所有请求头都会携带此Session ID，浏览器关闭，因此会话功能得以实现。

当然，客户端这边的Session ID可能会失效（服务端清除Session ID，因为会话过期或者其它问题），所以服务端收到客户端的请求后，先检查Session ID是否有效，无效则重定向到登录界面，让用户重新登录，获取新的Session ID。

后续打算再解决服务器Session ID过多的问题，采用定期清理的方法。