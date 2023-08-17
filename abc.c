/*******************************************************************************
 * Copyright (c) 2009, 2018 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Ian Craggs - initial API and implementation and/or initial documentation
 *    Ian Craggs - MQTT 3.1.1 support
 *    Ian Craggs - change will message test back to using proxy
 *******************************************************************************/

/**
 * @file
 * Tests for the MQ Telemetry MQTT C client
 */

#include "MQTTClient.h"
#include <string.h>
#include <stdlib.h>

#if !defined(_WINDOWS) // 非windows系统
#include <sys/time.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#else
#include <windows.h>
#define setenv(a, b, c) _putenv_s(a, b)
#endif

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0])) // The number of devices in the device struct

void usage(void) // 打印信息并退出程序
{
	printf("help!!\n");
	exit(EXIT_FAILURE);
}

struct Options
{
	char *connection; /**< connection to system under test. */ // 指向系统测试连接的字符串指针（默认值为"tcp://iot.eclipse.org:1883"）
	char **haconnections;									   // 指向HA连接的字符串指针数组（默认值为NULL）
	char *proxy_connection;									   // 指向代理连接的字符串指针（默认值为"tcp://localhost:1883"）
	int hacount;											   // HA连接的数量（默认值为0）
	int verbose;											   // 是否启用详细输出（默认值为0，即禁用）
	int test_no;											   // 测试编号（默认值为0）
	int MQTTVersion;										   // MQTT版本（默认值为3.1.1，默认值为MQTTVERSION_DEFAULT，即使用默认版本）
	int iterations;											   // 测试循环次数（默认值为1）
} options =
	{
		"tcp://iot.eclipse.org:1883",
		NULL,
		"tcp://localhost:1883",
		0,
		0,
		0,
		MQTTVERSION_DEFAULT,
		1,
};

void getopts(int argc, char **argv) // argc 表示命令行参数的数量  argv 是一个指向命令行参数字符串数组的指针
{
	int count = 1;

	while (count < argc)
	{
		if (strcmp(argv[count], "--test_no") == 0) // 如果为测试编号，将下一个参数解析为整数并赋值给测试编号
		{
			if (++count < argc)
				options.test_no = atoi(argv[count]);
			else
				usage();
		}
		else if (strcmp(argv[count], "--connection") == 0) // 如果为连接字符串，同上并打印设置的连接信息
		{
			if (++count < argc)
			{
				options.connection = argv[count];
				printf("\nSetting connection to %s\n", options.connection);
			}
			else
				usage();
		}
		else if (strcmp(argv[count], "--haconnections") == 0) // HA连接字符串
		{
			if (++count < argc)
			{
				char *tok = strtok(argv[count], " ");				// 使用strtok函数对下一个参数进行分割  使用空格作为分隔符 分割后的第一个子字符串将被赋值给指针变量tok
				options.hacount = 0;								// 初始化HA连接的数量为0
				options.haconnections = malloc(sizeof(char *) * 5); // 动态分配内存，存储最多五个字符串指针
				while (tok)											// 循环 tok非空就继续循环
				{
					options.haconnections[options.hacount] = malloc(strlen(tok) + 1); // 动态分配内存，存储字符串长度+1的字符串指针(用于存储结尾的空字符)
					strcpy(options.haconnections[options.hacount], tok);			  // strcpy函数将子字符串tok复制到字符串指针options.haconnections[options.hacount]中
					options.hacount++;												  // HA连接的数量增加1
					tok = strtok(NULL, " ");										  // 使用strtok函数对下一个参数进行分割  使用空格作为分隔 (继续获取直到所有子字符串处理完毕)
				}
			}
			else
				usage();
		}
		else if (strcmp(argv[count], "--proxy_connection") == 0) // 如果为代理连接字符串，将下一个参数赋值给他
		{
			if (++count < argc)
				options.proxy_connection = argv[count];
			else
				usage();
		}
		else if (strcmp(argv[count], "--MQTTversion") == 0) // 如果为协议版本，将下一个参数赋值给他并打印版本编号
		{
			if (++count < argc)
			{
				options.MQTTVersion = atoi(argv[count]);
				printf("setting MQTT version to %d\n", options.MQTTVersion);
			}
			else
				usage();
		}
		else if (strcmp(argv[count], "--iterations") == 0) // 如果为循环次数，将下一个参数赋值给他
		{
			if (++count < argc)
				options.iterations = atoi(argv[count]);
			else
				usage();
		}
		else if (strcmp(argv[count], "--verbose") == 0) // 如果为详细输出，将下一个参数设置为1，表示启用详细输出，并打印设置的信息。
		{
			options.verbose = 1;
			printf("\nSetting verbose on\n");
		}
		count++;
	}
}

// MyLog函数用于打印日志信息
#define LOGA_DEBUG 0
#define LOGA_INFO 1
#include <stdarg.h>
#include <time.h>
#include <sys/timeb.h>
void MyLog(int LOGA_level, char *format, ...) // MyLog函数用于打印日志信息
{
	static char msg_buf[256]; // 静态字符数组 存储格式化日志消息
	va_list args;			  // va_list 指向参数的指针 解决变参问题的一组宏
	struct timeb ts;		  // 包含有关当前时间信息 变量

	struct tm *timeinfo; // 存储本地时间信息 指针

	if (LOGA_level == LOGA_DEBUG && options.verbose == 0) // 如果日志级别为调试级别且不启用详细输出 则不打印日志信息 立即返回
		return;

	ftime(&ts);										  // 获取当前时间并进行储存
	timeinfo = localtime(&ts.time);					  // 将当前时间转换为本地时间信息 存储于timeinfo指针
	strftime(msg_buf, 80, "%Y%m%d %H%M%S", timeinfo); // 将本地时间信息按指定格式格式化为字符串 并储存于msg_buf

	sprintf(&msg_buf[strlen(msg_buf)], ".%.3hu ", ts.millitm); // 将毫秒部分转换为字符串并追加到msg_buf

	va_start(args, format);																   // 宏初始化args变量
	vsnprintf(&msg_buf[strlen(msg_buf)], sizeof(msg_buf) - strlen(msg_buf), format, args); // 根据format字符串格式化可变参数 并将结果存储在msg_buf中 msg_buf的大小限制为sizeof(msg_buf) - strlen(msg_buf)，以防止缓冲区溢出
	va_end(args);																		   // 宏清理args变量
				  // 用于计算数组 msg_buf 中剩余可用空间的大小（数组总大小减去已使用字符串长度） sizeof(msg_buf) 返回 msg_buf 数组的总大小（以字节为单位） 包括数组中的所有元素和内部填充 strlen(msg_buf) 返回以空字符 \0 结尾的字符串 msg_buf 的长度（以字节为单位），即字符串中的字符数量
	printf("%s\n", msg_buf); // 将格式化日志消息打印到标准输出
	fflush(stdout);			 // 刷新输出缓冲区 确保日志消息立即可见
}

