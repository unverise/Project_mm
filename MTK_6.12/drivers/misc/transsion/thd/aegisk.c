/*Transsion Top Secret*/
/*
 * Copyright (C) 2025 Transsion Inc.All rights reserved.
 *
 * @author chonglin.yang
 */
#include "aegisk.h"
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <linux/string.h>
#include "thd_util.h"

#define NETLINK_AEGIS 25
#define MAX_STR_LEN 256
#define MAX_PARAMS 3
#define AUTH_TOKEN "AEGIS_SECURE_TOKEN_2025"

// 内核到用户态消息类型枚举
enum kernel_msg_type {
    KERNEL_MSG_START_AEGIS = 0x1000,     // 启动AEGIS
    KERNEL_MSG_REGISTER_LISTENER,        // 注册监听器
    KERNEL_MSG_UNREGISTER_LISTENER,      // 取消注册监听器
    KERNEL_MSG_UPLOAD_BINDER_INFO,        // 上传Binder信息
    KERNEL_MSG_AUTH_RESPONSE             // 认证响应
};

// 用户态到内核消息类型枚举
enum user_msg_type {
    USER_MSG_AUTH_AND_INIT_REQUEST = 0x2000, // 初始化和认证请求
    USER_MSG_DISCONNECT,                     // 断开连接
    USER_MSG_CALL_BACK_EVENT,                // 事件通知
    USER_MSG_TRACE_BINDER_INFO,              // 追踪Binder信息
};

// 消息结构
struct aegis_nlmsg {
    int type;
    char params[MAX_PARAMS][MAX_STR_LEN];
};

// 客户端信息结构
struct aegis_client {
    pid_t pid;
    bool connected;
    bool authenticated;
    bool trace_binder_supported;
};

static struct sock *aegis_nl_sock = NULL;
static struct aegis_client client_info = {0};
static DEFINE_SPINLOCK(client_lock);

// 验证AEGIS身份
static bool verify_aegis_identity(const char *auth_token)
{
    return strcmp(auth_token, AUTH_TOKEN) == 0;
}

// 更新客户端连接状态
static void update_client_connection(pid_t pid, bool connected, bool authenticated)
{
    unsigned long flags;

    spin_lock_irqsave(&client_lock, flags);
    client_info.pid = pid;
    client_info.connected = connected;
    client_info.authenticated = authenticated;
    spin_unlock_irqrestore(&client_lock, flags);
}

static void update_trace_binder_supported( bool supported )
{
    unsigned long flags;
    spin_lock_irqsave(&client_lock, flags);
    client_info.trace_binder_supported = supported;
    spin_unlock_irqrestore(&client_lock, flags);
}

// 获取当前客户端PID
static pid_t get_client_pid(void)
{
    unsigned long flags;
    pid_t pid;

    spin_lock_irqsave(&client_lock, flags);
    pid = client_info.connected ? client_info.pid : 0;
    spin_unlock_irqrestore(&client_lock, flags);

    return pid;
}

// 检查客户端是否已认证
static bool is_client_authenticated(void)
{
    unsigned long flags;
    bool authenticated;

    spin_lock_irqsave(&client_lock, flags);
    authenticated = client_info.authenticated;
    spin_unlock_irqrestore(&client_lock, flags);

    return authenticated;
}

// 检查客户端是否支持追踪Binder信息
static bool is_trace_binder_supported(void)
{
    unsigned long flags;
    bool supported;

    spin_lock_irqsave(&client_lock, flags);
    supported = client_info.trace_binder_supported;
    spin_unlock_irqrestore(&client_lock, flags);

    return supported;
}