#if defined(WIN32) || defined(_WINDOWS) // 对不同系统实现了延迟并获取计时器的值作为起始时间
#define mqsleep(A) Sleep(1000 * A)
#define START_TIME_TYPE DWORD
static DWORD start_time = 0;
START_TIME_TYPE start_clock(void)
{
	return GetTickCount();
}
#elif defined(AIX)
#define mqsleep sleep
#define START_TIME_TYPE struct timespec
START_TIME_TYPE start_clock(void)
{
	static struct timespec start;
	clock_gettime(CLOCK_REALTIME, &start);
	return start;
}
#else
#define mqsleep sleep
#define START_TIME_TYPE struct timeval
/* TODO - unused - remove? static struct timeval start_time; */
START_TIME_TYPE start_clock(void)
{
	struct timeval start_time;
	gettimeofday(&start_time, NULL);
	return start_time;
}
#endif

#if defined(WIN32) // 不同系统下计算其实是检索到当前时间的时间间隔(以毫秒为单位)
long elapsed(START_TIME_TYPE start_time)
{
	return GetTickCount() - start_time;
}
#elif defined(AIX)
#define assert(a)
long elapsed(struct timespec start)
{
	struct timespec now, res;

	clock_gettime(CLOCK_REALTIME, &now);
	ntimersub(now, start, res);
	return (res.tv_sec) * 1000L + (res.tv_nsec) / 1000000L;
}
#else
long elapsed(START_TIME_TYPE start_time)
{
	struct timeval now, res;

	gettimeofday(&now, NULL);
	timersub(&now, &start_time, &res);
	return (res.tv_sec) * 1000 + (res.tv_usec) / 1000;
}
#endif

#define assert(a, b, c, d) myassert(__FILE__, __LINE__, a, b, c, d) // 定义宏 file与line（字符串常量与整数常量，用以表示当前代码的文件名与行号） 并处理信息
#define assert1(a, b, c, d, e) myassert(__FILE__, __LINE__, a, b, c, d, e)

int tests = 0;					   // 测试数量
int failures = 0;				   // 测试失败数量
FILE *xml;						   // 文件指针，用于处理XML文件
START_TIME_TYPE global_start_time; // 起始时间类型
char output[3000];				   // 输出缓冲区
char *cur_output = output;		   // 指向output指针 跟踪当前输出位置

void write_test_result(void) // 测试结果写到XML中
{
	long duration = elapsed(global_start_time); // 时间间隔存储于duration

	fprintf(xml, " time=\"%ld.%.3ld\" >\n", duration / 1000, duration % 1000); // 时间间隔写入XML
	if (cur_output != output)
	{
		fprintf(xml, "%s", output); // 不相等则写入
		cur_output = output;		// 方便下次写入从头开始
	}
	fprintf(xml, "</testcase>\n"); // 表示当前测试结束
}

void myassert(char *filename, int lineno, char *description, int value, char *format, ...) // 断言操作
// 文件名 行号 断言描述 断言的值 记录额外信息的格式字符串
{
	++tests; // 记录总共执行的断言数量
	if (!value)
	{
		va_list args;

		++failures;																								  // 失败次数
		MyLog(LOGA_INFO, "Assertion failed, file %s, line %d, description: %s\n", filename, lineno, description); // 断言失败记录

		va_start(args, format);
		vprintf(format, args); // 打印附加信息
		va_end(args);

		cur_output += sprintf(cur_output, "<failure type=\"%s\">file %s, line %d </failure>\n",
							  description, filename, lineno); // 生成表示失败的字符串 加到cur_output后
	}
	else
		MyLog(LOGA_DEBUG, "Assertion succeeded, file %s, line %d, description: %s", filename, lineno, description); // 记录断言成功
}

/*********************************************************************

Test1: single-threaded client 单线程客户端

*********************************************************************/
void test1_sendAndReceive(MQTTClient *c, int qos, char *test_topic) // 发送和接收消息 输入分别为 c MQTT客户端指针 qos 消息的服务质量等级 test_topic 测试主题的名称
{
	MQTTClient_deliveryToken dt;								// 消息传递令牌
	MQTTClient_message pubmsg = MQTTClient_message_initializer; // 客户端消息初始化结构体
	MQTTClient_message *m = NULL;								// 指向MQTT客户端指针
	char *topicName = NULL;										// 主题名称指针
	int topicLen;												// 主题名称长度
	int i = 0;													// 循环计数变量
	int iterations = 50;										// 收发消息的迭代次数
	int rc;														// MQTT客户端操作的结果代码

	MyLog(LOGA_DEBUG, "%d messages at QoS %d", iterations, qos);												 // 日志信息
	pubmsg.payload = "a much longer message that we can shorten to the extent that we need to payload up to 11"; // 发送消息内容
	pubmsg.payloadlen = 11;																						 // 消息长度
	pubmsg.qos = qos;																							 // 服务质量等级
	pubmsg.retained = 0;																						 // 是否保留

	for (i = 0; i < iterations; ++i) // 执行迭代次数
	{
		if (i % 10 == 0) // 是否为10的倍数 不同函数发送信息
			rc = MQTTClient_publish(c, test_topic, pubmsg.payloadlen, pubmsg.payload, pubmsg.qos, pubmsg.retained, &dt);
		else
			rc = MQTTClient_publishMessage(c, test_topic, &pubmsg, &dt);
		assert("Good rc from publish", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);

		if (qos > 0) // 若服务质量等级足够 等待消息传递完成
		{
			rc = MQTTClient_waitForCompletion(c, dt, 5000L);
			assert("Good rc from waitforCompletion", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);
		}

		rc = MQTTClient_receive(c, &topicName, &topicLen, &m, 5000); // 接收消息并将个参数赋值给rc
		assert("Good rc from receive", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);
		if (topicName) // 非空都继续进行
		{
			MyLog(LOGA_DEBUG, "Message received on topic %s is %.*s", topicName, m->payloadlen, (char *)(m->payload)); // 日志信息
			if (pubmsg.payloadlen != m->payloadlen ||
				memcmp(m->payload, pubmsg.payload, m->payloadlen) != 0) // 有效载荷长度是否相等  或 memcmp 比较前两者的消息长度数目的字节 是否相等
			{
				failures++;																						  // 不相等则错误次数增加
				MyLog(LOGA_INFO, "Error: wrong data - received lengths %d %d", pubmsg.payloadlen, m->payloadlen); // 报错信息
				break;
			}
			MQTTClient_free(topicName); // 释放分配主题名称的内存
			MQTTClient_freeMessage(&m); // 释放分配消息结构体的内存
		}
		else
			printf("No message received within timeout period\n"); // 输出没收到消息
	}

	/* receive any outstanding messages */
	MQTTClient_receive(c, &topicName, &topicLen, &m, 2000); // 接收消息并赋值给topicname
	while (topicName)										// 判断其是否存在
	{
		printf("Message received on topic %s is %.*s.\n", topicName, m->payloadlen, (char *)(m->payload)); // 打印接收消息
		MQTTClient_free(topicName);																		   // 释放先前分配的内存
		MQTTClient_freeMessage(&m);																		   // 释放先前分配结构体的内存
		MQTTClient_receive(c, &topicName, &topicLen, &m, 2000);											   // 接收下一条消息 并持续更新
	}
}

int test1(struct Options options)
{
	int subsqos = 2;
	MQTTClient c; // 初始化客户端变量
	MQTTClient_connectOptions opts = MQTTClient_connectOptions_initializer;
	MQTTClient_willOptions wopts = MQTTClient_willOptions_initializer;
	int rc = 0;
	char *test_topic = "C client test1";

	fprintf(xml, "<testcase classname=\"test1\" name=\"single threaded client using receive\""); // 写入文件 表明测试用例
	global_start_time = start_clock();															 // 起始时间函数
	failures = 0;
	MyLog(LOGA_INFO, "Starting test 1 - single threaded client using receive");

	rc = MQTTClient_create(&c, options.connection, "single_threaded_test",
						   MQTTCLIENT_PERSISTENCE_DEFAULT, NULL);				// 创建客户端
	assert("good rc from create", rc == MQTTCLIENT_SUCCESS, "rc was %d\n", rc); // 验证
	if (rc != MQTTCLIENT_SUCCESS)
	{
		MQTTClient_destroy(&c); // 不成功则销毁
		goto exit;				//
	}

	opts.keepAliveInterval = 20; //.代表结构体下的变量 指向opts下的参数
	opts.cleansession = 1;
	opts.username = "testuser";
	opts.password = "testpassword";
	opts.MQTTVersion = options.MQTTVersion;
	if (options.haconnections != NULL)
	{
		opts.serverURIs = options.haconnections;
		opts.serverURIcount = options.hacount;
	}

	opts.will = &wopts;					 // 代表opts.will指向了wopts结构体的地址
	opts.will->message = "will message"; // 以下均为wopts指针指向了message等wopts下的message等
	opts.will->qos = 1;
	opts.will->retained = 0;
	opts.will->topicName = "will topic";
	opts.will = NULL;

	MyLog(LOGA_DEBUG, "Connecting");
	rc = MQTTClient_connect(c, &opts); // 客户端与连接选项建立代理连接 成功后赋值给rc
	assert("Good rc from connect", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);
	if (rc != MQTTCLIENT_SUCCESS)
		goto exit;

	rc = MQTTClient_subscribe(c, test_topic, subsqos); // 订阅指定主题并赋值
	assert("Good rc from subscribe", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);

	test1_sendAndReceive(c, 0, test_topic); // 0 1 2等级向指定主题接送并收发消息
	test1_sendAndReceive(c, 1, test_topic);
	test1_sendAndReceive(c, 2, test_topic);

	MyLog(LOGA_DEBUG, "Stopping\n"); // 操作结束

	rc = MQTTClient_unsubscribe(c, test_topic); // 取消订阅主题
	assert("Unsubscribe successful", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);
	rc = MQTTClient_disconnect(c, 0); // 断开代理连接
	assert("Disconnect successful", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);

	/* Just to make sure we can connect again */
	rc = MQTTClient_connect(c, &opts); // 重连
	assert("Connect successful", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);
	rc = MQTTClient_disconnect(c, 0); // 断开连接
	assert("Disconnect successful", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);

	MQTTClient_destroy(&c); // 销毁客户端对象 释放相关资源

exit:
	MyLog(LOGA_INFO, "TEST1: test %s. %d tests run, %d failures.",
		  (failures == 0) ? "passed" : "failed", tests, failures); // 失败日志
	write_test_result();										   // 存储错误信息
	return failures;											   // 返回失败次数
}

/*********************************************************************

Test2: multi-threaded client using callbacks

*********************************************************************/
volatile int test2_arrivedcount = 0;							  // 防止test2_arrivedcount变量被优化
int test2_deliveryCompleted = 0;								  // 记录消息传递的次数
MQTTClient_message test2_pubmsg = MQTTClient_message_initializer; // 结构体进行初始化

void test2_deliveryComplete(void *context, MQTTClient_deliveryToken dt) // 回调函数 用于表示消息传递的完成次数
{
	++test2_deliveryCompleted;
}