// 发送消息到客户端
static int send_to_client(struct aegis_nlmsg *msg)
{
    struct sk_buff *skb;
    struct nlmsghdr *nlh;
    int ret;
    pid_t pid = get_client_pid();
    bool in_atomic_context = in_atomic() || irqs_disabled();
    //printk("AEGIS send_to_client, pid=%d, type=0x%x\n", pid, msg->type);

    if (msg->type != KERNEL_MSG_AUTH_RESPONSE && pid == 0) {
        printk(KERN_WARNING "AEGIS No connected AEGIS client to send to\n");
        return -ENOTCONN;
    }

    if (msg->type != KERNEL_MSG_AUTH_RESPONSE && !is_client_authenticated()) {
        printk(KERN_WARNING "AEGIS Client not authenticated\n");
        return -EACCES;
    }

    // 分配sk_buff
    skb = nlmsg_new(sizeof(struct aegis_nlmsg), in_atomic_context ? GFP_ATOMIC : GFP_KERNEL);
    if (!skb) {
        printk(KERN_ERR "AEGIS Failed to allocate skb for message\n");
        return -ENOMEM;
    }

    // 添加Netlink头
    nlh = nlmsg_put(skb, 0, 0, NLMSG_DONE, sizeof(struct aegis_nlmsg), 0);
    if (!nlh) {
        kfree_skb(skb);
        return -EMSGSIZE;
    }

    // 复制消息数据
    memcpy(nlmsg_data(nlh), msg, sizeof(struct aegis_nlmsg));

    // 发送到客户端
    ret = nlmsg_unicast(aegis_nl_sock, skb, pid);
    if(ret == -EAGAIN ) { //接收缓冲区已满||网络子系统临时拥塞||目标套接字处于非阻塞模式且无法立即处理消息,稍后重试
        printk(KERN_WARNING "AEGIS send_to_client EAGAIN, pid=%d, type=0x%x\n", pid, msg->type);
    }else if (ret < 0) {
        printk(KERN_ERR "AEGIS Failed to send message to client PID=%d: %d\n", pid, ret);
        // 发送失败，可能是客户端已退出
        update_client_connection(pid, false, false);
    }

    return ret;
}

// 处理认证和初始化请求
static int handle_auth_and_init_request(struct aegis_nlmsg *msg, pid_t pid)
{
    struct aegis_nlmsg resp;
    int ret;

    // 验证AEGIS身份,初始化需要先认证
    if (verify_aegis_identity(msg->params[0])) {
        printk(KERN_INFO "AEGIS client authentication successful\n");

        // 准备认证成功响应
        memset(&resp, 0, sizeof(resp));
        resp.type = KERNEL_MSG_AUTH_RESPONSE;
        strncpy(resp.params[0], "success", MAX_STR_LEN-1);
        strncpy(resp.params[1], "Authentication successful", MAX_STR_LEN-1);
        update_client_connection(pid, true, true);
    } else {
        printk(KERN_WARNING "AEGIS client authentication failed\n");

        // 准备认证失败响应
        memset(&resp, 0, sizeof(resp));
        resp.type = KERNEL_MSG_AUTH_RESPONSE;
        strncpy(resp.params[0], "failed", MAX_STR_LEN-1);
        strncpy(resp.params[1], "Invalid authentication token", MAX_STR_LEN-1);
        update_client_connection(pid, false, false);
    }

    // 发送认证响应
    ret = send_to_client(&resp);
    return ret;
}

// 处理打开或关闭binder信息上传功能
static int handle_enable_trace_binder_info(struct aegis_nlmsg *msg, pid_t pid)
{
    bool enable = strcmp(msg->params[0], "open") == 0;
    update_trace_binder_supported(enable);
    return 0;
}

// 处理来自客户端的消息
static void handle_client_message(struct aegis_nlmsg *msg, pid_t pid)
{
    int i;
    //printk(KERN_INFO "Received AEGIS message type=0x%x from PID=%d\n", msg->type, pid);

    for (i = 0; i < MAX_PARAMS; i++) {
        if (strlen(msg->params[i]) > 0) {
            printk(KERN_INFO "  Param[%d]: %s\n", i, msg->params[i]);
        }
    }

    // 根据消息类型处理
    switch (msg->type) {
        case USER_MSG_AUTH_AND_INIT_REQUEST:
            handle_auth_and_init_request(msg, pid);
            break;

        case USER_MSG_DISCONNECT:
            update_client_connection(pid, false, false);
            break;

        case USER_MSG_CALL_BACK_EVENT:
            // 处理用户态事件通知
            // 这里可以添加具体的事件处理逻辑
            printk(KERN_INFO "AEGIS User event received: %s\n", msg->params[0]);
            break;
        case USER_MSG_TRACE_BINDER_INFO:
            handle_enable_trace_binder_info(msg, pid);
            printk(KERN_INFO "AEGIS Trace Binder Info: %s\n", msg->params[0]);
            break;

        default:
            printk(KERN_WARNING "Unknown message type from AEGIS client: 0x%x\n", msg->type);
            break;
    }
}