int test2_messageArrived(void *context, char *topicName, int topicLen, MQTTClient_message *m) // 消息到达的判断函数
{
	++test2_arrivedcount; // 记录已收到的消息数量
	MyLog(LOGA_DEBUG, "Callback: %d message received on topic %s is %.*s.",
		  test2_arrivedcount, topicName, m->payloadlen, (char *)(m->payload)); // 信息日志
	if (test2_pubmsg.payloadlen != m->payloadlen ||
		memcmp(m->payload, test2_pubmsg.payload, m->payloadlen) != 0) // 判断消息是否一致
	{
		failures++;
		MyLog(LOGA_INFO, "Error: wrong data received lengths %d %d\n", test2_pubmsg.payloadlen, m->payloadlen); // 错误次数及日志
	}
	MQTTClient_free(topicName); // 释放内存
	MQTTClient_freeMessage(&m);
	return 1;
}

void test2_sendAndReceive(MQTTClient *c, int qos, char *test_topic) // 收发消息
{
	MQTTClient_deliveryToken dt; // 消息传递令牌
	int i = 0;
	int iterations = 50;  // 迭代次数
	int rc = 0;			  // 结果代码rc
	int wait_seconds = 0; // 等待时间

	test2_deliveryCompleted = 0; // 发送完成标志

	MyLog(LOGA_INFO, "%d messages at QoS %d", iterations, qos);										  // 日志
	test2_pubmsg.payload = "a much longer message that we can shorten to the extent that we need to"; // 结构体消息
	test2_pubmsg.payloadlen = 27;																	  // 空格导致27长度
	test2_pubmsg.qos = qos;
	test2_pubmsg.retained = 0;

	for (i = 1; i <= iterations; ++i)
	{
		if (i % 10 == 0) // 消息发送
			rc = MQTTClient_publish(c, test_topic, test2_pubmsg.payloadlen, test2_pubmsg.payload,
									test2_pubmsg.qos, test2_pubmsg.retained, NULL);
		else
			rc = MQTTClient_publishMessage(c, test_topic, &test2_pubmsg, &dt);
		assert("Good rc from publish", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);

#if defined(WIN32) // 100ms暂停
		Sleep(100);
#else
		usleep(100000L);
#endif

		wait_seconds = 10;
		while ((test2_arrivedcount < i) && (wait_seconds-- > 0)) // 判断数据到达次数及等待时间
		{
			MyLog(LOGA_DEBUG, "Arrived %d count %d", test2_arrivedcount, i);
#if defined(WIN32) // 100ms暂停
			Sleep(1000);
#else
			usleep(1000000L);
#endif
		}
		assert("Message Arrived", wait_seconds > 0,
			   "Time out waiting for message %d\n", i); // 断言
	}
	if (qos > 0) // qos大于0情况下的操作
	{
		/* MQ Telemetry can send a message to a subscriber before the server has
		   completed the QoS 2 handshake with the publisher. For QoS 1 and 2,
		   allow time for the final delivery complete callback before checking
		   that all expected callbacks have been made */
		// 留出时间进行最终交付并回调 对于QoS 1和2，服务器可能在与发布者进行QoS 2握手之前，
		// 就向订阅者发送消息。通过等待交付完成的回调，可以确保在检查所有预期的回调之前，允许足够的时间进行交付完成。
		wait_seconds = 10;
		while ((test2_deliveryCompleted < iterations) && (wait_seconds-- > 0)) // 完成足够迭代次数并有足够等待时间
		{
			MyLog(LOGA_DEBUG, "Delivery Completed %d count %d", test2_deliveryCompleted, i);
#if defined(WIN32) // 时间等待
			Sleep(1000);
#else
			usleep(1000000L);
#endif
		}
		assert("All Deliveries Complete", wait_seconds > 0,
			   "Number of deliveryCompleted callbacks was %d\n",
			   test2_deliveryCompleted); // 断言
	}
}

int test2(struct Options options)
{
	char *testname = "test2";
	int subsqos = 2; // 订阅消息指令等级
	/* TODO - usused - remove ? MQTTClient_deliveryToken* dt = NULL; */
	MQTTClient c;
	MQTTClient_connectOptions opts = MQTTClient_connectOptions_initializer;
	int rc = 0;
	char *test_topic = "C client test2";

	fprintf(xml, "<testcase classname=\"test1\" name=\"multi-threaded client using callbacks\"");
	MyLog(LOGA_INFO, "Starting test 2 - multi-threaded client using callbacks");
	global_start_time = start_clock();
	failures = 0;

	MQTTClient_create(&c, options.connection, "multi_threaded_sample", MQTTCLIENT_PERSISTENCE_DEFAULT, NULL); // 创建客户端并初始化

	opts.keepAliveInterval = 20;
	opts.cleansession = 1;
	opts.MQTTVersion = options.MQTTVersion;
	opts.username = "testuser";
	opts.binarypwd.data = "testpassword";
	opts.binarypwd.len = (int)strlen(opts.binarypwd.data);
	if (options.haconnections != NULL)
	{
		opts.serverURIs = options.haconnections;
		opts.serverURIcount = options.hacount;
	}

	rc = MQTTClient_setCallbacks(c, NULL, NULL, test2_messageArrived, test2_deliveryComplete); // 回调函数
	assert("Good rc from setCallbacks", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);

	MyLog(LOGA_DEBUG, "Connecting");
	rc = MQTTClient_connect(c, &opts); // 连接
	assert("Good rc from connect", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);
	if (rc != MQTTCLIENT_SUCCESS)
		goto exit;

	rc = MQTTClient_subscribe(c, test_topic, subsqos); // 订阅主题
	assert("Good rc from subscribe", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);

	test2_sendAndReceive(c, 0, test_topic); // 收发消息
	test2_sendAndReceive(c, 1, test_topic);
	test2_sendAndReceive(c, 2, test_topic);

	MyLog(LOGA_DEBUG, "Stopping");

	rc = MQTTClient_unsubscribe(c, test_topic); // 取消订阅
	assert("Unsubscribe successful", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);
	rc = MQTTClient_disconnect(c, 0); // 断连
	assert("Disconnect successful", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);

	MQTTClient_destroy(&c); // 销毁客户端

exit:
	MyLog(LOGA_INFO, "%s: test %s. %d tests run, %d failures.",
		  (failures == 0) ? "passed" : "failed", testname, tests, failures);
	write_test_result(); // 错误信息
	return failures;
}

/*********************************************************************

Test 3: connack return codes

for AMQTDD, needs an amqtdd.cfg of:

	allow_anonymous false
  password_file passwords

and a passwords file of:

	Admin:Admin

*********************************************************************/
int test3(struct Options options)
{
	char *testname = "test3";
	int rc;
	MQTTClient c;
	MQTTClient_connectOptions opts = MQTTClient_connectOptions_initializer;
	MQTTClient_willOptions wopts = MQTTClient_willOptions_initializer;

	fprintf(xml, "<testcase classname=\"test1\" name=\"connack return codes\"");
	global_start_time = start_clock();
	failures = 0;
	MyLog(LOGA_INFO, "Starting test 3 - connack return codes");

#if 0
	/* clientid too long (RC = 2) */
	rc = MQTTClient_create(&c, options.connection, "client_ID_too_long_for_MQTT_protocol_version_3",
		MQTTCLIENT_PERSISTENCE_NONE, NULL);
	assert("good rc from create",  rc == MQTTCLIENT_SUCCESS, "rc was %d\n", rc);
	rc = MQTTClient_connect(c, &opts);
	assert("identifier rejected", rc == 2, "rc was %d\n", rc);
	MQTTClient_destroy(&c);
#endif
	/* broker unavailable (RC = 3)  - TDD when allow_anonymous not set*/
	rc = MQTTClient_create(&c, options.connection, "The C Client", MQTTCLIENT_PERSISTENCE_NONE, NULL);
	assert("good rc from create", rc == MQTTCLIENT_SUCCESS, "rc was %d\n", rc);
#if 0
	rc = MQTTClient_connect(c, &opts);
	assert("broker unavailable", rc == 3, "rc was %d\n", rc);

	/* authentication failure (RC = 4) */
	opts.username = "Admin";
	opts.password = "fred";
	rc = MQTTClient_connect(c, &opts);
	assert("Bad user name or password", rc == 4, "rc was %d\n", rc);
#endif

	/* authorization failure (RC = 5) */
	opts.username = "Admin";
	opts.password = "Admin";
	/*opts.will = &wopts;    "Admin" not authorized to publish to Will topic by default
	opts.will->message = "will message";
	opts.will->qos = 1;
	opts.will->retained = 0;
	opts.will->topicName = "will topic";*/
	rc = MQTTClient_connect(c, &opts);
	// assert("Not authorized", rc == 5, "rc was %d\n", rc);

#if 0
	/* successful connection (RC = 0) */
	opts.username = "Admin";
	opts.password = "Admin";
  opts.will = NULL;
	rc = MQTTClient_connect(c, &opts);
	assert("successful connection", rc == MQTTCLIENT_SUCCESS,  "rc was %d\n", rc);
	MQTTClient_disconnect(c, 0);
	MQTTClient_destroy(&c);
#endif

	/* TODO - unused - remove ? exit: */
	MyLog(LOGA_INFO, "%s: test %s. %d tests run, %d failures.",
		  (failures == 0) ? "passed" : "failed", testname, tests, failures);
	write_test_result();
	return failures;
}

/*********************************************************************

Test 4: client persistence 1


*********************************************************************/
int test4_run(int qos)
{
	char *testname = "test 4";
	char *topic = "Persistence test 1";
	int subsqos = 2;
	MQTTClient c;
	MQTTClient_connectOptions opts = MQTTClient_connectOptions_initializer;
	MQTTClient_message *m = NULL;
	char *topicName = NULL;
	int topicLen;
	MQTTClient_deliveryToken *tokens = NULL;
	int mytoken = -99;
	char buffer[100];
	int count = 3;
	int i, rc;

	failures = 0;
	MyLog(LOGA_INFO, "Starting test 4 - persistence, qos %d", qos);

	MQTTClient_create(&c, options.connection, "xrctest1_test_4", MQTTCLIENT_PERSISTENCE_DEFAULT, NULL);

	opts.keepAliveInterval = 20;
	opts.reliable = 0;
	opts.MQTTVersion = options.MQTTVersion;
	if (options.haconnections != NULL)
	{
		opts.serverURIs = options.haconnections;
		opts.serverURIcount = options.hacount;
	}

	MyLog(LOGA_DEBUG, "Cleanup by connecting clean session\n");
	opts.cleansession = 1;
	if ((rc = MQTTClient_connect(c, &opts)) != 0)
	{
		assert("Good rc from connect", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);
		return -1;
	}
	opts.cleansession = 0;
	MQTTClient_disconnect(c, 0);

	MyLog(LOGA_DEBUG, "Connecting\n");
	if ((rc = MQTTClient_connect(c, &opts)) != 0)
	{
		assert("Good rc from connect", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);
		return -1;
	}

	/* subscribe so we can get messages back */
	rc = MQTTClient_subscribe(c, topic, subsqos);
	assert("Good rc from subscribe", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);

	/* send messages so that we can receive the same ones */
	for (i = 0; i < count; ++i)
	{
		sprintf(buffer, "Message sequence no %d", i);
		rc = MQTTClient_publish(c, topic, 10, buffer, qos, 0, NULL);
		assert("Good rc from publish", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);
	}

	/* disconnect immediately without receiving the incoming messages */
	MQTTClient_disconnect(c, 0); /* now there should be "orphaned" publications */

	rc = MQTTClient_getPendingDeliveryTokens(c, &tokens);
	assert("getPendingDeliveryTokens rc == 0", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);

	assert("should get some tokens back", tokens != NULL, "tokens was %p", tokens);
	if (tokens)
	{
		int i = 0;

		while (tokens[i] != -1)
			MyLog(LOGA_DEBUG, "Pending delivery token %d", tokens[i++]);
		MQTTClient_free(tokens);
		assert1("no of tokens should be count", i == count, "no of tokens %d count %d", i, count);
		mytoken = tokens[0];
	}

	MQTTClient_destroy(&c); /* force re-reading persistence on create */

	MQTTClient_create(&c, options.connection, "xrctest1_test_4", MQTTCLIENT_PERSISTENCE_DEFAULT, NULL);

	rc = MQTTClient_getPendingDeliveryTokens(c, &tokens);
	assert("getPendingDeliveryTokens rc == 0", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);

	assert("should get some tokens back", tokens != NULL, "tokens was %p", tokens);
	if (tokens)
	{
		int i = 0;
		while (tokens[i] != -1)
			MyLog(LOGA_DEBUG, "Pending delivery token %d", tokens[i++]);
		MQTTClient_free(tokens);
		assert1("no of tokens should be count", i == count, "no of tokens %d count %d", i, count);
	}

	MyLog(LOGA_DEBUG, "Reconnecting");
	if (MQTTClient_connect(c, &opts) != 0)
	{
		assert("Good rc from connect", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);
		return -1;
	}

	for (i = 0; i < count; ++i)
	{
		int dup = 0;
		do
		{
			dup = 0;
			MQTTClient_receive(c, &topicName, &topicLen, &m, 5000);
			if (m && m->dup)
			{
				assert("No duplicates should be received for qos 2", qos == 1, "qos is %d", qos);
				MyLog(LOGA_DEBUG, "Duplicate message id %d", m->msgid);
				MQTTClient_freeMessage(&m);
				MQTTClient_free(topicName);
				dup = 1;
			}
		} while (dup == 1);
		assert("should get a message", m != NULL, "m was %p", m);
		if (m)
		{
			MyLog(LOGA_DEBUG, "Received message id %d", m->msgid);
			assert("topicName is correct", strcmp(topicName, topic) == 0, "topicName is %s", topicName);
			MQTTClient_freeMessage(&m);
			MQTTClient_free(topicName);
		}
	}

	MQTTClient_yield(); /* allow any unfinished protocol exchanges to finish */

	rc = MQTTClient_getPendingDeliveryTokens(c, &tokens);
	assert("getPendingDeliveryTokens rc == 0", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);
	assert("should get no tokens back", tokens == NULL, "tokens was %p", tokens);

	MQTTClient_disconnect(c, 0);

	MQTTClient_destroy(&c);

	/* TODO - unused -remove? exit: */
	MyLog(LOGA_INFO, "%s: test %s. %d tests run, %d failures.",
		  (failures == 0) ? "passed" : "failed", testname, tests, failures);

	return failures;
}