// 内核接收消息回调
static void nl_recv_msg(struct sk_buff *skb)
{
    struct nlmsghdr *nlh;
    struct aegis_nlmsg *msg;
    pid_t pid;

    nlh = nlmsg_hdr(skb);
    pid = nlh->nlmsg_pid;

    // 验证消息
    if (nlh->nlmsg_len < NLMSG_HDRLEN || skb->len < nlh->nlmsg_len) {
        printk(KERN_ERR "AEGIS Invalid netlink message\n");
        return;
    }

    msg = NLMSG_DATA(nlh);

    // 处理客户端消息
    handle_client_message(msg, pid);
}

static void copy_param(char *dest, const char *src, size_t max_len) {
    if (src) {
        strncpy(dest, src, max_len - 1);
        dest[max_len - 1] = '\0'; // 确保以空字符结尾
    } else {
        dest[0] = '\0'; // 如果源字符串为空，直接设置为空字符串
    }
}

// 导出的内核函数 - 启动AEGIS
int startAegisK(const char *name, const char *pids, const char *extra)
{
    struct aegis_nlmsg msg;

    if (!name) {
        printk(KERN_ERR "AEGIS Invalid name\n");
        return -EINVAL;
    }
    memset(&msg, 0, sizeof(msg));
    msg.type = KERNEL_MSG_START_AEGIS;

    copy_param(msg.params[0], name, MAX_STR_LEN);
    copy_param(msg.params[1], pids, MAX_STR_LEN);
    copy_param(msg.params[2], extra, MAX_STR_LEN);

    return send_to_client(&msg);
}
EXPORT_SYMBOL(startAegisK);

int uploadBinderInfo2Aegis(const char *type, const char *binder_info,const char *extra){
    struct aegis_nlmsg msg;

    if (!type) {
        printk(KERN_ERR "AEGIS Invalid type \n");
        return -EINVAL;
    }

    if (!is_trace_binder_supported()) {
        return -EINVAL;
    }

    memset(&msg, 0, sizeof(msg));
    msg.type = KERNEL_MSG_UPLOAD_BINDER_INFO;

    copy_param(msg.params[0], type, MAX_STR_LEN);
    copy_param(msg.params[1], binder_info, MAX_STR_LEN);
    copy_param(msg.params[2], extra, MAX_STR_LEN);

    return send_to_client(&msg);
}
EXPORT_SYMBOL(uploadBinderInfo2Aegis);

static struct netlink_kernel_cfg cfg = {
    .input = nl_recv_msg,
};

static void set_netlink_buffers(struct sock *sk, int wmem, int rmem) {
    sk->sk_sndbuf = wmem; // 设置发送缓冲区大小
    sk->sk_rcvbuf = rmem; // 设置接收缓冲区大小
}

int aegisk_init(void)
{
    int size = 128 * 1024; // 设置缓冲区大小为 128KB
    // 创建Netlink socket
    aegis_nl_sock = netlink_kernel_create(&init_net, NETLINK_AEGIS, &cfg);
    if (!aegis_nl_sock) {
        printk(KERN_ALERT "Error creating AEGIS netlink socket,group=%d\n",cfg.groups);
        return -ENOMEM;
    }

    set_netlink_buffers(aegis_nl_sock, size, size);

    // 初始化客户端信息
    update_client_connection(0, false, false);

    printk(KERN_INFO "AEGIS Netlink kernel module loaded\n");
    return 0;
}

int aegisk_exit(void)
{
    // 释放Netlink socket
    if (aegis_nl_sock) {
        netlink_kernel_release(aegis_nl_sock);
    }

    printk(KERN_INFO "AEGIS Netlink kernel module unloaded\n");
    return 0;
}