int test4(struct Options options)
{
	int rc = 0;
	fprintf(xml, "<testcase classname=\"test1\" name=\"persistence\"");
	global_start_time = start_clock();
	rc = test4_run(1) + test4_run(2);
	fprintf(xml, " time=\"%ld\" >\n", elapsed(global_start_time) / 1000);
	if (cur_output != output)
	{
		fprintf(xml, "%s", output);
		cur_output = output;
	}
	fprintf(xml, "</testcase>\n");
	return rc;
}

/*********************************************************************

Test 5: disconnect with quiesce timeout should allow exchanges to complete

*********************************************************************/
int test5(struct Options options)
{
	char *testname = "test 5";
	char *topic = "Persistence test 2";
	int subsqos = 2;
	MQTTClient c;
	MQTTClient_connectOptions opts = MQTTClient_connectOptions_initializer;
	MQTTClient_deliveryToken *tokens = NULL;
	char buffer[100];
	int count = 5;
	int i, rc;

	fprintf(xml, "<testcase classname=\"test1\" name=\"disconnect with quiesce timeout should allow exchanges to complete\"");
	global_start_time = start_clock();
	failures = 0;
	MyLog(LOGA_INFO, "Starting test 5 - disconnect with quiesce timeout should allow exchanges to complete");

	MQTTClient_create(&c, options.connection, "xrctest1_test_5", MQTTCLIENT_PERSISTENCE_DEFAULT, NULL);

	opts.keepAliveInterval = 20;
	opts.cleansession = 0;
	opts.reliable = 0;
	opts.MQTTVersion = options.MQTTVersion;
	if (options.haconnections != NULL)
	{
		opts.serverURIs = options.haconnections;
		opts.serverURIcount = options.hacount;
	}

	MyLog(LOGA_DEBUG, "Connecting");
	if ((rc = MQTTClient_connect(c, &opts)) != 0)
	{
		assert("Good rc from connect", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);
		MQTTClient_destroy(&c);
		goto exit;
	}

	rc = MQTTClient_subscribe(c, topic, subsqos);
	assert("Good rc from subscribe", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);

	for (i = 0; i < count; ++i)
	{
		sprintf(buffer, "Message sequence no %d", i);
		rc = MQTTClient_publish(c, topic, 10, buffer, 1, 0, NULL);
		assert("Good rc from publish", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);
	}

	MQTTClient_disconnect(c, 1000); /* now there should be no "orphaned" publications */
	MyLog(LOGA_DEBUG, "Disconnected");

	rc = MQTTClient_getPendingDeliveryTokens(c, &tokens);
	assert("getPendingDeliveryTokens rc == 0", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);

	assert("should get no tokens back", tokens == NULL, "tokens was %p", tokens);

	MQTTClient_destroy(&c);

exit:
	MyLog(LOGA_INFO, "%s: test %s. %d tests run, %d failures.",
		  (failures == 0) ? "passed" : "failed", testname, tests, failures);
	write_test_result();
	return failures;
}

/*********************************************************************

Test 6: connectionLost and will message

*********************************************************************/
MQTTClient test6_c1, test6_c2;
volatile int test6_will_message_arrived = 0;
volatile int test6_connection_lost_called = 0;

void test6_connectionLost(void *context, char *cause)
{
	MQTTClient c = (MQTTClient)context;
	printf("%s -> Callback: connection lost\n", (c == test6_c1) ? "Client-1" : "Client-2");
	test6_connection_lost_called = 1;
}

void test6_deliveryComplete(void *context, MQTTClient_deliveryToken token)
{
	printf("Client-2 -> Callback: publish complete for token %d\n", token);
}

char *test6_will_topic = "C Test 2: will topic";
char *test6_will_message = "will message from Client-1";

int test6_messageArrived(void *context, char *topicName, int topicLen, MQTTClient_message *m)
{
	MQTTClient c = (MQTTClient)context;
	printf("%s -> Callback: message received on topic '%s' is '%.*s'.\n",
		   (c == test6_c1) ? "Client-1" : "Client-2", topicName, m->payloadlen, (char *)(m->payload));
	if (c == test6_c2 && strcmp(topicName, test6_will_topic) == 0 && memcmp(m->payload, test6_will_message, m->payloadlen) == 0)
		test6_will_message_arrived = 1;
	MQTTClient_free(topicName);
	MQTTClient_freeMessage(&m);
	return 1;
}

int test6(struct Options options)
{
	char *testname = "test6";
	MQTTClient_connectOptions opts = MQTTClient_connectOptions_initializer;
	MQTTClient_willOptions wopts = MQTTClient_willOptions_initializer;
	MQTTClient_connectOptions opts2 = MQTTClient_connectOptions_initializer;
	int rc, count;
	char *mqttsas_topic = "MQTTSAS topic";

	failures = 0;
	MyLog(LOGA_INFO, "Starting test 6 - connectionLost and will messages");
	fprintf(xml, "<testcase classname=\"test1\" name=\"connectionLost and will messages\"");
	global_start_time = start_clock();

	opts.keepAliveInterval = 2;
	opts.cleansession = 1;
	opts.MQTTVersion = MQTTVERSION_3_1_1;
	opts.will = &wopts;
	opts.will->message = test6_will_message;
	opts.will->qos = 1;
	opts.will->retained = 0;
	opts.will->topicName = test6_will_topic;
	if (options.haconnections != NULL)
	{
		opts.serverURIs = options.haconnections;
		opts.serverURIcount = options.hacount;
	}

	/* Client-1 with Will options */
	rc = MQTTClient_create(&test6_c1, options.proxy_connection, "Client_1", MQTTCLIENT_PERSISTENCE_DEFAULT, NULL);
	assert("good rc from create", rc == MQTTCLIENT_SUCCESS, "rc was %d\n", rc);
	if (rc != MQTTCLIENT_SUCCESS)
		goto exit;

	rc = MQTTClient_setCallbacks(test6_c1, (void *)test6_c1, test6_connectionLost, test6_messageArrived, test6_deliveryComplete);
	assert("good rc from setCallbacks", rc == MQTTCLIENT_SUCCESS, "rc was %d\n", rc);
	if (rc != MQTTCLIENT_SUCCESS)
		goto exit;

	/* Connect to the broker */
	rc = MQTTClient_connect(test6_c1, &opts);
	assert("good rc from connect", rc == MQTTCLIENT_SUCCESS, "rc was %d\n", rc);
	if (rc != MQTTCLIENT_SUCCESS)
		goto exit;

	/* Client - 2 (multi-threaded) */
	rc = MQTTClient_create(&test6_c2, options.connection, "Client_2", MQTTCLIENT_PERSISTENCE_DEFAULT, NULL);
	assert("good rc from create", rc == MQTTCLIENT_SUCCESS, "rc was %d\n", rc);

	/* Set the callback functions for the client */
	rc = MQTTClient_setCallbacks(test6_c2, (void *)test6_c2, test6_connectionLost, test6_messageArrived, test6_deliveryComplete);
	assert("good rc from setCallbacks", rc == MQTTCLIENT_SUCCESS, "rc was %d\n", rc);

	/* Connect to the broker */
	opts2.keepAliveInterval = 20;
	opts2.cleansession = 1;
	MyLog(LOGA_INFO, "Connecting Client_2 ...");
	rc = MQTTClient_connect(test6_c2, &opts2);
	assert("Good rc from connect", rc == MQTTCLIENT_SUCCESS, "rc was %d\n", rc);

	rc = MQTTClient_subscribe(test6_c2, test6_will_topic, 2);
	assert("Good rc from subscribe", rc == MQTTCLIENT_SUCCESS, "rc was %d\n", rc);

	/* now send the command which will break the connection and cause the will message to be sent */
	rc = MQTTClient_publish(test6_c1, mqttsas_topic, (int)strlen("TERMINATE"), "TERMINATE", 0, 0, NULL);
	assert("Good rc from publish", rc == MQTTCLIENT_SUCCESS, "rc was %d\n", rc);

	MyLog(LOGA_INFO, "Waiting to receive the will message");
	count = 0;
	while (++count < 40)
	{
#if defined(WIN32)
		Sleep(1000L);
#else
		sleep(1);
#endif
		if (test6_will_message_arrived == 1 && test6_connection_lost_called == 1)
			break;
	}
	assert("will message arrived", test6_will_message_arrived == 1,
		   "will_message_arrived was %d\n", test6_will_message_arrived);
	assert("connection lost called", test6_connection_lost_called == 1,
		   "connection_lost_called %d\n", test6_connection_lost_called);

	rc = MQTTClient_unsubscribe(test6_c2, test6_will_topic);
	assert("Good rc from unsubscribe", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);

	rc = MQTTClient_isConnected(test6_c2);
	assert("Client-2 still connected", rc == 1, "isconnected is %d", rc);

	rc = MQTTClient_isConnected(test6_c1);
	assert("Client-1 not connected", rc == 0, "isconnected is %d", rc);

	rc = MQTTClient_disconnect(test6_c2, 100L);
	assert("Good rc from disconnect", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);

	MQTTClient_destroy(&test6_c1);
	MQTTClient_destroy(&test6_c2);

exit:
	MyLog(LOGA_INFO, "%s: test %s. %d tests run, %d failures.\n",
		  (failures == 0) ? "passed" : "failed", testname, tests, failures);
	write_test_result();
	return failures;
}

int test6a(struct Options options)
{
	char *testname = "test6a";
	MQTTClient_connectOptions opts = MQTTClient_connectOptions_initializer;
	MQTTClient_willOptions wopts = MQTTClient_willOptions_initializer;
	MQTTClient_connectOptions opts2 = MQTTClient_connectOptions_initializer;
	int rc, count;
	char *mqttsas_topic = "MQTTSAS topic";

	failures = 0;
	MyLog(LOGA_INFO, "Starting test 6 - connectionLost and binary will messages");
	fprintf(xml, "<testcase classname=\"test1\" name=\"connectionLost and binary will messages\"");
	global_start_time = start_clock();

	opts.keepAliveInterval = 2;
	opts.cleansession = 1;
	opts.MQTTVersion = MQTTVERSION_3_1_1;
	opts.will = &wopts;
	opts.will->payload.data = test6_will_message;
	opts.will->payload.len = (int)strlen(test6_will_message) + 1;
	opts.will->qos = 1;
	opts.will->retained = 0;
	opts.will->topicName = test6_will_topic;
	if (options.haconnections != NULL)
	{
		opts.serverURIs = options.haconnections;
		opts.serverURIcount = options.hacount;
	}

	/* Client-1 with Will options */
	rc = MQTTClient_create(&test6_c1, options.proxy_connection, "Client_1", MQTTCLIENT_PERSISTENCE_DEFAULT, NULL);
	assert("good rc from create", rc == MQTTCLIENT_SUCCESS, "rc was %d\n", rc);
	if (rc != MQTTCLIENT_SUCCESS)
		goto exit;

	rc = MQTTClient_setCallbacks(test6_c1, (void *)test6_c1, test6_connectionLost, test6_messageArrived, test6_deliveryComplete);
	assert("good rc from setCallbacks", rc == MQTTCLIENT_SUCCESS, "rc was %d\n", rc);
	if (rc != MQTTCLIENT_SUCCESS)
		goto exit;

	/* Connect to the broker */
	rc = MQTTClient_connect(test6_c1, &opts);
	assert("good rc from connect", rc == MQTTCLIENT_SUCCESS, "rc was %d\n", rc);
	if (rc != MQTTCLIENT_SUCCESS)
		goto exit;

	/* Client - 2 (multi-threaded) */
	rc = MQTTClient_create(&test6_c2, options.connection, "Client_2", MQTTCLIENT_PERSISTENCE_DEFAULT, NULL);
	assert("good rc from create", rc == MQTTCLIENT_SUCCESS, "rc was %d\n", rc);

	/* Set the callback functions for the client */
	rc = MQTTClient_setCallbacks(test6_c2, (void *)test6_c2, test6_connectionLost, test6_messageArrived, test6_deliveryComplete);
	assert("good rc from setCallbacks", rc == MQTTCLIENT_SUCCESS, "rc was %d\n", rc);

	/* Connect to the broker */
	opts2.keepAliveInterval = 20;
	opts2.cleansession = 1;
	MyLog(LOGA_INFO, "Connecting Client_2 ...");
	rc = MQTTClient_connect(test6_c2, &opts2);
	assert("Good rc from connect", rc == MQTTCLIENT_SUCCESS, "rc was %d\n", rc);

	rc = MQTTClient_subscribe(test6_c2, test6_will_topic, 2);
	assert("Good rc from subscribe", rc == MQTTCLIENT_SUCCESS, "rc was %d\n", rc);

	/* now send the command which will break the connection and cause the will message to be sent */
	rc = MQTTClient_publish(test6_c1, mqttsas_topic, (int)strlen("TERMINATE"), "TERMINATE", 0, 0, NULL);
	assert("Good rc from publish", rc == MQTTCLIENT_SUCCESS, "rc was %d\n", rc);

	MyLog(LOGA_INFO, "Waiting to receive the will message");
	count = 0;
	while (++count < 40)
	{
#if defined(WIN32)
		Sleep(1000L);
#else
		sleep(1);
#endif
		if (test6_will_message_arrived == 1 && test6_connection_lost_called == 1)
			break;
	}
	assert("will message arrived", test6_will_message_arrived == 1,
		   "will_message_arrived was %d\n", test6_will_message_arrived);
	assert("connection lost called", test6_connection_lost_called == 1,
		   "connection_lost_called %d\n", test6_connection_lost_called);

	rc = MQTTClient_unsubscribe(test6_c2, test6_will_topic);
	assert("Good rc from unsubscribe", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);

	rc = MQTTClient_isConnected(test6_c2);
	assert("Client-2 still connected", rc == 1, "isconnected is %d", rc);

	rc = MQTTClient_isConnected(test6_c1);
	assert("Client-1 not connected", rc == 0, "isconnected is %d", rc);

	rc = MQTTClient_disconnect(test6_c2, 100L);
	assert("Good rc from disconnect", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);

	MQTTClient_destroy(&test6_c1);
	MQTTClient_destroy(&test6_c2);

exit:
	MyLog(LOGA_INFO, "%s: test %s. %d tests run, %d failures.\n",
		  (failures == 0) ? "passed" : "failed", testname, tests, failures);
	write_test_result();
	return failures;
}

int main(int argc, char **argv)
{
	int rc = 0;
	int (*tests[])() = {NULL, test1, test2, test3, test4, test5, test6, test6a};
	int i;

	xml = fopen("TEST-test1.xml", "w");
	fprintf(xml, "<testsuite name=\"test1\" tests=\"%d\">\n", (int)(ARRAY_SIZE(tests) - 1));

	setenv("MQTT_C_CLIENT_TRACE", "ON", 1);
	setenv("MQTT_C_CLIENT_TRACE_LEVEL", "ERROR", 1);

	getopts(argc, argv);

	for (i = 0; i < options.iterations; ++i)
	{
		if (options.test_no == 0)
		{ /* run all the tests */
			for (options.test_no = 1; options.test_no < ARRAY_SIZE(tests); ++options.test_no)
				rc += tests[options.test_no](options); /* return number of failures.  0 = test succeeded */
		}
		else
			rc = tests[options.test_no](options); /* run just the selected test */
	}

	if (rc == 0)
		MyLog(LOGA_INFO, "verdict pass");
	else
		MyLog(LOGA_INFO, "verdict fail");

	fprintf(xml, "</testsuite>\n");
	fclose(xml);
	return rc;
}
